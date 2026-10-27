#pragma once
#include <thirdparty/imgui/imgui.h>

namespace dopamina {
namespace Style {
    const float topbar_height = 24.f;
    const float tab_height = 25.f;
    const float tab_padding = 10.f;
    const float outline_thickness = 1.f;
    const float text_outline_offset = 1.f;

    const float tab_anim_speed = 2.0f;
    const float collapse_anim_speed = 10.0f;

    const ImU32 col_outline = IM_COL32(10, 10, 10, 255);
    const ImU32 col_bg = IM_COL32(27, 33, 40, 255);
    const ImU32 col_text = IM_COL32(230, 230, 230, 255);
    const ImU32 col_widget_bg = IM_COL32(40, 49, 59, 255);


    const ImU32 col_tab_active_top = IM_COL32(89, 191, 255, 255);
    const ImU32 col_tab_active_bottom = IM_COL32(30, 91, 130, 255);

    const ImU32 col_anim_hover = IM_COL32(115, 209, 255, 255);
    const ImU32 col_pulse_bright = IM_COL32(160, 225, 255, 255);

    const ImU32 col_tab_inactive_top = IM_COL32(35, 43, 52, 255);
    const ImU32 col_tab_inactive_bottom = IM_COL32(26, 32, 40, 255);

    const ImU32 col_topbar_top = IM_COL32(64, 161, 218, 255);
    const ImU32 col_topbar_bottom = IM_COL32(23, 65, 94, 255);


    const ImU32 col_popup_bg = IM_COL32(35, 43, 52, 255);
}
}
