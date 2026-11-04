package io.github.buludas.demobenchmark

import android.content.Context
import androidx.datastore.preferences.core.*
import androidx.datastore.preferences.preferencesDataStore
import androidx.room.*
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import org.json.JSONObject

data class Options(val demo:Int=0,val api:Int=0,val medianQuality:Int=1,val srQuality:Int=1,val muted:Boolean=false,val theme:Int=0) {
    val quality:Int get()=if(demo==0)medianQuality else srQuality
    val demoName:String get()=if(demo==0)"Median" else "Second Reality"
    val apiName:String get()=if(api==0)"OpenGL ES 3.0" else "Vulkan"
    val qualityName:String get()=if(demo==0)listOf("最低","低","中","高","最高")[quality.coerceIn(0,4)] else if(quality==0)"原画質" else "高画質（補間）"
    val renderWidth:Int get()=if(demo==1){if(quality==0)640 else 1440}else listOf(640,960,1280,1600,1920)[quality.coerceIn(0,4)]
    val renderHeight:Int get()=if(demo==1){if(quality==0)480 else 1080}else listOf(360,540,720,900,1080)[quality.coerceIn(0,4)]
    val resolution:String get()="$renderWidth × $renderHeight"
    fun withQuality(value:Int)=if(demo==0)copy(medianQuality=value)else copy(srQuality=value)
}
private val Context.preferences by preferencesDataStore("settings")
class Settings(private val context:Context) {
    val values=context.preferences.data.map { p->Options(p[intPreferencesKey("demo")]?:0,p[intPreferencesKey("api")]?:0,(p[intPreferencesKey("medianQuality")]?:1).coerceIn(0,4),(p[intPreferencesKey("srQuality")]?:1).coerceIn(0,1),p[booleanPreferencesKey("muted")]?:false,p[intPreferencesKey("theme")]?:0) }
    suspend fun save(o:Options){context.preferences.edit { p->p[intPreferencesKey("demo")]=o.demo;p[intPreferencesKey("api")]=o.api;p[intPreferencesKey("medianQuality")]=o.medianQuality;p[intPreferencesKey("srQuality")]=o.srQuality;p[booleanPreferencesKey("muted")]=o.muted;p[intPreferencesKey("theme")]=o.theme }}
}
@Entity(tableName="runs")
data class RunResult(@PrimaryKey val id:String,val started:Long,val demo:Int,val api:Int,val quality:Int,val json:String,val pending:Boolean=false) {
    val data:JSONObject get()=JSONObject(json)
    val options:Options get()=Options(demo,api,if(demo==0)quality else 1,if(demo==1)quality else 1)
    val score:Long? get()=data.let { if(it.isNull("score"))null else it.optLong("score") }
    val fps:Double get()=data.optDouble("fps",0.0)
    val assessment:String get(){if(!data.optBoolean("complete"))return data.optString("reason","中断").ifBlank {"中断"};if(data.optBoolean("integratedMedian")){val p=data.optJSONObject("presentation")?.optDouble("p95Ms",0.0)?:0.0;return "表示："+when{fps>=90&&p<=14->"とても快適";fps>=55&&p<=25->"快適";fps>=27&&p<=50->"動作可能";fps>=13&&p<=100->"重い";else->"動作困難"}};val prefix=if(demo==1)"表示：" else "";return prefix+when {fps>=120->"とても快適";fps>=60->"快適";fps>=30->"動作可能";fps>=15->"重い";else->"動作困難"}}
}
@Dao
interface RunsDao {
    @Query("SELECT * FROM runs ORDER BY started DESC LIMIT 200") fun observe():Flow<List<RunResult>>
    @Query("SELECT * FROM runs WHERE pending = 1") suspend fun pending():List<RunResult>
    @Insert(onConflict=OnConflictStrategy.REPLACE) suspend fun save(row:RunResult)
}
@Database(entities=[RunResult::class],version=1,exportSchema=false)
abstract class ResultsDatabase:RoomDatabase(){abstract fun runs():RunsDao}
