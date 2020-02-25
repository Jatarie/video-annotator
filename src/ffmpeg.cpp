#include <queue>
extern "C" {
#include "avcodec.h"
#include "avformat.h"
};

AVStream* video_stream;
AVCodec* video_codec;
AVCodecParameters* video_codec_params;
AVCodecContext* video_codec_ctx;
// AVFrame* video_frame = av_frame_alloc();
// AVPacket* video_packet = av_packet_alloc();
uint32_t* rgb_frame;

AVStream* audio_stream;
AVCodec* audio_codec;
AVCodecContext* audio_codec_ctx;
AVCodecParameters* audio_codec_params;
// AVFrame* audio_frame = av_frame_alloc();
// AVPacket* audio_packet = av_packet_alloc();
float audio_buffer[2048];
std::queue<float> q;
// #define samples_per_frame 2940
#define samples_per_frame 1470
float next_frame[samples_per_frame];

AVFormatContext* video_format_ctx;
AVFormatContext* audio_format_ctx;

struct RGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

inline int clamp(int x, int min = 0, int max = 255) {
    if (x > max){
        return max;
    }
    if(x < min){
        return min;
    }
    return x;
}

void YUVtoRGB(int y, int u, int v, RGB* rgb) {
    int c = y - 16;
    int d = u - 128;
    int e = v - 128;

    rgb->r = clamp((298 * c + 409 * e + 128) >> 8);
    rgb->g = clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
    rgb->b = clamp((298 * c + 516 * d + 128) >> 8);
}

void initffmpeg(char* filename) {
    video_format_ctx = avformat_alloc_context();
    avformat_open_input(&video_format_ctx, filename, 0, 0);
    video_stream = video_format_ctx->streams[0];
    video_codec_params = video_format_ctx->streams[0]->codecpar;
    video_codec = avcodec_find_decoder(video_codec_params->codec_id);
    video_codec_ctx = avcodec_alloc_context3(video_codec);
    avcodec_parameters_to_context(video_codec_ctx, video_codec_params);
    avcodec_open2(video_codec_ctx, video_codec, 0);
    rgb_frame = (uint32_t*)malloc(sizeof(uint32_t) * video_codec_ctx->height * video_codec_ctx->width);

    audio_format_ctx = avformat_alloc_context();
    avformat_open_input(&audio_format_ctx, filename, 0, 0);
    audio_stream = audio_format_ctx->streams[1];
    audio_codec_params = audio_format_ctx->streams[1]->codecpar;
    audio_codec = avcodec_find_decoder(audio_codec_params->codec_id);
    audio_codec_ctx = avcodec_alloc_context3(audio_codec);
    avcodec_parameters_to_context(audio_codec_ctx, audio_codec_params);
    avcodec_open2(audio_codec_ctx, audio_codec, 0);
}

AVFrame* frame = av_frame_alloc();
void get_audio_frame() {
    int counter = 0;
    AVPacket* packet = av_packet_alloc();
    while (av_read_frame(audio_format_ctx, packet) >= 0) {
        if (packet->stream_index == 1) {
            avcodec_send_packet(audio_codec_ctx, packet);
            avcodec_receive_frame(audio_codec_ctx, frame);

            float* c1 = (float*)frame->data[0];
            float* c2 = (float*)frame->data[1];

            for (int i = 0; i < frame->nb_samples; i++) {
                float a = c1[i];
                audio_buffer[2 * i] = a;
                audio_buffer[2 * i + 1] = a;
            }
            av_packet_unref(packet);
            av_packet_free(&packet);

            return;
        }
        else {
            av_packet_unref(packet);
            av_packet_free(&packet);
            packet = av_packet_alloc();
        }
    }
}

void process_video_frame(AVFrame* video_frame) {
    int num_pixels = video_frame->height * video_frame->width;
    int boxes_per_line = video_frame->width / 2;
    RGB rgb;
    for (int y = 0; y < video_frame->height; y++) {
        for (int x = 0; x < video_frame->width; x++) {
            int box_x = x / 2;
            int box_y = y / 2;
            int cur_box = box_x + boxes_per_line * box_y;
            int _y = video_frame->data[0][y * video_frame->width + x];
            int _u = video_frame->data[1][cur_box];
            int _v = video_frame->data[2][cur_box];
            YUVtoRGB(_y, _u, _v, &rgb);
            // uint8_t* subpixel = (uint8_t*)&rgb_frame[(video_frame->height - y - 1) * video_frame->width + x];
            uint8_t* subpixel = (uint8_t*)&rgb_frame[y * video_frame->width + x];
            *subpixel++ = rgb.r;
            *subpixel++ = rgb.g;
            *subpixel = rgb.b;
        }
    }
}

AVFrame* video_frame = av_frame_alloc();
void get_video_frame(int process) {
    AVPacket* video_packet = av_packet_alloc();
    while (av_read_frame(video_format_ctx, video_packet) >= 0) {
        if (video_packet->stream_index == 0) {
            avcodec_send_packet(video_codec_ctx, video_packet);
            avcodec_receive_frame(video_codec_ctx, video_frame);
            if (process) {
                process_video_frame(video_frame);
            }
            av_packet_unref(video_packet);
            av_packet_free(&video_packet);
            return;
        }
        else {
            av_packet_unref(video_packet);
            av_packet_free(&video_packet);
            video_packet = av_packet_alloc();
        }
    }
}

void fill_queue() {
    while (q.size() < samples_per_frame) {
        get_audio_frame();
        for (int i = 0; i < 2048; i++) {
            q.push(audio_buffer[i]);
        }
    }
    for (int i = 0; i < samples_per_frame; i++) {
        next_frame[i] = q.front();
        q.pop();
    }
}

void SeekToTime(int time) {
    double video_duration_in_ms = (video_stream->nb_frames * 1000.0f) / (float)video_stream->r_frame_rate.num;
    double percent = (double)time / video_duration_in_ms;
    uint64_t target_video_dts = (double)video_stream->duration * percent;

    av_seek_frame(video_format_ctx, 0, target_video_dts, AVSEEK_FLAG_BACKWARD);
    do {
        get_video_frame(0);
    } while (video_stream->cur_dts < target_video_dts);

    float time_in_ms = ((video_stream->nb_frames * 1000.0f) / (double)video_stream->r_frame_rate.num) * ((double)video_stream->cur_dts / (double)video_stream->duration);
    uint64_t target_audio_dts = 44100 * time_in_ms / 1000;
    av_seek_frame(audio_format_ctx, 1, target_audio_dts, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY);
}
