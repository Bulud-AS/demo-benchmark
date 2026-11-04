#pragma once
#include <jni.h>
#include <android/asset_manager.h>
#include <android/native_window.h>
#include <atomic>
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
using Json=nlohmann::json;
using Bytes=std::vector<uint8_t>;
using Matrix=std::array<float,16>;
using Vec=std::array<float,3>;
double mono();
Bytes asset(AAssetManager*,const std::string&);
Json assetJson(AAssetManager*,const std::string&);
void need(bool,const std::string&);
Matrix identity(),multiply(const Matrix&,const Matrix&);
struct Params {
    Matrix matrix=identity();
    std::array<float,4> color{1,1,1,1},rect{1,1,0,0},crop{0,0,1,0},settings{1,0,0,0},view{},texSize{1,1,1,1};
};
static_assert(sizeof(Params)==160);
struct Quality {int id,width,height,samples;};
Quality quality(int demo,int id);
struct FrameTag {double time=0;bool measured=false;};
struct Backend {
    int width,height,samples;std::string vendor,renderer,version;bool hardware=true;
    int presentationWidth=0,presentationHeight=0,presentationResizes=0;
    Backend(int w,int h,int aa):width(w),height(h),samples(aa){}
    void presentationSize(int w,int h){if(presentationWidth&&(presentationWidth!=w||presentationHeight!=h))presentationResizes++;presentationWidth=w;presentationHeight=h;}
    virtual ~Backend()=default;
    virtual int texture(int,int,const uint8_t*,bool)=0;
    virtual int mesh(const void*,int,const void*,int)=0;
    virtual void bind(int,int,const float*)=0;
    virtual void draw(int,const Params&,int,int)=0;
    virtual void update(int,int,int,const uint8_t*)=0;
    virtual void begin()=0;
    virtual void end(int,FrameTag,bool,bool)=0;
    virtual std::vector<FrameTag> retire(bool)=0;
    virtual void capture(int,const std::string&)=0;
};
std::unique_ptr<Backend> createGles(ANativeWindow*,int,int,int,bool);
std::unique_ptr<Backend> createVulkan(ANativeWindow*,int,int,int,bool);
Json probe();
class SceneRenderer {
    AAssetManager* assets;Backend& gpu;Quality preset;bool second;
    Json timeline;double time=0;int rootId=0,whiteId=0,srId=0;uint32_t srSerial=~0u;
    struct Mesh {int id,count,material;};
    struct Scene {Json data;std::vector<Mesh> meshes;std::vector<Matrix> world,local;std::vector<int> channels;std::vector<std::array<float,4>> colors;std::map<std::string,int> names;};
    std::map<std::string,Scene> scenes;std::map<std::string,int> images;
    std::map<int,int> targets,cached;std::map<int,bool> activity;std::vector<int> freeTargets;
    bool active(int);int image(int);int target();void node(int,int);void evaluate(Scene&,double,int,Matrix&);
    double value(const Json&,const char*,double)const;
    int input(const Json&,const std::string&)const;
    std::string resource(const Json&,const std::string&)const;
public:
    int triangles=0,drawCalls=0;uint64_t loadedTriangles=0;
    SceneRenderer(AAssetManager*,Backend&,Quality,bool);
    int root()const{return rootId;}
    void render(double);
    void renderSource(const uint8_t*,int,int,uint32_t);
};
class Audio {
    struct Impl;std::unique_ptr<Impl> impl;
public:
    Audio();~Audio();
    void open(int rate,std::function<void(int16_t*,int)> callback,bool muted);
    void start();void close();double position()const;
    int underruns()const;bool timestampClock()const;
};
struct alignas(64) SharedSR {
    uint32_t magic=0x32425253,protocol=1;
    std::atomic<int32_t> stop{0},start{0},ready{0},state{0},part{0},ticks{0},frames{0};
    std::atomic<int64_t> epochUs{0};
    std::atomic_flag gate=ATOMIC_FLAG_INIT;
    int width=320,height=200,order=0,row=0;
    char error[256]{};
    uint8_t pixels[640*400*4]{};
};
static_assert(std::atomic<int32_t>::is_always_lock_free&&std::atomic<int64_t>::is_always_lock_free);
void runSecondReality(SharedSR*,bool,int);
struct Session {
    std::atomic<bool> cancel{false};std::mutex lock;std::string snapshot="{}",reason;SharedSR* worker=nullptr;
    void stop(const std::string& text){std::lock_guard guard(lock);reason=text;cancel=true;if(worker)worker->stop=1;}
    void publish(const Json& data){std::lock_guard guard(lock);snapshot=data.dump();}
};
