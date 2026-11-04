#include "Core.h"
#include "Performance.h"
#include "MedianWork.h"
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include <android/sharedmem_jni.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <thread>

AAssetManager* vulkanAssets=nullptr;
extern void setShaderAssets(AAssetManager*);
namespace {
std::string text(JNIEnv* env,jstring value){if(!value)return "";const char* p=env->GetStringUTFChars(value,nullptr);std::string result=p;env->ReleaseStringUTFChars(value,p);return result;}
jstring string(JNIEnv* env,const std::string& value){return env->NewStringUTF(value.c_str());}
struct Mapping {
    int fd=-1;SharedSR* data=nullptr;
    Mapping(JNIEnv* env,jobject shared){if(!shared)return;fd=ASharedMemory_dupFromJava(env,shared);need(fd>=0,"共有フレームを開けません");void* address=mmap(nullptr,sizeof(SharedSR),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);need(address!=MAP_FAILED,"共有フレームを割り当てられません");data=(SharedSR*)address;}
    ~Mapping(){if(data)munmap(data,sizeof(SharedSR));if(fd>=0)close(fd);}
};
struct NativeWindow {ANativeWindow* value;NativeWindow(JNIEnv* env,jobject surface):value(ANativeWindow_fromSurface(env,surface)){need(value!=nullptr,"画面を取得できません");}~NativeWindow(){ANativeWindow_release(value);}};
struct Pcm {Bytes bytes;size_t offset=0,count=0;};
Pcm wav(AAssetManager* assets){Pcm p;p.bytes=asset(assets,"soundtrack.wav");need(p.bytes.size()>=44&&memcmp(p.bytes.data(),"RIFF",4)==0&&memcmp(p.bytes.data()+8,"WAVE",4)==0,"音声データが正しくありません");auto u16=[&](size_t i){uint16_t v;memcpy(&v,p.bytes.data()+i,2);return v;};auto u32=[&](size_t i){uint32_t v;memcpy(&v,p.bytes.data()+i,4);return v;};bool format=false;for(size_t i=12;i+8<=p.bytes.size();){size_t length=u32(i+4);need(i+8+length<=p.bytes.size(),"音声データが欠けています");if(!memcmp(p.bytes.data()+i,"fmt ",4)){need(length>=16,"Invalid PCM format");format=u16(i+8)==1&&u16(i+10)==2&&u32(i+12)==44100&&u16(i+22)==16;}if(!memcmp(p.bytes.data()+i,"data",4)){p.offset=i+8;p.count=length/4;}i+=8+length+(length&1);}need(format&&p.count>0,"PCM 44.1kHz stereoが必要です");return p;}
Json run(Session* session,JNIEnv* env,AAssetManager* manager,jobject surface,const Json& config,jobject sharedObject){
    int demo=config.value("demo",0),api=config.value("api",0),q=config.value("quality",1);bool measuring=config.value("measuring",true),muted=config.value("muted",false);Quality preset=quality(demo,q);
    Json result={{"phase","preparing"},{"demo",demo},{"api",api},{"apiName",api?"Vulkan":"OpenGL ES 3.0"},{"quality",preset.id},{"width",preset.width},{"height",preset.height},{"samples",preset.samples},{"measuring",measuring},{"muted",muted},{"profile",demo?"android-paced-v5":measuring?"median-system-gamma-v1":"median-playback-gamma-v1"},{"complete",false},{"score",nullptr},{"fps",0.},{"frames",0},{"gpuMs",nullptr},{"onePercentLow",nullptr},{"elapsed",0.},{"duration",demo?0.:5728256./44100.},{"reason",""}};session->publish(result);
    uint64_t measured=0,total=0,submitted=0,measuredSubmitted=0,measuredDraws=0,measuredTriangles=0,gpuTimed=0;double gpuTotalMs=0,cpuTotalMs=0,waitTotalMs=0,submitTotalMs=0;
    struct Bin {uint64_t frames=0,draws=0,triangles=0,gpuTimed=0;double gpuMs=0,cpuMs=0,waitMs=0,submitMs=0;};std::map<int,Bin> bins;double measureWall=0,elapsed=0,epoch=0;std::unique_ptr<NativeWindow> window;std::unique_ptr<Backend> gpu;Pcm pcm;Audio audio;Mapping shared(env,sharedObject);std::atomic<size_t> cursor{0};
    try{
        window=std::make_unique<NativeWindow>(env,surface);setShaderAssets(manager);vulkanAssets=manager;
        gpu=api?createVulkan(window->value,preset.width,preset.height,preset.samples,measuring):createGles(window->value,preset.width,preset.height,preset.samples,measuring);
        result["requestedSamples"]=preset.samples;result["samples"]=gpu->samples;result["msaaFallback"]=preset.samples!=gpu->samples;preset.samples=gpu->samples;result["renderer"]=gpu->renderer;result["driver"]=gpu->version;result["vendor"]=gpu->vendor;result["hardware"]=gpu->hardware;session->publish(result);
        MedianWork integrated(measuring&&!demo,gpu->hardware,&session->cancel);result["integratedMedian"]=measuring&&!demo;
        Performance performance(measuring&&gpu->hardware);result["performanceStart"]=performance.data();
        SceneRenderer renderer(manager,*gpu,preset,demo!=0);result["loadedTriangles"]=renderer.loadedTriangles;
        auto consume=[&](bool wait){for(auto& tag:gpu->retire(wait)){total++;if(tag.measured){measured++;measuredDraws+=tag.draws;measuredTriangles+=tag.triangles;cpuTotalMs+=tag.cpuMs;waitTotalMs+=tag.waitMs;submitTotalMs+=tag.submitMs;
            auto& bin=bins[(int)tag.time];bin.frames++;bin.draws+=tag.draws;bin.triangles+=tag.triangles;bin.cpuMs+=tag.cpuMs;bin.waitMs+=tag.waitMs;bin.submitMs+=tag.submitMs;if(tag.gpuMs>=0){gpuTimed++;gpuTotalMs+=tag.gpuMs;bin.gpuTimed++;bin.gpuMs+=tag.gpuMs;}}}};
        std::string captures=config.value("captureDir",std::string());
        if(!captures.empty()&&!demo){std::filesystem::create_directories(captures);int i=0;for(double t:{2.7,18.,38.,84.,100.,124.}){if(session->cancel)break;gpu->begin();renderer.render(t);gpu->end(renderer.root(),{},true,false);gpu->capture(renderer.root(),captures+"/"+std::to_string(++i)+".ppm");}result["phase"]="finished";result["reason"]="画像検証";result["captures"]=i;session->publish(result);return result;}
        result["phase"]="preparing";session->publish(result);
        uint8_t black[]={0,0,0,255};
        if(!demo){result["renderProbes"]=Json::array();for(double t:{2.7,18.,38.,84.,100.,124.}){if(session->cancel)break;gpu->begin();renderer.render(t);gpu->end(renderer.root(),{},false,false);auto probe=gpu->inspect(renderer.root());probe["time"]=t;probe["drawCalls"]=renderer.drawCalls;probe["triangles"]=renderer.triangles;result["renderProbes"].push_back(probe);}pcm=wav(manager);audio.open(44100,[&](int16_t* out,int frames){size_t start=cursor.load(),n=std::min((size_t)frames,pcm.count-std::min(start,pcm.count));if(n)memcpy(out,pcm.bytes.data()+pcm.offset+start*4,n*4);if(n<(size_t)frames)memset(out+n*2,0,(frames-n)*4);cursor.store(start+n);},muted);}
        else {gpu->begin();renderer.renderSource(black,1,1,0);gpu->end(renderer.root(),{},false,preset.id==0);}
        consume(true);measured=total=0;result["backendDiagnosticsStart"]=gpu->diagnostics();
        bool natural=false;double limit=config.value("testDuration",0.);double duration=demo?0.:pcm.count/44100.;result["duration"]=duration;
        if(demo){need(shared.data!=nullptr,"Second Realityホストがありません");{std::lock_guard guard(session->lock);session->worker=shared.data;}double waitStart=mono();while(!shared.data->ready&&!session->cancel){need(mono()-waitStart<20,"Second Realityの準備がタイムアウトしました");std::this_thread::sleep_for(std::chrono::milliseconds(1));}shared.data->start=1;while(shared.data->frames==0&&!session->cancel){if(shared.data->state.load(std::memory_order_acquire)==3)throw std::runtime_error(shared.data->error);need(mono()-waitStart<25,"Second Realityの開始がタイムアウトしました");std::this_thread::sleep_for(std::chrono::milliseconds(1));}epoch=shared.data->epochUs.load()/1e6;}
        else if(!session->cancel){audio.start();epoch=mono();}integrated.start();
        double publishAt=0,recentAt=mono();uint64_t lastPreview=0;uint64_t recentFrames=0;Bytes source(640*400*4);int sourceW=1,sourceH=1;uint32_t serial=0;std::copy(black,black+4,source.begin());
        while(!session->cancel){
            double now=mono();elapsed=demo?std::max(0.,now-epoch):audio.position();
            if(demo){int state=shared.data->state.load(std::memory_order_acquire);if(state==3)throw std::runtime_error(shared.data->error);if(state==2){natural=true;break;}}
            else if(elapsed>=duration){natural=true;break;}
            if(limit>0&&elapsed>=limit)break;
            if(measuring&&elapsed>=3&&measureWall==0){measureWall=now;integrated.measure((int64_t)(now*1e9));gpu->presentation.reset((int64_t)(now*1e9));}
            if(!measuring&&session->displaySerial.load()==lastPreview){std::unique_lock lock(session->displayLock);session->displayWake.wait_for(lock,std::chrono::milliseconds(25),[&]{return session->cancel||session->displaySerial.load()!=lastPreview;});continue;}
            double beginAt=mono();gpu->begin();double recordAt=mono();
            if(demo){if(!shared.data->gate.test_and_set(std::memory_order_acquire)){uint32_t current=shared.data->frames.load();if(current!=serial){sourceW=shared.data->width;sourceH=shared.data->height;need(sourceW>0&&sourceW<=640&&sourceH>0&&sourceH<=400,"Invalid source frame");memcpy(source.data(),shared.data->pixels,sourceW*sourceH*4);serial=current;}shared.data->gate.clear(std::memory_order_release);}renderer.renderSource(source.data(),sourceW,sourceH,serial);}
            else renderer.render(elapsed);
            uint64_t pulse=session->displaySerial.load();bool preview=pulse!=lastPreview;if(preview){lastPreview=pulse;gpu->presentation.targetNs=session->displayTimeNs.load()+session->displayPeriodNs.load();}double recordEnd=mono();gpu->end(renderer.root(),{elapsed,measuring&&elapsed>=3,renderer.drawCalls,renderer.triangles,(recordEnd-recordAt)*1000,(recordAt-beginAt)*1000},preview,demo&&preset.id==0);performance.completed(mono()-recordAt);submitted++;if(measuring&&elapsed>=3)measuredSubmitted++;consume(false);
            if(now>=publishAt){double average=measuring?(measureWall>0?measured/std::max(.001,mono()-measureWall):0):(total-recentFrames)/std::max(.001,now-recentAt);result["phase"]=measuring&&elapsed<3?"warmup":"running";result["elapsed"]=elapsed;auto presentation=gpu->presentation.data(true);result["renderFps"]=average;result["presentation"]=presentation;result["fps"]=presentation["fps"];result["frames"]=measured;result["score"]=measuring&&measured&&gpu->hardware?(demo?Json((int64_t)std::llround(average/60*1000)):integrated.score(mono()-measureWall,average)):Json(nullptr);result["presentationWidth"]=gpu->presentationWidth;result["presentationHeight"]=gpu->presentationHeight;result["presentationResizes"]=gpu->presentationResizes;result["drawCalls"]=renderer.drawCalls;result["triangles"]=renderer.triangles;if(demo){result["sourceFrames"]=shared.data->frames.load();result["sourceTicks"]=shared.data->ticks.load();result["part"]=shared.data->part.load();}session->publish(result);publishAt=now+.5;recentAt=now;recentFrames=total;}
        }
        integrated.stop();need(integrated.correct(),"総合負荷の計算結果が一致しません");consume(true);double measuredSeconds=measureWall>0?mono()-measureWall:0;double average=measuredSeconds>0?measured/measuredSeconds:0;
        result["gpuMs"]=gpuTimed?Json(gpuTotalMs/gpuTimed):Json(nullptr);result["gpuTimedFrames"]=gpuTimed;result["cpuRecordMs"]=measured?cpuTotalMs/measured:0;result["submitPresentMs"]=measured?submitTotalMs/measured:0;result["frameSlotWaitMs"]=measured?waitTotalMs/measured:0;result["backendDiagnostics"]=gpu->diagnostics();result["performanceEnd"]=performance.data();
        Json intervals=Json::array();for(auto& [second,bin]:bins)intervals.push_back({{"playbackSecond",second},{"frames",bin.frames},{"averageDrawCalls",bin.frames?(double)bin.draws/bin.frames:0},{"averageTriangles",bin.frames?(double)bin.triangles/bin.frames:0},{"cpuRecordMs",bin.frames?bin.cpuMs/bin.frames:0},{"frameSlotWaitMs",bin.frames?bin.waitMs/bin.frames:0},{"submitPresentMs",bin.frames?bin.submitMs/bin.frames:0},{"gpuMs",bin.gpuTimed?Json(bin.gpuMs/bin.gpuTimed):Json(nullptr)}});result["frameIntervals"]=intervals;
        result["measuredSeconds"]=measuredSeconds;result["runWallSeconds"]=epoch>0?mono()-epoch:0;result["submittedFrames"]=submitted;result["completedFrames"]=total;result["measuredSubmittedFrames"]=measuredSubmitted;result["measuredDrawCalls"]=measuredDraws;result["measuredTriangles"]=measuredTriangles;result["averageDrawCalls"]=measured?(double)measuredDraws/measured:0;result["averageTriangles"]=measured?(double)measuredTriangles/measured:0;
        need(total==submitted&&measured==measuredSubmitted,"描画フレームの計数が一致しません");
        result["displayRefreshHz"]=1e9/session->displayPeriodNs.load();result["displayPulses"]=session->displaySerial.load();result["renderFps"]=average;result["presentation"]=gpu->presentation.data();result["fps"]=result["presentation"]["fps"];result["medianWork"]=integrated.data(measuredSeconds);result["frames"]=measured;result["elapsed"]=elapsed;
        result["presentationWidth"]=gpu->presentationWidth;result["presentationHeight"]=gpu->presentationHeight;result["presentationResizes"]=gpu->presentationResizes;
        bool eligible=measuring&&natural&&limit==0&&!session->cancel&&gpu->hardware&&measured>0&&integrated.correct();
        result["complete"]=eligible;result["score"]=eligible?(demo?Json((int64_t)std::llround(average/60*1000)):integrated.score(measuredSeconds,average)):Json(nullptr);result["phase"]="finished";
        std::string reason;
        if(session->cancel){std::lock_guard guard(session->lock);reason=session->reason.empty()?"中断":session->reason;}
        else if(limit>0)reason="短縮テスト";else if(!gpu->hardware)reason="ソフトウェアGPU";else if(!measuring)reason="再生";
        result["reason"]=reason;
        if(!demo){result["audioUnderruns"]=audio.underruns();result["audioClock"]=audio.timestampClock()?"audio-timestamp":"monotonic-fallback";audio.close();}
        else {result["sourceFrames"]=shared.data->frames.load();result["sourceTicks"]=shared.data->ticks.load();result["sourceFps"]=shared.data->frames.load()/std::max(.001,elapsed);result["sourceTickHz"]=shared.data->ticks.load()/std::max(.001,elapsed);result["part"]=shared.data->part.load();}
    }catch(const std::exception& e){result["phase"]="finished";result["complete"]=false;result["score"]=nullptr;result["reason"]="実行できませんでした";result["errorDetail"]=e.what();result["frames"]=measured;result["elapsed"]=elapsed;}
    {std::lock_guard guard(session->lock);if(session->worker){session->worker->stop=1;session->worker=nullptr;}}
    session->publish(result);return result;
}
}
#define JNI extern "C" JNIEXPORT
#define NAME(method) Java_io_github_buludas_demobenchmark_NativeEngine_##method
JNI jstring JNICALL NAME(capabilities)(JNIEnv* env,jobject){try{return string(env,probe().dump());}catch(const std::exception& e){return string(env,Json({{"gles",false},{"vulkan",false},{"error",e.what()}}).dump());}}
JNI jlong JNICALL NAME(create)(JNIEnv*,jobject){return (jlong)new Session();}
JNI void JNICALL NAME(cancel)(JNIEnv* env,jobject,jlong handle,jstring reason){if(handle)((Session*)handle)->stop(text(env,reason));}
JNI jstring JNICALL NAME(snapshot)(JNIEnv* env,jobject,jlong handle){auto* s=(Session*)handle;std::lock_guard guard(s->lock);return string(env,s->snapshot);}
JNI jstring JNICALL NAME(run)(JNIEnv* env,jobject,jlong handle,jobject assets,jobject surface,jstring config,jobject shared){try{return string(env,run((Session*)handle,env,AAssetManager_fromJava(env,assets),surface,Json::parse(text(env,config)),shared).dump());}catch(const std::exception& e){Json result={{"phase","finished"},{"complete",false},{"score",nullptr},{"reason","初期化できませんでした"},{"errorDetail",e.what()}};((Session*)handle)->publish(result);return string(env,result.dump());}}
JNI void JNICALL NAME(displayPulse)(JNIEnv*,jobject,jlong handle,jlong timeNs,jlong periodNs){if(!handle)return;auto* s=(Session*)handle;s->displayTimeNs=timeNs;s->displayPeriodNs=periodNs;s->displaySerial.fetch_add(1,std::memory_order_release);s->displayWake.notify_all();}
JNI void JNICALL NAME(destroy)(JNIEnv*,jobject,jlong handle){delete (Session*)handle;}
JNI jint JNICALL NAME(sharedSize)(JNIEnv*,jobject){return sizeof(SharedSR);}
JNI void JNICALL NAME(initializeShared)(JNIEnv* env,jobject,jobject shared){try{Mapping map(env,shared);new(map.data)SharedSR();}catch(const std::exception& e){env->ThrowNew(env->FindClass("java/lang/IllegalStateException"),e.what());}}
JNI void JNICALL NAME(worker)(JNIEnv* env,jobject,jobject shared,jboolean muted,jint first){try{Mapping map(env,shared);runSecondReality(map.data,muted,first);}catch(const std::exception& e){env->ThrowNew(env->FindClass("java/lang/IllegalStateException"),e.what());}}
