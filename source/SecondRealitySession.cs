using System;
using System.Diagnostics;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Threading;

namespace DemoBenchmark {
// One isolated native process per playback resets all original demo globals.
public sealed class SecondRealitySession : IDisposable {
    MemoryMappedFile map;MemoryMappedViewAccessor view;Mutex gate;Process process;
    public int State,Part,Ticks,SourceFrames,Width=1,Height=1,Order,Row;
    public double Seconds;
    public byte[] Pixels=new byte[]{0,0,0,255};
    public bool FrameChanged;
    public void Start(string home,bool muted,int startPart,double diagnosticLimit) {
        Dispose();
        string name="Local\\DemoBenchmarkSR-"+Guid.NewGuid().ToString("N");
        map=MemoryMappedFile.CreateNew(name,256+640*400*4);view=map.CreateViewAccessor();
        gate=new Mutex(false,name+"-lock");view.Write(0,0x31425253);view.Write(4,1);
        State=0;Part=startPart;Ticks=SourceFrames=0;Seconds=0;Width=Height=1;Pixels=new byte[]{0,0,0,255};FrameChanged=true;
        var info=new ProcessStartInfo(Path.Combine(home,"runtime","SecondRealityCore.exe"));
        info.UseShellExecute=false;info.CreateNoWindow=true;info.WorkingDirectory=home;
        info.Arguments=name+" "+name+"-lock "+Process.GetCurrentProcess().Id+" "+startPart+" "+(muted?"1":"0")+" "+diagnosticLimit.ToString("R",System.Globalization.CultureInfo.InvariantCulture);
        process=Process.Start(info);
    }
    public void Poll() {
        FrameChanged=false;if(view==null)return;
        State=view.ReadInt32(8);Seconds=view.ReadInt32(32)/1000.0;
        if(State==3) { byte[] message=new byte[192];view.ReadArray(64,message,0,message.Length);throw new InvalidOperationException(System.Text.Encoding.UTF8.GetString(message).TrimEnd('\0')); }
        if(process.HasExited&&State!=2&&State!=4)throw new InvalidOperationException("Second Realityが終了しました（"+process.ExitCode+"）。");
        if(view.ReadInt32(36)==SourceFrames)return;
        bool locked=false;
        try {
            locked=gate.WaitOne(0);if(!locked)return;
            int w=view.ReadInt32(16),h=view.ReadInt32(20),serial=view.ReadInt32(36);
            if(serial==0||w<1||w>640||h<1||h>400)return;
            if(Pixels.Length!=w*h*4)Pixels=new byte[w*h*4];
            view.ReadArray(256,Pixels,0,Pixels.Length);
            Width=w;Height=h;SourceFrames=serial;Part=view.ReadInt32(24);Ticks=view.ReadInt32(28);Seconds=view.ReadInt32(32)/1000.0;Order=view.ReadInt32(40);Row=view.ReadInt32(44);FrameChanged=true;
        } finally {if(locked)gate.ReleaseMutex();}
    }
    public void Dispose() {
        if(view!=null)view.Write(12,1);
        if(process!=null){if(!process.HasExited&&!process.WaitForExit(3000)){process.Kill();process.WaitForExit();}process.Dispose();process=null;}
        if(view!=null){view.Dispose();view=null;}if(map!=null){map.Dispose();map=null;}if(gate!=null){gate.Dispose();gate=null;}
    }
}
}
