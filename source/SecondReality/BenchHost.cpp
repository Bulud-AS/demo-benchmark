#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include "BenchHost.h"
#include "Graphics/Graphics.h"

// Shared-memory protocol v1. The host owns the map and mutex for one run.
// 0 magic; 4 protocol; 8 state; 12 stop; 16 w; 20 h; 24 part;
// 28 simulation ticks; 32 elapsed ms; 36 source frames; 40 order; 44 row;
// 48 source offset; 52 error; 56 natural end; pixels BGRA8 at 256.
namespace {
HANDLE mapping=nullptr,gate=nullptr,parent=nullptr;
unsigned char* memory=nullptr;
int startPart=0,modeW=320,modeH=200,currentPart=0,ticks=0,frames=0;
bool muted=false,finished=false;
double diagnosticLimit=0;
using Clock=std::chrono::steady_clock;
Clock::time_point epoch,nextTick;
int& field(int offset) { return *reinterpret_cast<int*>(memory+offset); }
double seconds() { return std::chrono::duration<double>(Clock::now()-epoch).count(); }
bool stopped() {
    return finished || field(12)!=0 || WaitForSingleObject(parent,0)==WAIT_OBJECT_0 || (diagnosticLimit>0 && seconds()>=diagnosticLimit);
}
}
bool bench_init(int argc,char** argv) {
    if(argc<4) return false;
    mapping=OpenFileMappingA(FILE_MAP_ALL_ACCESS,FALSE,argv[1]);
    gate=OpenMutexA(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,argv[2]);
    parent=OpenProcess(SYNCHRONIZE,FALSE,static_cast<DWORD>(atoi(argv[3])));
    if(!mapping||!gate||!parent) return false;
    memory=static_cast<unsigned char*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,256+640*400*4));
    if(!memory || field(0)!=0x31425253 || field(4)!=1) return false;
    if(argc>4) startPart=atoi(argv[4]);
    if(startPart<0||startPart>18) return false;
    muted=argc>5&&atoi(argv[5])!=0;
    if(argc>6) diagnosticLimit=atof(argv[6]);
    epoch=Clock::now();nextTick=epoch;
    timeBeginPeriod(1);field(8)=1;
    return true;
}
int bench_start_part(){return startPart;}
bool bench_muted(){return muted;}
void bench_part(int part){currentPart=part;}
void bench_finished(){finished=true;field(56)=1;}
int bench_vsync(){
    nextTick+=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0/70.0));
    while(Clock::now()<nextTick&&!stopped()) {
        if(std::chrono::duration<double>(nextTick-Clock::now()).count()>.002) Sleep(1);
        else SwitchToThread();
    }
    // Retain the original one-step semantics after long stalls; never speed up the music.
    if(std::chrono::duration<double>(Clock::now()-nextTick).count()>.1) nextTick=Clock::now();
    ticks++;
    return 1;
}
bool Graphics::Init(WindowType){return true;}
bool Graphics::WantsToQuit(){return stopped();}
void Graphics::ChangeMode(int w,int h,float){
    if(w<=0||w>640||h<=0||h>400) throw std::runtime_error("Invalid Second Reality video mode");
    modeW=w;modeH=h;Common::reset();
}
void Graphics::Update(){
    if(stopped())return;
    if(WaitForSingleObject(gate,2000)!=WAIT_OBJECT_0) throw std::runtime_error("Second Reality frame lock timeout");
    field(16)=modeW;field(20)=modeH;field(24)=currentPart;
    field(28)=ticks;field(32)=static_cast<int>(seconds()*1000);field(36)=++frames;
    field(40)=Music::getOrder();field(44)=Music::getRow();field(48)=Shim::startpixel;
    uint32_t* pixels=reinterpret_cast<uint32_t*>(memory+256);
    for(int i=0;i<modeW*modeH;i++) {
        size_t offset=static_cast<size_t>(Shim::startpixel)+i;
        uint32_t c=offset<Shim::VRAM_X*Shim::VRAM_Y?Shim::palette[Shim::vram[offset]]:0;
        pixels[i]=((c>>16)&255)|(c&0xff00)|((c&255)<<16)|0xff000000u;
    }
    ReleaseMutex(gate);
}
void Graphics::Close(){}
void bench_close(const char* error){
    if(memory){
        field(32)=static_cast<int>(seconds()*1000);
        if(error){field(52)=1;strncpy(reinterpret_cast<char*>(memory+64),error,191);field(8)=3;}
        else field(8)=finished?2:4;
        UnmapViewOfFile(memory);memory=nullptr;
    }
    timeEndPeriod(1);
    if(parent)CloseHandle(parent);if(gate)CloseHandle(gate);if(mapping)CloseHandle(mapping);
}
