using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Web.Script.Serialization;

namespace DemoBenchmark {
public sealed class LiveMeasurement {
    public int Count {get;private set;}public double FrameMilliseconds {get;private set;}
    public double AverageFps {get{return Count>0?Count/(FrameMilliseconds/1000.0):0;}}
    public long Score {get{return BenchmarkResult.ScoreForFps(AverageFps);}}
    public void Reset(){Count=0;FrameMilliseconds=0;}
    public void Add(double frameMilliseconds){if(frameMilliseconds<=0||double.IsNaN(frameMilliseconds)||double.IsInfinity(frameMilliseconds))return;Count++;FrameMilliseconds+=frameMilliseconds;}
    public string Format(double currentFps,bool measuring){return currentFps.ToString("F1",CultureInfo.InvariantCulture)+" FPS"+(measuring?"  ·  "+(Count>0?Score.ToString("N0",CultureInfo.CurrentCulture):"—")+" pts":"");}
}
public sealed class BenchmarkResult {
    public string workload="median-v2.1",scoreVersion="fps60-1000-v1";
    public string id,api,renderer,driver,quality,machine,startedUtc,finishedUtc,reason,errorDetail;
    public string renderingScope="native-3d",simulationClock="audio";public int sourceFrames,simulationTicks,lastPart;public double sourceAverageFps,simulationRateHz;
    public int apiId,qualityId,width,height,msaa,subdivision,measuredFrames,gpuSamples;
    public bool complete,hardware,vsync,gpuTimerQueries,gpuDisjoint,viewportStable;public int presentationWidth,presentationHeight;
    public long score;
    public double averageFps,onePercentLowFps,p95FrameMs,averageGpuMs,measuredSeconds,audioSeconds,wallSeconds;
    public double warmupSeconds=3,durationSeconds=Timeline.Duration;
    public int maximumTriangles,maximumDrawCalls;
    public static long ScoreForFps(double fps){return fps>0&&!double.IsNaN(fps)&&!double.IsInfinity(fps)?(long)Math.Round(fps/60.0*1000,MidpointRounding.AwayFromZero):0;}
    public string exclusionReason {
        get {
            if(complete)return "";
            switch(reason){
                case "viewport changed":return "画面サイズ変更";
                case "short diagnostic run":return "短縮計測";
                case "insufficient measured time":return "計測時間不足";
                case "no measured frames":return "計測フレームなし";
                case "vsync enabled":return "垂直同期あり";
                case "software renderer":return "GPU描画なし";
                case "audio clock stalled":return "音声の進行停止";
                case "demo timeout":return "再生時間超過";
                case "error":return "エラー";
                case "unsupported":return "非対応（スキップ）";
                case "initialization failed":return "初期化失敗（スキップ）";
                default:return "中断";
            }
        }
    }
    public string displayAssessment {get{return complete?assessment:"対象外："+exclusionReason;}}
    public string assessmentBasis { get { return "average-display-fps-v1"; } }
    public string assessment {
        get {
            if(!complete||double.IsNaN(averageFps)||double.IsInfinity(averageFps)||averageFps<0)return "評価対象外";
            string value=averageFps>=120?"とても快適":averageFps>=60?"快適":averageFps>=30?"動作可能":averageFps>=15?"重い":"動作困難";
            return workload=="secondreality-present-v1"?"表示："+value:value;
        }
    }
    public string ComparisonKey { get { return workload+"/"+scoreVersion+"/"+qualityId+"/"+width+"x"+height+"/"+msaa+"/"+subdivision+"/P"+presentationWidth+"x"+presentationHeight; } }
    public static BenchmarkResult Create(Renderer renderer,List<FrameSample> samples,bool complete,string reason,DateTime started,double elapsed,double audio,int outputWidth,int outputHeight,bool stable) {
        double seconds=samples.Sum(x=>x.frameMs)/1000.0;
        double[] sorted=samples.Select(x=>x.frameMs).OrderBy(x=>x).ToArray();
        double fps=samples.Count/Math.Max(0.000001,seconds);
        int slow=Math.Max(1,(int)Math.Ceiling(sorted.Length*.01));
        double duration=renderer.IsSecondReality?elapsed:Timeline.Duration;
        bool valid=complete&&stable&&samples.Count>0&&seconds>=duration-3-.5&&renderer.IsHardware&&!renderer.Vsync;
        string resultReason=valid?"completed":!stable?"viewport changed":!complete?reason:!renderer.IsHardware?"software renderer":renderer.Vsync?"vsync enabled":samples.Count==0?"no measured frames":"insufficient measured time";
        return new BenchmarkResult {
            workload=renderer.IsSecondReality?"secondreality-present-v1":"median-v2.1",durationSeconds=duration,
            renderingScope=renderer.IsSecondReality?"original-software-effects-and-gpu-presentation":"native-3d",simulationClock=renderer.IsSecondReality?"70hz-with-tracker-sync":"audio",simulationRateHz=renderer.IsSecondReality?70:0,
            id=DateTime.UtcNow.ToString("yyyyMMddTHHmmssfff",CultureInfo.InvariantCulture),
            api=renderer.ApiName,apiId=(int)renderer.Api,presentationWidth=outputWidth,presentationHeight=outputHeight,viewportStable=stable,renderer=renderer.Device,driver=renderer.Version,
            quality=renderer.Quality.Name,qualityId=renderer.Quality.Id,width=renderer.Width,height=renderer.Height,
            msaa=renderer.Samples>1?renderer.Samples:0,subdivision=renderer.Quality.Subdivision,
            complete=valid,hardware=renderer.IsHardware,vsync=renderer.Vsync,reason=resultReason,
            machine=Environment.MachineName,startedUtc=started.ToUniversalTime().ToString("o"),finishedUtc=DateTime.UtcNow.ToString("o"),
            measuredFrames=samples.Count,measuredSeconds=seconds,audioSeconds=audio,wallSeconds=elapsed,
            score=valid?ScoreForFps(fps):0,
            averageFps=fps,onePercentLowFps=sorted.Length==0?0:1000/sorted.Skip(sorted.Length-slow).Average(),
            p95FrameMs=sorted.Length==0?0:sorted[Math.Min(sorted.Length-1,(int)(sorted.Length*.95))],
            gpuTimerQueries=renderer.TimerSupported,gpuDisjoint=renderer.GpuDisjoint,gpuSamples=renderer.GpuSamples,
            averageGpuMs=renderer.GpuSamples>0&&!renderer.GpuDisjoint?renderer.GpuTotalMs/renderer.GpuSamples:0,
            maximumTriangles=samples.Count==0?0:samples.Max(x=>x.triangles),maximumDrawCalls=samples.Count==0?0:samples.Max(x=>x.drawCalls)
        };
    }
}
public sealed class ResultStore {
    readonly string folder;
    readonly JavaScriptSerializer json=new JavaScriptSerializer { MaxJsonLength=int.MaxValue };
    public ResultStore(string home){folder=Path.Combine(home,"results");}
    public string Save(BenchmarkResult result,List<FrameSample> samples) {
        Directory.CreateDirectory(folder);
        string stem=result.id+"-"+((RenderApi)result.apiId).ToString()+"-Q"+result.qualityId;
        string path=Path.Combine(folder,stem+".json");
        File.WriteAllText(path,json.Serialize(result),Encoding.UTF8);
        using(var csv=new StreamWriter(Path.Combine(folder,stem+".csv"),false,Encoding.UTF8)) {
            csv.WriteLine("audio_seconds,frame_ms,cpu_submit_ms,gpu_ms,triangles,draw_calls");
            foreach(FrameSample s in samples)csv.WriteLine(string.Format(CultureInfo.InvariantCulture,"{0:F6},{1:F6},{2:F6},{3:F6},{4},{5}",s.time,s.frameMs,s.submitMs,s.gpuMs,s.triangles,s.drawCalls));
        }
        return path;
    }
    public List<BenchmarkResult> Recent() {
        List<BenchmarkResult> rows=new List<BenchmarkResult>();
        if(!Directory.Exists(folder))return rows;
        foreach(string path in Directory.GetFiles(folder,"*.json").OrderByDescending(x=>x).Take(200)) {
            try { BenchmarkResult r=json.Deserialize<BenchmarkResult>(File.ReadAllText(path));if(r.complete&&r.hardware&&(r.workload=="median-v2.1"||r.workload=="secondreality-present-v1")&&r.scoreVersion=="fps60-1000-v1")rows.Add(r); }
            catch(ArgumentException) {} catch(InvalidOperationException) {}
        }
        return rows;
    }
}
}
