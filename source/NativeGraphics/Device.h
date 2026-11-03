#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_5.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

template<class T> void release(T*& p) { if(p){p->Release();p=nullptr;} }
inline void check(HRESULT hr,const char* operation) {
    if(FAILED(hr)){char text[256];snprintf(text,sizeof(text),"%s (0x%08lX)",operation,(unsigned long)hr);throw std::runtime_error(text);}
}
inline FARPROC systemProc(const wchar_t* dll,const char* name) {
    HMODULE module=LoadLibraryExW(dll,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!module)throw std::runtime_error("Required Windows graphics runtime is unavailable");
    FARPROC p=GetProcAddress(module,name);if(!p)throw std::runtime_error("Required graphics entry point is unavailable");return p;
}
struct Constants {
    float matrix[16]{};
    float color[4]{1,1,1,1};
    float rect[4]{1,1,0,0};
    float crop[4]{0,0,1,0};
    float settings[4]{1,0,0,0};
    float view[4]{};
    float texture[4]{1,1,1,1};
};
static_assert(sizeof(Constants)==160);
inline std::wstring gpuVendor(UINT id){switch(id){case 0x8086:return L"Intel";case 0x1002:return L"AMD";case 0x10de:return L"NVIDIA";case 0x1414:return L"Microsoft";default:return L"GPU";}}
ID3DBlob* compileShader(const char* entry,const char* profile,bool legacy);
struct Device {
    HWND window;int width,height,samples;bool vsync=false;std::wstring name,version,vendor;
    Device(HWND hwnd,int w,int h,int aa):window(hwnd),width(w),height(h),samples(aa){}
    virtual ~Device()=default;
    virtual int texture(int w,int h,const unsigned char* pixels,bool target)=0;
    virtual int mesh(const void* vertices,int vertexCount,const void* indices,int indexCount)=0;
    virtual void begin()=0;
    virtual void bind(int id,int clear,const float* color)=0;
    virtual void draw(int meshId,const Constants& constants,int source,int noise)=0;
    virtual void update(int id,int w,int h,const unsigned char* pixels)=0;
    virtual void present(int root,bool nearest)=0;
    virtual void capture(int root,unsigned char* rgba)=0;
    virtual void finish()=0;
    virtual bool setVsync(bool enabled){vsync=enabled;return true;}
};
std::unique_ptr<Device> makeD9(HWND,int,int,int,bool);
std::unique_ptr<Device> makeD11(HWND,int,int,int);
std::unique_ptr<Device> makeD12(HWND,int,int,int);
