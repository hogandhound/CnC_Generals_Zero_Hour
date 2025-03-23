#pragma once
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

typedef void (*clbk)(void* userdata, uint8_t* data, size_t len, float sampleRate, int channels);
struct OINK
{
    AVPacket* pkt;
    const AVCodec* vcodec, *acodec;
    AVFormatContext* fmt_ctx;
    AVCodecContext* vc, *ac;
    AVFrame* vframe, * aframe;
    int video_stream_index, audio_stream_index;
    bool nextLoaded;
//    FILE* file;
//#define OINK_INBUF_SIZE 4096
//    uint8_t inbuf[OINK_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
//    uint32_t dataOffset;
    //BINK
    int64_t FrameNum, Frames;
    uint32_t Width, Height;
};

#define OINKPRELOADALL 1
#define BINKSURFACE32 1
#define BINKSURFACE24 2
#define BINKSURFACE565 3
#define BINKSURFACE555 4
OINK* __stdcall BinkOpen(const char* name, unsigned int flags);

void __stdcall BinkSetSoundTrack(unsigned int track);

void BinkSetAudioCallback(void* userdata, clbk callback);

void __stdcall BinkClose(OINK* handle);

int __stdcall BinkWait(OINK* handle);

int __stdcall BinkDoFrame(OINK* handle);

int __stdcall BinkCopyToBuffer(
    OINK* handle, void* dest, int destpitch, unsigned int destheight, unsigned int destx, unsigned int desty, unsigned int flags);

void __stdcall BinkSetVolume(OINK* handle, int volume);

void __stdcall BinkNextFrame(OINK* handle);

void __stdcall BinkGoto(OINK* handle, unsigned int frame, int flags);