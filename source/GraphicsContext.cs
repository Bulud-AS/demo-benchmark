using System;
using System.IO;
using System.Runtime.InteropServices;

namespace DemoBenchmark {
public sealed class GraphicsContext : IDisposable {
    public readonly RenderApi Api;
    public bool IsES { get { return Api!=RenderApi.OpenGL; } }
    public bool Vsync { get; private set; }
    IntPtr dc,context,display,windowSurface,eglLibrary,glesLibrary,window;
    Native.SwapInterval wglSwap;
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate IntPtr EglProc([MarshalAs(UnmanagedType.LPStr)]string name);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate IntPtr EglPlatform(uint platform,IntPtr native,int[] attrib);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int EglInitialize(IntPtr display,out int major,out int minor);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int EglChoose(IntPtr display,int[] attributes,out IntPtr config,int size,out int count);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate IntPtr EglContext(IntPtr display,IntPtr config,IntPtr share,int[] attributes);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate IntPtr EglSurface(IntPtr display,IntPtr config,IntPtr window,int[] attributes);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int EglCurrent(IntPtr display,IntPtr draw,IntPtr read,IntPtr context);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int EglOne(IntPtr display);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int EglTwo(IntPtr display,IntPtr other);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int EglInterval(IntPtr display,int interval);
    EglProc getProc;EglCurrent makeCurrent;EglTwo swap,destroySurface,destroyContext;EglOne terminate;EglInterval swapInterval;
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr LoadLibraryEx(string path,IntPtr file,uint flags);
    [DllImport("kernel32.dll")] static extern bool FreeLibrary(IntPtr module);
    T Export<T>(string name) where T:class { IntPtr p=Native.GetProcAddress(eglLibrary,name);if(p==IntPtr.Zero)throw new NotSupportedException(name);return Marshal.GetDelegateForFunctionPointer(p,typeof(T)) as T; }
    public GraphicsContext(GLSurface surface,RenderApi api) {
        Api=api;window=surface.Handle;
        if(api==RenderApi.OpenGL) {
            Native.Resolver=null;dc=Native.GetDC(window);
            Native.PFD p=new Native.PFD();p.size=(ushort)Marshal.SizeOf(typeof(Native.PFD));p.version=1;p.flags=0x25;p.colorBits=32;p.alphaBits=8;p.depthBits=24;
            int format=Native.ChoosePixelFormat(dc,ref p);
            if(format==0||!Native.SetPixelFormat(dc,format,ref p))throw new Exception("OpenGLのピクセル形式を作成できません。");
            IntPtr old=Native.wglCreateContext(dc);
            if(old==IntPtr.Zero||!Native.wglMakeCurrent(dc,old))throw new Exception("OpenGLを初期化できません。");
            try { Native.CreateContext modern=Native.Load<Native.CreateContext>("wglCreateContextAttribsARB");context=modern(dc,IntPtr.Zero,new int[]{0x2091,3,0x2092,3,0x9126,1,0});if(context==IntPtr.Zero)throw new Exception("OpenGL 3.3に対応したGPUが必要です。");Native.wglMakeCurrent(dc,context); }
            finally { Native.wglDeleteContext(old); }
            try { wglSwap=Native.Load<Native.SwapInterval>("wglSwapIntervalEXT"); } catch { }
        } else {
            string folder=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"runtime","angle-hardware");
            glesLibrary=LoadLibraryEx(Path.Combine(folder,"libGLESv2.dll"),IntPtr.Zero,0x1100);
            eglLibrary=LoadLibraryEx(Path.Combine(folder,"libEGL.dll"),IntPtr.Zero,0x1100);
            if(eglLibrary==IntPtr.Zero||glesLibrary==IntPtr.Zero)throw new Exception("ANGLEを読み込めません。エラー "+Marshal.GetLastWin32Error());
            getProc=Export<EglProc>("eglGetProcAddress");
            EglPlatform platform=Marshal.GetDelegateForFunctionPointer(getProc("eglGetPlatformDisplayEXT"),typeof(EglPlatform)) as EglPlatform;
            display=platform(0x3202,IntPtr.Zero,new int[]{0x3203,api==RenderApi.Vulkan?0x3450:0x3208,0x3209,0x320A,0x3038});
            int major,minor;
            if(display==IntPtr.Zero||Export<EglInitialize>("eglInitialize")(display,out major,out minor)==0)throw new NotSupportedException(ApiNames.Display(api)+"を初期化できません。");
            IntPtr config;int count;
            if(Export<EglChoose>("eglChooseConfig")(display,new int[]{0x3033,4,0x3040,0x40,0x3024,8,0x3023,8,0x3022,8,0x3021,8,0x3038},out config,1,out count)==0||count==0)throw new NotSupportedException("EGLのピクセル形式を作成できません。");
            context=Export<EglContext>("eglCreateContext")(display,config,IntPtr.Zero,new int[]{0x3098,3,0x3038});
            windowSurface=Export<EglSurface>("eglCreateWindowSurface")(display,config,window,new int[]{0x3038});
            makeCurrent=Export<EglCurrent>("eglMakeCurrent");terminate=Export<EglOne>("eglTerminate");destroySurface=Export<EglTwo>("eglDestroySurface");destroyContext=Export<EglTwo>("eglDestroyContext");swap=Export<EglTwo>("eglSwapBuffers");swapInterval=Export<EglInterval>("eglSwapInterval");
            if(context==IntPtr.Zero||windowSurface==IntPtr.Zero||makeCurrent(display,windowSurface,windowSurface,context)==0)throw new NotSupportedException("EGL描画コンテキストを作成できません。");
            Native.Resolver=delegate(string name) { IntPtr address=getProc(name);return address==IntPtr.Zero?Native.GetProcAddress(glesLibrary,name):address; };
        }
    }
    public bool SetVsync(bool enabled) {
        bool ok=IsES?swapInterval(display,enabled?1:0)!=0:wglSwap!=null&&wglSwap(enabled?1:0)!=0;
        Vsync=enabled&&ok;return ok;
    }
    public void Present() { if(IsES) { if(swap(display,windowSurface)==0)throw new Exception("EGLの画面表示に失敗しました。"); } else Native.SwapBuffers(dc); }
    public void Dispose() {
        if(context==IntPtr.Zero)return;
        if(IsES) { makeCurrent(display,IntPtr.Zero,IntPtr.Zero,IntPtr.Zero);destroySurface(display,windowSurface);destroyContext(display,context);terminate(display);if(eglLibrary!=IntPtr.Zero)FreeLibrary(eglLibrary);if(glesLibrary!=IntPtr.Zero)FreeLibrary(glesLibrary); }
        else { Native.wglMakeCurrent(IntPtr.Zero,IntPtr.Zero);Native.wglDeleteContext(context);Native.ReleaseDC(window,dc); }
        context=IntPtr.Zero;Native.Resolver=null;
    }
}
}
