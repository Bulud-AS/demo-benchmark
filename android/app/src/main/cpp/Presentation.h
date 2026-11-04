#pragma once
#include <algorithm>
#include <cmath>

struct Presentation {
    int64_t targetNs=0,startNs=0;
    std::vector<int64_t> queued,shown;
    bool feedback=false;
    void reset(int64_t now){startNs=now;queued.clear();shown.clear();}
    void sent(){int64_t t=(int64_t)(mono()*1e9);if(t>=startNs)queued.push_back(t);}
    void displayed(int64_t t){if(t>=startNs&&(shown.empty()||t>shown.back()))shown.push_back(t);}
    Json data(bool recent=false)const{
        bool actual=feedback&&shown.size()>=2;const auto& times=actual?shown:queued;size_t first=0;
        if(recent&&!times.empty()){int64_t cutoff=(int64_t)(mono()*1e9)-1000000000;while(first<times.size()&&times[first]<cutoff)first++;}
        double fps=0,p95=0;std::vector<double> gaps;
        if(times.size()>first+1){fps=(times.size()-first-1)*1e9/(times.back()-times[first]);for(size_t i=first+1;i<times.size();i++)gaps.push_back((times[i]-times[i-1])/1e6);std::sort(gaps.begin(),gaps.end());p95=gaps[(size_t)((gaps.size()-1)*.95)];}
        return {{"fps",fps},{"source",actual?"display-feedback":"presentation-submission"},{"feedbackSupported",feedback},{"submitted",queued.size()},{"displayed",feedback?Json(shown.size()):Json(nullptr)},{"p95Ms",p95}};
    }
};
