#include "Device.h"
#include <map>
namespace {
struct Texture12 {
    int w,h,srv=-1;bool target,dirty=false;
    ID3D12Resource *image=nullptr,*color=nullptr,*depth=nullptr;
    D3D12_RESOURCE_STATES imageState=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,colorState=D3D12_RESOURCE_STATE_RENDER_TARGET;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{},dsv{};
    ~Texture12(){release(image);release(color);release(depth);}
};
struct Mesh12 {
    ID3D12Resource *vb=nullptr,*ib=nullptr;D3D12_VERTEX_BUFFER_VIEW vertices{};D3D12_INDEX_BUFFER_VIEW indices{};int count;
    ~Mesh12(){release(vb);release(ib);}
};
struct Frame12 {
    ID3D12CommandAllocator* allocator=nullptr;UINT64 fence=0;std::vector<ID3D12Resource*> uploads;std::vector<int> retiredSlots;
    void clear(){for(auto p:uploads)p->Release();uploads.clear();}~Frame12(){clear();release(allocator);}
};
struct D12:Device {
    ID3D12Device* device=nullptr;ID3D12CommandQueue* queue=nullptr;IDXGISwapChain3* swap=nullptr;
    ID3D12GraphicsCommandList* commands=nullptr;ID3D12Fence* fence=nullptr;HANDLE event=nullptr;UINT64 serial=0;
    ID3D12DescriptorHeap *srvHeap=nullptr,*rtvHeap=nullptr,*dsvHeap=nullptr;
    ID3D12RootSignature* signature=nullptr;ID3DBlob *sceneVS=nullptr,*quadVS=nullptr,*scenePS=nullptr,*quadPS=nullptr;
    std::array<Frame12,3> frames;int frame=0;bool open=false;int bound=0,backW=0,backH=0;UINT swapFlags=0;
    UINT srvStep=0,rtvStep=0,dsvStep=0;int nextSrv=0,nextRtv=2,nextDsv=0;std::vector<int> freeSrv;
    ID3D12Resource* buffers[2]{};std::map<int,ID3D12PipelineState*> pipelines;
    std::vector<std::unique_ptr<Texture12>> textures;std::vector<std::unique_ptr<Mesh12>> meshes;
    D12(HWND hwnd,int w,int h,int aa):Device(hwnd,w,h,aa){}
    ~D12(){try{if(queue&&fence)finish();}catch(...){}textures.clear();meshes.clear();for(auto& p:pipelines)release(p.second);for(auto& b:buffers)release(b);release(sceneVS);release(quadVS);release(scenePS);release(quadPS);release(signature);release(srvHeap);release(rtvHeap);release(dsvHeap);release(commands);for(auto& f:frames){f.clear();release(f.allocator);}release(swap);release(fence);release(queue);release(device);if(event)CloseHandle(event);}
    D3D12_CPU_DESCRIPTOR_HANDLE cpu(ID3D12DescriptorHeap* heap,int index,UINT step){auto h=heap->GetCPUDescriptorHandleForHeapStart();h.ptr+=(SIZE_T)index*step;return h;}
    D3D12_GPU_DESCRIPTOR_HANDLE gpu(int index){auto h=srvHeap->GetGPUDescriptorHandleForHeapStart();h.ptr+=(UINT64)index*srvStep;return h;}
    void wait(UINT64 value){if(value&&fence->GetCompletedValue()<value){check(fence->SetEventOnCompletion(value,event),"GPU fence event");if(WaitForSingleObject(event,30000)!=WAIT_OBJECT_0)throw std::runtime_error("Direct3D 12 GPU timeout");}check(device->GetDeviceRemovedReason(),"Direct3D 12 device removed");}
    void submit(){if(!open)return;check(commands->Close(),"Close command list");ID3D12CommandList* lists[]={commands};queue->ExecuteCommandLists(1,lists);check(queue->Signal(fence,++serial),"Signal GPU fence");frames[frame].fence=serial;frame=(frame+1)%3;open=false;}
    void finish()override{submit();check(queue->Signal(fence,++serial),"Flush GPU queue");wait(serial);}
    void start(){if(open)return;auto& f=frames[frame];wait(f.fence);freeSrv.insert(freeSrv.end(),f.retiredSlots.begin(),f.retiredSlots.end());f.retiredSlots.clear();f.clear();check(f.allocator->Reset(),"Reset command allocator");check(commands->Reset(f.allocator,nullptr),"Reset command list");open=true;ID3D12DescriptorHeap* heaps[]={srvHeap};commands->SetDescriptorHeaps(1,heaps);commands->SetGraphicsRootSignature(signature);}
    void barrier(ID3D12Resource* image,D3D12_RESOURCE_STATES& state,D3D12_RESOURCE_STATES next){if(state==next)return;D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=image;b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;b.Transition.StateBefore=state;b.Transition.StateAfter=next;commands->ResourceBarrier(1,&b);state=next;}
    ID3D12Resource* resource(const D3D12_RESOURCE_DESC& desc,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state,const D3D12_CLEAR_VALUE* clear=nullptr){
        D3D12_HEAP_PROPERTIES heap{};heap.Type=type;heap.CreationNodeMask=heap.VisibleNodeMask=1;ID3D12Resource* r=nullptr;check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,state,clear,__uuidof(ID3D12Resource),(void**)&r),"GPU resource allocation");return r;
    }
    ID3D12Resource* buffer(UINT64 size,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=size;d.Height=1;d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;return resource(d,type,state);}
    void init(){
        auto create=reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(systemProc(L"d3d12.dll","D3D12CreateDevice"));
        auto factoryProc=reinterpret_cast<HRESULT(WINAPI*)(REFIID,void**)>(systemProc(L"dxgi.dll","CreateDXGIFactory1"));IDXGIFactory4* factory=nullptr;check(factoryProc(__uuidof(IDXGIFactory4),(void**)&factory),"DXGI factory");
        for(UINT index=0;;index++){IDXGIAdapter1* adapter=nullptr;HRESULT enumerated=factory->EnumAdapters1(index,&adapter);if(enumerated==DXGI_ERROR_NOT_FOUND)break;check(enumerated,"Enumerate GPU");DXGI_ADAPTER_DESC1 desc{};adapter->GetDesc1(&desc);HRESULT hr=(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)?E_FAIL:create(adapter,D3D_FEATURE_LEVEL_11_0,__uuidof(ID3D12Device),(void**)&device);if(SUCCEEDED(hr)){name=desc.Description;vendor=gpuVendor(desc.VendorId);}release(adapter);if(device)break;}
        if(!device){release(factory);throw std::runtime_error("No hardware Direct3D 12 device (Feature Level 11_0 or later)");}
        version=L"Direct3D 12 / Native";
        IDXGIFactory5* modernFactory=nullptr;if(SUCCEEDED(factory->QueryInterface(__uuidof(IDXGIFactory5),(void**)&modernFactory))){BOOL tearing=FALSE;if(SUCCEEDED(modernFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,&tearing,sizeof(tearing)))&&tearing)swapFlags=DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;release(modernFactory);}
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaa={DXGI_FORMAT_R8G8B8A8_UNORM,(UINT)samples,D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,0};check(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&msaa,sizeof(msaa)),"MSAA query");if(!msaa.NumQualityLevels){release(factory);throw std::runtime_error("Selected MSAA is unsupported");}
        D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;check(device->CreateCommandQueue(&q,__uuidof(ID3D12CommandQueue),(void**)&queue),"Command queue");
        for(auto& f:frames)check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,__uuidof(ID3D12CommandAllocator),(void**)&f.allocator),"Command allocator");
        check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,frames[0].allocator,nullptr,__uuidof(ID3D12GraphicsCommandList),(void**)&commands),"Command list");check(commands->Close(),"Initial command list");
        check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,__uuidof(ID3D12Fence),(void**)&fence),"Fence");event=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!event)throw std::runtime_error("GPU event allocation failed");
        auto heap=[&](D3D12_DESCRIPTOR_HEAP_TYPE type,UINT count,D3D12_DESCRIPTOR_HEAP_FLAGS flags,ID3D12DescriptorHeap** result){D3D12_DESCRIPTOR_HEAP_DESC d={type,count,flags,0};check(device->CreateDescriptorHeap(&d,__uuidof(ID3D12DescriptorHeap),(void**)result),"Descriptor heap");};
        heap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,2048,D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,&srvHeap);heap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV,256,D3D12_DESCRIPTOR_HEAP_FLAG_NONE,&rtvHeap);heap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV,256,D3D12_DESCRIPTOR_HEAP_FLAG_NONE,&dsvHeap);
        srvStep=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);rtvStep=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);dsvStep=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        D3D12_DESCRIPTOR_RANGE ranges[2]={{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,1,0,0}};
        D3D12_ROOT_PARAMETER parameters[3]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[0].Constants={0,0,40};parameters[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
        for(int i=0;i<2;i++){parameters[i+1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[i+1].DescriptorTable={1,&ranges[i]};parameters[i+1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
        D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC root={3,parameters,1,&sampler,D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
        auto serialize=reinterpret_cast<PFN_D3D12_SERIALIZE_ROOT_SIGNATURE>(systemProc(L"d3d12.dll","D3D12SerializeRootSignature"));ID3DBlob *blob=nullptr,*errors=nullptr;HRESULT hr=serialize(&root,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors);release(errors);check(hr,"Root signature serialization");hr=device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),__uuidof(ID3D12RootSignature),(void**)&signature);release(blob);check(hr,"Root signature");
        sceneVS=compileShader("sceneVS","vs_5_0",false);quadVS=compileShader("quadVS","vs_5_0",false);scenePS=compileShader("scenePS","ps_5_0",false);quadPS=compileShader("quadPS","ps_5_0",false);
        RECT rc;GetClientRect(window,&rc);backW=std::max(1L,rc.right);backH=std::max(1L,rc.bottom);DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=backW;desc.Height=backH;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;desc.Scaling=DXGI_SCALING_STRETCH;desc.Flags=swapFlags;
        IDXGISwapChain1* base=nullptr;hr=factory->CreateSwapChainForHwnd(queue,window,&desc,nullptr,nullptr,&base);if(SUCCEEDED(hr)){factory->MakeWindowAssociation(window,DXGI_MWA_NO_ALT_ENTER);hr=base->QueryInterface(__uuidof(IDXGISwapChain3),(void**)&swap);}release(base);release(factory);check(hr,"Create Direct3D 12 swap chain");backbuffers();
        float quad[36]={-1,-1,0,0,0,0,1,-1,0,0,0,0,1,1,0,0,0,0,-1,-1,0,0,0,0,1,1,0,0,0,0,-1,1,0,0,0,0};uint32_t indices[]={0,1,2,3,4,5};mesh(quad,6,indices,6);
    }
    void backbuffers(){for(int i=0;i<2;i++){check(swap->GetBuffer(i,__uuidof(ID3D12Resource),(void**)&buffers[i]),"Backbuffer");device->CreateRenderTargetView(buffers[i],nullptr,cpu(rtvHeap,i,rtvStep));}}
    void begin()override{
        RECT rc;GetClientRect(window,&rc);int w=std::max(1L,rc.right),h=std::max(1L,rc.bottom);if(w!=backW||h!=backH){finish();for(auto& b:buffers)release(b);check(swap->ResizeBuffers(2,w,h,DXGI_FORMAT_R8G8B8A8_UNORM,swapFlags),"Resize swap chain");backW=w;backH=h;backbuffers();}start();
    }
    void upload(Texture12& t,const unsigned char* pixels){
        start();auto desc=t.image->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 bytes=0;device->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&bytes);ID3D12Resource* staging=buffer(bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);frames[frame].uploads.push_back(staging);void* data;D3D12_RANGE empty={0,0};check(staging->Map(0,&empty,&data),"Texture upload map");for(int y=0;y<t.h;y++)memcpy((char*)data+footprint.Offset+y*footprint.Footprint.RowPitch,pixels+y*t.w*4,t.w*4);staging->Unmap(0,nullptr);
        barrier(t.image,t.imageState,D3D12_RESOURCE_STATE_COPY_DEST);D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=t.image;dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=staging;src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=footprint;commands->CopyTextureRegion(&dst,0,0,0,&src,nullptr);barrier(t.image,t.imageState,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    int texture(int w,int h,const unsigned char* pixels,bool target)override{
        if((nextSrv>=2048&&freeSrv.empty())||nextRtv>=256||nextDsv>=256)throw std::runtime_error("Graphics descriptor limit exceeded");
        auto t=std::make_unique<Texture12>();t->w=w;t->h=h;t->target=target;if(freeSrv.empty())t->srv=nextSrv++;else{t->srv=freeSrv.back();freeSrv.pop_back();}
        D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=w;d.Height=h;d.DepthOrArraySize=d.MipLevels=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;d.Flags=target?D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET:D3D12_RESOURCE_FLAG_NONE;
        D3D12_CLEAR_VALUE clear{};clear.Format=d.Format;t->image=resource(d,D3D12_HEAP_TYPE_DEFAULT,t->imageState,target?&clear:nullptr);
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=d.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;device->CreateShaderResourceView(t->image,&srv,cpu(srvHeap,t->srv,srvStep));
        if(target){d.SampleDesc.Count=samples;if(samples>1)t->color=resource(d,D3D12_HEAP_TYPE_DEFAULT,t->colorState,&clear);else{t->color=t->image;t->color->AddRef();}t->rtv=cpu(rtvHeap,nextRtv++,rtvStep);device->CreateRenderTargetView(t->color,nullptr,t->rtv);d.Format=DXGI_FORMAT_D32_FLOAT;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;clear.Format=d.Format;clear.DepthStencil.Depth=1;t->depth=resource(d,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_DEPTH_WRITE,&clear);t->dsv=cpu(dsvHeap,nextDsv++,dsvStep);device->CreateDepthStencilView(t->depth,nullptr,t->dsv);}
        if(pixels)upload(*t,pixels);textures.push_back(std::move(t));return (int)textures.size();
    }
    ID3D12Resource* uploadBuffer(const void* data,UINT size,D3D12_RESOURCE_STATES end){
        start();ID3D12Resource* result=buffer(size,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);ID3D12Resource* staging=buffer(size,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);frames[frame].uploads.push_back(staging);void* dst;D3D12_RANGE empty={0,0};check(staging->Map(0,&empty,&dst),"Buffer upload map");memcpy(dst,data,size);staging->Unmap(0,nullptr);commands->CopyBufferRegion(result,0,staging,0,size);D3D12_RESOURCE_STATES state=D3D12_RESOURCE_STATE_COPY_DEST;barrier(result,state,end);return result;
    }
    int mesh(const void* vertices,int count,const void* indices,int indexCount)override{
        auto m=std::make_unique<Mesh12>();m->count=indexCount;m->vb=uploadBuffer(vertices,count*24,D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);m->ib=uploadBuffer(indices,indexCount*4,D3D12_RESOURCE_STATE_INDEX_BUFFER);m->vertices={m->vb->GetGPUVirtualAddress(),(UINT)count*24,24};m->indices={m->ib->GetGPUVirtualAddress(),(UINT)indexCount*4,DXGI_FORMAT_R32_UINT};meshes.push_back(std::move(m));return (int)meshes.size();
    }
    void resolve(int id){auto& t=*textures.at(id-1);if(!t.target)return;if(samples>1&&t.dirty){barrier(t.color,t.colorState,D3D12_RESOURCE_STATE_RESOLVE_SOURCE);barrier(t.image,t.imageState,D3D12_RESOURCE_STATE_RESOLVE_DEST);commands->ResolveSubresource(t.image,0,t.color,0,DXGI_FORMAT_R8G8B8A8_UNORM);}barrier(t.image,t.imageState,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);t.dirty=false;}
    void bind(int id,int clear,const float* color)override{
        start();bound=id;auto& t=*textures.at(id-1);if(samples>1)barrier(t.color,t.colorState,D3D12_RESOURCE_STATE_RENDER_TARGET);else barrier(t.image,t.imageState,D3D12_RESOURCE_STATE_RENDER_TARGET);commands->OMSetRenderTargets(1,&t.rtv,FALSE,&t.dsv);D3D12_VIEWPORT viewport={0,0,(float)t.w,(float)t.h,0,1};D3D12_RECT scissor={0,0,t.w,t.h};commands->RSSetViewports(1,&viewport);commands->RSSetScissorRects(1,&scissor);if(clear&1){commands->ClearRenderTargetView(t.rtv,color,0,nullptr);t.dirty=true;}if(clear&2)commands->ClearDepthStencilView(t.dsv,D3D12_CLEAR_FLAG_DEPTH,1,0,0,nullptr);
    }
    ID3D12PipelineState* pipeline(int kind){
        int aa=bound?samples:1,key=(aa<<4)|(bound?8:0)|kind;auto found=pipelines.find(key);if(found!=pipelines.end())return found->second;
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};d.pRootSignature=signature;ID3DBlob* vs=kind==0?sceneVS:quadVS;ID3DBlob* ps=kind==0?scenePS:quadPS;d.VS={vs->GetBufferPointer(),vs->GetBufferSize()};d.PS={ps->GetBufferPointer(),ps->GetBufferSize()};D3D12_INPUT_ELEMENT_DESC element={"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0};d.InputLayout={&element,1};d.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;d.SampleMask=~0u;d.NumRenderTargets=1;d.RTVFormats[0]=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=aa;d.DSVFormat=bound?DXGI_FORMAT_D32_FLOAT:DXGI_FORMAT_UNKNOWN;
        d.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;d.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;d.RasterizerState.DepthClipEnable=TRUE;d.RasterizerState.MultisampleEnable=TRUE;
        auto& b=d.BlendState.RenderTarget[0];b.BlendEnable=kind==1;b.SrcBlend=D3D12_BLEND_SRC_ALPHA;b.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;b.BlendOp=D3D12_BLEND_OP_ADD;b.SrcBlendAlpha=D3D12_BLEND_SRC_ALPHA;b.DestBlendAlpha=D3D12_BLEND_INV_SRC_ALPHA;b.BlendOpAlpha=D3D12_BLEND_OP_ADD;b.LogicOp=D3D12_LOGIC_OP_NOOP;b.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
        d.DepthStencilState.DepthEnable=kind==0;d.DepthStencilState.DepthWriteMask=kind==0?D3D12_DEPTH_WRITE_MASK_ALL:D3D12_DEPTH_WRITE_MASK_ZERO;d.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS_EQUAL;d.DepthStencilState.StencilReadMask=d.DepthStencilState.StencilWriteMask=255;d.DepthStencilState.FrontFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_ALWAYS};d.DepthStencilState.BackFace=d.DepthStencilState.FrontFace;
        ID3D12PipelineState* p=nullptr;check(device->CreateGraphicsPipelineState(&d,__uuidof(ID3D12PipelineState),(void**)&p),"Graphics pipeline");pipelines[key]=p;return p;
    }
    void draw(int meshId,const Constants& input,int source,int noise)override{
        start();Constants c=input;int kind=c.view[3]<.5f?0:c.view[3]==1?1:2;
        if(kind){resolve(source);if(noise!=source)resolve(noise);auto& t=*textures.at(source-1);c.texture[0]=(float)t.w;c.texture[1]=(float)t.h;c.texture[2]=1.f/t.w;c.texture[3]=1.f/t.h;commands->SetGraphicsRootDescriptorTable(1,gpu(t.srv));commands->SetGraphicsRootDescriptorTable(2,gpu(textures.at(noise-1)->srv));}
        commands->SetPipelineState(pipeline(kind));commands->SetGraphicsRoot32BitConstants(0,40,&c,0);auto& m=*meshes.at(meshId-1);commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);commands->IASetVertexBuffers(0,1,&m.vertices);commands->IASetIndexBuffer(&m.indices);commands->DrawIndexedInstanced(m.count,1,0,0,0);if(bound)textures.at(bound-1)->dirty=true;
    }
    void update(int id,int w,int h,const unsigned char* pixels)override{
        start();auto& t=*textures.at(id-1);if(t.w!=w||t.h!=h){int newer=texture(w,h,pixels,false);t.image->AddRef();frames[frame].uploads.push_back(t.image);frames[frame].retiredSlots.push_back(t.srv);textures[id-1].swap(textures[newer-1]);textures.pop_back();}else upload(t,pixels);
    }
    void present(int root,bool nearest)override{
        start();resolve(root);bound=0;UINT index=swap->GetCurrentBackBufferIndex();D3D12_RESOURCE_STATES state=D3D12_RESOURCE_STATE_PRESENT;barrier(buffers[index],state,D3D12_RESOURCE_STATE_RENDER_TARGET);auto target=cpu(rtvHeap,index,rtvStep);commands->OMSetRenderTargets(1,&target,FALSE,nullptr);float black[]={0,0,0,1};commands->ClearRenderTargetView(target,black,0,nullptr);D3D12_VIEWPORT vp={0,0,(float)backW,(float)backH,0,1};D3D12_RECT scissor={0,0,backW,backH};commands->RSSetViewports(1,&vp);commands->RSSetScissorRects(1,&scissor);
        Constants c;c.view[3]=4;c.settings[1]=nearest?1.f:0.f;float fit=std::min((float)backW/width,(float)backH/height);c.rect[0]=width*fit/backW;c.rect[1]=height*fit/backH;draw(1,c,root,root);barrier(buffers[index],state,D3D12_RESOURCE_STATE_PRESENT);submit();check(swap->Present(vsync?1:0,!vsync&&swapFlags?DXGI_PRESENT_ALLOW_TEARING:0),"Present Direct3D 12");
    }
    void capture(int root,unsigned char* pixels)override{
        start();resolve(root);auto& t=*textures.at(root-1);auto desc=t.image->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 size;device->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&size);ID3D12Resource* readback=buffer(size,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);barrier(t.image,t.imageState,D3D12_RESOURCE_STATE_COPY_SOURCE);D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=readback;dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=footprint;D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=t.image;src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;commands->CopyTextureRegion(&dst,0,0,0,&src,nullptr);barrier(t.image,t.imageState,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);finish();void* data;D3D12_RANGE range={0,(SIZE_T)size};HRESULT hr=readback->Map(0,&range,&data);if(SUCCEEDED(hr)){for(int y=0;y<height;y++)memcpy(pixels+y*width*4,(char*)data+footprint.Offset+y*footprint.Footprint.RowPitch,width*4);D3D12_RANGE empty={0,0};readback->Unmap(0,&empty);}release(readback);check(hr,"Capture readback");
    }
};
}
std::unique_ptr<Device> makeD12(HWND hwnd,int w,int h,int aa){auto d=std::make_unique<D12>(hwnd,w,h,aa);d->init();return d;}
