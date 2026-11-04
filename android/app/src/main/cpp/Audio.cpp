#include "Core.h"
#include <oboe/Oboe.h>
#include <algorithm>
#include <cstring>
#include <time.h>

struct Audio::Impl:oboe::AudioStreamDataCallback,oboe::AudioStreamErrorCallback {
    std::shared_ptr<oboe::AudioStream> stream;std::function<void(int16_t*,int)> fill;
    bool muted=false;std::atomic<bool> started{false},failed{false},timestamps{false};double epoch=0;int rate=44100;
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream*,void* data,int32_t count)override{
        fill((int16_t*)data,count);if(muted)memset(data,0,count*4);return oboe::DataCallbackResult::Continue;
    }
    void onErrorAfterClose(oboe::AudioStream*,oboe::Result)override{failed=true;}
};
Audio::Audio():impl(std::make_unique<Impl>()){}
Audio::~Audio(){close();}
void Audio::open(int rate,std::function<void(int16_t*,int)> callback,bool muted){
    close();impl->rate=rate;impl->fill=std::move(callback);impl->muted=muted;impl->failed=false;impl->timestamps=false;
    oboe::AudioStreamBuilder builder;builder.setDirection(oboe::Direction::Output)->setChannelCount(2)->setFormat(oboe::AudioFormat::I16)->setSampleRate(rate)->setPerformanceMode(oboe::PerformanceMode::LowLatency)->setSharingMode(oboe::SharingMode::Shared)->setUsage(oboe::Usage::Game)->setDataCallback(impl.get())->setErrorCallback(impl.get());
    auto result=builder.openStream(impl->stream);need(result==oboe::Result::OK,"音声出力を開けません: "+std::string(oboe::convertToText(result)));need(impl->stream->getFormat()==oboe::AudioFormat::I16&&impl->stream->getChannelCount()==2&&impl->stream->getSampleRate()==rate,"音声出力形式が一致しません");
}
void Audio::start(){if(impl->started)return;need(impl->stream!=nullptr,"音声の準備ができていません");impl->epoch=mono();auto result=impl->stream->requestStart();need(result==oboe::Result::OK,"音声を開始できません");impl->started=true;}
void Audio::close(){if(impl->stream){impl->stream->requestStop();impl->stream->close();impl->stream.reset();}impl->started=false;}
double Audio::position()const{
    need(!impl->failed,"音声出力が切断されました");if(!impl->started)return 0;int64_t frames=0,nanos=0;
    if(impl->stream&&impl->stream->getTimestamp(CLOCK_MONOTONIC,&frames,&nanos)==oboe::Result::OK){impl->timestamps=true;timespec now{};clock_gettime(CLOCK_MONOTONIC,&now);double current=now.tv_sec+now.tv_nsec/1e9,stamp=nanos/1e9;return std::max(0.,frames/(double)impl->rate+current-stamp);}
    return std::max(0.,mono()-impl->epoch);
}
int Audio::underruns()const{if(!impl->stream)return 0;auto count=impl->stream->getXRunCount();return count?count.value():0;}
bool Audio::timestampClock()const{return impl->timestamps;}
