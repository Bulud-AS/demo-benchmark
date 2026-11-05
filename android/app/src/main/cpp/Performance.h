#pragma once
#include "Core.h"
#include <algorithm>
#include <dlfcn.h>
#include <fstream>
#include <sched.h>
#include <unistd.h>

class Performance {
    void* library=nullptr;
    void* session=nullptr;
    int (*report)(void*,int64_t)=nullptr;
    void (*close)(void*)=nullptr;
    uint64_t reports=0;
public:
    explicit Performance(bool enabled){
        if(!enabled)return;
        library=dlopen("libandroid.so",RTLD_NOW|RTLD_LOCAL);
        if(!library)return;
        auto manager=(void*(*)())dlsym(library,"APerformanceHint_getManager");
        auto create=(void*(*)(void*,const int32_t*,size_t,int64_t))dlsym(library,"APerformanceHint_createSession");
        report=(int(*)(void*,int64_t))dlsym(library,"APerformanceHint_reportActualWorkDuration");
        close=(void(*)(void*))dlsym(library,"APerformanceHint_closeSession");
        if(manager&&create&&report&&close){int32_t tid=gettid();if(auto m=manager())session=create(m,&tid,1,1000000);}
    }
    ~Performance(){if(session)close(session);if(library)dlclose(library);}
    void completed(double seconds){if(session&&report(session,std::max<int64_t>(1,(int64_t)(seconds*1e9)))==0)reports++;}
    Json data()const{
        Json data={{"adpfActive",session!=nullptr},{"adpfReports",reports},{"adpfTargetNs",session?Json(1000000):Json(nullptr)},{"cpuAffinityPolicy","system"}};
        cpu_set_t mask;CPU_ZERO(&mask);bool affinity=sched_getaffinity(0,sizeof(mask),&mask)==0;
        Json cores=Json::array();long count=std::min<long>(sysconf(_SC_NPROCESSORS_CONF),CPU_SETSIZE);
        for(int i=0;i<count;i++){
            Json core={{"id",i},{"allowed",affinity?Json(CPU_ISSET(i,&mask)!=0):Json(nullptr)}};
            auto read=[&](const char* path,const char* key){std::ifstream file("/sys/devices/system/cpu/cpu"+std::to_string(i)+path);int64_t value;if(file>>value)core[key]=value;};
            read("/cpufreq/cpuinfo_max_freq","maximumKHz");read("/cpufreq/scaling_cur_freq","reportedCurrentKHz");read("/cpu_capacity","capacity");cores.push_back(core);
        }
        data["cpuCores"]=cores;return data;
    }
};
