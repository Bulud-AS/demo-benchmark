#pragma once
#include "MedianWork.h"

inline Json renderDiagnostic(Session& session,Backend& gpu,SceneRenderer& renderer,Json result){
    struct Case {const char* id;bool mixed,legacy,timing;};
    const Case cases[]={
        {"render-vsync",false,false,true},
        {"mixed-vsync",true,false,true},
        {"mixed-30hz",true,true,false},
        {"render-30hz",false,true,false},
        {"render-vsync-no-timing",false,false,false},
        {"render-vsync-repeat",false,false,true}
    };
    result["diagnostic"]=true;result["profile"]="median-render-diagnostic-v1";
    result["score"]=nullptr;result["complete"]=false;result["duration"]=36.;
    result["diagnosticStages"]=Json::array();
    bool supportedFeedback=gpu.presentation.feedback;double suiteStart=mono();
    for(int index=0;index<6&&!session.cancel;index++){
        auto c=cases[index];gpu.retire(true);gpu.presentation.timing=c.timing;gpu.presentation.legacyFuture=c.timing;
        MedianWork work(c.mixed,gpu.hardware,&session.cancel);
        Performance performance(gpu.hardware);
        Json stage={{"case",c.id},{"mixedLoad",c.mixed},{"preview",c.legacy?"legacy-30hz":"display-callback"},{"timingEnabled",c.timing},{"warmupSeconds",2.},{"sceneStartSeconds",20.},{"sceneEndSeconds",24.},{"performanceStart",performance.data()}};
        uint64_t submitted=0,completed=0,frames=0,gpuTimed=0,draws=0,triangles=0;
        double cpuMs=0,waitMs=0,submitMs=0,acquireMs=0,presentMs=0,gpuMs=0,firstScene=0,lastScene=0;
        auto consume=[&](bool wait){for(auto& t:gpu.retire(wait)){completed++;if(!t.measured)continue;if(!frames)firstScene=t.time;lastScene=t.time;frames++;cpuMs+=t.cpuMs;waitMs+=t.waitMs;submitMs+=t.submitMs;acquireMs+=t.acquireMs;presentMs+=t.presentMs;draws+=t.draws;triangles+=t.triangles;if(t.gpuMs>=0){gpuTimed++;gpuMs+=t.gpuMs;}}};
        double start=mono(),measureAt=0,nextPublish=0,lastPreview=0;uint64_t pulseSeen=session.displaySerial.load();
        work.start();
        while(!session.cancel){
            double now=mono(),elapsed=now-start;if(elapsed>=6)break;
            if(elapsed>=2&&!measureAt){consume(true);measureAt=mono();work.measure((int64_t)(measureAt*1e9));gpu.presentation.reset((int64_t)(measureAt*1e9));}
            double begin=mono();gpu.begin();double record=mono();
            renderer.render(18.+std::min(elapsed,6.));
            uint64_t pulse=session.displaySerial.load();bool preview=c.legacy?now-lastPreview>=1./30:pulse!=pulseSeen;
            if(preview){lastPreview=now;pulseSeen=pulse;gpu.presentation.targetNs=c.timing?session.displayTimeNs.load()+session.displayPeriodNs.load():0;}
            double recorded=mono();gpu.end(renderer.root(),{18.+elapsed,measureAt>0,renderer.drawCalls,renderer.triangles,(recorded-record)*1000,(record-begin)*1000},preview,false);submitted++;performance.completed(mono()-record);consume(false);
            if(now>=nextPublish){result["phase"]="running";result["diagnosticStage"]=index+1;result["diagnosticStageCount"]=6;result["diagnosticCase"]=c.id;result["elapsed"]=index*6.+std::min(elapsed,6.);result["fps"]=gpu.presentation.data(true)["fps"];session.publish(result);nextPublish=now+.5;}
        }
        work.stop();consume(true);double seconds=measureAt>0?mono()-measureAt:0;
        need(completed==submitted,"診断フレームの計数が一致しません");
        stage["firstMeasuredSceneSecond"]=firstScene;stage["lastMeasuredSceneSecond"]=lastScene;stage["frames"]=frames;stage["submittedFrames"]=submitted;stage["completedFrames"]=completed;stage["measuredSeconds"]=seconds;stage["renderFps"]=seconds>0?frames/seconds:0.;stage["presentation"]=gpu.presentation.data();
        stage["averageDrawCalls"]=frames?(double)draws/frames:0.;stage["averageTriangles"]=frames?(double)triangles/frames:0.;stage["cpuRecordMs"]=frames?cpuMs/frames:0.;stage["frameSlotWaitMs"]=frames?waitMs/frames:0.;stage["submitPresentMs"]=frames?submitMs/frames:0.;stage["acquireMs"]=frames?acquireMs/frames:0.;stage["queuePresentMs"]=frames?presentMs/frames:0.;stage["gpuMs"]=gpuTimed?Json(gpuMs/gpuTimed):Json(nullptr);
        stage["medianWork"]=work.data(seconds);stage["performanceEnd"]=performance.data();stage["backendDiagnostics"]=gpu.diagnostics();stage["interrupted"]=session.cancel.load();result["diagnosticStages"].push_back(stage);
    }
    gpu.presentation.timing=true;gpu.presentation.legacyFuture=false;gpu.presentation.feedback=supportedFeedback;
    result["phase"]="finished";result["reason"]=session.cancel?"診断を中断しました":"描画診断";result["diagnosticFinished"]=!session.cancel;result["runWallSeconds"]=mono()-suiteStart;
    result["frames"]=0;result["fps"]=0.;result["renderFps"]=0.;session.publish(result);return result;
}
