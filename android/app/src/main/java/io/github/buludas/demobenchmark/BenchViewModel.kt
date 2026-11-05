package io.github.buludas.demobenchmark

import android.app.Application
import android.content.*
import android.media.*
import android.os.*
import android.view.Surface
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import androidx.room.Room
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.*
import org.json.JSONObject
import java.io.File
import java.util.UUID
import kotlin.coroutines.resume

enum class Screen { Home,Running,Result,History,Credits }
data class UiState(val options:Options=Options(),val screen:Screen=Screen.Home,val caps:JSONObject=JSONObject(),val ready:Boolean=false,val busy:Boolean=false,val live:JSONObject=JSONObject(),val result:RunResult?=null,val notice:String="",val runPortrait:Boolean=false)
data class Request(val id:String,val started:Long,val options:Options,val measuring:Boolean,val duration:Double=0.0,val capture:Boolean=false,val first:Int=0,val diagnostic:Boolean=false)
class BenchViewModel(app:Application):AndroidViewModel(app) {
    private val context=app
    private val settings=Settings(app)
    private val db=Room.databaseBuilder(app,ResultsDatabase::class.java,"results.db").build()
    private val dao=db.runs()
    val history=dao.observe().stateIn(viewModelScope,SharingStarted.WhileSubscribed(5000),emptyList())
    private val mutable=MutableStateFlow(UiState())
    val ui:StateFlow<UiState> = mutable.asStateFlow()
    private val guard=Any();private var handle=0L
    private var request:Request?=null;private var task:Job?=null;private var cancelReason="";private var returnHome=false
    private var resultReturn=Screen.Home
    private var workerConnection:ServiceConnection?=null;private var shared:SharedMemory?=null
    private val audioManager=app.getSystemService(AudioManager::class.java)
    private var focus:AudioFocusRequest?=null
    var allowSoftware=false
    private val power=app.getSystemService(PowerManager::class.java)
    private val thermalListener=PowerManager.OnThermalStatusChangedListener { status->if(status>=PowerManager.THERMAL_STATUS_CRITICAL&&ui.value.busy)stop("端末温度による中断") }
    init {
        power.addThermalStatusListener(thermalListener)
        viewModelScope.launch {settings.values.collect { o->if(!ui.value.busy)mutable.update {it.copy(options=o)} }}
        viewModelScope.launch {
            withContext(Dispatchers.IO){for(row in dao.pending()){val d=row.data.put("phase","finished").put("complete",false).put("score",JSONObject.NULL).put("reason","前回の計測が中断されました");dao.save(row.copy(json=d.toString(),pending=false))}}
            val caps=withContext(Dispatchers.IO){try{JSONObject(NativeEngine.capabilities())}catch(e:Throwable){JSONObject().put("error",e.message)}}
            mutable.update { s->s.copy(caps=caps,ready=true,options=if(s.options.api==1&&!caps.optBoolean("vulkan"))s.options.copy(api=0)else s.options) }
        }
    }
    fun supported(api:Int)=ui.value.caps.optBoolean(if(api==0)"gles" else "vulkan")
    fun hardware(api:Int)=ui.value.caps.optBoolean(if(api==0)"hardware" else "vulkanHardware")
    fun configure(o:Options){if(ui.value.busy)return;mutable.update {it.copy(options=o,notice="")};viewModelScope.launch {settings.save(o)}}
    fun navigate(screen:Screen){if(!ui.value.busy)mutable.update {it.copy(screen=screen)}}
    fun toggleRunOrientation(){mutable.update {if(it.busy&&it.screen==Screen.Running)it.copy(runPortrait=!it.runPortrait)else it}}
    fun openResult(row:RunResult){resultReturn=ui.value.screen;mutable.update {it.copy(result=row,screen=Screen.Result)}}
    fun back(){when(ui.value.screen){Screen.Running->stop("中断",true);Screen.Result->mutable.update {it.copy(screen=resultReturn)};Screen.Credits,Screen.History->mutable.update {it.copy(screen=Screen.Home)};else->{}}}
    fun begin(measuring:Boolean,duration:Double=0.0,capture:Boolean=false,first:Int=0,diagnostic:Boolean=false){
        if(ui.value.busy||!ui.value.ready||!supported(ui.value.options.api))return
        if(measuring&&!hardware(ui.value.options.api)&&!allowSoftware)return
        val options=if(diagnostic)ui.value.options.copy(demo=0,muted=true)else ui.value.options
        if(!options.muted){
            val f=AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT).setAudioAttributes(AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build()).setOnAudioFocusChangeListener { if(it<0)stop("音声フォーカス喪失") }.setWillPauseWhenDucked(true).build()
            if(audioManager.requestAudioFocus(f)!=AudioManager.AUDIOFOCUS_REQUEST_GRANTED){mutable.update {it.copy(notice="音声を使用できません。音声をオフにして試してください。")};return}
            focus=f
        }
        cancelReason="";returnHome=false
        val r=Request(UUID.randomUUID().toString(),System.currentTimeMillis(),options,measuring,if(BuildConfig.DEBUG)duration else 0.0,BuildConfig.DEBUG&&capture,first,diagnostic)
        request=r
        mutable.update {it.copy(busy=true,live=JSONObject().put("phase","preparing"),runPortrait=false)}
        viewModelScope.launch {
            if(measuring)withContext(Dispatchers.IO){dao.save(RunResult(r.id,r.started,options.demo,options.api,options.quality,JSONObject().put("reason","準備中").put("score",JSONObject.NULL).toString(),true))}
            if(cancelReason.isBlank())mutable.update {it.copy(screen=Screen.Running)}else finishBeforeStart(r)
        }
    }
    private suspend fun bindWorker(r:Request):SharedMemory {
        val memory=SharedMemory.create("SecondReality",NativeEngine.sharedSize());NativeEngine.initializeShared(memory);shared=memory
        suspendCancellableCoroutine<Unit> { continuation->
            val connection=object:ServiceConnection {
                override fun onServiceConnected(name:ComponentName,binder:IBinder){
                    try{val message=Message.obtain(null,1);message.data=Bundle().apply{putParcelable("shared",memory);putBoolean("muted",r.options.muted);putInt("first",r.first)};Messenger(binder).send(message);if(continuation.isActive)continuation.resume(Unit)}catch(e:Exception){if(continuation.isActive)continuation.cancel(e)}
                }
                override fun onServiceDisconnected(name:ComponentName){if(ui.value.busy)stop("デモ処理が終了しました")}
            };workerConnection=connection
            if(!context.bindService(Intent(context,SecondRealityService::class.java),connection,Context.BIND_AUTO_CREATE))continuation.cancel(IllegalStateException("Second Realityを起動できません"))
        }
        return memory
    }
    private fun conditions():JSONObject {
        val battery=context.registerReceiver(null,IntentFilter(Intent.ACTION_BATTERY_CHANGED));val t=battery?.getIntExtra(BatteryManager.EXTRA_TEMPERATURE,Int.MIN_VALUE)?:Int.MIN_VALUE
        return JSONObject().put("powerSave",power.isPowerSaveMode).put("thermal",power.currentThermalStatus).put("batteryTemperature",if(t==Int.MIN_VALUE)JSONObject.NULL else t/10.0).put("charging",(battery?.getIntExtra(BatteryManager.EXTRA_PLUGGED,0)?:0)!=0)
    }
    fun displayPulse(timeNs:Long,periodNs:Long){synchronized(guard){if(handle!=0L)NativeEngine.displayPulse(handle,timeNs,periodNs)}}
    fun surfaceReady(surface:Surface){
        val r=request?:return;if(task?.isActive==true||cancelReason.isNotBlank())return
        task=viewModelScope.launch {
            var poll:Job?=null;var nativeResult:JSONObject;val diagnosticConditions=org.json.JSONArray()
            val initial=conditions()
            try {
                val memory=if(r.options.demo==1)bindWorker(r)else null
                if(cancelReason.isNotBlank())throw CancellationException(cancelReason)
                val cfg=JSONObject().put("demo",r.options.demo).put("api",r.options.api).put("quality",r.options.quality).put("muted",r.options.muted).put("measuring",r.measuring).put("testDuration",r.duration).put("diagnostic",r.diagnostic)
                if(r.capture)cfg.put("captureDir",File(context.filesDir,"captures").absolutePath)
                synchronized(guard){handle=NativeEngine.create();if(cancelReason.isNotBlank())NativeEngine.cancel(handle,cancelReason)}
                poll=launch(Dispatchers.IO){while(isActive){val raw=synchronized(guard){if(handle!=0L)NativeEngine.snapshot(handle)else "{}"};val snapshot=JSONObject(raw);if(r.diagnostic)diagnosticConditions.put(conditions().put("elapsed",snapshot.optDouble("elapsed",0.0)).put("stage",snapshot.optInt("diagnosticStage")));mutable.update {it.copy(live=snapshot)};delay(if(r.diagnostic)1000 else 250)}}
                val raw=withContext(Dispatchers.IO){NativeEngine.run(handle,context.assets,surface,cfg.toString(),memory)}
                nativeResult=JSONObject(raw)
            }catch(e:Throwable){nativeResult=JSONObject().put("complete",false).put("score",JSONObject.NULL).put("reason",cancelReason.ifBlank {"実行できませんでした"}).put("errorDetail",e.message)}
            finally {
                withContext(NonCancellable){poll?.cancelAndJoin();withContext(Dispatchers.IO){synchronized(guard){if(handle!=0L){NativeEngine.destroy(handle);handle=0L}}};workerConnection?.let {try{context.unbindService(it)}catch(_:Exception){}};workerConnection=null;shared?.close();shared=null;focus?.let {audioManager.abandonAudioFocusRequest(it)};focus=null}
            }
            nativeResult.put("id",r.id).put("started",r.started).put("appVersion",BuildConfig.VERSION_NAME).put("demo",r.options.demo).put("api",r.options.api).put("quality",r.options.quality).put("model",Build.MANUFACTURER+" "+Build.MODEL).put("sdk",Build.VERSION.SDK_INT).put("osRelease",Build.VERSION.RELEASE).put("osBuild",Build.DISPLAY).put("conditionStart",initial).put("conditionEnd",conditions())
            if(r.diagnostic)nativeResult.put("diagnosticConditions",diagnosticConditions)
            if(Build.VERSION.SDK_INT>=31)nativeResult.put("soc",Build.SOC_MODEL)
            val row=RunResult(r.id,r.started,r.options.demo,r.options.api,r.options.quality,nativeResult.toString())
            withContext(Dispatchers.IO){if(r.measuring)dao.save(row);File(context.filesDir,"latest-result.json").writeText(row.json)}
            request=null;resultReturn=Screen.Home;mutable.update {it.copy(busy=false,result=row,live=nativeResult,notice=if(!r.measuring&&nativeResult.has("errorDetail"))"再生できませんでした。描画方式か品質を変更してください。" else "",screen=if(returnHome||!r.measuring)Screen.Home else Screen.Result)}
        }
    }
    private suspend fun finishBeforeStart(r:Request){val d=JSONObject().put("complete",false).put("score",JSONObject.NULL).put("reason",cancelReason);val row=RunResult(r.id,r.started,r.options.demo,r.options.api,r.options.quality,d.toString());if(r.measuring)withContext(Dispatchers.IO){dao.save(row)};request=null;focus?.let {audioManager.abandonAudioFocusRequest(it)};focus=null;mutable.update {it.copy(busy=false,screen=Screen.Home)}}
    fun stop(reason:String="中断",home:Boolean=false){
        if(!ui.value.busy)return;if(cancelReason.isBlank())cancelReason=reason;returnHome=returnHome||home
        synchronized(guard){if(handle!=0L)NativeEngine.cancel(handle,cancelReason)}
        if(task?.isActive!=true&&ui.value.screen==Screen.Running){val r=request;request=null;if(r!=null)viewModelScope.launch{finishBeforeStart(r)}}
        if(home)mutable.update {it.copy(screen=Screen.Home)}
    }
    fun surfaceLost(){if(ui.value.busy)stop("画面が変更されました")}
    fun background(){if(ui.value.busy)stop("バックグラウンド移行",true)}
    override fun onCleared(){stop("終了");power.removeThermalStatusListener(thermalListener);super.onCleared()}
}
