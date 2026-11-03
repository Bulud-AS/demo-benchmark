using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

namespace DemoBenchmark {
public static class Native {
    [DllImport("opengl32.dll")] public static extern void glFrontFace(uint mode);
    [DllImport("opengl32.dll")] public static extern void glGetIntegerv(uint name,out int value);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void StorageMS(uint target,int samples,uint format,int w,int h);
    [DllImport("opengl32.dll")] public static extern void glDrawElements(uint mode,int count,uint type,IntPtr indices);
    [DllImport("opengl32.dll")] public static extern void glBlendFunc(uint src,uint dst);
    [DllImport("opengl32.dll")] public static extern void glDepthFunc(uint func);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void Uniform4(int location,float a,float b,float c,float d);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void ActiveTexture(uint unit);
    [StructLayout(LayoutKind.Sequential)] public struct PFD {public ushort size,version;public uint flags;public byte pixelType,colorBits,redBits,redShift,greenBits,greenShift,blueBits,blueShift,alphaBits,alphaShift,accumBits,accumRedBits,accumGreenBits,accumBlueBits,accumAlphaBits,depthBits,stencilBits,auxBuffers,layerType,reserved;public uint layerMask,visibleMask,damageMask;}
    [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr h);
    [DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr h,IntPtr dc);
    [DllImport("gdi32.dll")] public static extern int ChoosePixelFormat(IntPtr dc,ref PFD p);
    [DllImport("gdi32.dll")] public static extern bool SetPixelFormat(IntPtr dc,int fmt,ref PFD p);
    [DllImport("gdi32.dll")] public static extern bool SwapBuffers(IntPtr dc);
    [DllImport("opengl32.dll")] public static extern IntPtr wglCreateContext(IntPtr dc);
    [DllImport("opengl32.dll")] public static extern bool wglMakeCurrent(IntPtr dc,IntPtr rc);
    [DllImport("opengl32.dll")] public static extern bool wglDeleteContext(IntPtr rc);
    [DllImport("opengl32.dll",CharSet=CharSet.Ansi)] public static extern IntPtr wglGetProcAddress(string name);
    [DllImport("kernel32.dll",CharSet=CharSet.Ansi)] public static extern IntPtr GetProcAddress(IntPtr module,string name);
    [DllImport("kernel32.dll",CharSet=CharSet.Ansi)] static extern IntPtr LoadLibrary(string name);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [StructLayout(LayoutKind.Sequential)] public struct MSG{public IntPtr hwnd;public uint message;public UIntPtr wParam;public IntPtr lParam;public uint time;public Point pt;}
    [DllImport("user32.dll")] public static extern bool PeekMessage(out MSG msg,IntPtr h,uint min,uint max,uint remove);
    [DllImport("opengl32.dll")] public static extern IntPtr glGetString(uint name);
    [DllImport("opengl32.dll")] public static extern uint glGetError();
    [DllImport("opengl32.dll")] public static extern void glViewport(int x,int y,int w,int h);
    [DllImport("opengl32.dll")] public static extern void glClearColor(float r,float g,float b,float a);
    [DllImport("opengl32.dll")] public static extern void glClear(uint mask);
    [DllImport("opengl32.dll")] public static extern void glEnable(uint c);
    [DllImport("opengl32.dll")] public static extern void glDisable(uint c);
    [DllImport("opengl32.dll")] public static extern void glPolygonMode(uint face,uint mode);
    [DllImport("opengl32.dll")] public static extern void glReadPixels(int x,int y,int w,int h,uint format,uint type,IntPtr data);
    [DllImport("opengl32.dll")] public static extern void glFinish();
    [DllImport("opengl32.dll")] public static extern void glGenTextures(int n,out uint id);
    [DllImport("opengl32.dll")] public static extern void glBindTexture(uint target,uint id);
    [DllImport("opengl32.dll")] public static extern void glTexParameteri(uint target,uint name,int value);
    [DllImport("opengl32.dll")] public static extern void glTexImage2D(uint target,int level,int internalFmt,int w,int h,int border,uint fmt,uint type,IntPtr data);
    [DllImport("opengl32.dll")] public static extern void glDeleteTextures(int n,ref uint id);
    static IntPtr library=LoadLibrary("opengl32.dll");
    public static Func<string,IntPtr> Resolver;
    public static T Load<T>(string name) where T:class {if(Resolver!=null){IntPtr address=Resolver(name);if(address==IntPtr.Zero)throw new NotSupportedException(name);return Marshal.GetDelegateForFunctionPointer(address,typeof(T)) as T;}IntPtr p=wglGetProcAddress(name);if(p==IntPtr.Zero||p==new IntPtr(1)||p==new IntPtr(2)||p==new IntPtr(3)||p==new IntPtr(-1))p=GetProcAddress(library,name);if(p==IntPtr.Zero)throw new NotSupportedException(name+" が使用できません。GPUドライバーとOpenGL 3.3対応を確認してください。");return Marshal.GetDelegateForFunctionPointer(p,typeof(T)) as T;}
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate IntPtr CreateContext(IntPtr dc,IntPtr share,int[] attrib);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate int SwapInterval(int value);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void Gen(int n,out uint id);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void Bind(uint target,uint id);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void BindOne(uint id);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void Delete(int n,ref uint id);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void BufferData(uint target,IntPtr size,IntPtr data,uint usage);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void Attribute(uint index,int n,uint type,byte normalize,int stride,IntPtr offset);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void EnableAttribute(uint index);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void Divisor(uint index,uint divisor);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate uint CreateShader(uint type);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void ShaderSource(uint shader,int count,IntPtr strings,IntPtr lengths);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void ShaderAction(uint id);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void GetIv(uint id,uint name,out int value);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void GetLog(uint id,int capacity,out int length,StringBuilder log);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate uint CreateProgram();
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void Attach(uint program,uint shader);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate int GetUniform(uint program,[MarshalAs(UnmanagedType.LPStr)]string name);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void UniformMatrix(int location,int count,byte transpose,float[] matrix);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void UniformInt(int location,int value);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void DrawInstanced(uint mode,int first,int vertices,int instances);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void FramebufferTexture(uint target,uint attachment,uint textureTarget,uint texture,int level);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void RenderStorage(uint target,uint format,int w,int h);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void FramebufferRender(uint target,uint attachment,uint renderTarget,uint render);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate uint CheckFramebuffer(uint target);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void Blit(int x0,int y0,int x1,int y1,int dx0,int dy0,int dx1,int dy1,uint mask,uint filter);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void BeginQuery(uint target,uint id);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void EndQuery(uint target);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void GetQuery(uint id,uint name,out ulong result);
}
public class GLSurface:Control {
    protected override CreateParams CreateParams{get{CreateParams p=base.CreateParams;p.ClassStyle|=0x20;return p;}}
    public GLSurface(){SetStyle(ControlStyles.Opaque|ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint,true);TabStop=true;}
    protected override void OnPaintBackground(PaintEventArgs e){}
    protected override void OnPaint(PaintEventArgs e){}
}
}
