@file:OptIn(androidx.compose.material3.ExperimentalMaterial3Api::class,androidx.compose.material3.ExperimentalMaterial3ExpressiveApi::class)
package io.github.buludas.demobenchmark

import android.content.Intent
import android.content.pm.ActivityInfo
import android.os.Bundle
import android.view.Choreographer
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.compose.PredictiveBackHandler
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.compose.foundation.*
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.selection.toggleable
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.*
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.content.FileProvider
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.flow.first
import java.io.File
import java.text.NumberFormat
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class MainActivity:ComponentActivity() {
    private val model:BenchViewModel by viewModels()
    override fun onCreate(savedInstanceState:Bundle?){
        super.onCreate(savedInstanceState);enableEdgeToEdge()
        model.allowSoftware=BuildConfig.DEBUG&&intent.getBooleanExtra("qa_allow_software",false)
        setContent { App(model,this) }
        if(BuildConfig.DEBUG&&intent.getBooleanExtra("qa_autostart",false)){
            lifecycleScopeLaunch {
                model.ui.first {it.ready};model.configure(Options(demo=intent.getIntExtra("qa_demo",0),api=intent.getIntExtra("qa_api",0),medianQuality=intent.getIntExtra("qa_quality",0),srQuality=intent.getIntExtra("qa_quality",0).coerceIn(0,1),muted=true));delay(200)
                model.begin(true,intent.getStringExtra("qa_duration")?.toDoubleOrNull()?:4.5,intent.getBooleanExtra("qa_capture",false),intent.getIntExtra("qa_sr_part",0),intent.getBooleanExtra("qa_diagnostic",false))
            }
        }
    }
    private fun lifecycleScopeLaunch(block:suspend ()->Unit){lifecycleScope.launch{block()}}
    override fun onStop(){model.background();super.onStop()}
}

@Composable private fun App(vm:BenchViewModel,activity:ComponentActivity){
    val state by vm.ui.collectAsStateWithLifecycle()
    val systemDark=isSystemInDarkTheme();val dark=when(state.options.theme){1->false;2->true;else->systemDark}
    val colors=if(android.os.Build.VERSION.SDK_INT>=31){if(dark)dynamicDarkColorScheme(activity)else dynamicLightColorScheme(activity)}else if(dark)darkColorScheme(primary=Color(0xFFA9D0AE),onPrimary=Color(0xFF14381E),primaryContainer=Color(0xFF2D4F34),onPrimaryContainer=Color(0xFFC4ECCC))else lightColorScheme(primary=Color(0xFF47664E),onPrimary=Color.White,primaryContainer=Color(0xFFC8ECCE),onPrimaryContainer=Color(0xFF10351D))
    MaterialExpressiveTheme(colorScheme=colors){
        val inRun=state.screen==Screen.Running
        DisposableEffect(inRun){
            val previous=activity.requestedOrientation
            onDispose{if(inRun)activity.requestedOrientation=previous}
        }
        LaunchedEffect(inRun,state.runPortrait){
            if(inRun)activity.requestedOrientation=if(state.runPortrait)ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT else ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        }
        DisposableEffect(inRun,dark){val controller=WindowCompat.getInsetsController(activity.window,activity.window.decorView);controller.isAppearanceLightStatusBars=!inRun&&!dark;controller.isAppearanceLightNavigationBars=!inRun&&!dark;if(inRun){activity.window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);controller.systemBarsBehavior=WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE;controller.hide(WindowInsetsCompat.Type.systemBars())}else{activity.window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);controller.show(WindowInsetsCompat.Type.systemBars())};onDispose{activity.window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);controller.show(WindowInsetsCompat.Type.systemBars())}}
        PredictiveBackHandler(enabled=state.screen!=Screen.Home){events->try{events.collect{};vm.back()}catch(_:CancellationException){}}
        Surface(Modifier.fillMaxSize(),color=MaterialTheme.colorScheme.background){
            when(state.screen){
                Screen.Home->Home(vm,state)
                Screen.Running->Running(vm,state)
                Screen.Result->state.result?.let{ResultPage(vm,it)}
                Screen.History->History(vm)
                Screen.Credits->Credits(vm)
            }
        }
    }
}
private fun integer(value:Long)=NumberFormat.getIntegerInstance().format(value)
private fun decimal(value:Double)=String.format(Locale.getDefault(),"%.1f",value)
private fun clock(seconds:Double):String {val s=seconds.toInt().coerceAtLeast(0);return "%d:%02d".format(s/60,s%60)}
@Composable private fun Bar(title:String,back:(()->Unit)?=null,actions:@Composable RowScope.()->Unit={}){
    val scale=LocalDensity.current.fontScale
    TopAppBar(title={Text(title,maxLines=2,style=if(scale>1.3)MaterialTheme.typography.titleMedium else MaterialTheme.typography.titleLarge)},navigationIcon={if(back!=null)IconButton(onClick=back){Icon(Icons.AutoMirrored.Filled.ArrowBack,"戻る")}},actions=actions,expandedHeight=(64*scale.coerceAtLeast(1f)).dp,colors=TopAppBarDefaults.topAppBarColors(containerColor=MaterialTheme.colorScheme.background))
}
@Composable private fun Home(vm:BenchViewModel,state:UiState){
    var picker by remember {mutableStateOf<String?>(null)};var menu by remember {mutableStateOf(false)};val o=state.options
    Scaffold(topBar={Bar("Demo Benchmark",actions={IconButton(onClick={vm.navigate(Screen.History)},enabled=!state.busy){Icon(Icons.Outlined.History,"履歴")};Box{IconButton(onClick={menu=true},enabled=!state.busy){Icon(Icons.Filled.MoreVert,"その他")};DropdownMenu(expanded=menu,onDismissRequest={menu=false}){DropdownMenuItem(text={Text("外観")},onClick={menu=false;picker="theme"});DropdownMenuItem(text={Text("クレジット")},onClick={menu=false;vm.navigate(Screen.Credits)})}}})}){padding->
        BoxWithConstraints(Modifier.fillMaxSize().padding(padding)){
            val wide=maxWidth>=600.dp
            val settings:@Composable ()->Unit={
                Column(verticalArrangement=Arrangement.spacedBy(16.dp)){
                    Text("計測",style=MaterialTheme.typography.headlineLarge)
                    Card(shape=MaterialTheme.shapes.extraLarge,colors=CardDefaults.cardColors(containerColor=MaterialTheme.colorScheme.surfaceContainerLow)){
                        Choice("計測する映像",o.demoName,{picker="demo"},!state.busy)
                        HorizontalDivider(Modifier.padding(horizontal=20.dp),color=MaterialTheme.colorScheme.outlineVariant)
                        Choice("レンダリング方式",o.apiName,{picker="api"},!state.busy)
                        HorizontalDivider(Modifier.padding(horizontal=20.dp),color=MaterialTheme.colorScheme.outlineVariant)
                        Choice("描画品質",o.qualityName+" · "+o.resolution,{picker="quality"},!state.busy)
                    }
                    ListItem(headlineContent={Text("音声")},trailingContent={Switch(checked=!o.muted,onCheckedChange=null)},modifier=Modifier.toggleable(value=!o.muted,enabled=!state.busy,role=Role.Switch,onValueChange={vm.configure(o.copy(muted=!it))}),colors=ListItemDefaults.colors(containerColor=Color.Transparent))
                }
            }
            val actions:@Composable ()->Unit={
                Column(Modifier.fillMaxWidth(),verticalArrangement=Arrangement.spacedBy(12.dp)){
                    if(state.notice.isNotBlank())Text(state.notice,color=MaterialTheme.colorScheme.error,style=MaterialTheme.typography.bodyMedium)
                    if(state.ready&&!vm.supported(o.api))Text("この端末では描画方式を使用できません",style=MaterialTheme.typography.bodyMedium)
                    else if(state.ready&&!vm.hardware(o.api)&&!vm.allowSoftware)Text("このGPUは性能計測の対象外です",style=MaterialTheme.typography.bodyMedium)
                    Button(onClick={vm.begin(true)},enabled=state.ready&&!state.busy&&vm.supported(o.api)&&(vm.hardware(o.api)||vm.allowSoftware),modifier=Modifier.fillMaxWidth().heightIn(min=64.dp)){Icon(Icons.Filled.PlayArrow,null);Spacer(Modifier.width(8.dp));Text("計測する",style=MaterialTheme.typography.titleMedium)}
                    if(o.demo==0)TextButton(onClick={vm.begin(true,diagnostic=true)},enabled=state.ready&&!state.busy&&vm.supported(o.api)&&(vm.hardware(o.api)||vm.allowSoftware)){Text("描画診断")}
                    FilledTonalButton(onClick={vm.begin(false)},enabled=state.ready&&!state.busy&&vm.supported(o.api),modifier=Modifier.fillMaxWidth().heightIn(min=56.dp)){Text("再生",style=MaterialTheme.typography.titleMedium)}
                }
            }
            if(wide){Row(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(24.dp),horizontalArrangement=Arrangement.spacedBy(32.dp),verticalAlignment=Alignment.CenterVertically){Box(Modifier.weight(1f)){settings()};Box(Modifier.widthIn(max=300.dp).weight(.75f)){actions()}}}
            else Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(horizontal=24.dp).padding(top=16.dp,bottom=24.dp),verticalArrangement=Arrangement.spacedBy(28.dp)){settings();actions()}
        }
    }
    if(picker!=null){val kind=picker!!;val entries=when(kind){"demo"->listOf("Median" to "3D描画・約130秒","Second Reality" to "表示・音声・同期");"api"->listOf("OpenGL ES 3.0" to if(vm.supported(0))"" else "非対応","Vulkan" to if(vm.supported(1))"" else "非対応");"theme"->listOf("端末の設定" to "","ライト" to "","ダーク" to "");else->if(o.demo==0)listOf("最低" to "640 × 360 · MSAAなし","低" to "960 × 540 · MSAAなし","中" to "1280 × 720 · MSAA 2倍","高" to "1600 × 900 · MSAA 4倍","最高" to "1920 × 1080 · MSAA 4倍")else listOf("原画質" to "640 × 480","高画質（補間）" to "1440 × 1080")};val selected=when(kind){"demo"->o.demo;"api"->o.api;"theme"->o.theme;else->o.quality};val title=when(kind){"demo"->"計測する映像";"api"->"レンダリング方式";"theme"->"外観";else->"描画品質"}
        ModalBottomSheet(onDismissRequest={picker=null},sheetState=rememberModalBottomSheetState(skipPartiallyExpanded=true)){
            Text(title,Modifier.padding(horizontal=24.dp,vertical=12.dp),style=MaterialTheme.typography.headlineSmall)
            LazyColumn(Modifier.fillMaxWidth().heightIn(max=460.dp).navigationBarsPadding()){items(entries.size){i->val enabled=kind!="api"||vm.supported(i);ListItem(headlineContent={Text(entries[i].first)},supportingContent={if(entries[i].second.isNotBlank())Text(entries[i].second)},leadingContent={RadioButton(selected=i==selected,onClick=null,enabled=enabled)},modifier=Modifier.selectable(selected=i==selected,enabled=enabled,role=Role.RadioButton,onClick={vm.configure(when(kind){"demo"->o.copy(demo=i);"api"->o.copy(api=i);"theme"->o.copy(theme=i);else->o.withQuality(i)});picker=null}).heightIn(min=64.dp))}}
        }
    }
}
@Composable private fun Choice(label:String,value:String,onClick:()->Unit,enabled:Boolean){ListItem(headlineContent={Text(label)},supportingContent={Text(value,style=MaterialTheme.typography.bodyLarge)},trailingContent={Icon(Icons.Filled.KeyboardArrowDown,null)},modifier=Modifier.clickable(enabled=enabled,role=Role.Button,onClick=onClick).heightIn(min=80.dp),colors=ListItemDefaults.colors(containerColor=Color.Transparent))}
@Composable private fun Running(vm:BenchViewModel,state:UiState){
    val live=state.live;val phase=live.optString("phase","preparing")
    val displayView=LocalView.current
    DisposableEffect(vm){
        val choreographer=Choreographer.getInstance()
        val callback=object:Choreographer.FrameCallback{override fun doFrame(timeNs:Long){
            val hz=displayView.display?.refreshRate?.takeIf{it>0}?:60f
            vm.displayPulse(timeNs,(1_000_000_000.0/hz).toLong());choreographer.postFrameCallback(this)
        }}
        choreographer.postFrameCallback(callback)
        onDispose{choreographer.removeFrameCallback(callback)}
    }
    Box(Modifier.fillMaxSize().background(Color.Black)){
        BoxWithConstraints(Modifier.fillMaxSize()){
            val renderWidth=state.options.renderWidth;val renderHeight=state.options.renderHeight
            val aspect=renderWidth.toFloat()/renderHeight
            val displayWidth=minOf(maxWidth,maxHeight*aspect)
            AndroidView(factory={context->SurfaceView(context).apply{
                val view=this
                holder.setFixedSize(renderWidth,renderHeight)
                holder.addCallback(object:SurfaceHolder.Callback{
                    private var started=false
                    private val start=Runnable{if(holder.surface.isValid){started=true;vm.surfaceReady(holder.surface)}}
                    override fun surfaceCreated(holder:SurfaceHolder){view.postDelayed(start,250)}
                    override fun surfaceChanged(holder:SurfaceHolder,format:Int,width:Int,height:Int){if(!started){view.removeCallbacks(start);view.postDelayed(start,250)}}
                    override fun surfaceDestroyed(holder:SurfaceHolder){view.removeCallbacks(start);if(started)vm.surfaceLost()}
                })
            }},modifier=Modifier.align(Alignment.Center).size(displayWidth,displayWidth/aspect))
        }
        Column(Modifier.align(Alignment.TopStart).windowInsetsPadding(WindowInsets.safeDrawing.only(WindowInsetsSides.Top+WindowInsetsSides.Horizontal)).padding(16.dp).widthIn(max=320.dp),verticalArrangement=Arrangement.spacedBy(8.dp)){
            Surface(shape=MaterialTheme.shapes.large,color=MaterialTheme.colorScheme.surfaceContainerHigh.copy(alpha=.94f)){
                Column(Modifier.padding(horizontal=16.dp,vertical=12.dp),verticalArrangement=Arrangement.spacedBy(4.dp)){
                    if(phase=="preparing"){
                        Row(verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(8.dp)){LoadingIndicator(Modifier.size(32.dp));Text("準備中",style=MaterialTheme.typography.titleMedium)}
                    }else{
                        FlowRow(horizontalArrangement=Arrangement.spacedBy(16.dp)){Text(decimal(live.optDouble("fps",0.0))+" FPS",style=MaterialTheme.typography.titleMedium);if(live.optBoolean("diagnostic"))Text("診断 "+live.optInt("diagnosticStage")+" / 6") else if(live.optBoolean("measuring",true))Text((if(live.isNull("score"))"—" else integer(live.optLong("score")))+" pts",style=MaterialTheme.typography.titleMedium)}
                        val elapsed=live.optDouble("elapsed",0.0);val duration=live.optDouble("duration",0.0)
                        Text(if(phase=="warmup")"ウォームアップ" else if(duration>0)"残り "+clock(duration-elapsed)else "経過 "+clock(elapsed),style=MaterialTheme.typography.bodyMedium)
                    }
                }
            }
        }
        Row(Modifier.align(Alignment.BottomCenter).fillMaxWidth().windowInsetsPadding(WindowInsets.safeDrawing.only(WindowInsetsSides.Bottom+WindowInsetsSides.Horizontal)).padding(16.dp),horizontalArrangement=Arrangement.SpaceBetween,verticalAlignment=Alignment.CenterVertically){
            FilledTonalButton(onClick={vm.toggleRunOrientation()},modifier=Modifier.heightIn(min=56.dp)){Icon(Icons.Filled.ScreenRotation,null);Spacer(Modifier.width(8.dp));Text(if(state.runPortrait)"横向き" else "縦向き")}
            FilledTonalButton(onClick={vm.stop()},modifier=Modifier.heightIn(min=56.dp)){Icon(Icons.Filled.Stop,null);Spacer(Modifier.width(8.dp));Text("停止")}
        }
    }
}
@Composable private fun ResultPage(vm:BenchViewModel,row:RunResult){
    val context=LocalContext.current;val data=row.data;var details by remember(row.id){mutableStateOf(false)}
    Scaffold(topBar={Bar("結果",{vm.back()})}){padding->Column(Modifier.fillMaxSize().padding(padding).verticalScroll(rememberScrollState()).padding(24.dp),verticalArrangement=Arrangement.spacedBy(20.dp)){
        Text(if(data.optBoolean("diagnostic"))"描画診断" else if(data.optBoolean("integratedMedian"))"総合スコア" else if(row.demo==0)"スコア" else "表示スコア",style=MaterialTheme.typography.titleMedium)
        Text(row.score?.let{integer(it)}?:"—",style=if(LocalDensity.current.fontScale>1.3)MaterialTheme.typography.headlineMedium else MaterialTheme.typography.displayMedium)
        if(row.score!=null)Text("pts",style=MaterialTheme.typography.bodyLarge)
        Text(row.assessment,style=MaterialTheme.typography.headlineSmall)
        Text(row.options.demoName+" / "+row.options.apiName+"\n"+row.options.qualityName+" · "+data.optInt("width",0)+" × "+data.optInt("height",0)+(if(data.has("samples"))" · MSAA "+data.optInt("samples")+"×" else ""),style=MaterialTheme.typography.bodyLarge)
        Card(colors=CardDefaults.cardColors(containerColor=MaterialTheme.colorScheme.surfaceContainerLow)){
            if(data.optBoolean("diagnostic")){val stages=data.optJSONArray("diagnosticStages");if(stages!=null)for(i in 0 until stages.length()){val s=stages.getJSONObject(i);Metric((i+1).toString()+" · "+listOf("描画のみ","総合負荷あり","総合負荷・旧表示","描画のみ・旧表示","表示時刻指定なし","描画のみ・再確認")[i.coerceAtMost(5)],decimal(s.optDouble("renderFps"))+" 描画FPS / "+decimal(s.getJSONObject("presentation").optDouble("fps"))+" 表示更新FPS")}}else Metric(if(data.has("presentation"))"平均表示更新FPS" else "平均描画FPS",if(data.optLong("frames")>0)decimal(row.fps)else "—")
            if(!data.isNull("onePercentLow"))Metric("1% Low",decimal(data.optDouble("onePercentLow")))

            if(row.demo==1)Metric("ソース更新FPS",if(data.has("sourceFps"))decimal(data.optDouble("sourceFps"))else "—")
        }
        Button(onClick={vm.configure(row.options.copy(muted=vm.ui.value.options.muted,theme=vm.ui.value.options.theme));vm.navigate(Screen.Home);vm.begin(true,diagnostic=data.optBoolean("diagnostic"))},enabled=!vm.ui.value.busy&&vm.supported(row.api)&&(vm.hardware(row.api)||vm.allowSoftware),modifier=Modifier.fillMaxWidth().heightIn(min=56.dp)){Text(if(data.optBoolean("diagnostic"))"もう一度診断" else "もう一度計測")}
        OutlinedButton(onClick={val dir=File(context.cacheDir,"shared").apply{mkdirs()};val file=File(dir,"${row.id}.json").apply{writeText(row.json)};val uri=FileProvider.getUriForFile(context,BuildConfig.APPLICATION_ID+".files",file);context.startActivity(Intent.createChooser(Intent(Intent.ACTION_SEND).setType("application/json").putExtra(Intent.EXTRA_STREAM,uri).addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION),"結果を共有"))},modifier=Modifier.fillMaxWidth().heightIn(min=48.dp)){Text("共有")}
        TextButton(onClick={details=!details}){Text(if(details)"詳細を閉じる" else "詳細")}
        if(details){if(data.has("renderFps"))Metric("画面外の描画FPS",decimal(data.optDouble("renderFps")));if(data.has("presentation"))Text("FPS計測："+data.getJSONObject("presentation").optString("source"));Text("Android描画性能計測 · "+data.optString("appVersion","—")+" · "+data.optString("profile","android-offscreen-v1"),style=MaterialTheme.typography.bodyMedium);Text(data.optString("model")+"\n"+data.optString("renderer")+"\n"+data.optString("driver"),style=MaterialTheme.typography.bodyMedium);Text("計測フレーム "+integer(data.optLong("frames")),style=MaterialTheme.typography.bodyMedium);if(data.has("audioUnderruns"))Text("音声アンダーラン "+data.optInt("audioUnderruns"));val conditions=data.optJSONObject("conditionEnd");if(conditions!=null&&!conditions.isNull("batteryTemperature"))Text("バッテリー温度 "+decimal(conditions.optDouble("batteryTemperature"))+" ℃");val error=data.optString("errorDetail");if(error.isNotBlank())Text(error.take(1200),style=MaterialTheme.typography.bodySmall)}
    }}
}
@Composable private fun Metric(label:String,value:String){ListItem(headlineContent={Text(label)},supportingContent={Text(value,style=MaterialTheme.typography.titleLarge)},colors=ListItemDefaults.colors(containerColor=Color.Transparent))}
@Composable private fun History(vm:BenchViewModel){val rows by vm.history.collectAsStateWithLifecycle();Scaffold(topBar={Bar("履歴",{vm.back()})}){padding->if(rows.isEmpty())Box(Modifier.fillMaxSize().padding(padding),contentAlignment=Alignment.Center){Text("計測結果はまだありません")}else LazyColumn(Modifier.fillMaxSize().padding(padding),contentPadding=PaddingValues(vertical=12.dp)){items(rows,key={it.id}){row->ListItem(headlineContent={Text(row.options.demoName+" · "+row.options.qualityName)},supportingContent={Text(row.options.apiName+"\n"+SimpleDateFormat("MM/dd HH:mm",Locale.getDefault()).format(Date(row.started)))},trailingContent={Text(row.score?.let{integer(it)+" pts"}?:if(row.pending)"計測中" else "—")},modifier=Modifier.clickable(role=Role.Button){vm.openResult(row)});HorizontalDivider(Modifier.padding(horizontal=16.dp))}}}}
@Composable private fun Credits(vm:BenchViewModel){val context=LocalContext.current;val credits=remember {context.assets.open("CREDITS.txt").bufferedReader().use{it.readText()}};Scaffold(topBar={Bar("クレジット",{vm.back()})}){padding->Column(Modifier.fillMaxSize().padding(padding).verticalScroll(rememberScrollState()).padding(24.dp),verticalArrangement=Arrangement.spacedBy(20.dp)){Text("Demo Benchmark "+BuildConfig.VERSION_NAME,style=MaterialTheme.typography.titleLarge);Text(credits,style=MaterialTheme.typography.bodyMedium);Text("Android: Kotlin / Jetpack Compose / Material 3 Expressive / AndroidX / Oboe / nlohmann JSON / stb_image",style=MaterialTheme.typography.bodyMedium);val names=remember{context.assets.list("")?.filter{it.contains("LICENSE",true)||it.contains("COPYING",true)}?.sorted().orEmpty()};var selected by remember{mutableStateOf<String?>(null)};for(name in names){TextButton(onClick={selected=if(selected==name)null else name}){Text(name)};if(selected==name)Text(remember(name){context.assets.open(name).bufferedReader().use{it.readText()}},style=MaterialTheme.typography.bodySmall)}}}}
