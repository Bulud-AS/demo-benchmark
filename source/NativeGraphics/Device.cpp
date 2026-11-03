#include "Device.h"

static const char* shader=R"HLSL(
#ifdef LEGACY
float4x4 transformMatrix : register(c0);
float4 color : register(c4);
float4 rect : register(c5);
float4 crop : register(c6);
float4 settings : register(c7);
float4 view : register(c8);
float4 texSize : register(c9);
sampler2D sourceTex : register(s0);
sampler2D noiseTex : register(s1);
#define SAMPLE_SOURCE(p) tex2D(sourceTex,p)
#define SAMPLE_NOISE(p) tex2D(noiseTex,p)
#define POSITION_OUT POSITION
#define COLOR_OUT COLOR0
#else
cbuffer Parameters : register(b0) {
float4x4 transformMatrix;float4 color;float4 rect;float4 crop;float4 settings;float4 view;float4 texSize;
};
Texture2D sourceTex : register(t0);
Texture2D noiseTex : register(t1);
SamplerState linearClamp : register(s0);
#define SAMPLE_SOURCE(p) sourceTex.Sample(linearClamp,p)
#define SAMPLE_NOISE(p) noiseTex.Sample(linearClamp,p)
#define POSITION_OUT SV_POSITION
#define COLOR_OUT SV_TARGET
#endif
struct Vertex {float4 position:POSITION_OUT;float2 uv:TEXCOORD0;};
Vertex sceneVS(float3 position:POSITION) {
    Vertex o;o.position=mul(transformMatrix,float4(position,1));
    o.position.z=(o.position.z+o.position.w)*.5;
    o.position.xy+=float2(-view.x,view.y)*view.z*o.position.w;
    o.uv=0;return o;
}
Vertex quadVS(float2 position:POSITION) {
    Vertex o;o.position=float4(position*rect.xy+rect.zw,0,1);
    o.position.xy+=float2(-view.x,view.y)*view.z;
    o.uv=view.w>=2?(position+1)*.5:(position/max(crop.z,.00001)*rect.xy+1)*.5-crop.xy;
    return o;
}
float4 scenePS(Vertex i):COLOR_OUT {return color;}
float4 weights(float t){return float4(-.5*t+t*t-.5*t*t*t,1-2.5*t*t+1.5*t*t*t,.5*t+2*t*t-1.5*t*t*t,-.5*t*t+.5*t*t*t);}
float4 quadPS(Vertex i):COLOR_OUT {
    float2 uv=float2(i.uv.x,1-i.uv.y);
    if(view.w<1.5){
        float2 q=i.uv;
        if(settings.y>.5)q.x+=(SAMPLE_NOISE(float2(1-q.y,1-settings.w)).r-.5)*settings.z;
        return float4(SAMPLE_SOURCE(float2(q.x,1-q.y)).rgb,settings.x);
    }
    if(view.w<2.5 || (view.w>3.5 && settings.y>.5)){
        uv=(clamp(floor(uv*texSize.xy),0,texSize.xy-1)+.5)*texSize.zw;
        return SAMPLE_SOURCE(uv);
    }
    if(view.w>3.5)return SAMPLE_SOURCE(uv);
    float2 pos=uv*texSize.xy-.5,base=floor(pos);
    float4 wx=weights(frac(pos.x)),wy=weights(frac(pos.y));float3 c=0;
    [unroll]for(int y=0;y<4;y++)[unroll]for(int x=0;x<4;x++){
        float2 p=(clamp(base+float2(x-1,y-1),0,texSize.xy-1)+.5)*texSize.zw;
        c+=SAMPLE_SOURCE(p).rgb*wx[x]*wy[y];
    }
    return float4(saturate(c),1);
}
)HLSL";

ID3DBlob* compileShader(const char* entry,const char* profile,bool legacy){
    typedef HRESULT(WINAPI* Compiler)(LPCVOID,SIZE_T,LPCSTR,const D3D_SHADER_MACRO*,ID3DInclude*,LPCSTR,LPCSTR,UINT,UINT,ID3DBlob**,ID3DBlob**);
    static auto compile=reinterpret_cast<Compiler>(systemProc(L"d3dcompiler_47.dll","D3DCompile"));
    D3D_SHADER_MACRO definitions[]={{"LEGACY","1"},{nullptr,nullptr}};
    ID3DBlob *code=nullptr,*errors=nullptr;
    HRESULT hr=compile(shader,strlen(shader),"DemoBenchmark",legacy?definitions:nullptr,nullptr,entry,profile,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
    if(FAILED(hr)){std::string message=errors?static_cast<const char*>(errors->GetBufferPointer()):"Shader compilation failed";release(errors);release(code);throw std::runtime_error(message);}
    release(errors);return code;
}
static thread_local std::string error;
#define EXPORT extern "C" __declspec(dllexport)
#define GUARD(body) try{body;return 1;}catch(const std::exception& e){error=e.what();return 0;}
EXPORT const char* db_error(){return error.c_str();}
EXPORT Device* db_create(int api,HWND window,int width,int height,int samples){
    try{
        std::unique_ptr<Device> d;
        if(api==3||api==4)d=makeD9(window,width,height,samples,api==4);
        else if(api==5)d=makeD11(window,width,height,samples);
        else if(api==6)d=makeD12(window,width,height,samples);
        else throw std::runtime_error("Unknown native rendering API");
        return d.release();
    }catch(const std::exception& e){error=e.what();return nullptr;}
}
EXPORT void db_destroy(Device* d){delete d;}
EXPORT const wchar_t* db_name(Device* d){return d->name.c_str();}
EXPORT const wchar_t* db_vendor(Device* d){return d->vendor.c_str();}
EXPORT const wchar_t* db_version(Device* d){return d->version.c_str();}
EXPORT int db_texture(Device* d,int w,int h,const unsigned char* pixels,int target){try{return d->texture(w,h,pixels,target!=0);}catch(const std::exception& e){error=e.what();return 0;}}
EXPORT int db_mesh(Device* d,const void* vertices,int count,const void* indices,int indexCount){try{return d->mesh(vertices,count,indices,indexCount);}catch(const std::exception& e){error=e.what();return 0;}}
EXPORT int db_begin(Device* d){GUARD(d->begin())}
EXPORT int db_bind(Device* d,int id,int flags,const float* color){GUARD(d->bind(id,flags,color))}
EXPORT int db_draw(Device* d,int mesh,const Constants* c,int source,int noise){GUARD(d->draw(mesh,*c,source,noise))}
EXPORT int db_update(Device* d,int id,int w,int h,const unsigned char* pixels){GUARD(d->update(id,w,h,pixels))}
EXPORT int db_present(Device* d,int root,int nearest){GUARD(d->present(root,nearest!=0))}
EXPORT int db_capture(Device* d,int root,unsigned char* pixels){GUARD(d->capture(root,pixels))}
EXPORT int db_finish(Device* d){GUARD(d->finish())}
EXPORT int db_vsync(Device* d,int enabled){try{return d->setVsync(enabled!=0)?1:0;}catch(const std::exception& e){error=e.what();return 0;}}
