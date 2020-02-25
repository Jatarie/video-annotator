#include "windows.h"
#include "mmreg.h"

#include "imgui.cpp"
#include "imgui_impl_opengl2.cpp"
#include "imgui_impl_glfw.cpp"
#include "imgui_draw.cpp"
#include "imgui_widgets.cpp"

#include "imgui_video.cpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

struct hms {
    uint32_t h;
    uint32_t m;
    uint32_t s;
};

hms milliseconds_to_hms(int milliseconds) {
    hms rt = {};
    while (milliseconds >= 1000 * 3600) {
        rt.h += 1;
        milliseconds -= 1000 * 3600;
    }
    while (milliseconds >= 1000 * 60) {
        rt.m += 1;
        milliseconds -= 1000 * 60;
    }
    rt.s = milliseconds / 1000;
    return rt;
}

struct annotation {
    char* category;
    char* description;
    uint32_t vod_id;
    uint32_t start_ms;
    uint32_t end_ms;
};

void export_to_csv(annotation* annotations, int num) {
    FILE* f = fopen("stuff.csv", "w");
    char buf[8] = "nothing";
    fprintf(f, "timestamp, start, end, duration, category, description\n");
    for (int i = 0; i < num; i++) {
        annotation a = annotations[i];
        fprintf(f, "twitch.tv/what?3h41m3s, %d, %d, %d, %s, %s\n", a.start_ms, a.end_ms, a.end_ms - a.start_ms, a.category, a.description);
    }
    fclose(f);
}

int main() {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    uint64_t counts_per_ms = (double)frequency.QuadPart / 1000.0f;

    if (!glfwInit())
        return -1;
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Hello World", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    ImGuiStyle* style = &ImGui::GetStyle();
    // style->WindowPadding = ImVec2(100, 100);
    style->WindowRounding = 0.0f;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

    annotation annotations[256] = {};
    uint32_t num_annotations = 0;
    uint32_t start_annotation = 0;
    char category_buf[256] = {0};
    char description_buf[256] = {0};

    double end = 0;
    LARGE_INTEGER qpc_start = {};
    QueryPerformanceCounter(&qpc_start);
    LARGE_INTEGER w = {};
    uint32_t player_height = 0;
    uint32_t player_width = 0;
    char* filename = "E:\\vods\\out.mp4";
    // char* filename = "high.mp4";

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();
        ImGui::ShowMetricsWindow();

        ImGui::Begin("filename.mp4");
        ImGui::VideoPlayer(window, filename);
        player_height = ImGui::GetWindowHeight();
        player_width = ImGui::GetWindowHeight();
        ImGui::End();

        ImGui::Begin("Video Controls");
        if(ImGui::Button("what")){
            filename = "out.mp4";
        }
        hms hourMinuteSecond = milliseconds_to_hms(ImGui::time_in_ms_video);
        ImGui::Text("%02d:%02d:%02d", hourMinuteSecond.h, hourMinuteSecond.m, hourMinuteSecond.s);
        ImGui::InputText("Category", category_buf, 256);
        ImGui::InputText("Description", description_buf, 256);
        if (ImGui::IsKeyPressed(0x101, 0)) {
            char* category = (char*)malloc(sizeof(char) * 256);
            char* description = (char*)malloc(sizeof(char) * 256);
            for (int i = 0; i < 256; i++) {
                category[i] = category_buf[i];
                category_buf[i] = '\0';
                description[i] = description_buf[i];
                description_buf[i] = '\0';
            }
            annotations[num_annotations].category = category;
            annotations[num_annotations].description = description;
            annotations[num_annotations].start_ms = start_annotation;
            annotations[num_annotations].end_ms = ImGui::time_in_ms_video;
            start_annotation = ImGui::time_in_ms_video;
            num_annotations += 1;

            export_to_csv(annotations, num_annotations);
        }
        ImGui::End();

        ImGui::Begin("Summary");
        for (int i = 0; i < num_annotations; i++) {
            ImGui::Text(annotations[i].category);
            ImGui::SameLine();
            ImGui::Text(annotations[i].description);
            ImGui::SameLine();
            ImGui::Text("%d", annotations[i].start_ms);
            ImGui::SameLine();
            ImGui::Text("%d", annotations[i].end_ms);
        }

        ImGui::SetWindowPos(ImVec2(0, 780));
        ImGui::BeginMainMenuBar();
        ImGui::MenuItem("Open Video");
        ImGui::MenuItem("Export CSV");
        ImGui::MenuItem("Import CSV");
        ImGui::EndMainMenuBar();
        ImGui::End();

        ImGui::Render();

        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}
