#pragma once
#include <thirdparty/imgui/imgui.h>
#include <thirdparty/imgui/imgui_impl_win32.h>
#include <thirdparty/imgui/imgui_impl_dx11.h>
#include <thirdparty/imgui/imgui_internal.h>
#include <src/engine/sdk.h>

inline ImFont* g_font = nullptr;
inline ImFont* g_watermark_font = nullptr;
inline ImFont* g_esp_font = nullptr;
namespace overlay { extern ID3D11ShaderResourceView* platform_textures[5]; extern ID3D11ShaderResourceView* rank_textures[23]; }

namespace updater {
	void run();
}

namespace esp {
    inline constexpr float fov_triangle_thickness = 1.0f;
    inline bool flags_enabled = true;
    inline bool fov_triangle_filled = true;
    inline bool fov_triangle_outline = false;
    inline bool fov_triangle_dynamic = true;

	void draw(ImDrawList* dl);

	inline bool  enabled       = true;
	inline bool  box           = false;
    inline bool text_outline = true;
    inline bool box_outline = false;
    inline bool corner_box_outline = false;
    inline bool skeleton_outline = false;
    inline bool tracers_outline = false;

	inline bool  skeleton      = true;
	inline bool  tracers       = false;
	inline bool  show_name     = true;
	inline bool  show_distance = true;
	inline bool  show_platform = false;
	inline bool  show_rank     = false;
	inline bool  show_weapon   = false;
	inline bool  show_ammo     = false;
	inline bool  fov_triangles = false;

	inline float box_thickness = 1.0f;
    inline float corner_box_thickness = 1.0f;
    inline float skeleton_thickness = 1.0f;
    inline float tracers_thickness = 1.0f;
    inline int box_type = 0;
	inline constexpr float max_distance  = 300.0f;
	inline float fov_triangle_size   = 12.0f;
	inline float fov_triangle_offset = 18.0f;
	inline float fov_triangle_width  = 2.4f;
	inline float text_size           = 15.0f;

    struct feature_colors {
        ImVec4 visible = ImVec4(0.35f, 0.75f, 1.0f, 1.0f);
        ImVec4 hidden = ImVec4(1, 1, 1, 1);
        ImU32 get(bool is_visible) const {
            return ImGui::ColorConvertFloat4ToU32(is_visible ? visible : hidden);
        }
    };
    inline feature_colors box_colors;
    inline feature_colors corner_box_colors;
    inline feature_colors skeleton_colors;
    inline feature_colors tracers_colors;
    inline feature_colors flags_colors;
    inline feature_colors arrows_colors;

}

namespace aimbot {
    inline constexpr int bone_head = 110;
    inline constexpr int bone_neck = 66;
    inline constexpr int bone_chest = 2;
    inline constexpr int bone_pelvis = 0;
    inline ImVec4 fov_color = ImVec4(1, 1, 1, 1);
    inline float fov_thickness = 1.0f;
    inline bool fov_outline = false;
	inline bool enabled = false;
	inline bool visibility_check = true;
	inline int bone = bone_head;
	inline bool show_fov = true;
	inline int smoothness = 5;
	inline int fov = 100;
	inline int aimbot_keybind = VK_RBUTTON;
}

namespace config {
	inline constexpr const char* folder = "C:\\azurecheats.cc";
	inline constexpr const char* file = "C:\\azurecheats.cc\\config.ini";

	inline void ensure_folder() {
		CreateDirectoryA(folder, nullptr);
	}

	inline bool read_bool(const char* section, const char* key, bool fallback) {
		return GetPrivateProfileIntA(section, key, fallback ? 1 : 0, file) != 0;
	}

	inline int read_int(const char* section, const char* key, int fallback) {
		return GetPrivateProfileIntA(section, key, fallback, file);
	}

	inline float read_float(const char* section, const char* key, float fallback) {
		char value[64] = {};
		char fallback_text[64] = {};
		sprintf_s(fallback_text, "%.6f", fallback);
		GetPrivateProfileStringA(section, key, fallback_text, value, sizeof(value), file);
		return static_cast<float>(atof(value));
	}

	inline ImVec4 read_color(const char* section, const char* key, const ImVec4& fallback) {
		char value[128] = {};
		char fallback_text[128] = {};
		sprintf_s(fallback_text, "%.6f,%.6f,%.6f,%.6f", fallback.x, fallback.y, fallback.z, fallback.w);
		GetPrivateProfileStringA(section, key, fallback_text, value, sizeof(value), file);

		ImVec4 color = fallback;
		sscanf_s(value, "%f,%f,%f,%f", &color.x, &color.y, &color.z, &color.w);
		color.x = std::clamp(color.x, 0.0f, 1.0f);
		color.y = std::clamp(color.y, 0.0f, 1.0f);
		color.z = std::clamp(color.z, 0.0f, 1.0f);
		color.w = std::clamp(color.w, 0.0f, 1.0f);
		return color;
	}

	inline void write_bool(const char* section, const char* key, bool value) {
		WritePrivateProfileStringA(section, key, value ? "1" : "0", file);
	}

	inline void write_int(const char* section, const char* key, int value) {
		char text[32] = {};
		sprintf_s(text, "%d", value);
		WritePrivateProfileStringA(section, key, text, file);
	}

	inline void write_float(const char* section, const char* key, float value) {
		char text[64] = {};
		sprintf_s(text, "%.6f", value);
		WritePrivateProfileStringA(section, key, text, file);
	}

	inline void write_color(const char* section, const char* key, const ImVec4& color) {
		char text[128] = {};
		sprintf_s(text, "%.6f,%.6f,%.6f,%.6f", color.x, color.y, color.z, color.w);
		WritePrivateProfileStringA(section, key, text, file);
	}

	inline void load() {
		ensure_folder();
        aimbot::fov_color = read_color("aim", "fov_color", aimbot::fov_color);
        aimbot::fov_thickness = std::clamp(read_float("aim", "fov_thickness", aimbot::fov_thickness), 1.0f, 5.0f);
		aimbot::fov_outline = read_bool("aim", "fov_outline", aimbot::fov_outline);
		aimbot::visibility_check = read_bool("aim", "visibility_check", aimbot::visibility_check);
        esp::fov_triangle_filled = read_bool("visuals", "fov_triangle_filled", esp::fov_triangle_filled);
        esp::fov_triangle_outline = read_bool("visuals", "fov_triangle_outline", esp::fov_triangle_outline);
        esp::fov_triangle_dynamic = read_bool("visuals", "fov_triangle_dynamic", esp::fov_triangle_dynamic);

        esp::flags_enabled = read_bool("visuals", "flags_enabled", esp::flags_enabled);
		esp::enabled = read_bool("visuals", "enabled", esp::enabled);
		esp::box = read_bool("visuals", "box", esp::box);
        const bool legacy_corner = read_bool("visuals", "corner_box", false);
        esp::box_type = std::clamp(read_int("visuals", "box_type", legacy_corner ? 1 : 0), 0, 1);
        if (legacy_corner) esp::box = true;
        const bool legacy_outline = read_bool("visuals", "outline", false);
        esp::text_outline = read_bool("visuals", "text_outline", esp::text_outline);
        esp::box_outline = read_bool("visuals", "box_outline", legacy_outline);
        esp::corner_box_outline = read_bool("visuals", "corner_box_outline", legacy_outline);
        esp::skeleton_outline = read_bool("visuals", "skeleton_outline", legacy_outline);
        esp::tracers_outline = read_bool("visuals", "tracers_outline", legacy_outline);

		esp::skeleton = read_bool("visuals", "skeleton", esp::skeleton);
		esp::tracers = read_bool("visuals", "tracers", esp::tracers);
		esp::show_name = read_bool("visuals", "show_name", esp::show_name);
		esp::show_distance = read_bool("visuals", "show_distance", esp::show_distance);
		esp::show_platform = read_bool("visuals", "show_platform", esp::show_platform);
		esp::show_rank = read_bool("visuals", "show_rank", esp::show_rank);
		esp::show_weapon = read_bool("visuals", "show_weapon", esp::show_weapon);
		esp::show_ammo = read_bool("visuals", "show_ammo", esp::show_ammo);
		esp::fov_triangles = read_bool("visuals", "fov_triangles", esp::fov_triangles);
		esp::fov_triangle_size = std::clamp(read_float("visuals", "fov_triangle_size", esp::fov_triangle_size), 2.0f, 30.0f);
		esp::fov_triangle_offset = std::clamp(read_float("visuals", "fov_triangle_offset", esp::fov_triangle_offset), 0.0f, 80.0f);
		esp::fov_triangle_width = std::clamp(read_float("visuals", "fov_triangle_width", esp::fov_triangle_width), 1.7f, 2.9f);
        esp::box_thickness = std::clamp(read_float("visuals", "box_thickness", esp::box_thickness), 1.0f, 5.0f);
        esp::corner_box_thickness = std::clamp(read_float("visuals", "corner_box_thickness", esp::corner_box_thickness), 1.0f, 5.0f);
        esp::skeleton_thickness = std::clamp(read_float("visuals", "skeleton_thickness", esp::skeleton_thickness), 1.0f, 5.0f);
        esp::tracers_thickness = std::clamp(read_float("visuals", "tracers_thickness", esp::tracers_thickness), 1.0f, 5.0f);
		esp::text_size = std::clamp(read_float("visuals", "text_size", esp::text_size), 10.0f, 30.0f);
        // Carry the old shared palette forward only when a feature has no saved colors.
        const ImVec4 legacy_visible = read_color("visuals", "visible_color", esp::box_colors.visible);
        const ImVec4 legacy_hidden = read_color("visuals", "hidden_color", esp::box_colors.hidden);
        esp::flags_colors.visible = read_color("visuals", "flags_visible_color", read_color("visuals", "name_visible_color", legacy_visible));
        esp::flags_colors.hidden = read_color("visuals", "flags_hidden_color", read_color("visuals", "name_hidden_color", legacy_hidden));
        esp::box_colors.visible = read_color("visuals", "box_visible_color", legacy_visible);
        esp::box_colors.hidden = read_color("visuals", "box_hidden_color", legacy_hidden);
        esp::corner_box_colors.visible = read_color("visuals", "corner_box_visible_color", legacy_visible);
        esp::corner_box_colors.hidden = read_color("visuals", "corner_box_hidden_color", legacy_hidden);
        esp::skeleton_colors.visible = read_color("visuals", "skeleton_visible_color", legacy_visible);
        esp::skeleton_colors.hidden = read_color("visuals", "skeleton_hidden_color", legacy_hidden);
        esp::tracers_colors.visible = read_color("visuals", "tracers_visible_color", legacy_visible);
        esp::tracers_colors.hidden = read_color("visuals", "tracers_hidden_color", legacy_hidden);
        esp::arrows_colors.visible = read_color("visuals", "arrows_visible_color", legacy_visible);
        esp::arrows_colors.hidden = read_color("visuals", "arrows_hidden_color", legacy_hidden);


		aimbot::enabled = read_bool("aim", "enabled", aimbot::enabled);
		aimbot::show_fov = read_bool("aim", "show_fov", aimbot::show_fov);
		aimbot::smoothness = std::clamp(read_int("aim", "smoothness", aimbot::smoothness), 1, 20);
		aimbot::fov = std::clamp(read_int("aim", "fov", aimbot::fov), 10, 500);
		aimbot::aimbot_keybind = read_int("aim", "keybind", aimbot::aimbot_keybind);
		aimbot::bone = read_int("aim", "bone", aimbot::bone_head);
		if (aimbot::bone != aimbot::bone_neck && aimbot::bone != aimbot::bone_chest && aimbot::bone != aimbot::bone_pelvis)
			aimbot::bone = aimbot::bone_head;
	}

	inline void save() {
		ensure_folder();
        write_color("aim", "fov_color", aimbot::fov_color);
        write_float("aim", "fov_thickness", aimbot::fov_thickness);
		write_bool("aim", "fov_outline", aimbot::fov_outline);
		write_bool("aim", "visibility_check", aimbot::visibility_check);
        write_bool("visuals", "fov_triangle_filled", esp::fov_triangle_filled);
        write_bool("visuals", "fov_triangle_outline", esp::fov_triangle_outline);
        write_bool("visuals", "fov_triangle_dynamic", esp::fov_triangle_dynamic);

        write_bool("visuals", "flags_enabled", esp::flags_enabled);
        write_color("visuals", "flags_visible_color", esp::flags_colors.visible);
        write_color("visuals", "flags_hidden_color", esp::flags_colors.hidden);
		write_bool("visuals", "enabled", esp::enabled);
		write_bool("visuals", "box", esp::box);
        write_int("visuals", "box_type", esp::box_type);
        write_bool("visuals", "corner_box", false);
        write_bool("visuals", "text_outline", esp::text_outline);
        write_bool("visuals", "box_outline", esp::box_outline);
        write_bool("visuals", "corner_box_outline", esp::corner_box_outline);
        write_bool("visuals", "skeleton_outline", esp::skeleton_outline);
        write_bool("visuals", "tracers_outline", esp::tracers_outline);

		write_bool("visuals", "skeleton", esp::skeleton);
		write_bool("visuals", "tracers", esp::tracers);
		write_bool("visuals", "show_name", esp::show_name);
		write_bool("visuals", "show_distance", esp::show_distance);
		write_bool("visuals", "show_platform", esp::show_platform);
		write_bool("visuals", "show_rank", esp::show_rank);
		write_bool("visuals", "show_weapon", esp::show_weapon);
		write_bool("visuals", "show_ammo", esp::show_ammo);
		write_bool("visuals", "fov_triangles", esp::fov_triangles);
		write_float("visuals", "fov_triangle_size", esp::fov_triangle_size);
		write_float("visuals", "fov_triangle_offset", esp::fov_triangle_offset);
		write_float("visuals", "fov_triangle_width", esp::fov_triangle_width);
        write_float("visuals", "box_thickness", esp::box_thickness);
        write_float("visuals", "corner_box_thickness", esp::corner_box_thickness);
        write_float("visuals", "skeleton_thickness", esp::skeleton_thickness);
        write_float("visuals", "tracers_thickness", esp::tracers_thickness);
		write_float("visuals", "text_size", esp::text_size);
        write_color("visuals", "box_visible_color", esp::box_colors.visible);
        write_color("visuals", "box_hidden_color", esp::box_colors.hidden);
        write_color("visuals", "corner_box_visible_color", esp::corner_box_colors.visible);
        write_color("visuals", "corner_box_hidden_color", esp::corner_box_colors.hidden);
        write_color("visuals", "skeleton_visible_color", esp::skeleton_colors.visible);
        write_color("visuals", "skeleton_hidden_color", esp::skeleton_colors.hidden);
        write_color("visuals", "tracers_visible_color", esp::tracers_colors.visible);
        write_color("visuals", "tracers_hidden_color", esp::tracers_colors.hidden);
        write_color("visuals", "arrows_visible_color", esp::arrows_colors.visible);
        write_color("visuals", "arrows_hidden_color", esp::arrows_colors.hidden);


		write_bool("aim", "enabled", aimbot::enabled);
		write_bool("aim", "show_fov", aimbot::show_fov);
		write_int("aim", "smoothness", aimbot::smoothness);
		write_int("aim", "fov", aimbot::fov);
		write_int("aim", "keybind", aimbot::aimbot_keybind);
		write_int("aim", "bone", aimbot::bone);
	}
}
