package io.github.buludas.demobenchmark
import android.content.res.AssetManager
import android.os.SharedMemory
import android.view.Surface

object NativeEngine {
    init { System.loadLibrary("demobenchmark") }
    external fun capabilities():String
    external fun create():Long
    external fun cancel(handle:Long,reason:String)
    external fun snapshot(handle:Long):String
    external fun run(handle:Long,assets:AssetManager,surface:Surface,config:String,shared:SharedMemory?):String
    external fun destroy(handle:Long)
    external fun sharedSize():Int
    external fun initializeShared(shared:SharedMemory)
    external fun worker(shared:SharedMemory,muted:Boolean,first:Int)
}
