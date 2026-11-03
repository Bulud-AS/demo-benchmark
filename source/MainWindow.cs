using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Web.Script.Serialization;
using System.Windows.Forms;

namespace DemoBenchmark {
public sealed class FrameSample { public double time,frameMs,submitMs,gpuMs;public int triangles,drawCalls; }
public sealed class MainWindow : Form {
    readonly string home=AppDomain.CurrentDomain.BaseDirectory;
    readonly string[] args;
    readonly Panel viewport=new Panel();
    readonly FlowLayoutPanel toolbar=new FlowLayoutPanel();
    readonly ComboBox demoBox=new ComboBox(),apiBox=new ComboBox(),qualityBox=new ComboBox();
    readonly Button play=new Button(),measure=new Button(),compare=new Button(),stop=new Button(),history=new Button();
    readonly Label status=new Label();
    readonly Stopwatch clock=new Stopwatch();
    readonly List<FrameSample> samples=new List<FrameSample>();
    readonly LiveMeasurement live=new LiveMeasurement();
    readonly List<BenchmarkResult> batchResults=new List<BenchmarkResult>();
    readonly Queue<RenderApi> queue=new Queue<RenderApi>();
    GLSurface surface;Renderer renderer;Soundtrack sound;ResultStore results;SecondRealitySession second;double lastSourceProgress,sourceClock;int previousSourceFrames;int tracedPart=-1;
    bool ready,changing,running,measuring,fullscreen,closing,batch,viewportStable;int outputWidth,outputHeight;
    DateTime started;long previous,displayStamp;double previousAudio;
    double testDuration=Timeline.Duration;
    int queueQuality;Rectangle savedBounds;FormBorderStyle savedBorder;
    public MainWindow(string[] arguments) {
        args=arguments;
        SuspendLayout();
        Text="Demo Benchmark";Font=new Font("Yu Gothic UI",9F);AutoScaleDimensions=new SizeF(96,96);AutoScaleMode=AutoScaleMode.Dpi;
        ClientSize=new Size(1160,700);MinimumSize=new Size(1080,500);StartPosition=FormStartPosition.CenterScreen;BackColor=Color.Black;KeyPreview=true;
        viewport.Dock=DockStyle.Fill;viewport.BackColor=Color.Black;
        toolbar.Dock=DockStyle.Bottom;toolbar.AutoSize=true;toolbar.AutoSizeMode=AutoSizeMode.GrowAndShrink;toolbar.WrapContents=false;
        toolbar.Padding=new Padding(8,7,8,7);toolbar.BackColor=Color.FromArgb(246,246,246);toolbar.Margin=Padding.Empty;
        demoBox.DropDownStyle=apiBox.DropDownStyle=qualityBox.DropDownStyle=ComboBoxStyle.DropDownList;demoBox.Width=130;demoBox.Items.AddRange(new object[]{"Median","Second Reality"});
        apiBox.Width=172;qualityBox.Width=112;
        foreach(RenderApi api in Enum.GetValues(typeof(RenderApi)))apiBox.Items.Add(ApiNames.Display(api));
        
        var buttons=new[]{play,measure,compare,stop,history};var labels=new[]{"再生","計測","全方式","停止","結果"};
        toolbar.Controls.Add(Selector("計測する映像",demoBox));toolbar.Controls.Add(Selector("レンダリング方式",apiBox));toolbar.Controls.Add(Selector("レンダリング精度",qualityBox));
        for(int i=0;i<buttons.Length;i++) { buttons[i].Text=labels[i];buttons[i].AutoSize=true;buttons[i].MinimumSize=new Size(62,28);buttons[i].FlatStyle=FlatStyle.System;buttons[i].Anchor=AnchorStyles.Left|AnchorStyles.Bottom;toolbar.Controls.Add(buttons[i]); }
        foreach(Control c in toolbar.Controls)c.Margin=new Padding(3,2,3,2);
        status.AutoSize=true;status.Anchor=AnchorStyles.Left|AnchorStyles.Bottom;status.TextAlign=ContentAlignment.MiddleLeft;status.Padding=new Padding(10,5,0,0);status.Margin=new Padding(3,2,3,2);toolbar.Controls.Add(status);
        Controls.Add(viewport);Controls.Add(toolbar);
        demoBox.SelectedIndex=Arg("--demo","Median").Replace(" ","").Equals("SecondReality",StringComparison.OrdinalIgnoreCase)?1:0;
        FillQualities();apiBox.SelectedIndex=(int)ParseApi(Arg("--api","OpenGL"));qualityBox.SelectedIndex=Math.Max(0,Math.Min(Presets.Length-1,int.Parse(Arg("--quality",IsSecondReality?"1":"3"),CultureInfo.InvariantCulture)));
        demoBox.SelectionChangeCommitted+=(sender,e)=>{FillQualities();SelectionChanged(sender,e);};
        apiBox.SelectionChangeCommitted+=SelectionChanged;qualityBox.SelectionChangeCommitted+=SelectionChanged;
        play.Click+=(s,e)=>Begin(false);measure.Click+=(s,e)=>Begin(true);stop.Click+=(s,e)=>Cancel();
        compare.Click+=(s,e)=>BeginBatch();history.Click+=(s,e)=>ShowHistory();
        Shown+=OnShown;KeyDown+=OnKey;
        FormClosing+=(s,e)=> { closing=true;queue.Clear();batch=false;StopRun(false,"closed");Application.Idle-=OnIdle;if(second!=null)second.Dispose();if(sound!=null)sound.Dispose();DestroyRenderer(); };
        ResumeLayout(true);
        SetControls(false);
    }
    static Control Selector(string caption,ComboBox box) {
        var group=new FlowLayoutPanel { AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,FlowDirection=FlowDirection.TopDown,WrapContents=false,Padding=Padding.Empty };
        var label=new Label { Text=caption,AutoSize=true,Margin=new Padding(0,0,0,4) };
        box.AccessibleName=caption;box.Margin=Padding.Empty;
        group.Controls.Add(label);group.Controls.Add(box);return group;
    }
    string Arg(string name,string fallback) { int i=Array.IndexOf(args,name);return i>=0&&i+1<args.Length?args[i+1]:fallback; }
    static RenderApi ParseApi(string text) { return (RenderApi)Enum.Parse(typeof(RenderApi),text,true); }
    RenderApi SelectedApi { get { return (RenderApi)apiBox.SelectedIndex; } }
    bool IsSecondReality {get{return demoBox.SelectedIndex==1;}}
    string Workload {get{return IsSecondReality?"secondreality-present-v1":"median-v2.1";}}
    QualityPreset[] Presets {get{return IsSecondReality?QualityPreset.SecondReality:QualityPreset.All;}}
    QualityPreset SelectedQuality { get { return Presets[qualityBox.SelectedIndex]; } }
    void FillQualities(){qualityBox.Items.Clear();foreach(QualityPreset q in Presets)qualityBox.Items.Add(q);qualityBox.SelectedIndex=IsSecondReality?1:3;}
    void SetDuration(){double full=IsSecondReality?1800:Timeline.Duration;testDuration=Math.Min(full,double.Parse(Arg("--duration",full.ToString("R",CultureInfo.InvariantCulture)),CultureInfo.InvariantCulture));if(testDuration<=0)throw new ArgumentOutOfRangeException("duration");}
    void OnShown(object sender,EventArgs e) {
        try {
            results=new ResultStore(home);sound=new Soundtrack(Path.Combine(home,"assets","soundtrack.wav"));sound.Muted=args.Contains("--mute");
            SetDuration();
            CreateRenderer(SelectedApi,SelectedQuality);
            string srProbe=Arg("--sr-probe","");if(srProbe!=""){ProbeSecondReality(srProbe);Close();return;}
            string matrix=Arg("--matrix-test","");
            if(matrix!="") { CaptureMatrix(matrix);Close();return; }
            string capture=Arg("--capture","");
            if(capture!="") { CaptureFrames(capture);Close();return; }
            ready=true;Application.Idle+=OnIdle;renderer.Render(0);status.Text="";SetControls(false);
            if(args.Contains("--all"))BeginBatch();else if(args.Contains("--benchmark"))Begin(true);
        } catch(Exception ex) { Fail(ex); }
    }
    void ProbeSecondReality(string folder){
        Directory.CreateDirectory(folder);renderer.SetVsync(false);
        int part=int.Parse(Arg("--sr-part","0"),CultureInfo.InvariantCulture);double duration=double.Parse(Arg("--duration","5"),CultureInfo.InvariantCulture);
        using(var probe=new SecondRealitySession()){
            probe.Start(home,args.Contains("--mute"),part,duration);renderer.SecondReality=probe;var timer=Stopwatch.StartNew();long draws=0;double captureAt=duration*.7;bool captured=false;
            var snapshots=new List<object>();int oldPart=-1;double nextSnapshot=0;
            while(timer.Elapsed.TotalSeconds<duration+5){
                probe.Poll();double t=timer.Elapsed.TotalSeconds;
                renderer.Render(t);draws++;
                if(probe.SourceFrames>0&&(probe.Part!=oldPart||t>=nextSnapshot)){snapshots.Add(new {wall=t,probe.Seconds,probe.Part,probe.Ticks,probe.SourceFrames,probe.Width,probe.Height,probe.Order,probe.Row});oldPart=probe.Part;nextSnapshot=t+2;}
                if(!captured&&t>=captureAt){renderer.Capture(Path.Combine(folder,"frame.png"));captured=true;}
                if(probe.State==2||probe.State==4)break;
            }
            File.WriteAllText(Path.Combine(folder,"probe.json"),new JavaScriptSerializer().Serialize(new{api=renderer.ApiName,quality=renderer.Quality.Name,renderer.Device,renderer.Width,renderer.Height,renderer.Vsync,probe.State,probe.Part,probe.Ticks,probe.SourceFrames,probe.Seconds,draws,wall=timer.Elapsed.TotalSeconds,snapshots}),Encoding.UTF8);
            renderer.SecondReality=null;
        }
    }
    void CaptureFrames(string folder) {
        if(IsSecondReality){ProbeSecondReality(folder);return;}
        Directory.CreateDirectory(folder);renderer.SetVsync(false);int index=0;
        foreach(string value in Arg("--times","2.7,6,18,30,34,38,45,54,68,84,100,112,120,124").Split(',')) {
            renderer.Render(double.Parse(value,CultureInfo.InvariantCulture));renderer.Capture(Path.Combine(folder,(++index).ToString("00")+".png"));
        }
        var info=new { api=renderer.ApiName,quality=renderer.Quality.Name,qualityId=renderer.Quality.Id,renderer.Width,renderer.Height,msaa=renderer.Samples>1?renderer.Samples:0,subdivision=renderer.Quality.Subdivision,renderer.LoadedTriangles,renderer.Vendor,renderer.Device,renderer.Version,hardware=renderer.IsHardware,timerQueries=renderer.TimerSupported };
        File.WriteAllText(Path.Combine(folder,"renderer.json"),new JavaScriptSerializer().Serialize(info),Encoding.UTF8);
    }
    void CaptureMatrix(string folder) {
        foreach(RenderApi api in Enum.GetValues(typeof(RenderApi)))foreach(QualityPreset quality in Presets) {
            CreateRenderer(api,quality);
            CaptureFrames(Path.Combine(folder,api+"-Q"+quality.Id));
        }
    }
    void DestroyRenderer() {
        Renderer old=renderer;renderer=null;if(old!=null)old.Dispose();
        if(surface!=null) { viewport.Controls.Remove(surface);surface.Dispose();surface=null; }
    }
    void CreateRenderer(RenderApi api,QualityPreset quality) {
        changing=true;status.Text="準備中…";SetControls(true);status.Refresh();
        DestroyRenderer();surface=new GLSurface { Dock=DockStyle.Fill };viewport.Controls.Add(surface);
        renderer=new Renderer(surface,Path.Combine(home,"assets"),api,quality,IsSecondReality);
        if(!renderer.IsHardware)throw new NotSupportedException("ハードウェアGPUを使用できません。");
        surface.Resize+=(sender,e)=> { if(renderer!=null&&!running&&!changing)renderer.Present(); };
        changing=false;
    }
    void SelectionChanged(object sender,EventArgs e) {
        if(!ready||changing)return;
        try { queue.Clear();batch=false;StopRun(false,"settings changed");SetDuration();CreateRenderer(SelectedApi,SelectedQuality);renderer.Render(0);status.Text="";SetControls(false); }
        catch(Exception ex) { Fail(ex); }
    }
    void SetControls(bool preparing) {
        bool busy=running||preparing||batch;
        demoBox.Enabled=apiBox.Enabled=qualityBox.Enabled=!busy;
        play.Enabled=measure.Enabled=compare.Enabled=!busy&&renderer!=null;
        stop.Enabled=running;history.Enabled=results!=null&&!busy;
    }
    void Begin(bool bench) {
        if(renderer==null||changing)return;
        try {
            StopRun(false,"restarted");measuring=bench;samples.Clear();live.Reset();previous=0;previousAudio=0;
            SetControls(true);status.Text="準備中…";status.Refresh();
            renderer.Measuring=false;if(bench&&!renderer.SetVsync(false))throw new NotSupportedException("垂直同期を無効にできません。");
            if(bench&&!IsSecondReality)foreach(double t in new double[]{2.7,6,18,30,34,38,45,54,68,84,100,112,120,124})renderer.Render(t);
            renderer.FinishMetrics();renderer.ResetMetrics();renderer.SetVsync(!bench&&!IsSecondReality);
            if(bench&&renderer.Vsync)throw new NotSupportedException("垂直同期を無効にできません。");
            renderer.Render(0);renderer.FinishMetrics();renderer.ResetMetrics();renderer.Measuring=bench;
            outputWidth=surface.ClientSize.Width;outputHeight=surface.ClientSize.Height;viewportStable=true;
            if(IsSecondReality){
                if(second!=null)second.Dispose();second=new SecondRealitySession();renderer.ResetSecondReality();
                int part=int.Parse(Arg("--sr-part","0"),CultureInfo.InvariantCulture);
                second.Start(home,args.Contains("--mute"),part,args.Contains("--duration")?testDuration:0);renderer.SecondReality=second;
                lastSourceProgress=sourceClock=0;previousSourceFrames=0;tracedPart=-1;
            }else sound.Play();
            started=DateTime.UtcNow;clock.Restart();running=true;displayStamp=0;
            status.Text=batch?((int)renderer.Api+1)+" / 3":"";SetControls(false);
        } catch(Exception ex) { Fail(ex); }
    }
    BenchmarkResult StopRun(bool complete,string reason) {
        if(!running)return null;
        double audio=IsSecondReality?sourceClock:sound.Position,elapsed=clock.Elapsed.TotalSeconds;running=false;clock.Stop();sound.Stop();
        renderer.FinishMetrics();BenchmarkResult result=null;
        if(measuring) {
            result=BenchmarkResult.Create(renderer,samples,complete,reason,started,elapsed,audio,outputWidth,outputHeight,viewportStable);
            if(IsSecondReality&&second!=null){result.sourceFrames=second.SourceFrames;result.sourceAverageFps=second.SourceFrames/Math.Max(.001,sourceClock);result.simulationTicks=second.Ticks;result.lastPart=second.Part;}
            results.Save(result,samples);
            if(batch)batchResults.Add(result);
            status.Text=result.averageFps.ToString("F1",CultureInfo.InvariantCulture)+" FPS  ·  "+(result.complete?result.score.ToString("N0",CultureInfo.CurrentCulture):"—")+" pts  ·  "+result.displayAssessment;
        }
        else status.Text="";
        if(second!=null)second.Dispose();renderer.Measuring=false;measuring=false;SetControls(false);return result;
    }
    void Cancel() { queue.Clear();batch=false;StopRun(false,"cancelled");SetControls(false); }
    void BeginBatch() {
        if(renderer==null||changing)return;
        Cancel();batchResults.Clear();batch=true;queueQuality=qualityBox.SelectedIndex;queue.Clear();
        foreach(RenderApi api in Enum.GetValues(typeof(RenderApi)))queue.Enqueue(api);
        NextBatch();
    }
    void NextBatch() {
        if(closing)return;
        if(queue.Count==0) { batch=false;SetControls(false);if(args.Contains("--all"))Close();else ShowResults(batchResults.ToArray(),SelectedQuality.Name);return; }
        RenderApi api=queue.Dequeue();
        try {
            changing=true;apiBox.SelectedIndex=(int)api;qualityBox.SelectedIndex=queueQuality;changing=false;
            if(renderer==null||renderer.Api!=api||renderer.Quality.Id!=queueQuality)CreateRenderer(api,Presets[queueQuality]);
            Begin(true);
        } catch(Exception ex) { Fail(ex); }
    }
    void OnIdle(object sender,EventArgs e) {
        try {
            Native.MSG message;
            while(!Native.PeekMessage(out message,IntPtr.Zero,0,0,0)) {
                if(!running||changing) { System.Threading.Thread.Sleep(4);return; }
                if(WindowState==FormWindowState.Minimized) { Cancel();return; }
                if(measuring&&(surface.ClientSize.Width!=outputWidth||surface.ClientSize.Height!=outputHeight))viewportStable=false;
                double audio;
                bool ended=false,full=false;
                if(IsSecondReality){
                    second.Poll();sourceClock=clock.Elapsed.TotalSeconds;audio=sourceClock;
                    if(second.SourceFrames!=previousSourceFrames){previousSourceFrames=second.SourceFrames;lastSourceProgress=audio;}
                    string trace=Arg("--sr-trace","");
                    if(trace!=""&&second.SourceFrames>0&&second.Part!=tracedPart){Directory.CreateDirectory(trace);File.AppendAllText(Path.Combine(trace,"parts.jsonl"),new JavaScriptSerializer().Serialize(new{wall=audio,second.Seconds,second.Part,second.Order,second.Row,second.Ticks,second.SourceFrames})+Environment.NewLine);tracedPart=second.Part;}
                    if(audio-lastSourceProgress>15)throw new InvalidOperationException("Second Realityの進行が停止しました。");
                    ended=second.State==2||second.State==4;full=second.State==2&&Arg("--sr-part","0")=="0"&&!args.Contains("--duration");
                }else{audio=sound.Position;ended=audio>=testDuration-.001;full=testDuration>=Timeline.Duration;}
                if(ended) {
                    BenchmarkResult completed=StopRun(full,"short diagnostic run");
                    if(batch)BeginInvoke((Action)NextBatch);
                    else if(args.Contains("--benchmark"))Close();
                    else if(completed!=null)
                        BeginInvoke((Action)(()=>{if(!closing&&!IsDisposed)ShowResults(new[]{completed},completed.api+" / "+completed.quality);}));
                    return;
                }
                if(clock.Elapsed.TotalSeconds>testDuration+2) { StopRun(false,IsSecondReality?"demo timeout":"audio clock stalled");queue.Clear();batch=false;if(args.Contains("--benchmark")||args.Contains("--all")){Environment.ExitCode=1;Close();}return; }
                renderer.Render(audio);long end=Stopwatch.GetTimestamp();
                double frame=previous==0?0:(end-previous)*1000.0/Stopwatch.Frequency;previous=end;
                if(measuring&&audio>=3&&previousAudio>=3&&frame>0){samples.Add(new FrameSample { time=audio,frameMs=frame,submitMs=renderer.SubmissionMs,gpuMs=renderer.GpuMs,triangles=renderer.Triangles,drawCalls=renderer.DrawCalls });live.Add(frame);}
                previousAudio=audio;
                if(end-displayStamp>Stopwatch.Frequency/4) {
                    if(frame>0)status.Text=(batch?((int)renderer.Api+1)+"/3  ·  ":"")+live.Format(1000/frame,measuring);
                    displayStamp=end;
                }
            }
        } catch(Exception ex) { Fail(ex); }
    }
    void ShowHistory() {
        string key=Workload+"/fps60-1000-v1/"+SelectedQuality.Id+"/"+SelectedQuality.Width+"x"+SelectedQuality.Height+"/"+(SelectedQuality.Samples>1?SelectedQuality.Samples:0)+"/"+SelectedQuality.Subdivision+"/P"+viewport.ClientSize.Width+"x"+viewport.ClientSize.Height;
        var rows=results.Recent().Where(r=>r.ComparisonKey==key&&r.machine==Environment.MachineName).GroupBy(r=>r.apiId+"/"+r.ComparisonKey).Select(g=>g.OrderByDescending(r=>r.finishedUtc).First()).OrderByDescending(r=>r.score).ToArray();
        ShowResults(rows,SelectedQuality.Name);
    }
    void ShowResults(IEnumerable<BenchmarkResult> rows,string title) {
        using(Form dialog=new Form()) {
            dialog.SuspendLayout();
            dialog.Text="結果 — "+(IsSecondReality?"Second Reality / ":"")+title;dialog.Font=Font;
            dialog.AutoScaleDimensions=new SizeF(96,96);dialog.AutoScaleMode=AutoScaleMode.Dpi;
            dialog.StartPosition=FormStartPosition.CenterParent;dialog.ClientSize=new Size(1000,210);dialog.MinimumSize=new Size(900,180);
            DataGridView table=new DataGridView { Dock=DockStyle.Fill,ReadOnly=true,AllowUserToAddRows=false,AllowUserToDeleteRows=false,RowHeadersVisible=false,AutoSizeColumnsMode=DataGridViewAutoSizeColumnsMode.Fill,SelectionMode=DataGridViewSelectionMode.FullRowSelect,BackgroundColor=Color.White,BorderStyle=BorderStyle.None,ColumnHeadersHeightSizeMode=DataGridViewColumnHeadersHeightSizeMode.AutoSize,AutoSizeRowsMode=DataGridViewAutoSizeRowsMode.AllCells };
            table.Columns.Add("api","方式");table.Columns.Add("score","スコア");table.Columns.Add("assessment","評価");table.Columns.Add("fps",IsSecondReality?"表示FPS":"平均FPS");table.Columns.Add("low","1% Low");table.Columns.Add("gpu","GPU ms");if(IsSecondReality)table.Columns.Add("source","生成FPS");
            table.Columns[0].FillWeight=190;table.Columns[2].FillWeight=190;
            table.Columns[2].ToolTipText="平均FPSによる表示の目安。1% Lowも確認してください。";
            table.Columns[2].DefaultCellStyle.WrapMode=DataGridViewTriState.True;
            table.DefaultCellStyle.Padding=new Padding(5,4,5,4);
            foreach(BenchmarkResult r in rows){var cells=new List<object>{r.api,r.complete?r.score.ToString("N0"):"—",r.displayAssessment,r.measuredFrames>0?r.averageFps.ToString("F1"):"—",r.measuredFrames>0?r.onePercentLowFps.ToString("F1"):"—",r.gpuTimerQueries&&!r.gpuDisjoint?r.averageGpuMs.ToString("F3"):"—"};if(IsSecondReality)cells.Add(r.sourceAverageFps.ToString("F1"));table.Rows.Add(cells.ToArray());}
            dialog.Controls.Add(table);dialog.ResumeLayout(true);dialog.ShowDialog(this);
        }
    }
    void OnKey(object sender,KeyEventArgs e) {
        if((demoBox.DroppedDown||apiBox.DroppedDown||qualityBox.DroppedDown)&&(e.KeyCode==Keys.Space||e.KeyCode==Keys.F5))return;
        if(e.KeyCode==Keys.F11){ToggleFullscreen();e.Handled=true;}
        else if(e.KeyCode==Keys.Escape){if(fullscreen)ToggleFullscreen();else Cancel();e.Handled=true;}
        else if(e.KeyCode==Keys.F5&&!running){Begin(true);e.Handled=true;}
        else if(e.KeyCode==Keys.Space&&!running){Begin(false);e.Handled=true;}
    }
    void ToggleFullscreen() {
        if(!fullscreen){savedBounds=Bounds;savedBorder=FormBorderStyle;toolbar.Visible=false;FormBorderStyle=FormBorderStyle.None;Bounds=Screen.FromControl(this).Bounds;fullscreen=true;}
        else{toolbar.Visible=true;FormBorderStyle=savedBorder;Bounds=savedBounds;fullscreen=false;}
    }
    void Fail(Exception ex) {
        queue.Clear();batch=false;changing=false;
        try { StopRun(false,"error");if(sound!=null)sound.Stop();File.WriteAllText(Path.Combine(home,"error.log"),ex.ToString(),Encoding.UTF8); } catch { }
        Environment.ExitCode=1;
        if(args.Contains("--capture")||args.Contains("--benchmark")||args.Contains("--all")){Close();return;}
        status.Text="使用不可";SetControls(false);demoBox.Enabled=apiBox.Enabled=qualityBox.Enabled=true;
        MessageBox.Show(this,ex.Message,"Demo Benchmark",MessageBoxButtons.OK,MessageBoxIcon.Error);
    }
}
static class Entry {
    [STAThread]static void Main(string[] args) {
        Native.SetProcessDPIAware();Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new MainWindow(args));
    }
}
}
