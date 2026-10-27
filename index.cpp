#include <include.h>
#include <src/cache/index.h>
#include <src/rendering/menu_theme.h>
#include <numbers>
#include <cmath>
#include <cstdio>
#include <bit>
#include <cfloat>
#include <array>

static inline uintptr_t get_world_pointer()
{
    uintptr_t engine_pointer = read<uintptr_t>(base + GENGINE);
    uintptr_t viewport_pointer = read<uintptr_t>(engine_pointer + VIEWPORT);
    uintptr_t world_pointer = read<uintptr_t>(viewport_pointer + VIEWPORT_WORLD);

    return world_pointer;
}

namespace updater {
	void run() {
		while (true) {
			auto world_pointer = get_world_pointer();
			auto game_instance = read<u64>(world_pointer + GAME_INSTANCE);
			if (!game_instance) { Sleep(250); continue; }

			auto localplayer_pointer = read<u64>(game_instance + LOCAL_PLAYERS);
			if (!localplayer_pointer) { Sleep(250); continue; }

			auto local_player = read<u64>(localplayer_pointer);
			if (!local_player) { Sleep(250); continue; }

			auto player_controller = read<u64>(local_player + PLAYER_CONTROLLER);
			if (!player_controller) { Sleep(250); continue; }

			auto local_player_state         = read<u64>(player_controller + PLAYER_STATE);
			auto local_pawn = read<u64>(player_controller + LOCAL_PAWN);

			udata::world next_world = {
				world_pointer,
				game_instance,
				local_player,
				player_controller,
				local_player_state,
				local_pawn,
				(int)read<uint8_t>(local_player_state + TEAM_INDEX),
				0,
				{}
			};

			auto game_state = read<u64>(world_pointer + GAME_STATE);
			auto player_array = read<numerics::tarray<u64>>(game_state + PLAYER_ARRAY);
			auto players = player_array.get_all();

			next_world.game_state   = game_state;
			next_world.player_array = players;

			std::vector<udata::actor> next_actors;
			next_actors.reserve(players.size());

			for (auto player_state : players) {
				if (!player_state) continue;

				int team_id = (int)read<uint8_t>(player_state + TEAM_INDEX);
				int local_team_id = next_world.team_id;
				if (local_team_id != 255 && team_id != 255 && team_id == local_team_id) continue;

				auto pawn = read<u64>(player_state + PAWN_PRIVATE);
				if (!pawn || pawn == local_pawn) continue;

				auto mesh = read<u64>(pawn + MESH);
				if (!mesh) continue;

				next_actors.push_back({ player_state, pawn, mesh, team_id, {} });
			}

			{
				std::lock_guard<std::mutex> cache_lock(udata::actor_mutex);
				udata::actor_t = std::move(next_actors);
			}
			{
				std::lock_guard<std::mutex> cache_lock(udata::world_mutex);
				udata::world_t = std::move(next_world);
			}

			Sleep(50);
		}
	}

}

namespace esp {

	static std::string get_player_name(uintptr_t player_state) {
		try {
			if (!player_state) return "NPC";

			const uintptr_t name_pointer = read<uintptr_t>(player_state + PLAYERNAME);
			if (!name_pointer) return "NPC";

			const int length = read<int>(name_pointer + 0x10);
			const auto name_length = static_cast<int64_t>(length);
			if (!name_length || length <= 0 || length > 100) return "NPC";

			const uintptr_t name_data_pointer = read<uintptr_t>(name_pointer + 0x8);
			if (!name_data_pointer) return "NPC";

			std::vector<wchar_t> buffer(static_cast<size_t>(length) + 1, L'\0');
			read2(name_data_pointer, buffer.data(), static_cast<size_t>(length) * sizeof(wchar_t));

			char initial_offset = static_cast<char>(name_length - 1);
			if (!static_cast<uint32_t>(name_length))
				initial_offset = 0;

			int character_index = 0;
			uint16_t* character_pointer = reinterpret_cast<uint16_t*>(buffer.data());
			for (int decode_offset = (initial_offset) & 3; ; *character_pointer++ += decode_offset & 7) {
				int last_character_index = static_cast<int>(name_length - 1);
				if (!static_cast<uint32_t>(name_length))
					last_character_index = 0;
				if (character_index >= last_character_index)
					break;
				decode_offset += 3;
				++character_index;
			}

			std::wstring player_name{ buffer.data() };
			return std::string(player_name.begin(), player_name.end());
		}
		catch (...) {
			return "NPC";
		}
	}

	static std::string get_rank(int32_t tier) {
		static const std::string ranks[] = {
			"Bronze 1", "Bronze 2", "Bronze 3",
			"Silver 1", "Silver 2", "Silver 3",
			"Gold 1", "Gold 2", "Gold 3",
			"Platinum 1", "Platinum 2", "Platinum 3",
			"Diamond 1", "Diamond 2", "Diamond 3",
			"Elite 1", "Elite 2", "Elite 3",
			"Champion 1", "Champion 2", "Champion 3",
			"Unreal", "Unranked"
		};

		if (tier >= 0 && tier < 22)
			return ranks[tier];
		return ranks[22];
	}

	static std::string read_device_name(uintptr_t player_state) {
		wchar_t platform_chars[64] = {};
		const uintptr_t platform_pointer = read<uintptr_t>(player_state + PLATFORM);
		if (!platform_pointer) return "Unknown";

		read2(platform_pointer, platform_chars, sizeof(platform_chars));

		std::wstring platform_wide(platform_chars);
		std::string platform(platform_wide.begin(), platform_wide.end());

		if (platform == "XBL" || platform == "XSX") return "XBOX";
		if (platform == "PSN" || platform == "PS5") return "PS5";
		if (platform == "SWT") return "SWITCH";
		if (platform == "WIN") return "PC";
		return "Unknown";
	}

	static std::string read_weapon_name(uintptr_t weapon) {
		try {
			if (!weapon) return "Building";

			const uint64_t weapon_data = read<uint64_t>(weapon + WEAPON_DATA);
			if (!weapon_data) return "Building";

			const uint64_t weapon_text_pointer = read<uint64_t>(weapon_data + WEAPON_NAME);
			if (!weapon_text_pointer) return "Building";

			const uint64_t weapon_text_data = read<uint64_t>(weapon_text_pointer + 0x20);
			const int weapon_text_length = read<int>(weapon_text_pointer + 0x28);
			if (!weapon_text_data || weapon_text_length <= 0 || weapon_text_length > 1024) return "Building";

			std::wstring wide_weapon_name;
			wide_weapon_name.resize(static_cast<size_t>(weapon_text_length));
			read2(weapon_text_data, wide_weapon_name.data(), static_cast<size_t>(weapon_text_length) * sizeof(wchar_t));

			if (!wide_weapon_name.empty() && wide_weapon_name.back() != L'\0')
				wide_weapon_name.push_back(L'\0');

			std::string result;
			result.resize(wide_weapon_name.size());
			WideCharToMultiByte(CP_UTF8, 0, wide_weapon_name.c_str(), -1, result.data(), static_cast<int>(result.size()), nullptr, nullptr);
			if (!result.empty())
				result.resize(strlen(result.c_str()));
			return result.empty() ? "Building" : result;
		}
		catch (...) {
			return "Building";
		}
	}

	static bool is_actor_visible(uintptr_t mesh, double seconds) {
		// [rt] PrimitiveComponent.LastRenderTime for CL-59343437 (was 0x534 pre-42.40)
		const float last_render_time = read<float>(mesh + LAST_RENDER_TIME);
		return seconds - last_render_time <= 0.06f;
	}

	static void draw_text_outlined(ImDrawList* draw_list, float center_x, float y, const char* text, ImU32 color) {
		ImFont* font = g_esp_font ? g_esp_font : g_font;
		const float font_size = text_size;
		ImVec2 text_dimensions = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text);
		float  x  = center_x - text_dimensions.x * 0.5f;
		static const ImU32 outline = IM_COL32(0, 0, 0, 230);
        if (text_outline) {
		draw_list->AddText(font, font_size, ImVec2(x - 1, y    ), outline, text);
		draw_list->AddText(font, font_size, ImVec2(x + 1, y    ), outline, text);
		draw_list->AddText(font, font_size, ImVec2(x    , y - 1), outline, text);
		draw_list->AddText(font, font_size, ImVec2(x    , y + 1), outline, text);
        }
		draw_list->AddText(font, font_size, ImVec2(x    , y    ), color, text);
	}

	static void draw_corner_box(ImDrawList* draw_list, float x, float y, float w, float h,
		ImU32 color, bool outlined) {
		const float corner_width = std::clamp(w * 0.30f, 2.0f, w * 0.5f);
		const float corner_height = std::clamp(h * 0.30f, 2.0f, h * 0.5f);
		const std::pair<ImVec2, ImVec2> lines[] = {
			{{x, y}, {x + corner_width, y}}, {{x, y}, {x, y + corner_height}},
			{{x + w - corner_width, y}, {x + w, y}}, {{x + w, y}, {x + w, y + corner_height}},
			{{x, y + h - corner_height}, {x, y + h}}, {{x, y + h}, {x + corner_width, y + h}},
			{{x + w - corner_width, y + h}, {x + w, y + h}}, {{x + w, y + h - corner_height}, {x + w, y + h}}
		};
		for (const auto& line : lines) {
			if (outlined)
				draw_list->AddLine(line.first, line.second, IM_COL32(0, 0, 0, 230), corner_box_thickness + 2.0f);
			draw_list->AddLine(line.first, line.second, color, corner_box_thickness);
		}
	}

	static void draw_fov_triangle(ImDrawList* draw_list, const numerics::fvector& world_location,
		float fov_radius, float distance_m, ImU32 color) {
		float triangle_size = fov_triangle_size;

		constexpr float near_distance = 5.0f;
		constexpr float far_distance = 200.0f;
		constexpr float max_scale = 1.6f;
		constexpr float min_scale = 0.6f;
		float distance_fraction = (distance_m - near_distance) / (far_distance - near_distance);
		distance_fraction = std::clamp(distance_fraction, 0.0f, 1.0f);
		distance_fraction = distance_fraction * distance_fraction * (3.0f - 2.0f * distance_fraction);
		if (fov_triangle_dynamic)
            triangle_size *= max_scale + (min_scale - max_scale) * distance_fraction;

		const float yaw = camera::rotation.yaw * static_cast<float>(std::numbers::pi) / 180.0f;
		const float world_offset_x = static_cast<float>(world_location.x - camera::location.x);
		const float world_offset_y = static_cast<float>(world_location.y - camera::location.y);
		const float sin_yaw = sinf(yaw);
		const float minus_cos_yaw = -cosf(yaw);

		const float x = -(world_offset_y * minus_cos_yaw + world_offset_x * sin_yaw);
		const float y = world_offset_x * minus_cos_yaw - world_offset_y * sin_yaw;
		const float angle = atan2f(y, x);
		const float radius = fov_radius + triangle_size + fov_triangle_offset;
		const float width = fov_triangle_width;
		const ImVec2 center{
			static_cast<float>(numerics::screen_width_center),
			static_cast<float>(numerics::screen_height_center)
		};
		const ImVec2 triangle_center{
			center.x + cosf(angle) * radius,
			center.y + sinf(angle) * radius
		};
		const ImVec2 point1{
			triangle_center.x + cosf(angle) * triangle_size,
			triangle_center.y + sinf(angle) * triangle_size
		};
		const ImVec2 point2{
			triangle_center.x + cosf(angle + width) * triangle_size,
			triangle_center.y + sinf(angle + width) * triangle_size
		};
		const ImVec2 point3{
			triangle_center.x + cosf(angle - width) * triangle_size,
			triangle_center.y + sinf(angle - width) * triangle_size
		};

        if (fov_triangle_outline)
            draw_list->AddTriangle(point1, point2, point3, IM_COL32(0, 0, 0, 230), fov_triangle_thickness + 2.0f);
        if (fov_triangle_filled)
            draw_list->AddTriangleFilled(point1, point2, point3, color);
        draw_list->AddTriangle(point1, point2, point3, color, fov_triangle_thickness);
	}

	static void draw_watermark(ImDrawList* draw_list) {
		char text[64];
		sprintf_s(text, "AzureCheats.cc | %.0f FPS", ImGui::GetIO().Framerate);

		constexpr ImVec2 position{ 12.0f, 12.0f };
		constexpr ImVec2 padding{ 10.0f, 5.0f };
		ImFont* watermark_font = g_watermark_font ? g_watermark_font : g_font;
		const float font_size = watermark_font ? watermark_font->FontSize : ImGui::GetFontSize();
		const ImVec2 text_size = watermark_font
			? watermark_font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text)
			: ImGui::CalcTextSize(text);
		const ImVec2 box_max{
			position.x + text_size.x + padding.x * 2.0f,
			position.y + text_size.y + padding.y * 2.0f
		};

        draw_list->AddRectFilledMultiColor(position, box_max,
            dopamina::Style::col_topbar_top, dopamina::Style::col_topbar_top,
            dopamina::Style::col_topbar_bottom, dopamina::Style::col_topbar_bottom);
        draw_list->AddRect(position, box_max, dopamina::Style::col_outline);
        const ImVec2 text_position{ position.x + padding.x, position.y + padding.y };
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                if (!x && !y) continue;
                draw_list->AddText(watermark_font, font_size, text_position + ImVec2(float(x), float(y)),
                    dopamina::Style::col_outline, text);
            }
        }
        draw_list->AddText(watermark_font, font_size, text_position, IM_COL32(255, 255, 255, 255), text);
	}

	struct bounds {
		double min_x, max_x, min_y, max_y;
	};

	struct frame_bones {
		bones::snapshot snapshot{};
		std::array<numerics::fvector, 111> values{};
		std::array<uint8_t, 111> loaded{};

		explicit frame_bones(const bones::snapshot& source) : snapshot(source) {}

		numerics::fvector get(int bone_id) {
			if (!snapshot.valid || bone_id < 0 || bone_id >= static_cast<int>(values.size()))
				return {};
			const size_t index = static_cast<size_t>(bone_id);
			if (!loaded[index]) {
				values[index] = bones::bone_from_snapshot(snapshot, bone_id);
				loaded[index] = 1;
			}
			return values[index];
		}
	};

	struct prepared_actor {
		udata::actor actor{};
		frame_bones bone_data;
		std::string name{};
		std::string platform{};
		std::string rank{};
		std::string weapon{};
		int ammo_count = 0;
		bool has_weapon = false;
		bool visible = false;

		prepared_actor(const udata::actor& source, const bones::snapshot& snapshot)
			: actor(source), bone_data(snapshot) {}
	};

	static const int box_bone_ids[] = {
		110, 66, 2,
		9, 10, 11, 38, 39, 40,
		78, 79, 82, 71, 72, 75,
		0
	};

	static bounds compute_bounds(frame_bones& bones) {
		bounds actor_bounds = { 1e9, -1e9, 1e9, -1e9 };
		for (int bone_id : box_bone_ids) {
			auto bone_world_position = bones.get(bone_id);
			if (bone_world_position.x == 0.0 && bone_world_position.y == 0.0 && bone_world_position.z == 0.0) continue;
			auto bone_screen_position = w2s::project(bone_world_position);
			if (bone_screen_position.x < actor_bounds.min_x) actor_bounds.min_x = bone_screen_position.x;
			if (bone_screen_position.x > actor_bounds.max_x) actor_bounds.max_x = bone_screen_position.x;
			if (bone_screen_position.y < actor_bounds.min_y) actor_bounds.min_y = bone_screen_position.y;
			if (bone_screen_position.y > actor_bounds.max_y) actor_bounds.max_y = bone_screen_position.y;
		}

		auto head = bones.get(110);
		if (!(head.x == 0.0 && head.y == 0.0 && head.z == 0.0)) {
			head.z += 18.0;
			auto top = w2s::project(head);
			if (top.y < actor_bounds.min_y) actor_bounds.min_y = top.y;
		}

		double bounds_width = actor_bounds.max_x - actor_bounds.min_x;
		double bounds_height = actor_bounds.max_y - actor_bounds.min_y;
		actor_bounds.min_x -= bounds_width * 0.15;
		actor_bounds.max_x += bounds_width * 0.15;
		actor_bounds.min_y -= bounds_height * 0.12;
		actor_bounds.max_y += bounds_height * 0.05;
		return actor_bounds;
	}

	static const std::pair<int, int> skeleton_links[] = {
		{ 110, 66 },
		{  66,  2 },
		{  66,  9 }, {  9, 10 }, { 10, 11 },
		{  66, 38 }, { 38, 39 }, { 39, 40 },
		{   2, 78 }, { 78, 79 }, { 79, 82 },
		{   2, 71 }, { 71, 72 }, { 72, 75 }
	};

	void draw(ImDrawList* draw_list) {
		draw_watermark(draw_list);
		udata::closest_mesh = 0;
		udata::closest_distance = FLT_MAX;

		const bool draw_esp = enabled;
		if (!draw_esp && !aimbot::enabled) return;

		std::vector<udata::actor> local_actors;
		uintptr_t gworld = 0;
		{
			std::lock_guard<std::mutex> cache_lock(udata::actor_mutex);
			local_actors = udata::actor_t;
		}
		{
			std::lock_guard<std::mutex> cache_lock(udata::world_mutex);
			gworld = udata::world_t.gworld;
		}

		// Finish every remote read before taking the camera snapshot. Projecting actors
		// while reads are still in progress makes the camera progressively older and
		// causes the ESP to trail behind during movement and rotation.
		const bool show_weapon_info = flags_enabled && (show_weapon || show_ammo);
		const bool needs_bounds = draw_esp && (box || skeleton || show_name || show_distance ||
			show_platform || show_rank || show_weapon_info || fov_triangles);
		std::vector<prepared_actor> prepared_actors;
		prepared_actors.reserve(local_actors.size());
		static std::unordered_map<uintptr_t, std::string> weapon_name_cache;
		const double current_seconds = (draw_esp || aimbot::enabled) && gworld
			? read<double>(gworld + SECONDS)
			: 0.0;

		for (const auto& actor : local_actors) {
			auto snapshot = bones::get_snapshot(actor.mesh);
			if (!snapshot.valid) continue;

			prepared_actors.emplace_back(actor, snapshot);
			auto& prepared = prepared_actors.back();
			const auto root = prepared.bone_data.get(0);
			if (root.x == 0.0 && root.y == 0.0 && root.z == 0.0) {
				prepared_actors.pop_back();
				continue;
			}

			if (aimbot::enabled || needs_bounds)
				prepared.bone_data.get(110);

			if (needs_bounds) {
				for (const int bone_id : box_bone_ids)
					prepared.bone_data.get(bone_id);
			}

			if (draw_esp || aimbot::enabled)
				prepared.visible = is_actor_visible(actor.mesh, current_seconds);

			if (draw_esp && flags_enabled && show_name)
				prepared.name = get_player_name(actor.player_state);

			if (draw_esp && flags_enabled && show_platform)
				prepared.platform = read_device_name(actor.player_state);

			if (draw_esp && flags_enabled && show_rank) {
				const uintptr_t rank_component = read<uintptr_t>(actor.player_state + HABANERO_COMPONENT);
				prepared.rank = rank_component
					? get_rank(read<uint32_t>(rank_component + RANKED_PROGRESS + 0x10))
					: "Unranked";
			}

			if (draw_esp && show_weapon_info) {
				const uintptr_t weapon = read<uintptr_t>(actor.pawn_private + CURRENT_WEAPON);
				if (weapon) {
					prepared.has_weapon = true;
					if (show_weapon) {
						if (weapon_name_cache.size() >= 512)
							weapon_name_cache.clear();
						auto& cached_weapon_name = weapon_name_cache[weapon];
						if (cached_weapon_name.empty())
							cached_weapon_name = read_weapon_name(weapon);
						prepared.weapon = cached_weapon_name;
					}
					if (show_ammo)
						prepared.ammo_count = read<int>(weapon + AMMO_COUNT);
				}
			}
		}

		// Use one fresh draw-frame camera for every projection in this frame.
		camera::update();
		const float screen_aspect_ratio = static_cast<float>(numerics::screen_width) /
			static_cast<float>(numerics::screen_height);
		w2s::inv_fov         = tanf(camera::fov * static_cast<float>(std::numbers::pi) / 360.0f) / screen_aspect_ratio;
		w2s::rotation_matrix = matrix_ops::rotation_matrix(camera::rotation);


		for (auto& prepared : prepared_actors) {
			const auto& actor = prepared.actor;
			auto& actor_bones = prepared.bone_data;
			const auto root = actor_bones.get(0);


			float distance_meters = (float)((root - camera::location).length() * 0.01);
			if (distance_meters > max_distance) continue;

			auto root_screen = w2s::project(root);
			if (draw_esp && fov_triangles && (!w2s::on_screen(root_screen) || getCrossDistance(root_screen.x, root_screen.y,
						numerics::screen_width_center, numerics::screen_height_center) > aimbot::fov)) {
				draw_fov_triangle(draw_list, root, static_cast<float>(aimbot::fov), distance_meters, arrows_colors.get(prepared.visible));
			}

			if (!w2s::on_screen(root_screen)) continue;

			if (aimbot::enabled) {
				const auto head = actor_bones.get(110);
				const auto head_screen = w2s::project(head);
				if (w2s::on_screen(head_screen)) {
					const float crosshair_distance = static_cast<float>(getCrossDistance(
						head_screen.x, head_screen.y,
						numerics::screen_width_center, numerics::screen_height_center));
					if (crosshair_distance <= static_cast<float>(aimbot::fov) &&
						crosshair_distance < udata::closest_distance) {
						udata::closest_distance = crosshair_distance;
						udata::closest_mesh = actor.mesh;
					}
				}
			}

			if (!draw_esp) continue;

			if (tracers) {
				const ImVec2 start{
					static_cast<float>(numerics::screen_width_center),
					static_cast<float>(numerics::screen_height)
				};
				const ImVec2 end{ static_cast<float>(root_screen.x), static_cast<float>(root_screen.y) };
				if (tracers_outline)
					draw_list->AddLine(start, end, IM_COL32(0, 0, 0, 220), tracers_thickness + 2.0f);
				draw_list->AddLine(start, end, tracers_colors.get(prepared.visible), tracers_thickness);
			}

			if (!needs_bounds) continue;

			auto actor_bounds = compute_bounds(actor_bones);
			if (actor_bounds.max_x <= actor_bounds.min_x || actor_bounds.max_y <= actor_bounds.min_y) continue;

			float box_x = (float)actor_bounds.min_x, box_y = (float)actor_bounds.min_y;
			float box_width = (float)(actor_bounds.max_x - actor_bounds.min_x);
			float box_height = (float)(actor_bounds.max_y - actor_bounds.min_y);

            if (box && box_type != 1) {
                if (box_outline)
                    draw_list->AddRect(ImVec2(box_x, box_y), ImVec2(box_x + box_width, box_y + box_height),
                        IM_COL32(0, 0, 0, 230), 0.0f, 0, box_thickness + 2.0f);
                draw_list->AddRect(ImVec2(box_x, box_y), ImVec2(box_x + box_width, box_y + box_height),
                    box_colors.get(prepared.visible), 0.0f, 0, box_thickness);
            }
            if (box && box_type != 0)
                draw_corner_box(draw_list, box_x, box_y, box_width, box_height, corner_box_colors.get(prepared.visible), corner_box_outline);

			if (skeleton) {
				for (const auto& link : skeleton_links) {
					auto first_bone_world = actor_bones.get(link.first);
					auto second_bone_world = actor_bones.get(link.second);
					if (first_bone_world.x == 0.0 && first_bone_world.y == 0.0 && first_bone_world.z == 0.0) continue;
					if (second_bone_world.x == 0.0 && second_bone_world.y == 0.0 && second_bone_world.z == 0.0) continue;
					auto first_bone_screen = w2s::project(first_bone_world);
					auto second_bone_screen = w2s::project(second_bone_world);
					if (skeleton_outline) {
						draw_list->AddLine(
							ImVec2((float)first_bone_screen.x, (float)first_bone_screen.y),
							ImVec2((float)second_bone_screen.x, (float)second_bone_screen.y),
							IM_COL32(0, 0, 0, 230), skeleton_thickness + 2.0f);
					}
					draw_list->AddLine(
						ImVec2((float)first_bone_screen.x, (float)first_bone_screen.y),
						ImVec2((float)second_bone_screen.x, (float)second_bone_screen.y),
						skeleton_colors.get(prepared.visible), skeleton_thickness);
				}
			}

			if (!flags_enabled) continue;
			float center_x = box_x + box_width * 0.5f;

			if (show_name) {
				if (!prepared.name.empty()) {
					float y_above = box_y - text_size;
					draw_text_outlined(draw_list, center_x, y_above, prepared.name.c_str(), flags_colors.get(prepared.visible));
				}
			}

			if (show_distance) {
				char distance_text[32];
				sprintf_s(distance_text, "%.0fm", distance_meters);
				draw_text_outlined(draw_list, center_x, box_y + box_height + 1.0f, distance_text, flags_colors.get(prepared.visible));
			}

			float y_below = box_y + box_height + 1.0f;
			if (show_distance)
				y_below += text_size;

			if (show_platform && !prepared.platform.empty()) {
				draw_text_outlined(draw_list, center_x, y_below, prepared.platform.c_str(), flags_colors.get(prepared.visible));
				y_below += text_size;
			}

			if (show_rank && !prepared.rank.empty()) {
				draw_text_outlined(draw_list, center_x, y_below, prepared.rank.c_str(), flags_colors.get(prepared.visible));
				y_below += text_size;
			}

			if (show_weapon_info && prepared.has_weapon) {
                const std::string weapon_text = show_weapon ? prepared.weapon : std::string{};
                char ammo_text[16] = {};
                if (show_ammo)
                    sprintf_s(ammo_text, "(%d)", std::clamp(prepared.ammo_count, 0, 999));
                ImFont* font = g_esp_font ? g_esp_font : g_font;
                const float weapon_width = font->CalcTextSizeA(text_size, FLT_MAX, 0, weapon_text.c_str()).x;
                const float ammo_width = font->CalcTextSizeA(text_size, FLT_MAX, 0, ammo_text).x;
                const float gap = !weapon_text.empty() && show_ammo
                    ? font->CalcTextSizeA(text_size, FLT_MAX, 0, " ").x : 0.0f;
                const float left = center_x - (weapon_width + gap + ammo_width) * 0.5f;
                if (!weapon_text.empty())
                    draw_text_outlined(draw_list, left + weapon_width * 0.5f, y_below,
                        weapon_text.c_str(), flags_colors.get(prepared.visible));
                if (show_ammo)
                    draw_text_outlined(draw_list, left + weapon_width + gap + ammo_width * 0.5f, y_below,
                        ammo_text, flags_colors.get(prepared.visible));
			}
		}

	}

}
