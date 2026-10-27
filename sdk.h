#pragma once
#include <algorithm>
#include <d3d9types.h>
#include <mutex>
#include <numbers>
#include <unordered_map>
#include <vector>
#include <windows.h>
#include <src/memory/driver.h>
#include "offsets.h"

namespace numerics {

	inline int   screen_width         = GetSystemMetrics(0);
	inline int   screen_height        = GetSystemMetrics(1);
	inline int   screen_width_center  = screen_width  / 2;
	inline int   screen_height_center = screen_height / 2;
	inline float aspect_ratio         = (float)screen_width / screen_height;

	class fvector {
	public:
		double x = 0.0, y = 0.0, z = 0.0;

		fvector() = default;
		fvector(double a, double b, double c) : x(a), y(b), z(c) {}

		fvector operator+(const fvector& v) const { return { x + v.x, y + v.y, z + v.z }; }
		fvector operator-(const fvector& v) const { return { x - v.x, y - v.y, z - v.z }; }
		fvector operator*(double s)          const { return { x * s, y * s, z * s }; }
		fvector operator/(double s)          const { return { x / s, y / s, z / s }; }

		double dot     (const fvector& v) const { return x * v.x + y * v.y + z * v.z; }
		double length  ()                 const { return std::sqrt(x * x + y * y + z * z); }
		double distance(const fvector& v) const { return (*this - v).length(); }
	};

	class fvector2d {
	public:
		double x = 0.0, y = 0.0;

		fvector2d() = default;
		fvector2d(double a, double b) : x(a), y(b) {}
	};

	class frotator {
	public:
		double pitch = 0.0, yaw = 0.0, roll = 0.0;

		frotator() = default;
		frotator(double p, double y, double r) : pitch(p), yaw(y), roll(r) {}
	};

	struct fplane : fvector { double w = 0.0; };

	struct alignas(16) matrix_elements {
		double m11, m12, m13, m14;
		double m21, m22, m23, m24;
		double m31, m32, m33, m34;
		double m41, m42, m43, m44;
		matrix_elements()
			: m11(0), m12(0), m13(0), m14(0),
			  m21(0), m22(0), m23(0), m24(0),
			  m31(0), m32(0), m33(0), m34(0),
			  m41(0), m42(0), m43(0), m44(0) {}
	};

	struct alignas(16) dbl_matrix {
		union {
			matrix_elements elements;
			double m[4][4];
		};
		dbl_matrix() : elements() {}
	};

	struct alignas(16) fmatrix : public dbl_matrix {
		fplane x_plane;
		fplane y_plane;
		fplane z_plane;
		fplane w_plane;
		fmatrix() : dbl_matrix(), x_plane(), y_plane(), z_plane(), w_plane() {}
	};

	template<class T>
	class tarray {
	public:
		T*       data      = nullptr;
		uint32_t count     = 0;
		uint32_t max_count = 0;

		tarray() = default;

		T get(uintptr_t idx) {
			return read<T>((uintptr_t)data + idx * sizeof(T));
		}

		std::vector<T> get_all() {
			if (count <= 0 || count > max_count) return {};
			try {
				std::vector<T> buf(count);
				read2((uintptr_t)data, buf.data(), sizeof(T) * count);
				return buf;
			} catch (const std::bad_alloc&) {
				return {};
			}
		}
	};

}

namespace matrix_ops {

	inline D3DMATRIX multiply(const D3DMATRIX& a, const D3DMATRIX& b) {
		D3DMATRIX r{};
		r._11 = a._11*b._11 + a._12*b._21 + a._13*b._31 + a._14*b._41;
		r._12 = a._11*b._12 + a._12*b._22 + a._13*b._32 + a._14*b._42;
		r._13 = a._11*b._13 + a._12*b._23 + a._13*b._33 + a._14*b._43;
		r._14 = a._11*b._14 + a._12*b._24 + a._13*b._34 + a._14*b._44;

		r._21 = a._21*b._11 + a._22*b._21 + a._23*b._31 + a._24*b._41;
		r._22 = a._21*b._12 + a._22*b._22 + a._23*b._32 + a._24*b._42;
		r._23 = a._21*b._13 + a._22*b._23 + a._23*b._33 + a._24*b._43;
		r._24 = a._21*b._14 + a._22*b._24 + a._23*b._34 + a._24*b._44;

		r._31 = a._31*b._11 + a._32*b._21 + a._33*b._31 + a._34*b._41;
		r._32 = a._31*b._12 + a._32*b._22 + a._33*b._32 + a._34*b._42;
		r._33 = a._31*b._13 + a._32*b._23 + a._33*b._33 + a._34*b._43;
		r._34 = a._31*b._14 + a._32*b._24 + a._33*b._34 + a._34*b._44;

		r._41 = a._41*b._11 + a._42*b._21 + a._43*b._31 + a._44*b._41;
		r._42 = a._41*b._12 + a._42*b._22 + a._43*b._32 + a._44*b._42;
		r._43 = a._41*b._13 + a._42*b._23 + a._43*b._33 + a._44*b._43;
		r._44 = a._41*b._14 + a._42*b._24 + a._43*b._34 + a._44*b._44;
		return r;
	}

	inline D3DMATRIX rotation_matrix(const numerics::frotator& rot) {
		float rx = (float)(rot.pitch * std::numbers::pi / 180.0);
		float ry = (float)(rot.yaw   * std::numbers::pi / 180.0);
		float rz = (float)(rot.roll  * std::numbers::pi / 180.0);

		float sp = sinf(rx), cp = cosf(rx);
		float sy = sinf(ry), cy = cosf(ry);
		float sr = sinf(rz), cr = cosf(rz);

		D3DMATRIX m{};
		m.m[0][0] = cp * cy;
		m.m[0][1] = cp * sy;
		m.m[0][2] = sp;
		m.m[0][3] = 0.f;

		m.m[1][0] = sr * sp * cy - cr * sy;
		m.m[1][1] = sr * sp * sy + cr * cy;
		m.m[1][2] = -sr * cp;
		m.m[1][3] = 0.f;

		m.m[2][0] = -(cr * sp * cy + sr * sy);
		m.m[2][1] = cy * sr - cr * sp * sy;
		m.m[2][2] = cr * cp;
		m.m[2][3] = 0.f;

		m.m[3][0] = m.m[3][1] = m.m[3][2] = 0.0f;
		m.m[3][3] = 1.0f;
		return m;
	}

	struct ftransform {
		numerics::fplane  rot;
		numerics::fvector translation;
		uint8_t           pad_38[8];
		numerics::fvector scale = { 1.0, 1.0, 1.0 };
		uint8_t           pad_58[8];

		D3DMATRIX to_matrix_with_scale() const {
			D3DMATRIX m{};
			const numerics::fvector adjusted_scale{
				scale.x == 0.0 ? 1.0 : scale.x,
				scale.y == 0.0 ? 1.0 : scale.y,
				scale.z == 0.0 ? 1.0 : scale.z
			};
			m._41 = (float)translation.x;
			m._42 = (float)translation.y;
			m._43 = (float)translation.z;

			auto x2 = rot.x + rot.x, y2 = rot.y + rot.y, z2 = rot.z + rot.z;
			auto xx = rot.x * x2, yy = rot.y * y2, zz = rot.z * z2;
			m._11 = (float)((1.0f - (yy + zz)) * adjusted_scale.x);
			m._22 = (float)((1.0f - (xx + zz)) * adjusted_scale.y);
			m._33 = (float)((1.0f - (xx + yy)) * adjusted_scale.z);

			auto yz = rot.y * z2, wx = rot.w * x2;
			m._32 = (float)((yz - wx) * adjusted_scale.z);
			m._23 = (float)((yz + wx) * adjusted_scale.y);

			auto xy = rot.x * y2, wz = rot.w * z2;
			m._21 = (float)((xy - wz) * adjusted_scale.y);
			m._12 = (float)((xy + wz) * adjusted_scale.x);

			auto xz = rot.x * z2, wy = rot.w * y2;
			m._31 = (float)((xz + wy) * adjusted_scale.z);
			m._13 = (float)((xz - wy) * adjusted_scale.x);

			m._14 = m._24 = m._34 = 0.0f;
			m._44 = 1.0f;
			return m;
		}
	};
	static_assert(sizeof(ftransform) == 0x60, "FTransform layout must match the game");

}

namespace bones {

	struct snapshot {
		uintptr_t              bone_array = 0;
		matrix_ops::ftransform c2w{};
		bool                   valid = false;
	};

	inline snapshot get_snapshot(uintptr_t mesh) {
		snapshot snap{};
		if (!mesh) return snap;

		// CachedComponentSpaceTransforms is a plain TArray<FTransform>: the first
		// member is the data pointer. The old "+0x48 / +0x10" indirection belonged
		// to a ComponentSpaceTransformsArray layout this build no longer has.
		snap.bone_array = read<uintptr_t>(mesh + BONE_ARRAY);
		if (!snap.bone_array) {
			snap.bone_array = read<uintptr_t>(mesh + BONE_ARRAY_ALT);
		}
		if (!snap.bone_array) return snap;

		snap.c2w = read<matrix_ops::ftransform>(mesh + COMPONENT_TO_WORLD);
		snap.valid = true;
		return snap;
	}

	inline numerics::fvector bone_from_snapshot(const snapshot& s, int bone_id) {
		if (!s.valid || !s.bone_array || bone_id < 0)
			return { 0.0, 0.0, 0.0 };
		const auto b = read<matrix_ops::ftransform>(s.bone_array +
			(static_cast<uintptr_t>(bone_id) * 0x60));
		auto mat = matrix_ops::multiply(b.to_matrix_with_scale(), s.c2w.to_matrix_with_scale());
		return { mat._41, mat._42, mat._43 };
	}

}

namespace udata {

	struct world {
		uintptr_t gworld;
		uintptr_t owning_game_instance;
		uintptr_t local_player;
		uintptr_t player_controller;
		uintptr_t player_state;
		uintptr_t local_pawn;
		int       team_id;
		uintptr_t game_state;
		std::vector<u64> player_array;
	};

	struct actor {
		uintptr_t player_state;
		uintptr_t pawn_private;
		uintptr_t mesh;
		int       team_id;
		numerics::fvector location;
	};

	inline uintptr_t closest_mesh;
	inline float closest_distance;

	inline world world_t;
	inline std::mutex world_mutex;
	inline std::vector<actor> actor_t;
	inline std::mutex actor_mutex;
}

struct FNRot
{
	double a;
	char pad_0008[24];
	double b;
	char pad_0028[424];
	double c;
};

// One diagnostic line per second next to the executable. Tells you at a glance
// which link of the world/camera/bone chain is dead without attaching a debugger.
namespace diag {

	inline std::atomic<unsigned long long> g_last_log{ 0 };

	inline bool due() {
		const unsigned long long now = GetTickCount64();
		if (now - g_last_log.load() < 1000) return false;
		g_last_log = now;
		return true;
	}

	inline void log(const std::string& text) {
		FILE* file = nullptr;
		if (fopen_s(&file, "sdk_diag.log", "ab") != 0 || !file) return;
		const std::string line = text + "\r\n";
		fwrite(line.data(), 1, line.size(), file);
		fclose(file);
	}
}

namespace camera {
	struct view_point {
		numerics::fvector  location{};
		numerics::frotator rotation{};
		float               fov = 0.0f;
	};

	inline numerics::fvector location{};
	inline numerics::frotator rotation{};
	inline float             fov = 0.0f;

	inline view_point get_view_point() {
		view_point result{};
		uintptr_t player_controller = 0;
		{
			std::lock_guard<std::mutex> lock(udata::world_mutex);
			player_controller = udata::world_t.player_controller;
		}
		if (!player_controller) return result;

		// APlayerController -> APlayerCameraManager -> CameraCachePrivate.POV.
		// During load/spectate transitions CameraCachePrivate can hold a stale or
		// zeroed entry (diag log 2026-10-24: in-match fov fell back to 80 while the
		// same chain read fine in the lobby), so retry the last-frame cache before
		// assuming a default.
		const uintptr_t camera_manager =
			read<uintptr_t>(player_controller + PLAYER_CAMERA_MANAGER);
		if (!camera_manager) return result;

		auto read_pov = [&camera_manager](std::uint64_t cache_offset) {
			view_point out{};
			const uintptr_t pov = camera_manager + cache_offset + CAMERA_POV;
			out.location = read<numerics::fvector>(pov + POV_LOCATION);
			out.rotation = read<numerics::frotator>(pov + POV_ROTATION);
			out.fov      = read<float>(pov + POV_FOV);
			return out;
		};

		const auto pov_is_sane = [](const view_point& v) {
			return v.fov > 1.0f && v.fov < 170.0f && std::isfinite(v.fov);
		};

		result = read_pov(CAMERA_CACHE);
		if (!pov_is_sane(result))
			result = read_pov(LAST_FRAME_CAMERA_CACHE);

		// a dead camera chain yields fov 0 / nan; the old code fell back to ~80
		if (!pov_is_sane(result))
			result.fov = 80.0f;

		return result;
	}

	inline void update() {
		const view_point current = get_view_point();
		location = current.location;
		rotation = current.rotation;
		fov      = current.fov;

		if (!diag::due()) return;

		uintptr_t world_pointer = 0, game_instance = 0, controller = 0, local_pawn = 0;
		size_t players = 0;
		{
			std::lock_guard<std::mutex> lock(udata::world_mutex);
			world_pointer = udata::world_t.gworld;
			game_instance = udata::world_t.owning_game_instance;
			controller    = udata::world_t.player_controller;
			local_pawn    = udata::world_t.local_pawn;
			players       = udata::world_t.player_array.size();
		}

		uintptr_t mesh = 0;
		{
			std::lock_guard<std::mutex> lock(udata::actor_mutex);
			if (!udata::actor_t.empty()) mesh = udata::actor_t.front().mesh;
		}

		uintptr_t bone_array = 0, bone_probe_a00 = 0, bone_probe_a10 = 0;
		numerics::fvector c2w_translation{}, root_bone{};
		if (mesh) {
			const bones::snapshot snapshot = bones::get_snapshot(mesh);
			bone_array       = snapshot.bone_array;
			c2w_translation  = snapshot.c2w.translation;
			root_bone        = bones::bone_from_snapshot(snapshot, 0);
			// raw soundings: which of the two transform arrays is actually
			// populated on this mesh, independent of the fallback logic above
			bone_probe_a00 = read<uintptr_t>(mesh + 0xA00);
			bone_probe_a10 = read<uintptr_t>(mesh + 0xA10);
		}

		// object-integrity + raw memory soundings. vtable/class must both land
		// inside the game image; the qword windows show whether a TArray
		// (ptr, count) pair or the FTransform actually lives near the offsets
		// we read, or whether reads through this object fail entirely.
		const uintptr_t base_addr   = base;
		const uintptr_t pawn_vtable = local_pawn ? read<uintptr_t>(local_pawn) : 0;
		const uintptr_t pawn_class  = local_pawn ? read<uintptr_t>(local_pawn + 0x10) : 0;
		const uintptr_t mesh_vtable = mesh ? read<uintptr_t>(mesh) : 0;
		const uintptr_t mesh_class  = mesh ? read<uintptr_t>(mesh + 0x10) : 0;
		const uintptr_t root_comp   = local_pawn ? read<uintptr_t>(local_pawn + ROOT_COMPONENT) : 0;
		const numerics::fvector root_rel = root_comp
			? read<numerics::fvector>(root_comp + RELATIVE_LOCATION) : numerics::fvector{};

		const uintptr_t camera_manager_probe = controller
			? read<uintptr_t>(controller + PLAYER_CAMERA_MANAGER) : 0;
		const uintptr_t cm_vtable = camera_manager_probe ? read<uintptr_t>(camera_manager_probe) : 0;
		const uintptr_t cm_cache_0 = camera_manager_probe ? read<uintptr_t>(camera_manager_probe + CAMERA_CACHE) : 0;
		const uintptr_t cm_cache_8 = camera_manager_probe ? read<uintptr_t>(camera_manager_probe + CAMERA_CACHE + 8) : 0;

		const uintptr_t mesh_window[9] = {
			mesh ? read<uintptr_t>(mesh + 0x9F0) : 0,
			mesh ? read<uintptr_t>(mesh + 0x9F8) : 0,
			mesh ? read<uintptr_t>(mesh + 0xA00) : 0,
			mesh ? read<uintptr_t>(mesh + 0xA08) : 0,
			mesh ? read<uintptr_t>(mesh + 0xA10) : 0,
			mesh ? read<uintptr_t>(mesh + 0xA18) : 0,
			mesh ? read<uintptr_t>(mesh + 0xA20) : 0,
			mesh ? read<uintptr_t>(mesh + 0xA28) : 0,
			mesh ? read<uintptr_t>(mesh + 0xA30) : 0,
		};
		const uintptr_t mesh_c2w_window[4] = {
			mesh ? read<uintptr_t>(mesh + 0x1D0) : 0,
			mesh ? read<uintptr_t>(mesh + 0x1D8) : 0,
			mesh ? read<uintptr_t>(mesh + 0x1E0) : 0,
			mesh ? read<uintptr_t>(mesh + 0x1E8) : 0,
		};

		char buffer[1600]{};
		std::snprintf(buffer, sizeof(buffer),
			"world=0x%llX gi=0x%llX pc=0x%llX pawn=0x%llX players=%zu | "
			"cam=(%.0f %.0f %.0f) rot=(%.1f %.1f %.1f) fov=%.1f | "
			"mesh=0x%llX bones=0x%llX c2w=(%.0f %.0f %.0f) root0=(%.0f %.0f %.0f) | "
			"a00=0x%llX a10=0x%llX | "
			"base=0x%llX pvt=0x%llX pcls=0x%llX mvt=0x%llX mcls=0x%llX | "
			"rc=0x%llX rrel=(%.0f %.0f %.0f) | "
			"cm=0x%llX cmvt=0x%llX cmc0=0x%llX cmc8=0x%llX | "
			"mw=%llX %llX %llX %llX %llX %llX %llX %llX %llX | "
			"cw=%llX %llX %llX %llX",
			(unsigned long long)world_pointer, (unsigned long long)game_instance,
			(unsigned long long)controller, (unsigned long long)local_pawn, players,
			location.x, location.y, location.z,
			rotation.pitch, rotation.yaw, rotation.roll, fov,
			(unsigned long long)mesh, (unsigned long long)bone_array,
			c2w_translation.x, c2w_translation.y, c2w_translation.z,
			root_bone.x, root_bone.y, root_bone.z,
			(unsigned long long)bone_probe_a00, (unsigned long long)bone_probe_a10,
			(unsigned long long)base_addr,
			(unsigned long long)pawn_vtable, (unsigned long long)pawn_class,
			(unsigned long long)mesh_vtable, (unsigned long long)mesh_class,
			(unsigned long long)root_comp, root_rel.x, root_rel.y, root_rel.z,
			(unsigned long long)camera_manager_probe, (unsigned long long)cm_vtable,
			(unsigned long long)cm_cache_0, (unsigned long long)cm_cache_8,
			(unsigned long long)mesh_window[0], (unsigned long long)mesh_window[1],
			(unsigned long long)mesh_window[2], (unsigned long long)mesh_window[3],
			(unsigned long long)mesh_window[4], (unsigned long long)mesh_window[5],
			(unsigned long long)mesh_window[6], (unsigned long long)mesh_window[7],
			(unsigned long long)mesh_window[8],
			(unsigned long long)mesh_c2w_window[0], (unsigned long long)mesh_c2w_window[1],
			(unsigned long long)mesh_c2w_window[2], (unsigned long long)mesh_c2w_window[3]);
		diag::log(buffer);
	}

}

namespace w2s {

	inline D3DMATRIX rotation_matrix;
	inline float     inv_fov = 0.0f;

	inline numerics::fvector2d project(const numerics::fvector& loc) {
		auto& ax = rotation_matrix.m[0];
		auto& ay = rotation_matrix.m[1];
		auto& az = rotation_matrix.m[2];

		auto d = loc - camera::location;

		auto tx = d.dot(numerics::fvector(ay[0], ay[1], ay[2]));
		auto ty = d.dot(numerics::fvector(az[0], az[1], az[2]));
		auto tz = max(d.dot(numerics::fvector(ax[0], ax[1], ax[2])), 1.0);

		double sx = numerics::screen_width_center  + tx * (numerics::screen_height_center / inv_fov) / tz;
		double sy = numerics::screen_height_center - ty * (numerics::screen_height_center / inv_fov) / tz;
		return numerics::fvector2d(sx, sy);
	}

	inline bool on_screen(const numerics::fvector2d& p) {
		return p.x >= 0 && p.x <= numerics::screen_width &&
		       p.y >= 0 && p.y <= numerics::screen_height;
	}

}

inline double getCrossDistance(double x1, double y1, double x2, double y2)
{
	return std::hypot(x2 - x1, y2 - y1);
}
