#pragma once
#include <include.h>
#include <src/cache/index.h>

uintptr_t Memoryaim = ROTATION_INPUT; // PlayerController.RotationInput, confirmed 0x4B0 in the 42.40 dump

inline bool is_visible(uintptr_t mesh)
{
	if (!mesh) return false;

	uintptr_t world = 0;
	{
		std::lock_guard<std::mutex> lock(udata::world_mutex);
		world = udata::world_t.gworld;
	}
	if (!world) return false;

	const double current_seconds = read<double>(world + SECONDS);
	// [rt] PrimitiveComponent.LastRenderTime for CL-59343437 (was 0x290 pre-42.40)
	const float last_render_time = read<float>(mesh + LAST_RENDER_TIME);
	return current_seconds - last_render_time <= 0.06f;
}
bool memory_event(numerics::fvector newpos)
{
	uintptr_t player_controller = 0;
	{
		std::lock_guard<std::mutex> lock(udata::world_mutex);
		player_controller = udata::world_t.player_controller;
	}
	if (!player_controller || player_controller < 0x10000) {
		return false;
	}

	write<numerics::fvector>(player_controller + Memoryaim, newpos); //write Vectors to control rotation
	return true;
}

void memoryaim(uintptr_t target_mesh)
{
	if (!target_mesh) return;

	int smoothness = aimbot::smoothness * 5;
	auto snap = bones::get_snapshot(target_mesh);
	numerics::fvector head3d = bones::bone_from_snapshot(snap, aimbot::bone);
	numerics::fvector2d head2d = w2s::project(head3d);

	if (head2d.x == 0 || head2d.y == 0) return;

	numerics::fvector2d target{};
	
	target.x = (head2d.x - numerics::screen_width_center) / smoothness;
	target.y = (head2d.y - numerics::screen_height_center) / smoothness;

	numerics::fvector new_rotation = { -target.y / 5, target.x / 5, 0 };
	memory_event(new_rotation);
}

void Aimbot(ImDrawList* dl)
{
	if (!aimbot::enabled) return;
	if (aimbot::show_fov)
	{
		float radius = static_cast<float>(aimbot::fov);
		ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
		if (aimbot::fov_outline)
            dl->AddCircle(center, radius, IM_COL32(0, 0, 0, 230), 0, aimbot::fov_thickness + 1.0f);
        dl->AddCircle(center, radius, ImGui::ColorConvertFloat4ToU32(aimbot::fov_color), 0, aimbot::fov_thickness);
	}

	bool hotkey = (GetAsyncKeyState(aimbot::aimbot_keybind) & 0x8000) != 0;
	if (!hotkey) return;

	if (!udata::closest_mesh) return;
	if (aimbot::visibility_check && !is_visible(udata::closest_mesh)) return;

	memoryaim(udata::closest_mesh);
}
