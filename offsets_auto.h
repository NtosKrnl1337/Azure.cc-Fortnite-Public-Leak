#pragma once
// Offset auto-update for Fortnite.
//
// Pulls https://www.cheatoffsets.com/api/games/fortnite/current, parses the
// fenced cpp namespaces out of the markdown body and rewrites the runtime
// offset table in src/engine/offsets.h. The feed is free, keyless and read-only;
// it answers 304 Not Modified when nothing changed, so polling costs nothing.
//
// Boot path:   offsets.txt (last good dump, no network)  ->  apply
// Poll path:   GET /current with If-None-Match           ->  200 -> parse+apply
//                                                             304 -> keep table
//
// The dump is cached as offsets.txt beside the executable and the ETag as
// offsets.etag, so a cold start without internet keeps the last known build.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>
#include <winhttp.h>

#include <src/engine/offsets.h>

#pragma comment(lib, "winhttp.lib")

namespace offsets_auto {

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------
	inline constexpr wchar_t     k_host[]         = L"www.cheatoffsets.com";
	inline constexpr wchar_t     k_path[]         = L"/api/games/fortnite/current";
	inline constexpr wchar_t     k_cache_file[]   = L"offsets.txt";
	inline constexpr wchar_t     k_etag_file[]    = L"offsets.etag";
	inline constexpr wchar_t     k_on_file[]      = L"offsets_auto.on";
	inline constexpr wchar_t     k_off_file[]     = L"offsets_auto.off";
	inline constexpr const char* k_log_file       = "offsets_update.log";
	inline constexpr int         k_timeout_ms     = 8000;
	inline constexpr DWORD       k_poll_interval  = 10u * 60u * 1000u;  // feed docs: 5-15 min

	// ------------------------------------------------------------------
	//  State
	// ------------------------------------------------------------------
	inline std::atomic<bool>  g_started{ false };
	inline std::mutex         g_mutex;
	inline std::string        g_etag;          // guarded by g_mutex
	inline std::string        g_version;       // guarded by g_mutex
	inline std::atomic<int>   g_applied{ 0 };  // offsets resolved on the last successful sync
	inline std::atomic<long>  g_last_sync{ 0 }; // unix seconds

	using offset_map = std::map<std::string, std::uint64_t>;

	// ------------------------------------------------------------------
	//  Logging (file + console, never throws)
	// ------------------------------------------------------------------
	inline void log_line(const std::string& text)
	{
		std::string stamp;
		{
			SYSTEMTIME st{};
			GetLocalTime(&st);
			char buf[64]{};
			std::snprintf(buf, sizeof(buf), "[%04u-%02u-%02u %02u:%02u:%02u] ",
				st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
			stamp = buf;
		}

		if (FILE* f = nullptr; fopen_s(&f, k_log_file, "ab") == 0 && f) {
			std::string line = stamp + text + "\r\n";
			fwrite(line.data(), 1, line.size(), f);
			fclose(f);
		}
		if (GetConsoleWindow())
			std::printf("%s%s\n", stamp.c_str(), text.c_str());
	}

	// ------------------------------------------------------------------
	//  Small helpers
	// ------------------------------------------------------------------
	inline std::wstring widen(const std::string& s)
	{
		if (s.empty()) return {};
		int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
		if (n <= 0) return {};
		std::wstring out((std::size_t)n, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
		return out;
	}

	inline std::string narrow(const std::wstring& s)
	{
		if (s.empty()) return {};
		int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
		if (n <= 0) return {};
		std::string out((std::size_t)n, '\0');
		WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n, nullptr, nullptr);
		return out;
	}

	inline std::string trim(const std::string& s)
	{
		std::size_t b = 0, e = s.size();
		while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
		while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) --e;
		return s.substr(b, e - b);
	}

	inline bool starts_with(const std::string& s, const char* prefix)
	{
		const std::size_t n = std::strlen(prefix);
		return s.size() >= n && std::memcmp(s.data(), prefix, n) == 0;
	}

	// ------------------------------------------------------------------
	//  HTTP (WinHTTP, HTTPS, ETag aware)
	// ------------------------------------------------------------------
	struct http_response {
		int         status = 0;
		std::string body;
		std::string etag;
		std::string last_modified;
		std::string error;
	};

	inline std::string query_header(HINTERNET request, DWORD info_level, const wchar_t* name = nullptr)
	{
		wchar_t buf[512]{};
		DWORD len = sizeof(buf);
		if (!WinHttpQueryHeaders(request, info_level,
			name, buf, &len, WINHTTP_NO_HEADER_INDEX))
			return {};

		// len is in bytes and includes the terminator; drop it and any padding
		std::string value = narrow(std::wstring(buf, len / sizeof(wchar_t)));
		while (!value.empty() &&
			(value.back() == '\0' || value.back() == '\r' ||
			 value.back() == '\n' || value.back() == ' '))
			value.pop_back();
		return value;
	}

	inline http_response http_get(const std::string& if_none_match)
	{
		http_response out;

		HINTERNET session = WinHttpOpen(L"azure-offsets/1.0",
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS, 0);
		if (!session) { out.error = "WinHttpOpen failed"; return out; }

		WinHttpSetTimeouts(session, k_timeout_ms, k_timeout_ms, k_timeout_ms, k_timeout_ms);

		HINTERNET connect = WinHttpConnect(session, k_host, INTERNET_DEFAULT_HTTPS_PORT, 0);
		if (!connect) {
			out.error = "WinHttpConnect failed";
			WinHttpCloseHandle(session);
			return out;
		}

		HINTERNET request = WinHttpOpenRequest(connect, L"GET", k_path, nullptr,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
		if (!request) {
			out.error = "WinHttpOpenRequest failed";
			WinHttpCloseHandle(connect);
			WinHttpCloseHandle(session);
			return out;
		}

		std::wstring headers;
		if (!if_none_match.empty())
			headers = L"If-None-Match: " + widen(if_none_match) + L"\r\n";

		const BOOL sent = WinHttpSendRequest(request,
			headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
			headers.empty() ? 0 : (DWORD)-1L,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

		if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
			out.error = "request failed, winhttp error " + std::to_string(GetLastError());
			WinHttpCloseHandle(request);
			WinHttpCloseHandle(connect);
			WinHttpCloseHandle(session);
			return out;
		}

		DWORD status = 0, status_len = sizeof(status);
		if (!WinHttpQueryHeaders(request,
			WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_len, WINHTTP_NO_HEADER_INDEX))
			status = 0;
		out.status = (int)status;

		out.etag          = query_header(request, WINHTTP_QUERY_CUSTOM, L"ETag");
		out.last_modified = query_header(request, WINHTTP_QUERY_CUSTOM, L"Last-Modified");

		if (out.status == 200) {
			DWORD available = 0;
			while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
				std::string chunk(available, '\0');
				DWORD read = 0;
				if (!WinHttpReadData(request, chunk.data(), available, &read) || read == 0)
					break;
				out.body.append(chunk, 0, read);
			}
		}

		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return out;
	}

	// ------------------------------------------------------------------
	//  Minimal JSON string reader (only what the feed needs)
	// ------------------------------------------------------------------
	inline void append_utf8(std::string& out, unsigned int cp)
	{
		if (cp < 0x80) out.push_back((char)cp);
		else if (cp < 0x800) {
			out.push_back((char)(0xC0 | (cp >> 6)));
			out.push_back((char)(0x80 | (cp & 0x3F)));
		}
		else if (cp < 0x10000) {
			out.push_back((char)(0xE0 | (cp >> 12)));
			out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
			out.push_back((char)(0x80 | (cp & 0x3F)));
		}
		else {
			out.push_back((char)(0xF0 | (cp >> 18)));
			out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
			out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
			out.push_back((char)(0x80 | (cp & 0x3F)));
		}
	}

	// Extracts and unescapes a top-level string field: "field":"..."
	inline bool json_string(const std::string& json, const std::string& field, std::string& out)
	{
		const std::string needle = "\"" + field + "\"";
		std::size_t pos = json.find(needle);
		if (pos == std::string::npos) return false;

		pos = json.find(':', pos + needle.size());
		if (pos == std::string::npos) return false;

		// the value has to be a string, otherwise bail instead of walking into
		// the next key
		++pos;
		while (pos < json.size() &&
			(json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n'))
			++pos;
		if (pos >= json.size() || json[pos] != '"') return false;
		++pos;

		out.clear();
		out.reserve(json.size() / 4);

		for (std::size_t i = pos; i < json.size(); ++i) {
			const char c = json[i];
			if (c == '"') return true;
			if (c != '\\') { out.push_back(c); continue; }

			if (++i >= json.size()) break;
			switch (json[i]) {
			case '"':  out.push_back('"');  break;
			case '\\': out.push_back('\\'); break;
			case '/':  out.push_back('/');  break;
			case 'b':  out.push_back('\b'); break;
			case 'f':  out.push_back('\f'); break;
			case 'n':  out.push_back('\n'); break;
			case 'r':  out.push_back('\r'); break;
			case 't':  out.push_back('\t'); break;
			case 'u': {
				if (i + 4 >= json.size()) return true;
				unsigned int cp = (unsigned int)std::strtoul(json.substr(i + 1, 4).c_str(), nullptr, 16);
				i += 4;
				if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 < json.size() &&
					json[i + 1] == '\\' && json[i + 2] == 'u') {
					const unsigned int low =
						(unsigned int)std::strtoul(json.substr(i + 3, 4).c_str(), nullptr, 16);
					if (low >= 0xDC00 && low <= 0xDFFF) {
						cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
						i += 6;
					}
				}
				append_utf8(out, cp);
				break;
			}
			default: out.push_back(json[i]); break;
			}
		}
		return false; // unterminated
	}

	// ------------------------------------------------------------------
	//  Markdown parser:  namespace X {  inline uint64_t Field = 0x..;  }
	// ------------------------------------------------------------------
	inline void parse_dump(const std::string& markdown, offset_map& out)
	{
		std::string current;

		std::size_t pos = 0;
		while (pos <= markdown.size()) {
			std::size_t end = markdown.find('\n', pos);
			if (end == std::string::npos) end = markdown.size();

			const std::string line = trim(markdown.substr(pos, end - pos));
			pos = end + 1;

			if (line.empty()) continue;

			if (starts_with(line, "namespace ")) {
				const std::size_t brace = line.find('{');
				if (brace != std::string::npos) {
					std::string name = trim(line.substr(10, brace - 10));
					const std::size_t space = name.find_first_of(" \t");
					if (space != std::string::npos) name.resize(space);
					current = name;
				}
				continue;
			}

			if (line[0] == '}') {
				current.clear();
				continue;
			}

			if (current.empty() || !starts_with(line, "inline ")) continue;

			const std::size_t kw = line.find("uint64_t");
			if (kw == std::string::npos) continue;

			const std::size_t eq = line.find('=', kw);
			if (eq == std::string::npos) continue;

			std::string field = trim(line.substr(kw + 8, eq - kw - 8));
			if (field.empty()) continue;

			const std::size_t semi = line.find(';', eq);
			std::string value = trim(line.substr(eq + 1, (semi == std::string::npos ? line.size() : semi) - eq - 1));
			if (value.size() < 3 || (value[0] != '0') ||
				(value[1] != 'x' && value[1] != 'X')) continue;

			out[current + "." + field] =
				(std::uint64_t)std::strtoull(value.c_str() + 2, nullptr, 16);
		}
	}

	// ------------------------------------------------------------------
	//  Feed key  ->  runtime offset slot
	// ------------------------------------------------------------------
	struct mapping {
		const char*           key;
		std::uint64_t*        slot;
	};

	inline const std::vector<mapping>& table()
	{
		static const std::vector<mapping> rows = {
			{ "Engine.GameViewport",                   &VIEWPORT           },
			{ "GameViewportClient.World",              &VIEWPORT_WORLD     },
			{ "World.OwningGameInstance",              &GAME_INSTANCE      },
			{ "World.GameState",                       &GAME_STATE         },
			{ "GameInstance.LocalPlayers",             &LOCAL_PLAYERS      },
			{ "Controller.PlayerState",                &PLAYER_STATE       },
			{ "PlayerController.AcknowledgedPawn",     &LOCAL_PAWN         },
			{ "PlayerController.RotationInput",        &ROTATION_INPUT     },
			{ "GameStateBase.PlayerArray",             &PLAYER_ARRAY       },
			{ "PlayerController.NetConnection",        &NETCONNECTION      },
			{ "PlayerState.PawnPrivate",               &PAWN_PRIVATE       },
			{ "PlayerState.PlayerNamePrivate",         &PLAYERNAME         },
			{ "FortPlayerStateAthena.TeamIndex",       &TEAM_INDEX         },
			{ "FortPlayerStateAthena.KillScore",       &KILL_SCORE         },
			{ "FortPlayerState.HabaneroComponent",     &HABANERO_COMPONENT },
			{ "FortPlayerState.Platform",              &PLATFORM           },
			{ "Character.Mesh",                        &MESH               },
			{ "FortPawn.CurrentWeapon",                &CURRENT_WEAPON     },
			{ "FortPawn.HealthSet",                    &HEALTH_SET         },
			{ "FortWeapon.WeaponData",                 &WEAPON_DATA        },
			{ "FortWeapon.AmmoCount",                  &AMMO_COUNT         },
			{ "Actor.RootComponent",                   &ROOT_COMPONENT     },
			{ "SceneComponent.RelativeLocation",       &RELATIVE_LOCATION  },
			{ "PrimitiveComponent.LastRenderTime",     &LAST_RENDER_TIME   },
			{ "PlayerController.PlayerCameraManager",  &PLAYER_CAMERA_MANAGER },
			{ "PlayerCameraManager.CameraCachePrivate", &CAMERA_CACHE      },
			{ "SkeletalMeshComponent.CachedComponentSpaceTransforms", &BONE_ARRAY },
			{ "SkeletalMeshComponent.CachedBoneSpaceTransforms",      &BONE_ARRAY_ALT },
		};
		return rows;
	}

	// Applies whatever the dump provides. Missing keys keep their current value,
	// so a truncated or partial feed never blanks the table.
	inline int apply_dump(const offset_map& dump, std::vector<std::string>* changed = nullptr)
	{
		int applied = 0;

		for (const mapping& row : table()) {
			const auto it = dump.find(row.key);
			if (it == dump.end() || it->second == 0) continue;

			++applied;
			if (*row.slot == it->second) continue;

			if (changed) {
				char buf[192]{};
				std::snprintf(buf, sizeof(buf), "%s: 0x%llX -> 0x%llX",
					row.key,
					(unsigned long long)*row.slot,
					(unsigned long long)it->second);
				changed->emplace_back(buf);
			}
			*row.slot = it->second;
		}

		g_applied = applied;
		return applied;
	}

	// ------------------------------------------------------------------
	//  Disk cache
	// ------------------------------------------------------------------
	inline bool read_file(const wchar_t* path, std::string& out)
	{
		FILE* f = nullptr;
		if (_wfopen_s(&f, path, L"rb") != 0 || !f) return false;

		out.clear();
		char buf[65536];
		std::size_t got = 0;
		while ((got = fread(buf, 1, sizeof(buf), f)) > 0)
			out.append(buf, got);
		fclose(f);
		return !out.empty();
	}

	inline bool write_file(const wchar_t* path, const std::string& data)
	{
		FILE* f = nullptr;
		if (_wfopen_s(&f, path, L"wb") != 0 || !f) return false;
		const bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
		fclose(f);
		return ok;
	}

	inline std::string load_etag()
	{
		std::string raw;
		if (!read_file(k_etag_file, raw)) return {};
		return trim(raw);
	}

	inline void save_etag(const std::string& etag)
	{
		write_file(k_etag_file, etag);
	}

	// ------------------------------------------------------------------
	//  Sync
	// ------------------------------------------------------------------
	enum class sync_status { updated, unchanged, failed };

	// Applies the cached dump without touching the network.
	inline bool boot()
	{
		std::string cached;
		if (!read_file(k_cache_file, cached)) {
			log_line("no cached dump found, running on compiled-in offsets");
			return false;
		}

		offset_map dump;
		parse_dump(cached, dump);
		if (dump.empty()) {
			log_line("cached dump parsed empty, keeping compiled-in offsets");
			return false;
		}

		std::vector<std::string> changed;
		const int applied = apply_dump(dump, &changed);

		log_line("boot: " + std::to_string(applied) + " offsets from cache, " +
			std::to_string(changed.size()) + " changed");
		for (const std::string& line : changed) log_line("  " + line);

		{
			std::lock_guard<std::mutex> lock(g_mutex);
			g_etag = load_etag();
		}
		return true;
	}

	inline sync_status sync(bool force)
	{
		std::string etag;
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			etag = g_etag;
		}

		const http_response response = http_get(force ? std::string() : etag);

		if (!response.error.empty()) {
			log_line("sync failed: " + response.error);
			return sync_status::failed;
		}

		if (response.status == 304) {
			g_last_sync = (long)std::time(nullptr);
			return sync_status::unchanged;
		}

		if (response.status != 200) {
			log_line("sync failed: http " + std::to_string(response.status));
			return sync_status::failed;
		}

		std::string markdown, version;
		if (!json_string(response.body, "markdown", markdown) || markdown.empty()) {
			log_line("sync failed: no markdown in response");
			return sync_status::failed;
		}
		json_string(response.body, "version", version);

		offset_map dump;
		parse_dump(markdown, dump);
		if (dump.empty()) {
			log_line("sync failed: dump parsed empty");
			return sync_status::failed;
		}

		std::vector<std::string> changed;
		const int applied = apply_dump(dump, &changed);

		write_file(k_cache_file, markdown);
		if (!response.etag.empty()) {
			save_etag(response.etag);
			std::lock_guard<std::mutex> lock(g_mutex);
			g_etag = response.etag;
		}

		{
			std::lock_guard<std::mutex> lock(g_mutex);
			g_version = version;
		}

		g_last_sync = (long)std::time(nullptr);

		log_line("sync ok: build " + (version.empty() ? std::string("?") : version) +
			", " + std::to_string(applied) + " offsets, " +
			std::to_string(changed.size()) + " changed");
		for (const std::string& line : changed) log_line("  " + line);

		return sync_status::updated;
	}

	// ------------------------------------------------------------------
	//  Background poller
	// ------------------------------------------------------------------
	inline void poll_loop()
	{
		sync(true); // first round ignores the cached ETag: pick up anything missed offline

		for (;;) {
			std::this_thread::sleep_for(std::chrono::milliseconds(k_poll_interval));
			sync(false);
		}
	}

	// Applies the cache and starts the poller. Safe to call once.
	//
	// Opt-in on purpose: the feed publishes build 42.40-CL-59343437 with values
	// (GameState 0x1D8, OwningGameInstance 0x250, AmmoCount 0x10D8, ...) that
	// disagree with the runtime-verified offsets in offsets.h. Letting it run
	// silently overwrites a working table, so it stays off until you drop a file
	// named offsets_auto.on next to the executable. offsets_auto.off always wins.
	inline void start()
	{
		if (g_started.exchange(true)) return;

		std::string ignored;
		if (!read_file(k_on_file, ignored)) {
			log_line("offset feed disabled (create offsets_auto.on to enable)");
			return;
		}
		if (read_file(k_off_file, ignored)) {
			log_line("offsets_auto.off present, auto-update disabled");
			return;
		}

		boot();
		std::thread(&poll_loop).detach();
		log_line("auto-update running, poll every " +
			std::to_string(k_poll_interval / 60000) + " min");
	}

	// Snapshot for the menu / console.
	inline int applied_count() { return g_applied.load(); }

	inline std::string build_version()
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		return g_version;
	}
}
