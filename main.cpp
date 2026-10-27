#include <include.h>
#include <src/cache/index.h>
#include <src/rendering/index.h>
#include <src/memory/mapper.h>
#include <src/console/console.h>
#include <src/console/log.h>
#include <build.h>
#include <sstream>
#include <thirdparty/auth/auth.hpp>
#include <src/security/security.hpp>
#include <src/engine/offsets_auto.h>

HWND GetProcessWindow(uint32_t pid)
{
    std::pair<HWND, uint32_t> params = { 0, pid };
    BOOL bresult = LI_FN(EnumWindows).cached()([](HWND hwnd, LPARAM lparam) -> BOOL
        {
            auto pparams = (std::pair<HWND, uint32_t>*)(lparam);
            uint32_t processid = 0;
            if (LI_FN(GetWindowThreadProcessId).cached()(hwnd, reinterpret_cast<LPDWORD>(&processid)) && processid == pparams->second)
            {
                LI_FN(SetLastError).cached()((uint32_t)-1);
                pparams->first = hwnd;
                return FALSE;
            }
            return TRUE;
        }, (LPARAM)&params);

    if (!bresult && LI_FN(GetLastError).cached()() == -1 && params.first) return params.first;
    return 0;
}

bool WaitForFortnite(int timeout_seconds = 60)
{
    int elapsed = 0;
    while (elapsed < timeout_seconds)
    {
        DWORD pid = mem::find_process(_xor_(L"FortniteClient-Win64-Shipping.exe"));
        if (pid)
        {
            HWND wnd = GetProcessWindow(pid);
            if (wnd && LI_FN(IsWindowVisible).cached()(wnd))
            {
                RECT rect;
                LI_FN(GetWindowRect).cached()(wnd, &rect);
                int sw = LI_FN(GetSystemMetrics).cached()(SM_CXSCREEN);
                int sh = LI_FN(GetSystemMetrics).cached()(SM_CYSCREEN);
                if (rect.left == 0 && rect.top == 0 && rect.right == sw && rect.bottom == sh)
                {
                    mem::process_id = pid;
                    return true;
                }
            }
        }
        LI_FN(Sleep).cached()(1000);
        elapsed++;
    }
    return false;
}

[[noreturn]] static void SecureExit(int code) {
    LI_FN(Sleep).cached()(static_cast<DWORD>(rand() % 400 + 100));
    LI_FN(TerminateProcess).cached()(LI_FN(GetCurrentProcess).cached()(), static_cast<UINT>(code));
    __assume(false);
}

KeyAuth::api KeyAuthApp(
    _xor_("Azure Fortnite Public"),
    _xor_("nRxzTs8Nva"),
    LOCAL_VERSION,
    _xor_("https://keyauth.win/api/1.3/"),
    _xor_(""),
    false
);
KeyAuth::api::lockout_state login_guard{};

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    //InitSecurity();

    //KeyAuthApp.init();

    //if (!KeyAuthApp.response.success) {
    //    const std::string message = KeyAuthApp.response.message.empty()
    //        ? _xor_("Authentication service initialization failed.")
    //        : KeyAuthApp.response.message;
    //    LI_FN(MessageBoxA).cached()(nullptr, message.c_str(), _xor_("Authentication error"), MB_OK | MB_ICONERROR);
    //    LI_FN(Sleep).cached()(KeyAuth::api::kInitFailSleepMs);
    //    SecureExit(1);
    //    return 1;
    //}

    HWND consoleWindow = nullptr;
    if (!LI_FN(AttachConsole).cached()(ATTACH_PARENT_PROCESS)) {
        LI_FN(AllocConsole).cached()();
    }
    consoleWindow = LI_FN(GetConsoleWindow).cached()();

    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    freopen_s(&fDummy, "CONIN$", "r", stdin);

    std::cout.clear();
    std::clog.clear();
    std::cerr.clear();
    std::cin.clear();

    SetupConsole();

    LI_FN(SetConsoleTitleA).cached()(_xor_("Azure Fortnite Public | azurecheats.cc"));
    LI_FN(SetConsoleOutputCP).cached()(CP_UTF8);

    //std::string license;

    LOG("Product: ", BRIGHT_WHITE);
    LOGRAW("Fortnite Public\n", BRIGHT_BLUE);
    LOG("Version: ", BRIGHT_WHITE);
    LogRaw(LOCAL_VERSION + _xor_("\n\n"), BRIGHT_YELLOW);

    //LOG("Enter your license key -> ", BRIGHT_WHITE);

    //std::getline(std::cin, license);

    //std::cout << std::endl;

    //if (license.size() < 6) {
    //    LOG("Invalid license or HWID.", BRIGHT_RED);
    //    LI_FN(Sleep).cached()(KeyAuth::api::kInitFailSleepMs);
    //    SecureExit(0);
    //    return 0;
    //}

    //KeyAuthApp.license(license);
    //if (!KeyAuthApp.response.success) {
    //    LOG("Invalid license or HWID.", BRIGHT_RED);
    //    LI_FN(Sleep).cached()(KeyAuth::api::kInitFailSleepMs);
    //    SecureExit(0);
    //    return 0;
    //}

    LOG("License accepted!\n", GREEN);
    LOG("Initializing cheat now", WHITE);
    SET_LOG_DELAY(1000);
    LogRaw("...", WHITE);
    SET_LOG_DELAY(10);

    std::cout << std::endl;

    if (!mem::find_driver()) {
        if (!MapDriverLocally() || !mem::find_driver())
        {
            LI_FN(MessageBoxA).cached()(nullptr, _xor_("Failed to load driver. Please check your anti-virus settings, make sure you are on a supported windows version and have fortnite closed."), _xor_("Error"), MB_ICONERROR);
            return 0;
        }
    }

    LOG("Successfully initialized!\n", WHITE);
    LOG("Please open Fortnite!\n", WHITE);

    if (!WaitForFortnite(120)) {
        LI_FN(MessageBoxA).cached()(nullptr, _xor_("Failed to find Fortnite. Make sure it's open and fullscreen."), _xor_("Error"), MB_ICONERROR);
        return 0;
    }

    base = mem::find_image();
    cr3 = mem::fetch_cr3();

    if (!base) {
        LI_FN(MessageBoxA).cached()(nullptr, _xor_("Failed to get game base. Please get assistance from your provider by sending a screenshot of this message."), _xor_("Error"), MB_ICONERROR);
        return 0;
    }

    if (!cr3) {
        LI_FN(MessageBoxA).cached()(nullptr, _xor_("Failed to get cr3. Please get assistance from your provider by sending a screenshot of this message."), _xor_("Error"), MB_ICONERROR);
        return 0;
    }

    LI_FN(Beep).cached()(750, 300);

    // offset feed: cached dump now, network refresh in the background
    offsets_auto::start();

    if (consoleWindow) {
        LI_FN(ShowWindow).cached()(consoleWindow, SW_HIDE);
    }

    config::load();
    std::thread(&updater::run).detach();
    const bool overlay_started = overlay::run();

    if (!overlay_started) {
        LI_FN(MessageBoxA).cached()(nullptr, _xor_("Failed to get game window for overlay."), _xor_("Error"), MB_ICONERROR);
        return 0;
    }

    return 0;
}
