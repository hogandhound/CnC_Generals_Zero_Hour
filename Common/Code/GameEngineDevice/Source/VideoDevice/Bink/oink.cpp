/**
 * @file
 *
 * @author OmniBlade
 *
 * @brief Stub library containing subset of functions from binkw32.dll as used by the SAGE engine.
 *
 * @copyright Thyme is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            3 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#define BINKDLL
#include <stddef.h>
#include <VideoDevice/Bink/oink.h>
extern "C" {
#include "libavdevice/avdevice.h"
#include <libswscale/swscale.h>
}
 /*
  * All of these function definitions are empty, they are just hear to generate a dummy dll that
  * exports the same symbols as the real binkw32.dll so we can link against it without the commercial
  * SDK.
  */

OINK* __stdcall BinkOpen(const char* name, unsigned int flags)
{
    OINK* ret = new OINK;

    //avdevice_register_all();
    
    ret->fmt_ctx = 0;
    if (avformat_open_input(&ret->fmt_ctx, name, NULL, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        delete ret;
        ret = 0;
        return ret;
    }


    if (avformat_find_stream_info(ret->fmt_ctx, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        delete ret;
        ret = 0;
        return ret;
    }

    /* select the video stream */
    const AVCodec* dec;
    ret->video_stream_index = -1;
    int hr = av_find_best_stream(ret->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    if (hr < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
        delete ret;
        ret = 0;
        return ret;
    }
    ret->video_stream_index = hr;
    ret->vcodec = dec;

    ret->vc = avcodec_alloc_context3(ret->vcodec);
    avcodec_parameters_to_context(ret->vc, ret->fmt_ctx->streams[ret->video_stream_index]->codecpar);
    avcodec_open2(ret->vc, ret->vcodec, NULL);
    ret->vframe = av_frame_alloc();

    ret->audio_stream_index = -1;
    hr = av_find_best_stream(ret->fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &dec, 0);
    if (hr < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
        delete ret;
        ret = 0;
        return ret;
    }
    ret->audio_stream_index = hr;
    ret->acodec = dec;

    ret->ac = avcodec_alloc_context3(ret->acodec);
    avcodec_parameters_to_context(ret->ac, ret->fmt_ctx->streams[ret->audio_stream_index]->codecpar);
    avcodec_open2(ret->ac, ret->acodec, NULL);
    ret->aframe = av_frame_alloc();

    ret->pkt = av_packet_alloc();

    ret->Width = ret->vc->width;
    ret->Height = ret->vc->height;
    ret->FrameNum = 0;
    for (uint32_t i = 0; i < ret->fmt_ctx->nb_streams; ++i)
    {
        if (ret->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            ret->Frames = ret->fmt_ctx->streams[i]->duration;
            break;
        }
    }
    ret->nextLoaded = false;
    //ret->file = fopen(name, "rb");

    //memset(ret->inbuf + OINK_INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    return ret;
}

void __stdcall BinkSetSoundTrack(unsigned int track)
{

}

void __stdcall BinkClose(OINK* handle)
{
    av_packet_free(&handle->pkt);
    avformat_close_input(&handle->fmt_ctx);
    avcodec_free_context(&handle->vc);
    av_frame_free(&handle->vframe);
    avcodec_free_context(&handle->ac);
    av_frame_free(&handle->aframe);
    delete handle;
}

int __stdcall BinkWait(OINK* handle)
{
    return 0;
}
static clbk audio_callback = 0;
static void* audio_data = 0;


static int decode(AVCodecContext* dec_ctx, AVFrame* frame, AVPacket* pkt, bool audio = false)
{
    int ret;
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        ret = 0;
        printf("BadCode\n");
        //fprintf(stderr, "Error sending a packet for decoding\n");
        //exit(1);
    }
    static float* audioMem = (float*)calloc(1024 * 1024 * 4, sizeof(float));
    size_t audioOffset = 0;

    float sampleRate = 44100.f;
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            if (audio)
            {
                audio_callback(audio_data, (uint8_t*)audioMem, audioOffset,// frame->sample_rate,
                    //(float)audioOffset, 
                    sampleRate,//frame->sample_rate, 
                    1// frame->ch_layout.nb_channels
                );
                //free(audioMem);
            }
            return 0;
        }
        if (ret == 0 && !audio)
            return 1;
        if (ret >= 0)
        {
            if (audio)
            {
                sampleRate = (float)frame->sample_rate;
                memcpy(audioMem + audioOffset, frame->buf[0]->data, frame->buf[0]->size);
                for (int i = 0; i < frame->buf[0]->size / 4; ++i)
                {
                    //audioMem[audioOffset + i] += 1;
                    //audioMem[audioOffset + i] *= 10.f;
                    //audioMem[audioOffset + i] *= 0.001f;
                }
                audioOffset += frame->nb_samples;
                /*av_frame_unref(frame);
                ret = avcodec_send_packet(dec_ctx, pkt);
                ret = avcodec_receive_frame(dec_ctx, frame);
                if (ret == 0 || ret == AVERROR_EOF)
                {
                }
                else*/
                    continue;
            }
            return 1;
        }
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            exit(1);
        }
    }
    return 0;
}

void BinkSetAudioCallback(void* userdata, clbk callback) { audio_callback = callback; audio_data = userdata; };

int __stdcall BinkDoFrame(OINK* handle)
{
    int ret = 1;
#if 1
    if (handle->nextLoaded)
        return true;
    bool audio = false, video = false;
    while (1)
    {
        if ((ret = av_read_frame(handle->fmt_ctx, handle->pkt)) < 0)
            break;

        if (handle->pkt->stream_index == handle->video_stream_index) {
            if (!decode(handle->vc, handle->vframe, handle->pkt))
                handle->FrameNum = handle->Frames;
            video = true;
        }
        if (handle->pkt->stream_index == handle->audio_stream_index) {
            if (decode(handle->ac, handle->aframe, handle->pkt,true))
            {
                ////AV_SAMPLE_FMT_FLTP;
                //audio_callback(audio_data, handle->aframe->extended_data[0], handle->aframe->nb_samples,
                //    (float)handle->aframe->sample_rate, handle->aframe->ch_layout.nb_channels);
                //audio_callback(audio_data, handle->aframe->buf[1]->data, handle->aframe->buf[1]->size,
                //    (float)handle->aframe->sample_rate, handle->aframe->ch_layout.nb_channels);
            }
            audio = true;
        }
        if (audio && video)
            break;
    }
    handle->nextLoaded = true;
#else
    handle->FrameNum = handle->Frames;
#endif
    return 1;
}
int __stdcall BinkCopyToBuffer(
    OINK* handle, void* dest, int destpitch, unsigned int destheight, unsigned int destx, unsigned int desty, unsigned int flags)
{
#if 1
    int outputStride = handle->Width * 4;
    switch (flags)
    {
    default:
    case BINKSURFACE32:
        break;
    case BINKSURFACE24:
        outputStride = handle->Width * 3;
        break;
    case BINKSURFACE565:
    case BINKSURFACE555:
        outputStride = handle->Width * 2;
        break;
    }
    if (destpitch >= outputStride && destheight == handle->vframe->height)
    {
        uint8_t* data = handle->vframe->data[0];
        bool freeData = false;
        if (handle->vc->pix_fmt == AV_PIX_FMT_YUV420P)
        {
            uint8_t* rgbOut[3];
            int rgbOutStride[3];
            rgbOutStride[0] = outputStride;
            rgbOut[0] = (uint8_t*)malloc((size_t)rgbOutStride[0] * handle->Height);
            struct SwsContext* i420ToRgbContext;

            AVPixelFormat outputType = AV_PIX_FMT_BGRA;
            switch (flags)
            {
            default:
            case BINKSURFACE32:
                break;
            case BINKSURFACE24: outputType = AV_PIX_FMT_RGB24; break;
            case BINKSURFACE565: outputType = AV_PIX_FMT_RGB565LE; break;
            case BINKSURFACE555: outputType = AV_PIX_FMT_RGB555LE; break;
            }
            i420ToRgbContext = sws_getContext(handle->Width, handle->Height, AV_PIX_FMT_YUV420P, handle->Width, handle->Height, outputType, SWS_BICUBIC, NULL, NULL, NULL);
            if (i420ToRgbContext == NULL) {
                fprintf(stderr, "Failed to allocate I420 to RGB conversion context.\n");
            }
            int toRgbRes = sws_scale(i420ToRgbContext, handle->vframe->data, handle->vframe->linesize, 0, handle->Height, rgbOut, rgbOutStride);
            data = rgbOut[0];
            freeData = true;
        }
        for (uint32_t i = 0; i < destheight; ++i)
        {
            memcpy(((char*)dest) + destpitch * i, data + i * outputStride,
                outputStride);
        }
        if (freeData)
            free(data);
        //memcpy(dest, handle->frame->data[0], handle->frame->linesize[0] * handle->frame->height);
    }
    else
    {
        return 0;
    }
#endif
    return 1;
}

void __stdcall BinkSetVolume(OINK* handle, int volume)
{

}

void __stdcall BinkNextFrame(OINK* handle)
{
    handle->FrameNum++;
    handle->nextLoaded = false;
    BinkDoFrame(handle);
}

void __stdcall BinkGoto(OINK* handle, unsigned int frame, int flags)
{

}