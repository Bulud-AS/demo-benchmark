using System;
using System.Runtime.InteropServices;
namespace DemoBenchmark {
public sealed class Gl {
[UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void glFrontFaceFn(uint mode);
public readonly glFrontFaceFn glFrontFace;
[UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void glGetIntegervFn(uint name,out int value);
public readonly glGetIntegervFn glGetIntegerv;
[UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void glDrawElementsFn(uint mode,int count,uint type,IntPtr indices);
public readonly glDrawElementsFn glDrawElements;
[UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void glBlendFuncFn(uint src,uint dst);
public readonly glBlendFuncFn glBlendFunc;
[UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void glDepthFuncFn(uint func);
public readonly glDepthFuncFn glDepthFunc;
[UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate IntPtr glGetStringFn(uint name);
public readonly glGetStringFn glGetString;
[UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate uint glGetErrorFn();
public readonly glGetErrorFn glGetError;
[UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void glViewportFn(int x,int y,int w,int h);
public readonly glViewportFn glViewport;
[UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void glClearColorFn(float r,float g,float b,float a);
public readonly glClearColorFn glClearColor;
[UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void glClearFn(uint mask);
public readonly glClearFn glClear;
[UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void glEnableFn(uint c);
public readonly glEnableFn glEnable;
[UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void glDisableFn(uint c);
public readonly glDisableFn glDisable;
[UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void glReadPixelsFn(int x,int y,int w,int h,uint format,uint type,IntPtr data);
public readonly glReadPixelsFn glReadPixels;
[UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void glFinishFn();
public readonly glFinishFn glFinish;
[UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void glGenTexturesFn(int n,out uint id);
public readonly glGenTexturesFn glGenTextures;
[UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void glBindTextureFn(uint target,uint id);
public readonly glBindTextureFn glBindTexture;
[UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void glTexParameteriFn(uint target,uint name,int value);
public readonly glTexParameteriFn glTexParameteri;
[UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void glTexImage2DFn(uint target,int level,int internalFmt,int w,int h,int border,uint fmt,uint type,IntPtr data);
public readonly glTexImage2DFn glTexImage2D;
[UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void glDeleteTexturesFn(int n,ref uint id);
public readonly glDeleteTexturesFn glDeleteTextures;
public Gl(){
glFrontFace=Native.Load<glFrontFaceFn>("glFrontFace");
glGetIntegerv=Native.Load<glGetIntegervFn>("glGetIntegerv");
glDrawElements=Native.Load<glDrawElementsFn>("glDrawElements");
glBlendFunc=Native.Load<glBlendFuncFn>("glBlendFunc");
glDepthFunc=Native.Load<glDepthFuncFn>("glDepthFunc");
glGetString=Native.Load<glGetStringFn>("glGetString");
glGetError=Native.Load<glGetErrorFn>("glGetError");
glViewport=Native.Load<glViewportFn>("glViewport");
glClearColor=Native.Load<glClearColorFn>("glClearColor");
glClear=Native.Load<glClearFn>("glClear");
glEnable=Native.Load<glEnableFn>("glEnable");
glDisable=Native.Load<glDisableFn>("glDisable");
glReadPixels=Native.Load<glReadPixelsFn>("glReadPixels");
glFinish=Native.Load<glFinishFn>("glFinish");
glGenTextures=Native.Load<glGenTexturesFn>("glGenTextures");
glBindTexture=Native.Load<glBindTextureFn>("glBindTexture");
glTexParameteri=Native.Load<glTexParameteriFn>("glTexParameteri");
glTexImage2D=Native.Load<glTexImage2DFn>("glTexImage2D");
glDeleteTextures=Native.Load<glDeleteTexturesFn>("glDeleteTextures");
}
}
}