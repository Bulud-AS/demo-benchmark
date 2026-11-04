package io.github.buludas.demobenchmark
import android.app.Service
import android.content.Intent
import android.os.*

class SecondRealityService:Service() {
    private var active=false
    private val messenger=Messenger(Handler(Looper.getMainLooper()) { message ->
        if(message.what==1&&!active){
            val shared=message.data.getParcelable<SharedMemory>("shared")
            val muted=message.data.getBoolean("muted")
            val first=message.data.getInt("first")
            if(shared!=null){active=true;Thread({
                try{NativeEngine.worker(shared,muted,first)}finally{shared.close();stopSelf()}
            },"SecondReality").start()}
        };true
    })
    override fun onBind(intent:Intent):IBinder=messenger.binder
    override fun onDestroy(){super.onDestroy();Process.killProcess(Process.myPid())}
}
