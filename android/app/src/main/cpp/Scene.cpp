#include "Core.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"

double mono(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
void need(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
Bytes asset(AAssetManager* manager,const std::string& name){AAsset* file=AAssetManager_open(manager,name.c_str(),AASSET_MODE_BUFFER);need(file!=nullptr,"Asset not found: "+name);Bytes bytes(AAsset_getLength64(file));size_t pos=0;while(pos<bytes.size()){int count=AAsset_read(file,bytes.data()+pos,bytes.size()-pos);if(count<=0){AAsset_close(file);throw std::runtime_error("Asset read failed: "+name);}pos+=count;}AAsset_close(file);return bytes;}
Json assetJson(AAssetManager* a,const std::string& path){auto bytes=asset(a,path);return Json::parse(bytes.begin(),bytes.end());}
Quality quality(int demo,int id){if(demo)return id?Quality{1,1440,1080,1}:Quality{0,640,480,1};static const Quality all[]={{0,640,360,1},{1,960,540,1},{2,1280,720,2},{3,1600,900,4},{4,1920,1080,4}};return all[std::clamp(id,0,4)];}
Matrix identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
Matrix multiply(const Matrix& a,const Matrix& b){Matrix c{};for(int i=0;i<4;i++)for(int j=0;j<4;j++)for(int k=0;k<4;k++)c[i*4+j]+=a[k*4+j]*b[i*4+k];return c;}
static Vec transform(const Matrix& m,Vec v,float w){return {m[0]*v[0]+m[4]*v[1]+m[8]*v[2]+m[12]*w,m[1]*v[0]+m[5]*v[1]+m[9]*v[2]+m[13]*w,m[2]*v[0]+m[6]*v[1]+m[10]*v[2]+m[14]*w};}
static float dot(Vec a,Vec b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static Vec norm(Vec a){float length=std::sqrt(dot(a,a));for(float& v:a)v/=length;return a;}
static Vec cross(Vec a,Vec b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
static Matrix view(Vec eye,Vec look,Vec up){for(float& x:look)x=-x;Vec z=norm(look),x=norm(cross(up,z)),y=cross(z,x);return {x[0],y[0],z[0],0,x[1],y[1],z[1],0,x[2],y[2],z[2],0,-dot(x,eye),-dot(y,eye),-dot(z,eye),1};}
static std::vector<float> key(const Json& keys,double time,bool quat){if(keys.empty())return quat?std::vector<float>{1,0,0,0}:std::vector<float>{1,1,1};int lo=0,hi=(int)keys.size()-1;while(lo+1<hi){int mid=(lo+hi)/2;if(keys[mid][0].get<double>()<=time)lo=mid;else hi=mid;}if(time>=keys[hi][0].get<double>())lo=hi;const auto& a=keys[lo];const auto& b=keys[std::min(lo+1,(int)keys.size()-1)];double t=&a==&b?0:std::clamp((time-a[0].get<double>())/(b[0].get<double>()-a[0].get<double>()),0.,1.),sign=1,wa=1-t,wb=t;if(quat){double d=0;for(size_t i=1;i<a.size();i++)d+=a[i].get<double>()*b[i].get<double>();if(d<0){sign=-1;d=-d;}if(d<.9995){double angle=std::acos(std::min(1.,d)),sn=std::sin(angle);wa=std::sin((1-t)*angle)/sn;wb=std::sin(t*angle)/sn;}}std::vector<float> v(a.size()-1);for(size_t i=0;i<v.size();i++)v[i]=(float)(a[i+1].get<double>()*wa+b[i+1].get<double>()*wb*sign);if(quat){double n=0;for(float x:v)n+=(double)x*x;n=std::sqrt(n);for(float& x:v)x=(float)(x/n);}return v;}
static Matrix trs(const std::vector<float>& p,const std::vector<float>& q,const std::vector<float>& s){float w=q[0],x=q[1],y=q[2],z=q[3];return {(1-2*y*y-2*z*z)*s[0],(2*x*y+2*w*z)*s[0],(2*x*z-2*w*y)*s[0],0,(2*x*y-2*w*z)*s[1],(1-2*x*x-2*z*z)*s[1],(2*y*z+2*w*x)*s[1],0,(2*x*z+2*w*y)*s[2],(2*y*z-2*w*x)*s[2],(1-2*x*x-2*y*y)*s[2],0,p[0],p[1],p[2],1};}
SceneRenderer::SceneRenderer(AAssetManager* manager,Backend& backend,Quality q,bool sr):assets(manager),gpu(backend),preset(q),second(sr){
    uint8_t white[]={255,255,255,255};whiteId=gpu.texture(1,1,white,false);
    if(sr){uint8_t black[]={0,0,0,255};srId=gpu.texture(1,1,black,false);rootId=target();return;}
    timeline=assetJson(assets,"timeline-android.json");Json lod=assetJson(assets,"quality/manifest.json");std::map<std::string,Bytes> buffers;
    for(auto& r:timeline["resources"].items()){
        std::string path=r.value().get<std::string>();if(path.size()<4)continue;
        if(path.substr(path.size()-4)==".DAE"){
            if(scenes.count(path))continue;std::string base=path.substr(0,path.size()-4);Scene scene;scene.data=assetJson(assets,base+".json");
            auto& d=scene.data;scene.world.resize(d["nodes"].size());scene.channels.assign(d["nodes"].size(),-1);
            for(size_t i=0;i<d["nodes"].size();i++){auto& n=d["nodes"][i];Matrix local{};for(int row=0;row<4;row++)for(int col=0;col<4;col++)local[row*4+col]=n["matrix"][col*4+row].get<float>();scene.local.push_back(local);scene.names[n["name"].get<std::string>()]=(int)i;}
            if(!d["animations"].empty())for(size_t i=0;i<d["animations"][0]["channels"].size();i++){std::string name=d["animations"][0]["channels"][i]["name"];auto found=scene.names.find(name);if(found!=scene.names.end())scene.channels[found->second]=(int)i;}
            for(auto& material:d["materials"]){std::array<float,4> c{1,1,1,1};if(material.contains("$clr.diffuse"))for(size_t i=0;i<material["$clr.diffuse"].size()&&i<4;i++)c[i]=material["$clr.diffuse"][i].get<float>();if(std::abs(c[0]-c[1])>.01||std::abs(c[1]-c[2])>.01)c={1,0,0,1};scene.colors.push_back(c);}
            for(size_t i=0;i<d["meshes"].size();i++){
                Json mesh=d["meshes"][i];std::string file=base+".mesh";
                for(auto& entry:lod)if(entry["scene"]==path&&entry["mesh"]==i){auto level=entry["levels"][q.id];for(const char* field:{"offset","indexOffset","vertices","indices"})mesh[field]=level[field];file="quality/"+level["file"].get<std::string>();break;}
                if(!buffers.count(file))buffers[file]=asset(assets,file);auto& bytes=buffers[file];size_t vo=mesh["offset"],io=mesh["indexOffset"];int vc=mesh["vertices"],ic=mesh["indices"];need(vo+(size_t)vc*24<=bytes.size()&&io+(size_t)ic*4<=bytes.size(),"Invalid mesh range");scene.meshes.push_back({gpu.mesh(bytes.data()+vo,vc,bytes.data()+io,ic),ic,mesh["material"]});loadedTriangles+=ic/3;
            }
            scenes.emplace(path,std::move(scene));
        }
    }
    AAssetDir* dir=AAssetManager_openDir(assets,"");while(const char* f=AAssetDir_getNextFileName(dir)){std::string file=f;if(file.ends_with(".png")||file.ends_with(".jpg")){auto bytes=asset(assets,file);int w,h,n;stbi_set_flip_vertically_on_load(gpu.version.find("Vulkan")==std::string::npos);uint8_t* pixels=stbi_load_from_memory(bytes.data(),(int)bytes.size(),&w,&h,&n,4);need(pixels!=nullptr,"Image decode failed: "+file);try{images[file]=gpu.texture(w,h,pixels,false);}catch(...){stbi_image_free(pixels);throw;}stbi_image_free(pixels);}}AAssetDir_close(dir);rootId=target();
}
int SceneRenderer::target(){if(!freeTargets.empty()){int id=freeTargets.back();freeTargets.pop_back();return id;}return gpu.texture(preset.width,preset.height,nullptr,true);}
double SceneRenderer::value(const Json& fx,const char* name,double fallback)const{
    const auto& params=fx["params"];if(!params.contains(name))return fallback;const auto& p=params[name];if(p.contains("value"))return p["value"].get<double>();
    for(auto& c:p["clips"]){double start=c["start"],length=c["length"];if(time<start||time>=start+length)continue;double t=time-start;auto& points=c["points"];need(!points.empty(),"Empty animation curve");if(t<=points[0]["time"].get<double>())return points[0]["value"].get<double>();size_t i=0;while(i+1<points.size()&&points[i+1]["time"].get<double>()<=t)i++;if(i+1==points.size())return points[i]["value"].get<double>();auto& a=points[i];auto& b=points[i+1];double f=(t-a["time"].get<double>())/(b["time"].get<double>()-a["time"].get<double>());std::string mode=a["mode"];if(mode=="hold")f=0;else if(mode=="smooth")f=(1-std::cos(f*3.1415))*.5;else if(mode=="fast")f=std::sqrt(f);else if(mode=="slow")f=f*f;return a["value"].get<double>()+(b["value"].get<double>()-a["value"].get<double>())*f;}return 0;
}
int SceneRenderer::input(const Json& n,const std::string& name)const{for(auto& c:n["inputs"])if(c["input"]==name)return c["source"].get<int>();return 0;}
std::string SceneRenderer::resource(const Json& n,const std::string& name)const{int id=input(n,name);if(!id)return "";auto& r=timeline["nodes"][std::to_string(id)]["resource"];return timeline["resources"][std::to_string(r.get<int>())];}
bool SceneRenderer::active(int id){
    if(!id)return false;if(activity.count(id))return activity[id];auto& n=timeline["nodes"][std::to_string(id)];std::string type=n["type"];bool yes=false;
    if(type=="resource")yes=true;else if(type=="rendertarget"||type=="output"){for(auto& c:n["inputs"])yes|=active(c["source"]);}else{auto& fx=timeline["effects"][std::to_string(n["effect"].get<int>())];if(fx["type"]=="layer")yes=active(input(n,"texInput"));else{bool animated=false;for(auto& p:fx["params"].items())if(p.value().contains("clips")){animated=true;for(auto& c:p.value()["clips"])if(time>=c["start"].get<double>()&&time<c["start"].get<double>()+c["length"].get<double>())yes=true;}if(!animated)yes=true;}}activity[id]=yes;return yes;
}
int SceneRenderer::image(int id){if(cached.count(id))return cached[id];auto& n=timeline["nodes"][std::to_string(id)];if(n["type"]=="resource")return images.at(timeline["resources"][std::to_string(n["resource"].get<int>())]);if(!targets.count(id))targets[id]=target();int t=targets[id];float black[4]={};gpu.bind(t,3,black);node(id,t);cached[id]=t;return t;}
void SceneRenderer::evaluate(Scene& s,double position,int camera,Matrix& vp){
    auto& d=s.data;double t=d["animations"].empty()?0:position*d["animations"][0]["duration"].get<double>();
    for(size_t i=0;i<s.world.size();i++){Matrix local=s.local[i];if(s.channels[i]>=0){auto& c=d["animations"][0]["channels"][s.channels[i]];local=trs(key(c["positions"],t,false),key(c["rotations"],t,true),key(c["scales"],t,false));}int parent=d["nodes"][i]["parent"];s.world[i]=parent<0?local:multiply(s.world[parent],local);}
    auto& c=d["cameras"][std::clamp(camera,0,(int)d["cameras"].size()-1)];auto& m=s.world[s.names.at(c["name"].get<std::string>())];float unit=std::sqrt(m[0]*m[0]+m[1]*m[1]+m[2]*m[2]),f=1.f/std::tan(c["fov"].get<float>()*.5f),near=c["near"].get<float>()*unit,far=c["far"].get<float>()*unit;Matrix projection={-f,0,0,0,0,f*(float)preset.width/preset.height,0,0,0,0,(far+near)/(near-far),-1,0,0,2*far*near/(near-far),0};Vec eye=c["position"].get<Vec>(),look=c["look"].get<Vec>(),up=c["up"].get<Vec>();for(float& x:look)x=-x;vp=multiply(projection,view(transform(m,eye,1),transform(m,look,0),transform(m,up,0)));
}
void SceneRenderer::node(int id,int destination){
    if(!active(id))return;auto& n=timeline["nodes"][std::to_string(id)];std::string type=n["type"];if(type=="rendertarget"||type=="output"){for(auto& c:n["inputs"])node(c["source"],destination);return;}if(type!="effect")return;auto& fx=timeline["effects"][std::to_string(n["effect"].get<int>())];
    if(fx["type"]=="assimprenderer"){
        auto& s=scenes.at(resource(n,"Assimp Scene"));Matrix vp;evaluate(s,value(fx,"time",0),(int)value(fx,"camera",0),vp);bool clear=resource(n,"DX Shader")=="ShaderClear.fx";float color[4]={1,1,1,1};gpu.bind(destination,clear?3:2,color);
        for(size_t i=0;i<s.world.size();i++){auto& meshes=s.data["nodes"][i]["meshes"];if(meshes.empty())continue;Matrix matrix=multiply(vp,s.world[i]);for(auto& index:meshes){auto& mesh=s.meshes[index.get<int>()];Params p;p.matrix=matrix;p.color=s.colors[mesh.material];gpu.draw(mesh.id,p,whiteId,whiteId);triangles+=mesh.count/3;drawCalls++;}}
    }else if(fx["type"]=="layer"){
        float alpha=(float)value(fx,"fAlpha",1);if(alpha<=0)return;int tex=image(input(n,"texInput"));float black[4]={};gpu.bind(destination,0,black);Params p;p.view[3]=1;p.rect={(float)value(fx,"fScaleX",1),(float)value(fx,"fScaleY",1),(float)value(fx,"fPosX",0),(float)value(fx,"fPosY",0)};p.crop={(float)value(fx,"fOffsetX",0),-(float)value(fx,"fOffsetY",0),(float)value(fx,"fZoom",1),0};p.settings={alpha,resource(n,"DX Shader")=="NoiseCrunch.fx"?1.f:0.f,(float)value(fx,"fWidth",0),(float)value(fx,"fTime",0)};gpu.draw(1,p,tex,images.at("noise-pixelate.png"));triangles+=2;drawCalls++;
    }
}
void SceneRenderer::render(double seconds){time=(seconds+.700)*1000;triangles=drawCalls=0;activity.clear();cached.clear();for(auto i=targets.begin();i!=targets.end();)if(!active(i->first)){freeTargets.push_back(i->second);i=targets.erase(i);}else++i;float black[4]={0,0,0,1};gpu.bind(rootId,3,black);node(10,rootId);for(auto i=targets.begin();i!=targets.end();)if(!cached.count(i->first)){freeTargets.push_back(i->second);i=targets.erase(i);}else++i;}
void SceneRenderer::renderSource(const uint8_t* pixels,int w,int h,uint32_t serial){if(srSerial!=serial){gpu.update(srId,w,h,pixels);srSerial=serial;}float black[4]={0,0,0,1};gpu.bind(rootId,3,black);Params p;p.view[3]=preset.id?3:2;gpu.draw(1,p,srId,whiteId);triangles=2;drawCalls=1;}
