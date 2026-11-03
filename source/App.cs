using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Web.Script.Serialization;
using System.Windows.Forms;
namespace DemoBenchmark {
public sealed class Soundtrack:IDisposable {
 [StructLayout(LayoutKind.Sequential,Pack=2)]struct WaveFormat{public ushort format,channels;public uint rate,bytesPerSec;public ushort align,bits,extra;}
 [StructLayout(LayoutKind.Sequential)]struct WaveHeader{public IntPtr data;public uint length,recorded;public UIntPtr user;public uint flags,loops;public IntPtr next;public UIntPtr reserved;}
 [StructLayout(LayoutKind.Sequential)]struct MMTime{public uint type,value,reserved;}
 [DllImport("winmm.dll")]static extern uint waveOutOpen(out IntPtr h,uint device,ref WaveFormat format,IntPtr callback,IntPtr instance,uint flags);
 [DllImport("winmm.dll")]static extern uint waveOutPrepareHeader(IntPtr h,IntPtr header,uint size);
 [DllImport("winmm.dll")]static extern uint waveOutWrite(IntPtr h,IntPtr header,uint size);
 [DllImport("winmm.dll")]static extern uint waveOutGetPosition(IntPtr h,ref MMTime time,uint size);
 [DllImport("winmm.dll")]static extern uint waveOutReset(IntPtr h);
 [DllImport("winmm.dll")]static extern uint waveOutSetVolume(IntPtr h,uint volume);
 [DllImport("winmm.dll")]static extern uint waveOutGetVolume(IntPtr h,out uint volume);
 [DllImport("winmm.dll")]static extern uint waveOutUnprepareHeader(IntPtr h,IntPtr header,uint size);
 [DllImport("winmm.dll")]static extern uint waveOutClose(IntPtr h);
 IntPtr handle,header;GCHandle pinned;byte[] pcm,silence;uint frames;public bool Playing,Muted;
 public Soundtrack(string path){using(BinaryReader r=new BinaryReader(File.OpenRead(path))){if(new string(r.ReadChars(4))!="RIFF")throw new Exception("Invalid WAV");r.ReadUInt32();if(new string(r.ReadChars(4))!="WAVE")throw new Exception("Invalid WAV");bool format=false;while(r.BaseStream.Position<r.BaseStream.Length){string chunk=new string(r.ReadChars(4));int n=r.ReadInt32();long next=r.BaseStream.Position+n+(n&1);if(chunk=="fmt "){format=r.ReadUInt16()==1&&r.ReadUInt16()==2&&r.ReadUInt32()==44100;r.ReadUInt32();format=format&&r.ReadUInt16()==4&&r.ReadUInt16()==16;}else if(chunk=="data")pcm=r.ReadBytes(n);r.BaseStream.Position=next;}if(!format||pcm==null)throw new Exception("PCM 44100 Hz stereo required");frames=(uint)(pcm.Length/4);}}
 static void Check(uint result){if(result!=0)throw new Exception("音声の再生に失敗しました。 ("+result+")");}
 public void Play(){Stop();WaveFormat fmt=new WaveFormat{format=1,channels=2,rate=44100,bytesPerSec=176400,align=4,bits=16};Check(waveOutOpen(out handle,uint.MaxValue,ref fmt,IntPtr.Zero,IntPtr.Zero,0));
  // Silence diagnostic playback in the PCM data, without persisting a zero session gain.
  // Recover the zero gain left by earlier versions on this app's own session.
  if(!Muted){uint volume;if(waveOutGetVolume(handle,out volume)==0&&volume==0)Check(waveOutSetVolume(handle,uint.MaxValue));}
  if(Muted&&silence==null)silence=new byte[pcm.Length];
  pinned=GCHandle.Alloc(Muted?silence:pcm,GCHandleType.Pinned);WaveHeader h=new WaveHeader{data=pinned.AddrOfPinnedObject(),length=(uint)pcm.Length};header=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(WaveHeader)));Marshal.StructureToPtr(h,header,false);Check(waveOutPrepareHeader(handle,header,(uint)Marshal.SizeOf(typeof(WaveHeader))));Check(waveOutWrite(handle,header,(uint)Marshal.SizeOf(typeof(WaveHeader))));Playing=true;}
 public double Position{get{if(!Playing)return 0;MMTime t=new MMTime{type=2};Check(waveOutGetPosition(handle,ref t,12));double seconds=t.type==2?t.value/44100.0:t.type==4?t.value/176400.0:t.value/1000.0;return Math.Min(seconds,frames/44100.0);}}
 public void Stop(){Playing=false;if(handle!=IntPtr.Zero){waveOutReset(handle);if(header!=IntPtr.Zero){waveOutUnprepareHeader(handle,header,(uint)Marshal.SizeOf(typeof(WaveHeader)));Marshal.FreeHGlobal(header);header=IntPtr.Zero;}waveOutClose(handle);handle=IntPtr.Zero;}if(pinned.IsAllocated)pinned.Free();}
 public void Dispose(){Stop();}
}
}
