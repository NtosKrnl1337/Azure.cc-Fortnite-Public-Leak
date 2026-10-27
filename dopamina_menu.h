#pragma once
// Adapted from Framwork by dopamina / LUNO menu.cpp.
#include <thirdparty/imgui/imgui.h>
#include <thirdparty/imgui/imgui_internal.h>
#include <vector>
#include <string>
#include <map>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <src/rendering/menu_theme.h>

namespace dopamina {


void AddTriangleFilledMultiColor(ImDrawList* dl, ImVec2 a, ImVec2 b, ImVec2 c, ImU32 col_a, ImU32 col_b, ImU32 col_c) {
    if (!dl) return;
    const ImVec2 uv = dl->_Data->TexUvWhitePixel;
    dl->PrimReserve(3, 3);
    dl->PrimWriteIdx((ImDrawIdx)(dl->_VtxCurrentIdx));
    dl->PrimWriteIdx((ImDrawIdx)(dl->_VtxCurrentIdx + 1));
    dl->PrimWriteIdx((ImDrawIdx)(dl->_VtxCurrentIdx + 2));
    dl->PrimWriteVtx(a, uv, col_a);
    dl->PrimWriteVtx(b, uv, col_b);
    dl->PrimWriteVtx(c, uv, col_c);
}

ImU32 ColorLerp(ImU32 col_a, ImU32 col_b, float t) {
    int r1 = (col_a >> 0) & 0xFF; int g1 = (col_a >> 8) & 0xFF; int b1 = (col_a >> 16) & 0xFF; int a1 = (col_a >> 24) & 0xFF;
    int r2 = (col_b >> 0) & 0xFF; int g2 = (col_b >> 8) & 0xFF; int b2 = (col_b >> 16) & 0xFF; int a2 = (col_b >> 24) & 0xFF;
    int r = r1 + (int)((r2 - r1) * t);
    int g = g1 + (int)((g2 - g1) * t);
    int b = b1 + (int)((b2 - b1) * t);
    int a = a1 + (int)((a2 - a1) * t);
    return IM_COL32(r, g, b, a);
}

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

ImVec2 RotateVertex(ImVec2 p, ImVec2 c, float s, float c_cos) {
    return ImVec2(c.x + (p.x - c.x) * c_cos - (p.y - c.y) * s, c.y + (p.x - c.x) * s + (p.y - c.y) * c_cos);
}

void DrawShadedArrow(ImDrawList* dl, ImVec2 center, float size, float angle_rad, bool hovered) {
    ImVec2 p1 = ImVec2(-size, -size);
    ImVec2 p2 = ImVec2(-size, size);
    ImVec2 p3 = ImVec2(size, 0);

    float s_sin = sinf(angle_rad);
    float c_cos = cosf(angle_rad);

    ImVec2 r1 = RotateVertex(p1, ImVec2(0, 0), s_sin, c_cos);
    ImVec2 r2 = RotateVertex(p2, ImVec2(0, 0), s_sin, c_cos);
    ImVec2 r3 = RotateVertex(p3, ImVec2(0, 0), s_sin, c_cos);

    ImU32 col_shadow = IM_COL32(0, 0, 0, 150);
    dl->AddTriangleFilled(ImVec2(center.x + r1.x + 1, center.y + r1.y + 1), ImVec2(center.x + r2.x + 1, center.y + r2.y + 1), ImVec2(center.x + r3.x + 1, center.y + r3.y + 1), col_shadow);

    ImU32 col_tip = hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(240, 240, 240, 255);
    ImU32 col_base = hovered ? IM_COL32(220, 220, 240, 255) : IM_COL32(160, 160, 160, 255);

    AddTriangleFilledMultiColor(dl, ImVec2(center.x + r3.x, center.y + r3.y), ImVec2(center.x + r1.x, center.y + r1.y), ImVec2(center.x + r2.x, center.y + r2.y), col_tip, col_base, col_base);
    dl->AddTriangle(ImVec2(center.x + r1.x, center.y + r1.y), ImVec2(center.x + r2.x, center.y + r2.y), ImVec2(center.x + r3.x, center.y + r3.y), IM_COL32(50, 50, 60, 255), 1.0f);
}

void AddTextWithOutline(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* text) {
    ImU32 outline_col = Style::col_outline;
    float o = Style::text_outline_offset;
    dl->AddText(ImVec2(pos.x - o, pos.y), outline_col, text);
    dl->AddText(ImVec2(pos.x + o, pos.y), outline_col, text);
    dl->AddText(ImVec2(pos.x, pos.y - o), outline_col, text);
    dl->AddText(ImVec2(pos.x, pos.y + o), outline_col, text);
    dl->AddText(ImVec2(pos.x - o, pos.y - o), outline_col, text);
    dl->AddText(ImVec2(pos.x + o, pos.y - o), outline_col, text);
    dl->AddText(ImVec2(pos.x - o, pos.y + o), outline_col, text);
    dl->AddText(ImVec2(pos.x + o, pos.y + o), outline_col, text);
    dl->AddText(pos, col, text);
}

std::map<ImGuiID, float> anim_state;
std::map<ImGuiID, float> slider_visuals;

struct ChildState {
    bool is_collapsed = false;
    float current_height = -1.0f;
};
std::map<ImGuiID, ChildState> child_states;


// Theme-native action and selection buttons, with standard keyboard activation.
bool CustomButton(const char* label, ImVec2 requested_size = ImVec2(0, 32), bool selected = false) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    const ImGuiID id = window->GetID(label);
    const ImVec2 text_size = ImGui::CalcTextSize(label, nullptr, true);
    const ImVec2 size = ImGui::CalcItemSize(requested_size, text_size.x + 24, 32);
    const ImRect rect(window->DC.CursorPos, window->DC.CursorPos + size);
    ImGui::ItemSize(rect);
    if (!ImGui::ItemAdd(rect, id)) return false;
    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior(rect, id, &hovered, &held);
    const bool focused = GImGui->NavId == id;
    float& animation = anim_state[id];
    const float target = hovered || focused || selected ? 1.0f : 0.0f;
    animation = Lerp(animation, target, ImMin(1.0f, ImGui::GetIO().DeltaTime * 14.0f));
    const ImU32 top = ColorLerp(Style::col_widget_bg, Style::col_topbar_top, animation);
    const ImU32 bottom = ColorLerp(Style::col_tab_inactive_bottom, Style::col_topbar_bottom, animation);
    ImDrawList* draw_list = window->DrawList;
    draw_list->AddRectFilledMultiColor(rect.Min, rect.Max,
        held ? bottom : top, held ? bottom : top, bottom, bottom);
    draw_list->AddRect(rect.Min, rect.Max, Style::col_outline);
    const ImU32 edge = ColorLerp(IM_COL32(65, 78, 90, 255), Style::col_anim_hover, animation);
    draw_list->AddRect(rect.Min + ImVec2(1, 1), rect.Max - ImVec2(1, 1), edge);
    if (selected)
        draw_list->AddRectFilled(rect.Min + ImVec2(2, 2), ImVec2(rect.Min.x + 4, rect.Max.y - 2), Style::col_pulse_bright);
    const ImVec2 text_pos = rect.Min + (size - text_size) * 0.5f + ImVec2(0, held ? 1.0f : 0.0f);
    draw_list->PushClipRect(rect.Min + ImVec2(4, 1), rect.Max - ImVec2(4, 1), true);
    const std::string display_label(label, ImGui::FindRenderedTextEnd(label));
    AddTextWithOutline(draw_list, text_pos, Style::col_text, display_label.c_str());
    draw_list->PopClipRect();
    return pressed;
}

bool CustomCheckbox(const char* label, bool* v) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const float size = 14.0f;
    const ImVec2 text_size = ImGui::CalcTextSize(label);
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect total_bb(pos, ImVec2(pos.x + size + (text_size.x > 0 ? 8 + text_size.x : 0), pos.y + size));
    const ImRect check_bb(pos, ImVec2(pos.x + size, pos.y + size));

    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed) *v = !(*v);

    float t = anim_state[id];
    float target = *v ? 1.0f : 0.0f;
    float anim_speed = 5.0f * ImGui::GetIO().DeltaTime;

    if (t < target) { t += anim_speed; if (t > target) t = target; }
    else if (t > target) { t -= anim_speed; if (t < target) t = target; }
    anim_state[id] = t;

    ImDrawList* dl = window->DrawList;
    dl->AddRectFilled(check_bb.Min, check_bb.Max, Style::col_widget_bg);
    dl->AddRect(check_bb.Min, check_bb.Max, Style::col_outline);

    if (t > 0.01f) {
        float ease_t = 1.0f - powf(1.0f - t, 3.0f);
        float pad = 5.0f * (1.0f - ease_t) + 2.0f;
        ImU32 fill_col_top = Style::col_tab_active_top;
        ImU32 fill_col_bot = Style::col_tab_active_bottom;

        if (*v && t > 0.9f) {
            float time = (float)ImGui::GetTime();
            float pulse = (sinf(time * 3.5f) + 1.0f) * 0.5f;
            fill_col_top = ColorLerp(Style::col_tab_active_top, Style::col_pulse_bright, pulse * 0.20f);
            fill_col_bot = ColorLerp(Style::col_tab_active_bottom, Style::col_pulse_bright, pulse * 0.10f);
        }
        else {
            fill_col_top = ColorLerp(Style::col_widget_bg, fill_col_top, t);
            fill_col_bot = ColorLerp(Style::col_widget_bg, fill_col_bot, t);
        }

        dl->AddRectFilledMultiColor(ImVec2(check_bb.Min.x + pad, check_bb.Min.y + pad), ImVec2(check_bb.Max.x - pad, check_bb.Max.y - pad), fill_col_top, fill_col_top, fill_col_bot, fill_col_bot);
    }

    ImU32 text_col = (hovered || *v) ? IM_COL32(255, 255, 255, 255) : Style::col_text;
    ImVec2 text_pos = ImVec2(check_bb.Max.x + 8, check_bb.Min.y - 1);
    AddTextWithOutline(dl, text_pos, text_col, label);
    return pressed;
}

bool CustomSlider(const char* label, float* v, float v_min, float v_max, const char* format = "%.1f") {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const float w = ImGui::GetContentRegionAvail().x - 4.0f;
    const float h = 16.0f;

    ImVec2 label_pos = window->DC.CursorPos;
    ImGui::ItemSize(ImVec2(w, ImGui::CalcTextSize(label).y + 4));
    ImDrawList* dl = window->DrawList;
    AddTextWithOutline(dl, label_pos, Style::col_text, label);

    const ImVec2 pos = window->DC.CursorPos;
    const ImRect frame_bb(pos, ImVec2(pos.x + w, pos.y + h));

    ImGui::ItemSize(frame_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(frame_bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(frame_bb, id, &hovered, &held);

    if (held || pressed) {
        float mouse_pos_x = g.IO.MousePos.x;
        float ratio = (mouse_pos_x - frame_bb.Min.x) / (frame_bb.Max.x - frame_bb.Min.x);
        if (ratio < 0.0f) ratio = 0.0f; if (ratio > 1.0f) ratio = 1.0f;
        *v = v_min + (v_max - v_min) * ratio;
    }

    float t_hover = anim_state[id];
    float target_hover = (hovered || held) ? 1.0f : 0.0f;
    float anim_speed_hover = 10.0f * ImGui::GetIO().DeltaTime;
    if (t_hover < target_hover) t_hover += anim_speed_hover; else t_hover -= anim_speed_hover;
    if (t_hover < 0.0f) t_hover = 0.0f; if (t_hover > 1.0f) t_hover = 1.0f;
    anim_state[id] = t_hover;

    if (slider_visuals.find(id) == slider_visuals.end()) slider_visuals[id] = *v;
    float& visual_val = slider_visuals[id];
    float glide_speed = 7.0f * ImGui::GetIO().DeltaTime;
    visual_val = Lerp(visual_val, *v, ImMin(glide_speed, 1.0f));

    dl->AddRectFilled(frame_bb.Min, frame_bb.Max, Style::col_widget_bg);
    float ratio_vis = (visual_val - v_min) / (v_max - v_min);
    if (ratio_vis < 0.0f) ratio_vis = 0.0f; if (ratio_vis > 1.0f) ratio_vis = 1.0f;
    ImVec2 fill_max = ImVec2(frame_bb.Min.x + (frame_bb.Max.x - frame_bb.Min.x) * ratio_vis, frame_bb.Max.y);

    if (ratio_vis > 0.001f) {
        ImU32 col_top = Style::col_tab_active_top;
        ImU32 col_bot = Style::col_tab_active_bottom;
        if (t_hover > 0.0f) {
            col_top = ColorLerp(col_top, Style::col_anim_hover, t_hover * 0.5f);
            col_bot = ColorLerp(col_bot, Style::col_anim_hover, t_hover * 0.2f);
        }
        dl->AddRectFilledMultiColor(frame_bb.Min, fill_max, col_top, col_top, col_bot, col_bot);
        ImU32 glow_col = IM_COL32(255, 255, 255, 150);
        dl->AddRectFilled(ImVec2(fill_max.x - 1, frame_bb.Min.y), fill_max, glow_col);
    }
    dl->AddRect(frame_bb.Min, frame_bb.Max, Style::col_outline);

    char value_buf[64]; sprintf(value_buf, format, *v);
    ImVec2 val_size = ImGui::CalcTextSize(value_buf);
    ImVec2 val_pos = ImVec2(frame_bb.Min.x + (frame_bb.GetWidth() - val_size.x) * 0.5f, frame_bb.Min.y + (frame_bb.GetHeight() - val_size.y) * 0.5f);
    AddTextWithOutline(dl, val_pos, IM_COL32(255, 255, 255, 255), value_buf);
    ImGui::Dummy(ImVec2(0, 4));
    return pressed || held;
}


bool CustomCombo(const char* label, int* current_item, const std::vector<const char*>& items) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const float w = ImGui::GetContentRegionAvail().x - 4.0f;
    const float h = 20.0f;

    ImVec2 label_pos = window->DC.CursorPos;
    ImGui::ItemSize(ImVec2(w, ImGui::CalcTextSize(label).y + 4));
    ImDrawList* dl = window->DrawList;
    AddTextWithOutline(dl, label_pos, Style::col_text, label);

    const ImVec2 pos = window->DC.CursorPos;
    const ImRect frame_bb(pos, ImVec2(pos.x + w, pos.y + h));

    ImGui::ItemSize(frame_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(frame_bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(frame_bb, id, &hovered, &held);
    bool popup_open = ImGui::IsPopupOpen(id, ImGuiPopupFlags_None);

    if (pressed) {
        ImGui::OpenPopup(id, ImGuiPopupFlags_None);
        popup_open = true;
    }

    dl->AddRectFilled(frame_bb.Min, frame_bb.Max, Style::col_widget_bg);
    if (hovered || popup_open) dl->AddRectFilled(frame_bb.Min, frame_bb.Max, IM_COL32(255, 255, 255, 10));
    dl->AddRect(frame_bb.Min, frame_bb.Max, Style::col_outline);

    const char* preview = (items.size() > 0 && *current_item >= 0 && *current_item < items.size()) ? items[*current_item] : "";
    ImVec2 text_size = ImGui::CalcTextSize(preview);
    ImVec2 text_pos = ImVec2(frame_bb.Min.x + 5, frame_bb.Min.y + (h - text_size.y) * 0.5f);
    AddTextWithOutline(dl, text_pos, IM_COL32(255, 255, 255, 255), preview);

    float t_open = anim_state[id];
    float target_open = popup_open ? 1.0f : 0.0f;
    float anim_speed = 10.0f * ImGui::GetIO().DeltaTime;
    if (t_open < target_open) t_open += anim_speed; else t_open -= anim_speed;
    if (t_open < 0.0f) t_open = 0.0f; if (t_open > 1.0f) t_open = 1.0f;
    anim_state[id] = t_open;

    ImVec2 arrow_center = ImVec2(frame_bb.Max.x - 12, frame_bb.Min.y + h * 0.5f);
    float arrow_angle = 3.14159f * 0.5f + (t_open * 3.14159f);
    DrawShadedArrow(dl, arrow_center, 3.0f, arrow_angle, hovered || popup_open);

    bool value_changed = false;
    if (popup_open) {
        ImGui::SetNextWindowPos(ImVec2(frame_bb.Min.x, frame_bb.Max.y + 2));
        ImGui::SetNextWindowSize(ImVec2(frame_bb.GetWidth(), -1));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, Style::col_popup_bg);
        ImGui::PushStyleColor(ImGuiCol_Border, Style::col_outline);

        if (ImGui::BeginPopupEx(id, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings)) {
            for (int i = 0; i < items.size(); i++) {
                bool is_selected = (i == *current_item);

                ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(255, 255, 255, 15));

                if (is_selected) {
                    ImVec2 cur = ImGui::GetCursorScreenPos();
                    float dot_y = cur.y + ImGui::GetTextLineHeight() * 0.5f;
                    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(cur.x + 3, dot_y), 2.5f, Style::col_pulse_bright);
                }

                ImGui::Indent(10.0f);
                if (ImGui::Selectable(items[i], is_selected)) {
                    *current_item = i;
                    value_changed = true;
                }
                ImGui::Unindent(10.0f);

                if (is_selected) ImGui::SetItemDefaultFocus();
                ImGui::PopStyleColor(2);
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
    }

    ImGui::Dummy(ImVec2(0, 4));
    return value_changed;
}


bool CustomMultiCombo(const char* label, std::vector<bool>& selected, const std::vector<const char*>& items) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);

    const float w = ImGui::GetContentRegionAvail().x - 4.0f;
    const float h = 20.0f;

    ImVec2 label_pos = window->DC.CursorPos;
    ImGui::ItemSize(ImVec2(w, ImGui::CalcTextSize(label).y + 4));
    ImDrawList* dl = window->DrawList;
    AddTextWithOutline(dl, label_pos, Style::col_text, label);

    const ImVec2 pos = window->DC.CursorPos;
    const ImRect frame_bb(pos, ImVec2(pos.x + w, pos.y + h));

    ImGui::ItemSize(frame_bb, Style::tab_padding);
    if (!ImGui::ItemAdd(frame_bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(frame_bb, id, &hovered, &held);
    bool popup_open = ImGui::IsPopupOpen(id, ImGuiPopupFlags_None);

    if (pressed) {
        ImGui::OpenPopup(id, ImGuiPopupFlags_None);
        popup_open = true;
    }

    dl->AddRectFilled(frame_bb.Min, frame_bb.Max, Style::col_widget_bg);
    if (hovered || popup_open) dl->AddRectFilled(frame_bb.Min, frame_bb.Max, IM_COL32(255, 255, 255, 10));
    dl->AddRect(frame_bb.Min, frame_bb.Max, Style::col_outline);

    std::string preview = "";
    int active_count = 0;
    for (size_t i = 0; i < items.size(); i++) {
        if (i < selected.size() && selected[i]) {
            if (active_count > 0) preview += ", ";
            preview += items[i];
            active_count++;
        }
    }
    if (active_count == 0) preview = "None";
    if (active_count == items.size() && items.size() > 1) preview = "All";

    ImVec2 text_size = ImGui::CalcTextSize(preview.c_str());
    dl->PushClipRect(frame_bb.Min, ImVec2(frame_bb.Max.x - 20, frame_bb.Max.y));
    ImVec2 text_pos = ImVec2(frame_bb.Min.x + 5, frame_bb.Min.y + (h - text_size.y) * 0.5f);
    AddTextWithOutline(dl, text_pos, IM_COL32(255, 255, 255, 255), preview.c_str());
    dl->PopClipRect();

    float t_open = anim_state[id];
    float target_open = popup_open ? 1.0f : 0.0f;
    float anim_speed = 10.0f * ImGui::GetIO().DeltaTime;
    if (t_open < target_open) t_open += anim_speed; else t_open -= anim_speed;
    if (t_open < 0.0f) t_open = 0.0f; if (t_open > 1.0f) t_open = 1.0f;
    anim_state[id] = t_open;

    ImVec2 arrow_center = ImVec2(frame_bb.Max.x - 12, frame_bb.Min.y + h * 0.5f);
    float arrow_angle = 3.14159f * 0.5f + (t_open * 3.14159f);
    DrawShadedArrow(dl, arrow_center, 3.0f, arrow_angle, hovered || popup_open);

    bool value_changed = false;
    if (popup_open) {
        ImGui::SetNextWindowPos(ImVec2(frame_bb.Min.x, frame_bb.Max.y + 2));
        ImGui::SetNextWindowSize(ImVec2(frame_bb.GetWidth(), -1));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, Style::col_popup_bg);
        ImGui::PushStyleColor(ImGuiCol_Border, Style::col_outline);

        if (ImGui::BeginPopupEx(id, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings)) {
            for (size_t i = 0; i < items.size(); i++) {
                if (i >= selected.size()) break;

                bool is_selected = selected[i];
                ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(255, 255, 255, 15));

                if (is_selected) {
                    ImVec2 cur = ImGui::GetCursorScreenPos();
                    float dot_y = cur.y + ImGui::GetTextLineHeight() * 0.5f;
                    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(cur.x + 3, dot_y), 2.5f, Style::col_pulse_bright);
                }

                ImGui::Indent(10.0f);
                if (ImGui::Selectable(items[i], is_selected, ImGuiSelectableFlags_DontClosePopups)) {
                    selected[i] = !selected[i];
                    value_changed = true;
                }
                ImGui::Unindent(10.0f);

                ImGui::PopStyleColor(2);
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
    }

    ImGui::Dummy(ImVec2(0, 4));
    return value_changed;
}


bool CustomColorPicker(const char* label, float col[4]) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const float w = ImGui::GetContentRegionAvail().x - 4.0f;
    const float h = 20.0f;

    ImDrawList* dl = window->DrawList;

    // Layout Setup
    float box_w = 25.0f;
    float box_h_inner = 10.0f;
    float gap = 10.0f;

    const ImVec2 pos = window->DC.CursorPos;
    ImVec2 label_size = ImGui::CalcTextSize(label);

    // Box Positionierung
    float box_start_x = pos.x + label_size.x + gap;
    float box_y_offset = (h - box_h_inner) * 0.5f;

    const ImRect box_bb(
        ImVec2(box_start_x, pos.y + box_y_offset),
        ImVec2(box_start_x + box_w, pos.y + box_y_offset + box_h_inner)
    );

    const ImRect total_bb(pos, ImVec2(pos.x + w, pos.y + h));

    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id)) return false;


    AddTextWithOutline(dl, ImVec2(pos.x, pos.y + (h - ImGui::GetFontSize()) * 0.5f), Style::col_text, label);

    ImGui::PushID(id);
    bool is_active = ImGui::IsPopupOpen("##color_picker");
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(box_bb, id, &hovered, &held);
    if (pressed) {
        ImGui::OpenPopup("##color_picker");
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 popup_size(240.0f, 220.0f);
        const ImVec2 max_pos = ImMax(viewport->WorkPos, viewport->WorkPos + viewport->WorkSize - popup_size);
        ImGui::SetNextWindowPos(ImClamp(ImGui::GetIO().MousePos, viewport->WorkPos, max_pos));
        is_active = true;
    }
    bool changed = false;
    ImGui::PushStyleColor(ImGuiCol_PopupBg, Style::col_popup_bg);
    ImGui::PushStyleColor(ImGuiCol_Border, Style::col_tab_active_top);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
    if (ImGui::BeginPopup("##color_picker", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetNextItemWidth(220.0f);
        changed = ImGui::ColorPicker4("##color", col,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
            ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview |
            ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_PickerHueBar);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::PopID();


    float t_hover = anim_state[id];
   
    float target_hover = (is_active || hovered || held) ? 1.0f : 0.0f;
    float anim_speed = 15.0f * ImGui::GetIO().DeltaTime;

    if (t_hover < target_hover) t_hover += anim_speed;
    else t_hover -= anim_speed;


    if (t_hover < 0.0f) t_hover = 0.0f;
    if (t_hover > 1.0f) t_hover = 1.0f;

    anim_state[id] = t_hover;


    dl->AddRectFilled(box_bb.Min, box_bb.Max, IM_COL32(80, 80, 80, 255));

  
    ImU32 converted_col = ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], col[3]));
    dl->AddRectFilled(box_bb.Min, box_bb.Max, converted_col);

    if (is_active) {
       
        dl->AddRect(box_bb.Min, box_bb.Max, Style::col_pulse_bright, 0.0f, 0, 1.5f);
    }
    else if (t_hover > 0.01f) {
     
        ImU32 base_col = Style::col_pulse_bright;
        int alpha = (int)(t_hover * 255.0f);
        ImU32 glow_col = (base_col & 0x00FFFFFF) | (alpha << 24);
        dl->AddRect(box_bb.Min, box_bb.Max, glow_col, 0.0f, 0, 1.5f);
    }
    else {
       
        dl->AddRect(box_bb.Min, box_bb.Max, Style::col_outline);
    }

    return changed;
}


// Feature visibility palette: checkbox, 14px split swatch, then clickable title.
bool CustomCheckboxColor(const char* label, bool* enabled, float visible[4], float hidden[4], bool* outlined = nullptr) {
    if (ImGui::GetCurrentWindow()->SkipItems) return false;
    ImGui::PushID(label);
    ImGui::BeginGroup();
    bool changed = CustomCheckbox("", enabled);
    ImGui::SameLine(0, 6.0f);
    const ImVec2 swatch_pos = ImGui::GetCursorScreenPos();
    const ImVec2 swatch_max = swatch_pos + ImVec2(14, 14);
    const bool pressed = ImGui::InvisibleButton("##swatch", ImVec2(14, 14));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(swatch_pos, swatch_max, IM_COL32(80, 80, 80, 255));
    dl->AddTriangleFilled(swatch_pos, ImVec2(swatch_max.x, swatch_pos.y), swatch_max,
        ImGui::ColorConvertFloat4ToU32(ImVec4(visible[0], visible[1], visible[2], visible[3])));
    dl->AddTriangleFilled(swatch_pos, swatch_max, ImVec2(swatch_pos.x, swatch_max.y),
        ImGui::ColorConvertFloat4ToU32(ImVec4(hidden[0], hidden[1], hidden[2], hidden[3])));
    dl->AddRect(swatch_pos, swatch_max, ImGui::IsItemHovered() ? Style::col_pulse_bright : Style::col_outline);
    if (pressed) {
        ImGui::OpenPopup("##visibility_colors");
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        const ImVec2 limit = ImMax(vp->WorkPos, vp->WorkPos + vp->WorkSize - ImVec2(240, outlined ? 275.0f : 250.0f));
        ImGui::SetNextWindowPos(ImClamp(ImGui::GetIO().MousePos, vp->WorkPos, limit));
    }
    ImGui::PushStyleColor(ImGuiCol_PopupBg, Style::col_popup_bg);
    ImGui::PushStyleColor(ImGuiCol_Border, Style::col_tab_active_top);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
    if (ImGui::BeginPopup("##visibility_colors", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGuiStorage* storage = ImGui::GetStateStorage();
        int selected = storage->GetInt(ImGui::GetID("##palette"), 0);
        if (ImGui::Selectable("Visible", selected == 0, ImGuiSelectableFlags_DontClosePopups, ImVec2(105, 20))) selected = 0;
        ImGui::SameLine(0, 10);
        if (ImGui::Selectable("Hidden", selected == 1, ImGuiSelectableFlags_DontClosePopups, ImVec2(105, 20))) selected = 1;
        storage->SetInt(ImGui::GetID("##palette"), selected);
        if (outlined) changed |= CustomCheckbox("Outline", outlined);
        ImGui::SetNextItemWidth(220);
        changed |= ImGui::ColorPicker4("##color", selected == 0 ? visible : hidden,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
            ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview |
            ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_PickerHueBar);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::SameLine(0, 8);
    const ImVec2 title_pos = ImGui::GetCursorScreenPos();
    if (ImGui::InvisibleButton("##title", ImVec2(ImGui::CalcTextSize(label).x, 14))) {
        *enabled = !*enabled;
        changed = true;
    }
    AddTextWithOutline(dl, title_pos + ImVec2(0, -1), Style::col_text, label);
    ImGui::EndGroup();
    ImGui::PopID();
    return changed;
}

// A checkbox and adjacent three-dot button with arbitrary popup content.
template <typename DrawSettings>
bool CustomCheckboxSettings(const char* label, bool* enabled, DrawSettings draw_settings) {
    if (ImGui::GetCurrentWindow()->SkipItems) return false;
    ImGui::PushID(label);
    ImGui::BeginGroup();
    bool changed = CustomCheckbox("", enabled);
    ImGui::SameLine(0, 6);
    const ImVec2 dots_pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton("##settings", ImVec2(14, 14));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 dot_color = ImGui::IsItemHovered() || ImGui::IsPopupOpen("##settings_popup")
        ? Style::col_pulse_bright : Style::col_text;
    for (int i = 0; i < 3; ++i)
        dl->AddCircleFilled(dots_pos + ImVec2(3.0f + i * 4.0f, 7), 1.2f, dot_color);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float popup_height = ImMin(460.0f, vp->WorkSize.y - 20.0f);
    if (pressed) {
        ImGui::OpenPopup("##settings_popup");
        const ImVec2 limit = ImMax(vp->WorkPos, vp->WorkPos + vp->WorkSize - ImVec2(300, popup_height));
        ImGui::SetNextWindowPos(ImClamp(ImGui::GetIO().MousePos, vp->WorkPos, limit));
    }
    ImGui::PushStyleColor(ImGuiCol_PopupBg, Style::col_popup_bg);
    ImGui::PushStyleColor(ImGuiCol_Border, Style::col_tab_active_top);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
    ImGui::SetNextWindowSizeConstraints(ImVec2(300, 0), ImVec2(300, popup_height));
    if (ImGui::BeginPopup("##settings_popup", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
        draw_settings();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::SameLine(0, 8);
    const ImVec2 title_pos = ImGui::GetCursorScreenPos();
    if (ImGui::InvisibleButton("##title", ImVec2(ImGui::CalcTextSize(label).x, 14))) {
        *enabled = !*enabled;
        changed = true;
    }
    AddTextWithOutline(dl, title_pos + ImVec2(0, -1), Style::col_text, label);
    ImGui::EndGroup();
    ImGui::PopID();
    return changed;
}

void BeginCustomChild(const char* label, ImVec2 size) {
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiID id = window->GetID(label);

    if (child_states.find(id) == child_states.end()) {
        child_states[id].current_height = size.y;
        child_states[id].is_collapsed = false;
    }
    ChildState& state = child_states[id];

    const float header_h = 20.0f;
    float target_height = state.is_collapsed ? header_h : size.y;
    float dt = ImGui::GetIO().DeltaTime;
    float lerp_speed = Style::collapse_anim_speed;
    state.current_height = Lerp(state.current_height, target_height, ImMin(dt * lerp_speed, 1.0f));
    if (fabs(state.current_height - target_height) < 0.5f) state.current_height = target_height;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    ImGui::BeginChild(label, ImVec2(size.x, state.current_height), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 p = ImGui::GetWindowPos();
    ImVec2 s = ImGui::GetWindowSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilledMultiColor(
        p, ImVec2(p.x + s.x, p.y + header_h),
        Style::col_tab_active_top, Style::col_tab_active_top,
        Style::col_tab_active_bottom, Style::col_tab_active_bottom
    );
    dl->AddRect(p, ImVec2(p.x + s.x, p.y + s.y), Style::col_outline);

    ImVec2 text_size = ImGui::CalcTextSize(label);
    ImVec2 text_pos = ImVec2(p.x + (s.x - text_size.x) * 0.5f, p.y + (header_h - text_size.y) * 0.5f);
    AddTextWithOutline(dl, text_pos, IM_COL32(255, 255, 255, 255), label);

    
    float open_factor = (state.current_height - header_h) / (size.y - header_h);
    if (open_factor < 0.0f) open_factor = 0.0f; if (open_factor > 1.0f) open_factor = 1.0f;

    ImVec2 tri_center = ImVec2(p.x + s.x - 15.0f, p.y + header_h * 0.5f);

   
    float angle = open_factor * (3.14159f * 0.5f);
    bool hovered = ImGui::IsMouseHoveringRect(p, ImVec2(p.x + s.x, p.y + header_h));

    DrawShadedArrow(dl, tri_center, 4.0f, angle, hovered);

 
    ImGui::SetCursorPos(ImVec2(0, 0));
    if (ImGui::InvisibleButton("##header_btn", ImVec2(size.x, header_h))) {
        state.is_collapsed = !state.is_collapsed;
    }

    ImGui::BeginDisabled(state.is_collapsed);
    ImGui::SetCursorPosY(header_h + 8.0f);
    ImGui::Indent(8.0f);
    ImGui::PushItemWidth(s.x - 16.0f);
}

void EndCustomChild() {
    ImGui::PopItemWidth();
    ImGui::Unindent(8.0f);
    ImGui::EndDisabled();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}


static int current_tab = 0;
static int prev_tab = -1;
static float fade_alpha = 1.0f;
static float open_progress = 0.0f;
static float menu_opacity = 0.0f;
static ImGuiWindow* fading_menu_window = nullptr;

template <typename DrawContent>
void RenderMenu(bool is_open, DrawContent draw_content) {
    // Keep drawing while closing; reverse smoothly if Insert is pressed again.
    const float step = ImMin(ImGui::GetIO().DeltaTime, 0.05f) / 0.2f;
    open_progress = ImClamp(open_progress + (is_open ? step : -step), 0.0f, 1.0f);
    if (!is_open) ImGui::ClosePopupsOverWindow(nullptr, false);
    menu_opacity = open_progress * open_progress * (3.0f - 2.0f * open_progress);
    fading_menu_window = nullptr;
    if (open_progress <= 0.0f) return;


    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Once, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Style::col_bg);

    ImGui::Begin("Azure Fortnite Public", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollWithMouse | (is_open ? 0 : ImGuiWindowFlags_NoInputs));
    fading_menu_window = ImGui::GetCurrentWindow();
    ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);
    ImGui::BeginDisabled(!is_open);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 win_pos = ImGui::GetWindowPos();
    ImVec2 win_size = ImGui::GetWindowSize();

   
    ImVec2 topbar_min = win_pos;
    ImVec2 topbar_max = ImVec2(win_pos.x + win_size.x, win_pos.y + Style::topbar_height);
    dl->AddRectFilledMultiColor(topbar_min, topbar_max, Style::col_topbar_top, Style::col_topbar_top, Style::col_topbar_bottom, Style::col_topbar_bottom);
    dl->AddRect(topbar_min, topbar_max, Style::col_outline);
    AddTextWithOutline(dl, ImVec2(win_pos.x + 10, win_pos.y + 4), IM_COL32(255, 255, 255, 255), "Azure Fortnite Public");

    // Tabs
    const int tab_count = 2;
    const char* tab_names[] = { "Aim", "Visuals" };
    float tab_y_min = win_pos.y + Style::topbar_height;
    float tab_y_max = tab_y_min + Style::tab_height;
    float tab_width = win_size.x / (float)tab_count;

    if (current_tab != prev_tab) {
        ImGui::ClosePopupsOverWindow(ImGui::GetCurrentWindow(), false);
        prev_tab = current_tab;
        fade_alpha = 0.f; 
    }

    if (fade_alpha < 1.0f) {
        fade_alpha += ImGui::GetIO().DeltaTime * Style::tab_anim_speed;
        if (fade_alpha > 1.0f) fade_alpha = 1.0f;
    }

    for (int i = 0; i < tab_count; i++) {
        float tx0 = win_pos.x + (float)i * tab_width;
        float tx1 = tx0 + tab_width;
        ImVec2 tab_min(tx0, tab_y_min); ImVec2 tab_max(tx1, tab_y_max);
        bool is_active = (current_tab == i);

        if (is_active) dl->AddRectFilledMultiColor(tab_min, tab_max, Style::col_tab_active_top, Style::col_tab_active_top, Style::col_tab_active_bottom, Style::col_tab_active_bottom);
        else dl->AddRectFilledMultiColor(tab_min, tab_max, Style::col_tab_inactive_top, Style::col_tab_inactive_top, Style::col_tab_inactive_bottom, Style::col_tab_inactive_bottom);

        const char* label = tab_names[i];
        ImVec2 label_size = ImGui::CalcTextSize(label);
        float label_x = tx0 + Style::tab_padding;
        float label_y = tab_y_min + (Style::tab_height - label_size.y) * 0.5f;
        ImU32 text_col = is_active ? IM_COL32(255, 255, 255, 255) : IM_COL32(160, 160, 160, 255);
        AddTextWithOutline(dl, ImVec2(label_x, label_y), text_col, label);
    }

    dl->AddLine(ImVec2(win_pos.x, tab_y_min), ImVec2(win_pos.x + win_size.x, tab_y_min), Style::col_outline);
    dl->AddLine(ImVec2(win_pos.x, tab_y_max), ImVec2(win_pos.x + win_size.x, tab_y_max), Style::col_outline);
    for (int i = 1; i < tab_count; i++) dl->AddLine(ImVec2(win_pos.x + (float)i * tab_width, tab_y_min), ImVec2(win_pos.x + (float)i * tab_width, tab_y_max), Style::col_outline);

    ImGui::SetCursorPos(ImVec2(0, Style::topbar_height));
    for (int i = 0; i < tab_count; i++) {
        ImGui::SetCursorPos(ImVec2(i * tab_width, Style::topbar_height));
        char tab_id[16]; sprintf(tab_id, "##tab%d", i);
        if (ImGui::InvisibleButton(tab_id, ImVec2(tab_width, Style::tab_height))) current_tab = i;

    }


    float base_content_y = Style::topbar_height + Style::tab_height + 10;
    float ease_progress = 1.0f - powf(1.0f - fade_alpha, 4.0f);
    float anim_offset_y = 30.0f * (1.0f - ease_progress);

    ImGui::SetCursorPos(ImVec2(10, base_content_y + anim_offset_y));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fade_alpha);

    ImGui::BeginGroup();

    draw_content(current_tab);

    ImGui::EndGroup();
    ImGui::PopStyleVar();

    dl->AddRect(win_pos, ImVec2(win_pos.x + win_size.x, win_pos.y + win_size.y), Style::col_outline);
    ImGui::EndDisabled();
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(2);

  
}

struct PopupFadeState {
    float progress = 0.0f;
    int last_frame = -1;
    int depth = 0;
    ImDrawList* snapshot = nullptr;
    ImDrawList* closing_frame = nullptr;
};
static std::map<ImGuiID, PopupFadeState> popup_fades;

void ClearPopupFades() {
    for (auto& entry : popup_fades) {
        IM_DELETE(entry.second.snapshot);
        IM_DELETE(entry.second.closing_frame);
    }
    popup_fades.clear();
}

void FadeVertices(ImDrawList* list, float opacity) {
    for (ImDrawVert& vertex : list->VtxBuffer) {
        const unsigned int alpha = (vertex.col & IM_COL32_A_MASK) >> IM_COL32_A_SHIFT;
        vertex.col = (vertex.col & ~IM_COL32_A_MASK) |
            (static_cast<ImU32>(alpha * opacity) << IM_COL32_A_SHIFT);
    }
}

// Apply after Render: include late decorations and retain non-interactive snapshots
// of dismissed popups until their fade finishes.
void ApplyMenuFade() {
    const float step = ImMin(ImGui::GetIO().DeltaTime, 0.05f) / 0.15f;
    const int frame = ImGui::GetFrameCount();
    for (auto& entry : popup_fades) {
        IM_DELETE(entry.second.closing_frame);
        entry.second.closing_frame = nullptr;
    }
    // Track logical popup IDs, not recycled ##Popup_N window IDs. Open popups
    // can be hidden for a sizing frame without having been dismissed.
    std::map<ImGuiID, int> open_depths;
    for (int i = 0; i < GImGui->OpenPopupStack.Size; ++i)
        open_depths[GImGui->OpenPopupStack[i].PopupId] = i;
    for (auto& entry : popup_fades) {
        if (open_depths.find(entry.first) != open_depths.end())
            entry.second.last_frame = frame;
    }
    if (fading_menu_window) {
        for (ImGuiWindow* window : GImGui->Windows) {
            if (!window->Active || window->Hidden) continue;
            ImGuiWindow* ancestor = window;
            while (ancestor && ancestor != fading_menu_window) ancestor = ancestor->ParentWindowInBeginStack;
            if (!ancestor) continue;
            float opacity = menu_opacity;
            if (window->Flags & ImGuiWindowFlags_Popup) {
                const auto open = open_depths.find(window->PopupId);
                if (open == open_depths.end()) {
                    // ImGui may still submit the window on its dismissal frame.
                    // The cached snapshot is the sole source for that fade-out.
                    FadeVertices(window->DrawList, 0.0f);
                    continue;
                }
                auto& state = popup_fades[window->PopupId];
                state.depth = open->second;
                state.last_frame = frame;
                state.progress = ImMin(state.progress + step, 1.0f);
                IM_DELETE(state.snapshot);
                state.snapshot = window->DrawList->CloneOutput();
                opacity *= state.progress * state.progress * (3.0f - 2.0f * state.progress);
            }
            FadeVertices(window->DrawList, opacity);
        }
    }
    std::vector<PopupFadeState*> closing_popups;
    for (auto it = popup_fades.begin(); it != popup_fades.end();) {
        auto& state = it->second;
        if (state.last_frame != frame) {
            state.progress = ImMax(0.0f, state.progress - step);
            bool replaced = false;
            for (const auto& open : open_depths) {
                if (open.second >= state.depth) { replaced = true; break; }
            }
            // Never place a closing snapshot above a replacement popup. Also
            // discard old tab popups instead of drawing over the new tab.
            if (state.progress <= 0.0f || menu_opacity <= 0.0f || replaced || current_tab != prev_tab) {
                IM_DELETE(state.snapshot);
                it = popup_fades.erase(it);
                continue;
            }
            if (state.snapshot) {
                state.closing_frame = state.snapshot->CloneOutput();
                FadeVertices(state.closing_frame, menu_opacity * state.progress * state.progress * (3.0f - 2.0f * state.progress));
                state.closing_frame->_VtxWritePtr = state.closing_frame->VtxBuffer.Data + state.closing_frame->VtxBuffer.Size;
                state.closing_frame->_IdxWritePtr = state.closing_frame->IdxBuffer.Data + state.closing_frame->IdxBuffer.Size;
                state.closing_frame->_VtxCurrentIdx = state.closing_frame->VtxBuffer.Size;
                closing_popups.push_back(&state);
            }
        }
        ++it;
    }
    // Preserve parent-before-child ordering when multiple popup levels close.
    std::sort(closing_popups.begin(), closing_popups.end(), [](const auto* a, const auto* b) {
        return a->depth < b->depth;
    });
    for (const auto* state : closing_popups)
        ImGui::GetDrawData()->AddDrawList(state->closing_frame);
}
} // namespace dopamina
