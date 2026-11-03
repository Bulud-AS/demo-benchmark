#include "Device.h"
namespace {
struct Texture11 {
    int w,h;bool target,dirty=false;
    ID3D11Texture2D *image=nullptr,*color=nullptr,*depth=nullptr;
    ID3D11ShaderResourceView* srv=nullptr;ID3D11RenderTargetView* rtv=nullptr;ID3D11DepthStencilView* dsv=nullptr;
    ~Texture11(){release(srv);release(rtv);release(dsv);release(image);release(color);release(depth);}
};
struct Mesh11 {ID3D11Buffer *vb=nullptr,*ib=nullptr;int count;~Mesh11(){release(vb);release(ib);}};
struct D11:Device {
    ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;IDXGISwapChain* swap=nullptr;
    ID3D11RenderTargetView* back=nullptr;ID3D11InputLayout* layout=nullptr;
    ID3D11VertexShader *sceneVS=nullptr,*quadVS=nullptr;ID3D11PixelShader *scenePS=nullptr,*quadPS=nullptr;
    ID3D11Buffer* constants=nullptr;ID3D11SamplerState* sampler=nullptr;ID3D11RasterizerState* raster=nullptr;
    ID3D11BlendState *blend=nullptr,*opaque=nullptr;ID3D11DepthStencilState *depthOn=nullptr,*depthOff=nullptr;
    int backW=0,backH=0,bound=0;std::vector<std::unique_ptr<Texture11>> textures;std::vector<std::unique_ptr<Mesh11>> meshes;
    D11(HWND hwnd,int w,int h,int aa):Device(hwnd,w,h,aa){}
    ~D11(){if(context){context->ClearState();context->Flush();}textures.clear();meshes.clear();release(back);release(layout);release(sceneVS);release(quadVS);release(scenePS);release(quadPS);release(constants);release(sampler);release(raster);release(blend);release(opaque);release(depthOn);release(depthOff);release(swap);release(context);release(device);}
    void init(){
        typedef HRESULT(WINAPI* Create)(IDXGIAdapter*,D3D_DRIVER_TYPE,HMODULE,UINT,const D3D_FEATURE_LEVEL*,UINT,UINT,const DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**,ID3D11Device**,D3D_FEATURE_LEVEL*,ID3D11DeviceContext**);
        auto create=reinterpret_cast<Create>(systemProc(L"d3d11.dll","D3D11CreateDeviceAndSwapChain"));
        RECT rc;GetClientRect(window,&rc);backW=std::max(1L,rc.right);backH=std::max(1L,rc.bottom);
        DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=backW;desc.BufferDesc.Height=backH;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.OutputWindow=window;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
        D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1,D3D_FEATURE_LEVEL_10_0},level;
        check(create(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,levels,3,D3D11_SDK_VERSION,&desc,&swap,&device,&level,&context),"Create hardware Direct3D 11 device");
        IDXGIDevice* dxgi=nullptr;IDXGIAdapter* adapter=nullptr;check(device->QueryInterface(__uuidof(IDXGIDevice),(void**)&dxgi),"DXGI device");dxgi->GetAdapter(&adapter);DXGI_ADAPTER_DESC gpu{};adapter->GetDesc(&gpu);name=gpu.Description;vendor=gpuVendor(gpu.VendorId);release(adapter);release(dxgi);
        version=level==D3D_FEATURE_LEVEL_11_0?L"Direct3D 11 / Feature Level 11_0":level==D3D_FEATURE_LEVEL_10_1?L"Direct3D 11 / Feature Level 10_1":L"Direct3D 11 / Feature Level 10_0";
        UINT quality=0;check(device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM,samples,&quality),"MSAA query");if(!quality)throw std::runtime_error("Selected MSAA is unsupported");
        auto vs=[&](const char* entry,ID3D11VertexShader** result){auto b=compileShader(entry,"vs_4_0",false);HRESULT hr=device->CreateVertexShader(b->GetBufferPointer(),b->GetBufferSize(),nullptr,result);if(SUCCEEDED(hr)&&!layout){D3D11_INPUT_ELEMENT_DESC e={"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0};hr=device->CreateInputLayout(&e,1,b->GetBufferPointer(),b->GetBufferSize(),&layout);}release(b);check(hr,entry);};
        auto ps=[&](const char* entry,ID3D11PixelShader** result){auto b=compileShader(entry,"ps_4_0",false);HRESULT hr=device->CreatePixelShader(b->GetBufferPointer(),b->GetBufferSize(),nullptr,result);release(b);check(hr,entry);};
        vs("sceneVS",&sceneVS);vs("quadVS",&quadVS);ps("scenePS",&scenePS);ps("quadPS",&quadPS);
        D3D11_BUFFER_DESC cb{};cb.ByteWidth=sizeof(Constants);cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;check(device->CreateBuffer(&cb,nullptr,&constants),"Constants");
        D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;check(device->CreateSamplerState(&sd,&sampler),"Sampler");
        D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;rd.MultisampleEnable=TRUE;check(device->CreateRasterizerState(&rd,&raster),"Rasterizer");
        D3D11_BLEND_DESC bd{};auto& b=bd.RenderTarget[0];b.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;b.SrcBlend=D3D11_BLEND_SRC_ALPHA;b.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;b.BlendOp=D3D11_BLEND_OP_ADD;b.SrcBlendAlpha=D3D11_BLEND_SRC_ALPHA;b.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;b.BlendOpAlpha=D3D11_BLEND_OP_ADD;check(device->CreateBlendState(&bd,&opaque),"Opaque blend");b.BlendEnable=TRUE;check(device->CreateBlendState(&bd,&blend),"Alpha blend");
        D3D11_DEPTH_STENCIL_DESC dd{};dd.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;dd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;dd.DepthEnable=TRUE;check(device->CreateDepthStencilState(&dd,&depthOn),"Depth state");dd.DepthEnable=FALSE;dd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;check(device->CreateDepthStencilState(&dd,&depthOff),"Depth disabled");
        backbuffer();IDXGIFactory* factory=nullptr;if(SUCCEEDED(swap->GetParent(__uuidof(IDXGIFactory),(void**)&factory))){factory->MakeWindowAssociation(window,DXGI_MWA_NO_ALT_ENTER);release(factory);}
        float quad[36]={-1,-1,0,0,0,0,1,-1,0,0,0,0,1,1,0,0,0,0,-1,-1,0,0,0,0,1,1,0,0,0,0,-1,1,0,0,0,0};uint32_t indices[]={0,1,2,3,4,5};mesh(quad,6,indices,6);
    }
    void backbuffer(){ID3D11Texture2D* image=nullptr;check(swap->GetBuffer(0,__uuidof(ID3D11Texture2D),(void**)&image),"Backbuffer");HRESULT hr=device->CreateRenderTargetView(image,nullptr,&back);release(image);check(hr,"Backbuffer view");}
    int texture(int w,int h,const unsigned char* pixels,bool target)override{
        auto t=std::make_unique<Texture11>();t->w=w;t->h=h;t->target=target;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|(target?D3D11_BIND_RENDER_TARGET:0);
        D3D11_SUBRESOURCE_DATA data={pixels,(UINT)w*4,0};check(device->CreateTexture2D(&desc,pixels?&data:nullptr,&t->image),"Texture allocation");check(device->CreateShaderResourceView(t->image,nullptr,&t->srv),"Texture view");
        if(target){if(samples>1){desc.SampleDesc.Count=samples;desc.BindFlags=D3D11_BIND_RENDER_TARGET;check(device->CreateTexture2D(&desc,nullptr,&t->color),"MSAA target");}else{t->color=t->image;t->color->AddRef();}check(device->CreateRenderTargetView(t->color,nullptr,&t->rtv),"Color view");desc.Format=DXGI_FORMAT_D32_FLOAT;desc.BindFlags=D3D11_BIND_DEPTH_STENCIL;check(device->CreateTexture2D(&desc,nullptr,&t->depth),"Depth target");check(device->CreateDepthStencilView(t->depth,nullptr,&t->dsv),"Depth view");}
        textures.push_back(std::move(t));return (int)textures.size();
    }
    int mesh(const void* vertices,int count,const void* indices,int indexCount)override{
        auto m=std::make_unique<Mesh11>();m->count=indexCount;D3D11_BUFFER_DESC d{};d.ByteWidth=count*24;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_VERTEX_BUFFER;D3D11_SUBRESOURCE_DATA data={vertices,0,0};check(device->CreateBuffer(&d,&data,&m->vb),"Vertex buffer");d.ByteWidth=indexCount*4;d.BindFlags=D3D11_BIND_INDEX_BUFFER;data.pSysMem=indices;check(device->CreateBuffer(&d,&data,&m->ib),"Index buffer");meshes.push_back(std::move(m));return (int)meshes.size();
    }
    void begin()override{RECT rc;GetClientRect(window,&rc);int w=std::max(1L,rc.right),h=std::max(1L,rc.bottom);if(w!=backW||h!=backH){context->OMSetRenderTargets(0,nullptr,nullptr);release(back);check(swap->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0),"Resize swap chain");backW=w;backH=h;backbuffer();}}
    void resolve(int id){auto& t=*textures.at(id-1);if(t.target&&samples>1&&t.dirty)context->ResolveSubresource(t.image,0,t.color,0,DXGI_FORMAT_R8G8B8A8_UNORM);t.dirty=false;}
    void bind(int id,int clear,const float* color)override{
        bound=id;auto& t=*textures.at(id-1);ID3D11ShaderResourceView* none[2]={};context->PSSetShaderResources(0,2,none);context->OMSetRenderTargets(1,&t.rtv,t.dsv);D3D11_VIEWPORT vp={0,0,(float)t.w,(float)t.h,0,1};context->RSSetViewports(1,&vp);if(clear&1){context->ClearRenderTargetView(t.rtv,color);t.dirty=true;}if(clear&2)context->ClearDepthStencilView(t.dsv,D3D11_CLEAR_DEPTH,1,0);
    }
    void draw(int meshId,const Constants& input,int source,int noise)override{
        Constants c=input;bool scene=c.view[3]<.5f;
        if(!scene){resolve(source);resolve(noise);auto& t=*textures.at(source-1);c.texture[0]=(float)t.w;c.texture[1]=(float)t.h;c.texture[2]=1.f/t.w;c.texture[3]=1.f/t.h;ID3D11ShaderResourceView* views[]={t.srv,textures.at(noise-1)->srv};context->PSSetShaderResources(0,2,views);}
        context->UpdateSubresource(constants,0,nullptr,&c,0,0);context->VSSetConstantBuffers(0,1,&constants);context->PSSetConstantBuffers(0,1,&constants);context->VSSetShader(scene?sceneVS:quadVS,nullptr,0);context->PSSetShader(scene?scenePS:quadPS,nullptr,0);context->PSSetSamplers(0,1,&sampler);context->RSSetState(raster);context->OMSetBlendState(c.view[3]==1?blend:opaque,nullptr,~0u);context->OMSetDepthStencilState(scene?depthOn:depthOff,0);
        auto& m=*meshes.at(meshId-1);UINT stride=24,offset=0;context->IASetInputLayout(layout);context->IASetVertexBuffers(0,1,&m.vb,&stride,&offset);context->IASetIndexBuffer(m.ib,DXGI_FORMAT_R32_UINT,0);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->DrawIndexed(m.count,0,0);if(bound)textures.at(bound-1)->dirty=true;
    }
    void update(int id,int w,int h,const unsigned char* pixels)override{
        auto& t=*textures.at(id-1);if(t.w!=w||t.h!=h){ID3D11ShaderResourceView* none[2]={};context->PSSetShaderResources(0,2,none);int newer=texture(w,h,pixels,false);textures[id-1].swap(textures[newer-1]);textures.pop_back();}else context->UpdateSubresource(t.image,0,nullptr,pixels,w*4,0);
    }
    void present(int root,bool nearest)override{
        resolve(root);bound=0;context->OMSetRenderTargets(1,&back,nullptr);D3D11_VIEWPORT vp={0,0,(float)backW,(float)backH,0,1};context->RSSetViewports(1,&vp);float black[]={0,0,0,1};context->ClearRenderTargetView(back,black);Constants c;c.view[3]=4;c.settings[1]=nearest?1.f:0.f;float fit=std::min((float)backW/width,(float)backH/height);c.rect[0]=width*fit/backW;c.rect[1]=height*fit/backH;draw(1,c,root,root);check(swap->Present(vsync?1:0,0),"Present Direct3D 11");check(device->GetDeviceRemovedReason(),"Direct3D 11 device removed");
    }
    void capture(int root,unsigned char* pixels)override{
        resolve(root);D3D11_TEXTURE2D_DESC desc{};textures.at(root-1)->image->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ID3D11Texture2D* copy=nullptr;check(device->CreateTexture2D(&desc,nullptr,&copy),"Capture buffer");context->CopyResource(copy,textures.at(root-1)->image);D3D11_MAPPED_SUBRESOURCE map;HRESULT hr=context->Map(copy,0,D3D11_MAP_READ,0,&map);if(SUCCEEDED(hr)){for(int y=0;y<height;y++)memcpy(pixels+y*width*4,(char*)map.pData+y*map.RowPitch,width*4);context->Unmap(copy,0);}release(copy);check(hr,"Capture readback");
    }
    void finish()override{
        ID3D11Query* query=nullptr;D3D11_QUERY_DESC desc={D3D11_QUERY_EVENT,0};check(device->CreateQuery(&desc,&query),"GPU fence");context->End(query);context->Flush();HRESULT hr;while((hr=context->GetData(query,nullptr,0,0))==S_FALSE)Sleep(0);release(query);check(hr,"GPU completion");
    }
};
}
std::unique_ptr<Device> makeD11(HWND hwnd,int w,int h,int aa){auto d=std::make_unique<D11>(hwnd,w,h,aa);d->init();return d;}
