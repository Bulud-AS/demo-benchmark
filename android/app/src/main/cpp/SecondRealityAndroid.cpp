#include "Core.h"
#include "Graphics/Graphics.h"
#include "Music/audioPlayer.h"
#include "Music/st3play.h"
#include <algorithm>
#include <cstring>
#include <thread>

std::recursive_mutex srAudioMutex;
#define PART(name) namespace name {void main();}
PART(Alku) PART(U2A) PART(OUTTA) PART(Beg) PART(Glenz) PART(Tunneli) PART(KOE) PART(Shutdown) PART(Forest) PART(Lens) PART(PLZ) PART(Dots) PART(Water) PART(Coman) PART(JPLogo) PART(U2E) PART(End) PART(Credits) PART(EndScrl)
namespace {
SharedSR* state=nullptr;bool mute=false,audioStarted=false;int videoW=320,videoH=400;double nextTick=0;std::unique_ptr<Audio> audio;
bool stopped(){return !state||state->stop.load()||state->state.load()==2;}
struct PartInfo {Music::Song song;int order,w,h;void(*run)();};
const PartInfo parts[]={
    {Music::Song::Skaven,0x00,320,400,Alku::main},{Music::Song::Skaven,0x0C,320,200,U2A::main},{Music::Song::Skaven,0x0D,320,200,OUTTA::main},{Music::Song::Skaven,0x0E,320,400,Beg::main},
    {Music::Song::PurpleMotion,0x00,320,400,Glenz::main},{Music::Song::PurpleMotion,0x0F,320,200,Tunneli::main},{Music::Song::PurpleMotion,0x14,320,200,KOE::main},{Music::Song::PurpleMotion,0x27,320,400,Shutdown::main},
    {Music::Song::PurpleMotion,0x2A,320,200,Forest::main},{Music::Song::PurpleMotion,0x2F,320,200,Lens::main},{Music::Song::PurpleMotion,0x3E,320,200,PLZ::main},{Music::Song::PurpleMotion,0x4D,320,200,Dots::main},
    {Music::Song::PurpleMotion,0x58,320,200,Water::main},{Music::Song::PurpleMotion,0x5E,320,200,Coman::main},{Music::Song::PurpleMotion,0x62,320,400,JPLogo::main},{Music::Song::Skaven,0x12,320,400,U2E::main},
    {Music::Song::Skaven,0x19,320,400,End::main},{Music::Song::Skaven,0x1C,320,400,Credits::main},{Music::Song::Skaven,0x2B,640,350,EndScrl::main}
};
}
namespace AudioPlayer {
bool openMixer(uint32_t rate,int32_t& samplesLeft,uint32_t& totalSampleCount){try{closeMixer();samplesLeft=0;totalSampleCount=0;audio=std::make_unique<Audio>();audio->open(rate,[](int16_t* out,int count){for(int offset=0;offset<count;){int n=std::min(count-offset,MIX_BUF_SAMPLES);st3play::FillAudioBuffer(out+offset*2,n);offset+=n;}},mute);audioStarted=false;return true;}catch(...){return false;}}
void closeMixer(){if(audio){audio->close();audio.reset();}audioStarted=false;}
void Update(bool yield){if(audio&&!audioStarted){audio->start();audioStarted=true;}if(yield)std::this_thread::yield();}
uint32_t getSamplePosition(){return audio?(uint32_t)(audio->position()*44100):0;}
}
bool Graphics::Init(WindowType){return true;}
bool Graphics::WantsToQuit(){return stopped();}
void Graphics::Close(){}
void Graphics::ChangeMode(int w,int h,float){need(w>0&&w<=640&&h>0&&h<=400,"Invalid source size");videoW=w;videoH=h;Common::reset();}
void Graphics::Update(){
    if(stopped()||state->gate.test_and_set(std::memory_order_acquire))return;
    state->width=videoW;state->height=videoH;state->order=Music::getOrder();state->row=Music::getRow();
    for(int i=0;i<videoW*videoH;i++){size_t offset=(size_t)Shim::startpixel+i;uint32_t c=offset<Shim::VRAM_X*Shim::VRAM_Y?Shim::palette[Shim::vram[offset]]:0;uint8_t* p=state->pixels+i*4;p[0]=(c>>16)&255;p[1]=(c>>8)&255;p[2]=c&255;p[3]=255;}
    state->frames.fetch_add(1);state->gate.clear(std::memory_order_release);
}
void demo_changemode(int w,int h,float jsss){Graphics g;g.ChangeMode(w,h,jsss);}
void demo_blit(){Graphics g;g.Update();}
bool demo_wantstoquit(){return stopped();}
int demo_vsync(bool updateAudio){
    if(updateAudio)AudioPlayer::Update(true);nextTick+=1./70.;while(mono()<nextTick&&!stopped())std::this_thread::sleep_for(std::chrono::microseconds(300));if(mono()-nextTick>.1)nextTick=mono();if(state)state->ticks.fetch_add(1);return 1;
}
void bench_finished(){if(state)state->state=2;}
void runSecondReality(SharedSR* shared,bool muted,int first){
    state=shared;mute=muted;need(state->magic==0x32425253&&state->protocol==1,"Invalid Second Reality protocol");state->ready=1;
    while(!state->start.load()&&!state->stop.load())std::this_thread::sleep_for(std::chrono::milliseconds(1));if(state->stop)return;
    state->epochUs=(int64_t)(mono()*1e6);nextTick=mono();state->state=1;
    try{
        Music::Song previous=Music::Song::COUNT;
        for(int index=std::clamp(first,0,18);index<19&&!stopped();index++){
            const auto& p=parts[index];if(previous!=p.song){previous=p.song;Music::start(p.song,p.order);}state->part=index;demo_changemode(p.w,p.h,Default_JSSS);p.run();if(stopped())break;Shim::finishedDemoFirstPart();
            if(index==7){while(!stopped()&&Music::getPlusFlags()>=0){demo_vsync(false);demo_blit();}while(!stopped()&&Music::getPlusFlags()<0){demo_vsync(false);demo_blit();}}
        }
        if(!state->stop&&state->state!=2)state->state=2;
    }catch(const std::exception& e){strncpy(state->error,e.what(),255);state->error[255]=0;state->state.store(3,std::memory_order_release);}
    Music::end();
}
