#pragma once
#include "Performance.h"
#include <cstring>
#include <thread>
#include <sys/resource.h>
#include <cmath>

class MedianWork {
    static constexpr size_t pixels=16384,tiles=512;
    std::vector<uint32_t> source,destination;
    std::vector<std::thread> threads;
    std::atomic<bool> running{false},active{false};
    std::atomic<int64_t> measuredAfter{0};
    std::atomic<uint64_t> cpuPixels{0},memoryBytes{0},checksum{0};
    std::atomic<bool> valid{true};
    int workers=0;bool hints=false;const std::atomic<bool>* cancellation=nullptr;
    std::mutex gate;std::condition_variable wake;
    static uint32_t filtered(const uint32_t* p,size_t i){return (4*p[i]+p[(i+1)%pixels]+p[(i+pixels-1)%pixels]+p[(i+128)%pixels]+p[(i+pixels-128)%pixels])/8;}
    void work(int id){
        setpriority(PRIO_PROCESS,gettid(),5);Performance performance(hints);
        {std::unique_lock lock(gate);wake.wait(lock,[&]{return active.load()||!running.load();});}
        size_t tile=id;uint64_t iteration=0;
        while(running&&!(cancellation&&cancellation->load())){
            auto* src=source.data()+tile*pixels;auto* dst=destination.data()+tile*pixels;double begin=mono();int64_t threshold=measuredAfter.load();uint64_t hash=0;
            if((iteration++&1)==0){for(size_t i=0;i<pixels;i++)dst[i]=filtered(src,i);for(size_t i=0;i<pixels;i++)hash=hash*33+dst[i];if(dst[0]!=filtered(src,0)||dst[pixels-1]!=filtered(src,pixels-1))valid=false;
                if(threshold&&begin*1e9>=threshold)cpuPixels.fetch_add(pixels,std::memory_order_relaxed);
            }else{memcpy(dst,src,pixels*4);if(memcmp(dst,src,pixels*4))valid=false;hash=dst[0]+dst[pixels-1];if(threshold&&begin*1e9>=threshold)memoryBytes.fetch_add(pixels*16,std::memory_order_relaxed);}
            checksum.fetch_xor(hash,std::memory_order_relaxed);performance.completed(mono()-begin);tile+=workers;if(tile>=tiles)tile=id;
        }
    }
public:
    MedianWork(bool enabled,bool hardware,const std::atomic<bool>* cancelled=nullptr):hints(hardware),cancellation(cancelled){
        if(!enabled)return;
        cpu_set_t mask;CPU_ZERO(&mask);int n=0;if(sched_getaffinity(0,sizeof(mask),&mask)==0)for(int i=0;i<CPU_SETSIZE;i++)n+=CPU_ISSET(i,&mask)?1:0;
        workers=hardware?std::clamp(n>0?n-1:1,1,32):1;
        source.resize(pixels*tiles);destination.resize(pixels*tiles);
        for(size_t i=0;i<source.size();i++)source[i]=(uint32_t)(((i*1664525u+1013904223u)>>24)&255);
        running=true;try{for(int i=0;i<workers;i++)threads.emplace_back([this,i]{work(i);});}catch(...){stop();throw;}
    }
    ~MedianWork(){stop();}
    void start(){active=true;wake.notify_all();}
    void measure(int64_t now){measuredAfter=now;}
    void stop(){running=false;wake.notify_all();for(auto& t:threads)if(t.joinable())t.join();}
    bool correct()const{return valid;}
    Json data(double seconds)const{return {{"workers",workers},{"workingSetBytes",source.size()*8},{"cpuPixels",cpuPixels.load()},{"memoryBytes",memoryBytes.load()},{"cpuPixelsPerSecond",seconds>0?cpuPixels.load()/seconds:0},{"memoryBytesPerSecond",seconds>0?memoryBytes.load()/seconds:0},{"checksum",checksum.load()},{"correct",valid.load()},{"workerNice",5}};}
    Json score(double seconds,double renderFps)const{
        auto d=data(seconds);double c=d["cpuPixelsPerSecond"],m=d["memoryBytesPerSecond"];
        if(workers==0||!valid||c<=0||m<=0||renderFps<=0)return nullptr;
        return (int64_t)std::llround(1000*std::exp(.5*std::log(renderFps/60)+.3*std::log(c/20000000)+.2*std::log(m/1000000000)));
    }
};
