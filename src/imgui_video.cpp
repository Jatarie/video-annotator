#include "win32_audio.cpp"
#include "ProgressSliderFloat.cpp"
#include "ffmpeg.cpp"

namespace ImGui {

GLuint video_texture = 0;
win32_sound_output SoundOutput = {};
bool playing = 0;
int64_t time_in_ms_video = 0;
int64_t time_in_ms_audio = 0;
float* prog = (float*)malloc(sizeof(float));
float prev_prog = 0;
int64_t video_duration_in_ms;
int64_t audio_duration_in_ms;
char* filepath;

void InitVideoPlayer(GLFWwindow* window, char* filename) {
    SoundOutput = {};
    playing = 0;
    time_in_ms_video = 0;
    time_in_ms_audio = 0;
    *prog = 0;
    prev_prog = 0;
    filepath = filename;
    initffmpeg(filename);
    video_duration_in_ms = (video_stream->nb_frames * 1000.0f) / (float)video_stream->r_frame_rate.num;
    audio_duration_in_ms = audio_stream->duration / 44.1f;
    HWND win32_window = glfwGetWin32Window(window);
    SoundOutput = InitSoundOutput();
    if (!GlobalSecondarySoundBuffer) {
        Win32InitDSound(win32_window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
    }
    GlobalSecondarySoundBuffer->Stop();
    int image_width = video_codec_ctx->width;
    int image_height = video_codec_ctx->height;

    glGenTextures(1, &video_texture);
    glBindTexture(GL_TEXTURE_2D, video_texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    int window_width;
    int window_height;
    glfwGetFramebufferSize(window, &window_width, &window_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgb_frame);
}

void VideoPlayer(GLFWwindow* window, char* filename) {
    static int initialized = 0;
    if (!initialized || filename != filepath) {
        InitVideoPlayer(window, filename);
        initialized = 1;
    }
    time_in_ms_video = video_duration_in_ms * ((double)video_stream->cur_dts / (double)video_stream->duration);
    time_in_ms_audio = audio_duration_in_ms * ((double)audio_stream->cur_dts / (double)audio_stream->duration);
    ImGui::Image((void*)(intptr_t)video_texture, ImVec2(video_codec_ctx->width, video_codec_ctx->height));
    ImGui::PushItemWidth(1280);
    ImGui::ProgressSliderFloat("##hey", prog, 0.0f, 1.0f, (float)video_stream->cur_dts / (float)video_stream->duration);
    ImGui::SetWindowSize(ImVec2(0, 0));
    // ImGui::SetWindowPos(ImVec2(0, 20));
    if (ImGui::IsKeyPressed(0x106, 0)) {
        SeekToTime(10000 + time_in_ms_video);
    }
    if (ImGui::IsKeyPressed(0x107, 0)) {
        SeekToTime(-10000 + time_in_ms_video);
    }
    if (ImGui::IsKeyPressed(0x108, 0)) {
        GlobalSecondarySoundBuffer->Stop();
        playing = 0;
    }
    if (ImGui::IsKeyPressed(0x109, 0)) {
        GlobalSecondarySoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
        playing = 1;
    }

    if (*prog != prev_prog) {
        SeekToTime(video_duration_in_ms * *prog);
        prev_prog = *prog;
    }

    DWORD WriteCursor;
    DWORD PlayCursor;
    DWORD BytesToWrite;
    DWORD ByteToLock;

    if ((SUCCEEDED(GlobalSecondarySoundBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)) && playing)) {
        ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
        DWORD TargetCursor = (PlayCursor + SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;

        if (ByteToLock > TargetCursor) {
            BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
            BytesToWrite += TargetCursor;
        }
        else {
            BytesToWrite = TargetCursor - ByteToLock;
        }
        while (BytesToWrite >= samples_per_frame * 4) {
            fill_queue();
            Win32FillSoundBuffer(&SoundOutput, ByteToLock, samples_per_frame * 4, next_frame);
            BytesToWrite -= samples_per_frame * 4;
            ByteToLock += samples_per_frame * 4;
            if (playing) {
                get_video_frame(1);
            }
        }
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, video_codec_ctx->width, video_codec_ctx->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgb_frame);
}
}  // namespace ImGui