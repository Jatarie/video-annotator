// #include "imgui.h"
// #include "imgui_internal.h"

namespace ImGui {
IMGUI_API bool ProgressSliderFloat(const char* label, float* v, float v_min, float v_max, float progress, const char* format = "%.3f", float power = 1.0f);  // adjust format to decorate the value with a prefix or a suffix for in-slider labels or unit display. Use power!=1.0 for power curve sliders
IMGUI_API bool ProgressSliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, float progress, const char* format = NULL, float power = 1.0f);
}  // namespace ImGui

static const char* PatchFormatStringFloatToInt(const char* fmt);

bool ImGui::ProgressSliderFloat(const char* label, float* v, float v_min, float v_max, float progress, const char* format, float power) {
    return ProgressSliderScalar(label, ImGuiDataType_Float, v, &v_min, &v_max, progress, format, power);
}
bool ImGui::ProgressSliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, float progress, const char* format, float power) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const float w = CalcItemWidth();

    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f));
    const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id, &frame_bb))
        return false;

    // Default format string when passing NULL
    if (format == NULL)
        format = DataTypeGetInfo(data_type)->PrintFmt;
    else if (data_type == ImGuiDataType_S32 && strcmp(format, "%d") != 0)  // (FIXME-LEGACY: Patch old "%.0f" format string to use "%d", read function more details.)
        format = PatchFormatStringFloatToInt(format);

    // Tabbing or CTRL-clicking on Slider turns it into an input box
    const bool hovered = ItemHoverable(frame_bb, id);
    bool temp_input_is_active = TempInputTextIsActive(id);
    bool temp_input_start = false;
    if (!temp_input_is_active) {
        const bool focus_requested = FocusableItemRegister(window, id);
        const bool clicked = (hovered && g.IO.MouseClicked[0]);
        if (focus_requested || clicked || g.NavActivateId == id || g.NavInputId == id) {
            SetActiveID(id, window);
            SetFocusID(id, window);
            FocusWindow(window);
            g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
            if (focus_requested || (clicked && g.IO.KeyCtrl) || g.NavInputId == id) {
                temp_input_start = true;
                FocusableItemUnregister(window);
            }
        }
    }
    if (temp_input_is_active || temp_input_start)
        return TempInputTextScalar(frame_bb, id, label, data_type, p_data, format);

    // Draw frame
    const ImU32 frame_col = GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : g.HoveredId == id ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    RenderNavHighlight(frame_bb, id);
    RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, g.Style.FrameRounding);

    // Slider behavior
    ImRect grab_bb;
    const bool value_changed = SliderBehavior(frame_bb, id, data_type, p_data, p_min, p_max, format, power, ImGuiSliderFlags_None, &grab_bb);
    if (value_changed)
        MarkItemEdited(id);

    grab_bb.Max.x = ((frame_bb.Max.x - frame_bb.Min.x) * progress) + frame_bb.Min.x;
    grab_bb.Min.x = grab_bb.Max.x - 10;

    // Render grab
    if (grab_bb.Max.x > grab_bb.Min.x)
        window->DrawList->AddRectFilled(grab_bb.Min, grab_bb.Max, GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), style.GrabRounding);

    // Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
    char value_buf[64];
    const char* value_buf_end = value_buf + DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format);
    RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2(0.5f, 0.5f));

    if (label_size.x > 0.0f)
        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.ItemFlags);
    return value_changed;
}