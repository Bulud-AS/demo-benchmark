#include "Device.h"

namespace {
struct Texture9 {
    int w,h;bool target,dirty=false;std::vector<unsigned char> pixels;
    IDirect3DTexture9* image=nullptr;IDirect3DSurface9 *color=nullptr,*depth=nullptr;
    void clear(){release(depth);release(color);release(image);}
    ~Texture9(){clear();}
};
struct Mesh9 {
    int count;std::vector<unsigned char> vertices,indices;
    IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;
    void clear(){release(vb);release(ib);}~Mesh9(){clear();}
};
struct D9:Device {
    IDirect3D9* api=nullptr;IDirect3D9Ex* apiEx=nullptr;
    IDirect3DDevice9* device=nullptr;IDirect3DDevice9Ex* deviceEx=nullptr;
    IDirect3DSurface9* back=nullptr;IDirect3DVertexDeclaration9* layout=nullptr;
    IDirect3DVertexShader9 *sceneVS=nullptr,*quadVS=nullptr;
    IDirect3DPixelShader9 *scenePS=nullptr,*quadPS=nullptr;
    D3DPRESENT_PARAMETERS pp{};D3DCAPS9 caps{};bool active=false,ex=false;
    int bound=0;std::vector<std::unique_ptr<Texture9>> textures;std::vector<std::unique_ptr<Mesh9>> meshes;
    D9(HWND hwnd,int w,int h,int aa):Device(hwnd,w,h,aa){}
    ~D9(){if(active)device->EndScene();textures.clear();meshes.clear();release(sceneVS);release(quadVS);release(scenePS);release(quadPS);release(layout);release(back);release(device);release(deviceEx);release(api);release(apiEx);}
    void init(bool extended){
        ex=extended;
        if(ex){auto create=reinterpret_cast<HRESULT(WINAPI*)(UINT,IDirect3D9Ex**)>(systemProc(L"d3d9.dll","Direct3DCreate9Ex"));check(create(D3D_SDK_VERSION,&apiEx),"Direct3D 9Ex");api=apiEx;api->AddRef();}
        else {auto create=reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(systemProc(L"d3d9.dll","Direct3DCreate9"));api=create(D3D_SDK_VERSION);if(!api)throw std::runtime_error("Direct3D 9 unavailable");}
        check(api->GetDeviceCaps(0,D3DDEVTYPE_HAL,&caps),"Direct3D 9 hardware");
        if(caps.VertexShaderVersion<D3DVS_VERSION(3,0)||caps.PixelShaderVersion<D3DPS_VERSION(3,0))throw std::runtime_error("Direct3D 9 requires Shader Model 3.0");
        if(samples>1){check(api->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,D3DFMT_A8R8G8B8,TRUE,(D3DMULTISAMPLE_TYPE)samples,nullptr),"Color MSAA unsupported");check(api->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,D3DFMT_D24X8,TRUE,(D3DMULTISAMPLE_TYPE)samples,nullptr),"Depth MSAA unsupported");}
        RECT rc;GetClientRect(window,&rc);pp.BackBufferWidth=std::max(1L,rc.right);pp.BackBufferHeight=std::max(1L,rc.bottom);
        pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.BackBufferCount=1;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;pp.Windowed=TRUE;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
        DWORD flags=D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE;
        if(ex){check(apiEx->CreateDeviceEx(0,D3DDEVTYPE_HAL,window,flags,&pp,nullptr,&deviceEx),"Create Direct3D 9Ex device");device=deviceEx;device->AddRef();}
        else check(api->CreateDevice(0,D3DDEVTYPE_HAL,window,flags,&pp,&device),"Create Direct3D 9 device");
        D3DADAPTER_IDENTIFIER9 id{};check(api->GetAdapterIdentifier(0,0,&id),"GPU information");wchar_t text[512];MultiByteToWideChar(CP_ACP,0,id.Description,-1,text,512);name=text;vendor=gpuVendor(id.VendorId);
        version=ex?L"Direct3D 9Ex / Shader Model 3.0":L"Direct3D 9 / Shader Model 3.0";
        D3DVERTEXELEMENT9 elements[]={{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},D3DDECL_END()};
        check(device->CreateVertexDeclaration(elements,&layout),"Vertex declaration");
        auto vs=[&](const char* entry,IDirect3DVertexShader9** result){auto b=compileShader(entry,"vs_3_0",true);HRESULT hr=device->CreateVertexShader((DWORD*)b->GetBufferPointer(),result);release(b);check(hr,entry);};
        auto ps=[&](const char* entry,IDirect3DPixelShader9** result){auto b=compileShader(entry,"ps_3_0",true);HRESULT hr=device->CreatePixelShader((DWORD*)b->GetBufferPointer(),result);release(b);check(hr,entry);};
        vs("sceneVS",&sceneVS);vs("quadVS",&quadVS);ps("scenePS",&scenePS);ps("quadPS",&quadPS);
        check(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back),"Backbuffer");
        float quad[36]={-1,-1,0,0,0,0,1,-1,0,0,0,0,1,1,0,0,0,0,-1,-1,0,0,0,0,1,1,0,0,0,0,-1,1,0,0,0,0};uint32_t indices[]={0,1,2,3,4,5};mesh(quad,6,indices,6);
    }
    void upload(Texture9& t){
        if(t.pixels.empty())return;D3DLOCKED_RECT lock{};check(t.image->LockRect(0,&lock,nullptr,D3DLOCK_DISCARD),"Texture upload");
        for(int y=0;y<t.h;y++){auto dst=(unsigned char*)lock.pBits+y*lock.Pitch;auto src=t.pixels.data()+y*t.w*4;for(int x=0;x<t.w;x++){dst[x*4]=src[x*4+2];dst[x*4+1]=src[x*4+1];dst[x*4+2]=src[x*4];dst[x*4+3]=src[x*4+3];}}
        check(t.image->UnlockRect(0),"Unlock texture");
    }
    void create(Texture9& t){
        check(device->CreateTexture(t.w,t.h,1,t.target?D3DUSAGE_RENDERTARGET:D3DUSAGE_DYNAMIC,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&t.image,nullptr),"Texture allocation");
        if(t.target){if(samples>1)check(device->CreateRenderTarget(t.w,t.h,D3DFMT_A8R8G8B8,(D3DMULTISAMPLE_TYPE)samples,0,FALSE,&t.color,nullptr),"MSAA target");else check(t.image->GetSurfaceLevel(0,&t.color),"Color target");check(device->CreateDepthStencilSurface(t.w,t.h,D3DFMT_D24X8,(D3DMULTISAMPLE_TYPE)(samples>1?samples:0),0,TRUE,&t.depth,nullptr),"Depth target");}
        else upload(t);
    }
    void create(Mesh9& m){
        check(device->CreateVertexBuffer((UINT)m.vertices.size(),D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&m.vb,nullptr),"Vertex buffer");
        check(device->CreateIndexBuffer((UINT)m.indices.size(),D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,D3DPOOL_DEFAULT,&m.ib,nullptr),"Index buffer");
        void* data;check(m.vb->Lock(0,0,&data,0),"Vertex upload");memcpy(data,m.vertices.data(),m.vertices.size());m.vb->Unlock();check(m.ib->Lock(0,0,&data,0),"Index upload");memcpy(data,m.indices.data(),m.indices.size());m.ib->Unlock();
    }
    int texture(int w,int h,const unsigned char* pixels,bool target)override{
        if(w>(int)caps.MaxTextureWidth||h>(int)caps.MaxTextureHeight)throw std::runtime_error("Selected precision exceeds GPU texture limits");
        auto t=std::make_unique<Texture9>();t->w=w;t->h=h;t->target=target;if(pixels)t->pixels.assign(pixels,pixels+w*h*4);create(*t);textures.push_back(std::move(t));return (int)textures.size();
    }
    int mesh(const void* vertices,int count,const void* indices,int indexCount)override{
        auto m=std::make_unique<Mesh9>();m->count=indexCount;m->vertices.assign((unsigned char*)vertices,(unsigned char*)vertices+count*24);m->indices.assign((unsigned char*)indices,(unsigned char*)indices+indexCount*4);create(*m);meshes.push_back(std::move(m));return (int)meshes.size();
    }
    void begin()override{
        if(active)return;RECT rc;GetClientRect(window,&rc);UINT w=std::max(1L,rc.right),h=std::max(1L,rc.bottom),interval=vsync?D3DPRESENT_INTERVAL_ONE:D3DPRESENT_INTERVAL_IMMEDIATE;
        if(w!=pp.BackBufferWidth||h!=pp.BackBufferHeight||interval!=pp.PresentationInterval){
            device->SetTexture(0,nullptr);device->SetTexture(1,nullptr);device->SetStreamSource(0,nullptr,0,0);device->SetIndices(nullptr);device->SetDepthStencilSurface(nullptr);device->SetRenderTarget(0,back);release(back);
            for(auto& t:textures)t->clear();for(auto& m:meshes)m->clear();
            pp.BackBufferWidth=w;pp.BackBufferHeight=h;pp.PresentationInterval=interval;
            check(ex?deviceEx->ResetEx(&pp,nullptr):device->Reset(&pp),"Reset Direct3D 9 device");
            check(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back),"Backbuffer");for(auto& t:textures)create(*t);for(auto& m:meshes)create(*m);
        }
        check(device->BeginScene(),"Begin scene");active=true;
    }
    void resolve(int id){auto& t=*textures.at(id-1);if(t.target&&samples>1&&t.dirty){bool resume=active;if(resume){check(device->EndScene(),"End scene for resolve");active=false;}IDirect3DSurface9* surface=nullptr;check(t.image->GetSurfaceLevel(0,&surface),"Resolve surface");HRESULT hr=device->StretchRect(t.color,nullptr,surface,nullptr,D3DTEXF_NONE);release(surface);check(hr,"Resolve MSAA");if(resume){check(device->BeginScene(),"Resume scene");active=true;}}t.dirty=false;}
    void bind(int id,int flags,const float* color)override{
        begin();auto& t=*textures.at(id-1);bound=id;device->SetTexture(0,nullptr);device->SetTexture(1,nullptr);check(device->SetRenderTarget(0,t.color),"Bind target");check(device->SetDepthStencilSurface(t.depth),"Bind depth");D3DVIEWPORT9 v={0,0,(DWORD)t.w,(DWORD)t.h,0,1};device->SetViewport(&v);
        if(flags){DWORD bits=((flags&1)?D3DCLEAR_TARGET:0)|((flags&2)?D3DCLEAR_ZBUFFER:0);check(device->Clear(0,nullptr,bits,D3DCOLOR_COLORVALUE(color[0],color[1],color[2],color[3]),1,0),"Clear target");if(flags&1)t.dirty=true;}
    }
    void draw(int meshId,const Constants& input,int source,int noise)override{
        Constants c=input;c.view[0]=1.f/(bound?width:pp.BackBufferWidth);c.view[1]=1.f/(bound?height:pp.BackBufferHeight);c.view[2]=1;
        bool scene=c.view[3]<.5f;
        if(!scene){resolve(source);resolve(noise);auto& t=*textures.at(source-1);c.texture[0]=(float)t.w;c.texture[1]=(float)t.h;c.texture[2]=1.f/t.w;c.texture[3]=1.f/t.h;device->SetTexture(0,t.image);device->SetTexture(1,textures.at(noise-1)->image);}
        device->SetVertexDeclaration(layout);device->SetVertexShader(scene?sceneVS:quadVS);device->SetPixelShader(scene?scenePS:quadPS);
        device->SetVertexShaderConstantF(0,(float*)&c,10);device->SetPixelShaderConstantF(0,(float*)&c,10);
        device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);device->SetRenderState(D3DRS_ZENABLE,scene);device->SetRenderState(D3DRS_ZWRITEENABLE,scene);device->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);device->SetRenderState(D3DRS_ALPHABLENDENABLE,c.view[3]==1);device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);device->SetRenderState(D3DRS_LIGHTING,FALSE);device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS,TRUE);
        for(int i=0;i<2;i++){device->SetSamplerState(i,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);device->SetSamplerState(i,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);device->SetSamplerState(i,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);device->SetSamplerState(i,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);}
        auto& m=*meshes.at(meshId-1);device->SetStreamSource(0,m.vb,0,24);device->SetIndices(m.ib);check(device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,(UINT)m.vertices.size()/24,0,m.count/3),"Draw mesh");if(bound)textures.at(bound-1)->dirty=true;
    }
    void update(int id,int w,int h,const unsigned char* pixels)override{auto& t=*textures.at(id-1);if(t.w!=w||t.h!=h){t.clear();t.w=w;t.h=h;t.pixels.assign(pixels,pixels+w*h*4);create(t);}else{t.pixels.assign(pixels,pixels+w*h*4);upload(t);}}
    void present(int root,bool nearest)override{
        begin();resolve(root);bound=0;device->SetRenderTarget(0,back);device->SetDepthStencilSurface(nullptr);D3DVIEWPORT9 vp={0,0,pp.BackBufferWidth,pp.BackBufferHeight,0,1};device->SetViewport(&vp);device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);
        Constants c;c.view[3]=4;c.settings[1]=nearest?1.f:0.f;float fit=std::min((float)pp.BackBufferWidth/width,(float)pp.BackBufferHeight/height);c.rect[0]=width*fit/pp.BackBufferWidth;c.rect[1]=height*fit/pp.BackBufferHeight;draw(1,c,root,root);
        check(device->EndScene(),"End scene");active=false;check(device->Present(nullptr,nullptr,nullptr,nullptr),"Present Direct3D 9");
    }
    void capture(int root,unsigned char* pixels)override{
        if(active){device->EndScene();active=false;}resolve(root);auto& t=*textures.at(root-1);IDirect3DSurface9 *src=nullptr,*copy=nullptr;
        check(t.image->GetSurfaceLevel(0,&src),"Capture source");HRESULT hr=device->CreateOffscreenPlainSurface(width,height,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&copy,nullptr);if(SUCCEEDED(hr))hr=device->GetRenderTargetData(src,copy);release(src);if(FAILED(hr)){release(copy);check(hr,"Capture");}
        D3DLOCKED_RECT lock;hr=copy->LockRect(&lock,nullptr,D3DLOCK_READONLY);if(SUCCEEDED(hr)){for(int y=0;y<height;y++)for(int x=0;x<width;x++){auto s=(unsigned char*)lock.pBits+y*lock.Pitch+x*4;auto d=pixels+(y*width+x)*4;d[0]=s[2];d[1]=s[1];d[2]=s[0];d[3]=s[3];}copy->UnlockRect();}release(copy);check(hr,"Capture readback");
    }
    void finish()override{if(active){device->EndScene();active=false;}IDirect3DQuery9* q=nullptr;check(device->CreateQuery(D3DQUERYTYPE_EVENT,&q),"GPU fence");q->Issue(D3DISSUE_END);while(q->GetData(nullptr,0,D3DGETDATA_FLUSH)==S_FALSE)Sleep(0);release(q);}
};
}
std::unique_ptr<Device> makeD9(HWND hwnd,int w,int h,int samples,bool ex){auto d=std::make_unique<D9>(hwnd,w,h,samples);d->init(ex);return d;}
