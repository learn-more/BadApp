/*
 * PROJECT:     BadApp
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Diagnostic / support functions
 * COPYRIGHT:   Copyright 2018 Mark Jansen (mark.jansen@reactos.org)
 */

#include "stdafx.h"
#include "version.h"
#include <wininet.h>
#include <shellapi.h>
#include <Objbase.h>

PCWSTR BADAPP_EXPORT AppExecutable()
{
    static WCHAR PathBuffer[MAX_PATH * 4] = { 0 };

    if (!PathBuffer[0])
        GetModuleFileNameW(NULL, PathBuffer, _countof(PathBuffer));

    return PathBuffer;
}


#define FTH_STATE_KEY               L"SOFTWARE\\Microsoft\\FTH\\State"

#define MAX_LAYER_LENGTH            256
#define GPLK_USER                   1
#define GPLK_MACHINE                2

typedef BOOL(WINAPI *SdbGetPermLayerKeysProc)(PCWSTR wszPath, PWSTR pwszLayers, PDWORD pdwBytes, DWORD dwFlags);


void BADAPP_EXPORT AnalyzeShims(LPCWSTR Executable)
{
    WCHAR Buffer[MAX_LAYER_LENGTH];
    DWORD dwBytes;
    HMODULE hModule = LoadLibraryW(L"apphelp.dll");
    SdbGetPermLayerKeysProc GetPerm = (SdbGetPermLayerKeysProc)GetProcAddress(hModule, "SdbGetPermLayerKeys");
    if (GetPerm)
    {
        dwBytes = sizeof(Buffer);
        memset(Buffer, 0, sizeof(Buffer));
        if (GetPerm(Executable, Buffer, &dwBytes, GPLK_USER))
        {
            Output(L"Shims(USER): %s", Buffer);
        }
        dwBytes = sizeof(Buffer);
        memset(Buffer, 0, sizeof(Buffer));
        if (GetPerm(Executable, Buffer, &dwBytes, GPLK_MACHINE))
        {
            Output(L"Shims(MACHINE): %s", Buffer);
        }
    }
    FreeLibrary(hModule);
}

BOOL BADAPP_EXPORT ShowChar(WCHAR ch)
{
    if (ch >= 0x20 && ch <= 0x7e)
    {
        return TRUE;
    }
    return FALSE;
}

#define CHARS_PER_LINE 8

void BADAPP_EXPORT AnalyzeFTHState(LPCWSTR Executable)
{
    HKEY StateKey;
    LSTATUS Status;
    BYTE Data[0x80];
    WCHAR Buffer[100], TextBuffer[CHARS_PER_LINE + 1], Tmp[40];
    DWORD x, y, dwType, dwDataSize;

    Status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, FTH_STATE_KEY, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY | KEY_ENUMERATE_SUB_KEYS, &StateKey);
    if (Status == ERROR_SUCCESS)
    {
        dwDataSize = sizeof(Data);

        Status = RegQueryValueExW(StateKey, Executable, NULL, &dwType, Data, &dwDataSize);
        if (Status == ERROR_SUCCESS)
        {
            Output(L"HKLM\\%s\\", FTH_STATE_KEY);
            Output(L"  [%s]=", Executable);
            for (y = 0; y < dwDataSize; y += CHARS_PER_LINE)
            {
                Buffer[0] = UNICODE_NULL;
                memset(TextBuffer, 0, sizeof(TextBuffer));
                for (x = 0; x < CHARS_PER_LINE; ++x)
                {
                    DWORD n = x + y;
                    if (n < dwDataSize)
                    {
                        xwprintf(Tmp, _countof(Tmp), L" %02X", Data[n]);
                        StringCchCatW(Buffer, _countof(Buffer), Tmp);
                        TextBuffer[n % CHARS_PER_LINE] = ShowChar(Data[n]) ? Data[n] : '.';
                    }
                    else
                    {
                        StringCchCatW(Buffer, _countof(Buffer), L"   ");
                    }
                    if (x+1 == CHARS_PER_LINE / 2)
                    {
                        StringCchCatW(Buffer, _countof(Buffer), L" ");
                    }
                }
                Output(L"0x%04X  %s    %s", y, Buffer, TextBuffer);
            }
        }

        CloseHandle(StateKey);
    }
}

BOOL BADAPP_EXPORT RelaunchAsAdmin()
{
    BOOL bSuccess = TRUE;
    SHELLEXECUTEINFOW shExInfo = { sizeof(shExInfo) };
    shExInfo.lpVerb = L"runas";
    shExInfo.fMask = SEE_MASK_UNICODE | SEE_MASK_NOZONECHECKS | SEE_MASK_NOASYNC;
    shExInfo.lpFile = AppExecutable();
    shExInfo.nShow = SW_SHOW;


    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (!ShellExecuteExW(&shExInfo))
    {
        Output(L"Unable to start task");
        bSuccess = FALSE;
    }
    CoUninitialize();
    return bSuccess;
}

static BOOL g_bAdmin = -1;
BOOL BADAPP_EXPORT IsRunAsAdmin()
{
    if (g_bAdmin == -1)
    {
        BOOL fIsRunAsAdmin = FALSE;
        PSID pAdministratorsGroup = NULL;

        // Allocate and initialize a SID of the administrators group.
        SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
        if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                     0, 0, 0, 0, 0, 0, &pAdministratorsGroup))
        {
            if (!CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin))
                fIsRunAsAdmin = FALSE;

            if (pAdministratorsGroup)
                FreeSid(pAdministratorsGroup);
        }
        g_bAdmin = fIsRunAsAdmin;
    }
    return g_bAdmin;
}

void BADAPP_EXPORT ResetFTHState(void)
{
    SHELLEXECUTEINFOW shExInfo = { sizeof(shExInfo) };
    shExInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    shExInfo.lpVerb = L"runas";
    shExInfo.fMask = SEE_MASK_UNICODE | SEE_MASK_NOZONECHECKS | SEE_MASK_NOASYNC;
    shExInfo.lpFile = L"rundll32";
    shExInfo.lpParameters = L"fthsvc.dll,FthSysprepSpecialize";
    shExInfo.nShow = SW_SHOW;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (ShellExecuteExW(&shExInfo))
    {
        DWORD dwExit;
        if (WaitForSingleObject(shExInfo.hProcess, 2000) == WAIT_OBJECT_0)
        {
            GetExitCodeProcess(shExInfo.hProcess, &dwExit);
            Output(L"Result: %u", dwExit);
        }
        else
        {
            Output(L"Timeout");
        }
        CloseHandle(shExInfo.hProcess);
    }
    else
    {
        Output(L"Unable to start task");
    }
    CoUninitialize();
}


typedef HRESULT(__stdcall* WerAddExcludedApplicationProc)(PCWSTR pwzExeName, BOOL bAllUsers);
typedef HRESULT(__stdcall* WerRemoveExcludedApplicationProc)(PCWSTR pwzExeName, BOOL bAllUsers);

void BADAPP_EXPORT EnableWER(BOOL bAllUsers)
{
    WCHAR PathBuffer[MAX_PATH * 4];
    GetModuleFileNameW(NULL, PathBuffer, _countof(PathBuffer));

    HMODULE mod = LoadLibraryW(L"wer.dll");
    WerRemoveExcludedApplicationProc proc = (WerRemoveExcludedApplicationProc)GetProcAddress(mod, "WerRemoveExcludedApplication");
    if (proc)
    {
        HRESULT hr = proc(AppExecutable(), bAllUsers);
        Output(L"WerRemoveExcludedApplication(bAllUsers=%d) = 0x%x", bAllUsers, hr);
    }
    else
    {
        Output(L"WerRemoveExcludedApplication not found");
    }
    if (mod)
        FreeLibrary(mod);
}

void BADAPP_EXPORT DisableWER(BOOL bAllUsers)
{
    HMODULE mod = LoadLibraryW(L"wer.dll");
    WerAddExcludedApplicationProc proc = (WerAddExcludedApplicationProc)GetProcAddress(mod, "WerAddExcludedApplication");
    if (proc)
    {
        HRESULT hr = proc(AppExecutable(), bAllUsers);
        Output(L"WerAddExcludedApplication(bAllUsers=%d) = 0x%x", bAllUsers, hr);
    }
    else
    {
        Output(L"WerAddExcludedApplication not found");
    }
    if (mod)
        FreeLibrary(mod);
}


#define WER_EXCLUDED_KEY               L"SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting\\ExcludedApplications"


BOOL BADAPP_EXPORT IsWerDisabled(LPCWSTR Executable, BOOL bAllUsers)
{
    HKEY StateKey;
    LSTATUS Status;
    DWORD dwValue, dwType, dwSize;
    BOOL bDisabled = FALSE;

    Status = RegOpenKeyExW(bAllUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, WER_EXCLUDED_KEY, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &StateKey);
    if (Status == ERROR_SUCCESS)
    {
        dwSize = sizeof(dwValue);
        Status = RegQueryValueExW(StateKey, Executable, NULL, &dwType, (LPBYTE)&dwValue, &dwSize);
        if (Status == ERROR_SUCCESS && dwSize == sizeof(DWORD) && dwType == REG_DWORD)
        {
            bDisabled = dwValue != 0;
        }

        RegCloseKey(StateKey);
    }

    return bDisabled;
}

void BADAPP_EXPORT AnalyzeWER(LPCWSTR ExeOnly)
{
    BOOL bCurrentUser, bAllUsers;

    bAllUsers = IsWerDisabled(ExeOnly, TRUE);
    bCurrentUser = IsWerDisabled(ExeOnly, FALSE);
    if (bAllUsers || bCurrentUser)
    {
        Output(L"WER disabled: %s%s%s",
               bAllUsers ? L"All Users" : L"",
               (bAllUsers && bCurrentUser) ? L", " : L"",
               bCurrentUser ? L"Current User" : L"");
    }
}

void BADAPP_EXPORT AnalyzeZoneID(LPCWSTR Executable)
{
    IInternetSecurityManager* pism;
    HRESULT hr;

    CoInitialize(NULL);

    hr = CoCreateInstance(&CLSID_InternetSecurityManager, NULL, CLSCTX_ALL, &IID_IInternetSecurityManager, &pism);
    if (SUCCEEDED(hr))
    {
        DWORD dwZone;
        hr = pism->lpVtbl->MapUrlToZone(pism, Executable, &dwZone, MUTZ_ISFILE | MUTZ_DONT_UNESCAPE);
        if (SUCCEEDED(hr) && dwZone != URLZONE_LOCAL_MACHINE)
        {
            PCWSTR wszZoneIds[6] =
            {
                L"URLZONE_LOCAL_MACHINE",
                L"URLZONE_INTRANET",
                L"URLZONE_TRUSTED",
                L"URLZONE_INTERNET",
                L"URLZONE_UNTRUSTED",
                L"<INVALID>"
            };
            Output(L"ZoneID: %u [%s]\n", dwZone, wszZoneIds[min(dwZone, 5)]);
        }

        pism->lpVtbl->Release(pism);
    }

    CoUninitialize();
}

#define IMAGEEXECOPTIONSSTRING L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options"

void BADAPP_EXPORT AnalyzeGlobalFlags(LPCWSTR ExeOnly)
{
    LONG Ret;
    HKEY HandleKey, HandleSubKey;
    WCHAR Buffer[20] = { 0 };
    DWORD Type, Len = sizeof(Buffer) - sizeof(WCHAR);

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, IMAGEEXECOPTIONSSTRING, 0, KEY_READ, &HandleKey) != ERROR_SUCCESS)
    {
        Output(L"Unable to open HKLM\\%s", IMAGEEXECOPTIONSSTRING);
        return;
    }

    Ret = RegCreateKeyExW(HandleKey, ExeOnly, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ, NULL, &HandleSubKey, NULL);
    CloseHandle(HandleKey);

    if (Ret != ERROR_SUCCESS)
        return;

    if (RegQueryValueExW(HandleSubKey, L"GlobalFlag", NULL, &Type, (BYTE*)Buffer, &Len) == ERROR_SUCCESS)
    {
        DWORD Flags = 0;
        if (Type == REG_SZ)
        {
            Flags = wcstoul_(Buffer, NULL, 16);
            Output(L"GlobalFlag(sz): %08x", Flags);
        }
        else if (Type == REG_DWORD && Len == sizeof(DWORD))
        {
            Flags = *(DWORD*)Buffer;
            Output(L"GlobalFlag(dw): %08x", Flags);
        }
        else
        {
            Output(L"Unknown type: %u", Type);
        }
    }

    CloseHandle(HandleSubKey);
}


void BADAPP_EXPORT RelaunchFN(void)
{
    if (RelaunchAsAdmin())
        PostQuitMessage(0);
}

void BADAPP_EXPORT EnableWerFN(void)
{
    EnableWER(TRUE);
    EnableWER(FALSE);
}

void BADAPP_EXPORT DisableWerFN(void)
{
    DisableWER(TRUE);
    DisableWER(FALSE);
}

void BADAPP_EXPORT CheckOsVersionFN(void)
{
    DWORD dwVersion;
    DWORD dwMajorVersion, dwMinorVersion, dwBuildNumber = 0;
    OSVERSIONINFOEXW VersionInfo;
    void (__stdcall* pRtlGetVersion)(OSVERSIONINFOEXW*);

    dwVersion = GetVersion();
    dwMajorVersion = (DWORD)(LOBYTE(LOWORD(dwVersion)));
    dwMinorVersion = (DWORD)(HIBYTE(LOWORD(dwVersion)));
    if (dwVersion < 0x80000000)
        dwBuildNumber = (DWORD)(HIWORD(dwVersion));

    Output(L"GetVersion() =    %d.%d (%d)", dwMajorVersion, dwMinorVersion, dwBuildNumber);

    ZeroMemory(&VersionInfo, sizeof(VersionInfo));
    VersionInfo.dwOSVersionInfoSize = sizeof(VersionInfo);
    if (GetVersionExW((LPOSVERSIONINFOW)&VersionInfo))
    {
        Output(L"GetVersionExW() = %d.%d (%d)", VersionInfo.dwMajorVersion, VersionInfo.dwMinorVersion, VersionInfo.dwBuildNumber);
        if (VersionInfo.wServicePackMajor || VersionInfo.wServicePackMinor || VersionInfo.szCSDVersion[0])
            Output(L"   Service Pack = %d.%d (%s)", (DWORD)VersionInfo.wServicePackMajor, (DWORD)VersionInfo.wServicePackMinor, VersionInfo.szCSDVersion);
    }
    else
    {
        Output(L"GetVersionExW() failed (%u)", GetLastError());
    }

    pRtlGetVersion = (void (__stdcall*)(OSVERSIONINFOEXW*))GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
    if (pRtlGetVersion)
    {
        ZeroMemory(&VersionInfo, sizeof(VersionInfo));
        VersionInfo.dwOSVersionInfoSize = sizeof(VersionInfo);
        pRtlGetVersion(&VersionInfo);
        Output(L"RtlGetVersion() = %d.%d (%d)", VersionInfo.dwMajorVersion, VersionInfo.dwMinorVersion, VersionInfo.dwBuildNumber);
        if (VersionInfo.wServicePackMajor || VersionInfo.wServicePackMinor || VersionInfo.szCSDVersion[0])
            Output(L"   Service Pack = %d.%d (%s)", (DWORD)VersionInfo.wServicePackMajor, (DWORD)VersionInfo.wServicePackMinor, VersionInfo.szCSDVersion);
    }
    else
    {
        Output(L"RtlGetVersion() not found (%u)", GetLastError());
    }
}

void BADAPP_EXPORT CheckVersionFN(void)
{
    HINTERNET hInternet = InternetOpenA("BadApp/" GIT_VERSION_STR, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet)
    {
        HINTERNET hUrl = InternetOpenUrlW(hInternet, L"https://github.com/learn-more/BadApp/releases/latest", NULL, 0,
                                          INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_AUTO_REDIRECT, 0);
        if (hUrl)
        {
            WCHAR Buf[512] = {0};
            DWORD dwBufSize = sizeof(Buf);
            DWORD dwIndex = 0;
            if (HttpQueryInfoW(hUrl, HTTP_QUERY_LOCATION, Buf, &dwBufSize, &dwIndex))
            {
                Output(L"Latest release: %s", Buf);
            }
            else
            {
                Output(L"Error %u calling HttpQueryInfoA", GetLastError());
            }
            InternetCloseHandle(hUrl);
        }
        else
        {
            Output(L"Error %u calling InternetOpenUrlA", GetLastError());
        }

        InternetCloseHandle(hInternet);
    }
    else
    {
        Output(L"Error %u calling InternetOpenA", GetLastError());
    }
}

static BAD_ACTION g_Actions[] =
{
    {
        L"Relaunch as admin",
        L"Relaunch BadApp as administrator. This allows the modification of some properties in HKLM.",
        RelaunchFN,
        ShieldIcon
    },
    {
        L"Reset FTH State",
        L"Reset the list of applications tracked by FTH. This also removes FTH from all applications, so certain applications might start to malfuction (again).",
        ResetFTHState,
        ShieldIcon
    },
    {
        L"Enable WER",
        L"Enable Windows Error Reporting for BadApp. First try to set this for all users, then for the current user.",
        EnableWerFN,
        ApplicationIcon
    },
    {
        L"Disable WER",
        L"Disable Windows Error Reporting for BadApp. First try to set this for all users, then for the current user.",
        DisableWerFN,
        ApplicationIcon
    },
    {
        L"OS version",
        L"Query the OS version using various techniques.",
        CheckOsVersionFN,
        OsIcon
    },
    {
        L"Check version",
        L"Check the github release page to see what the latest version is.",
        CheckVersionFN,
        GithubIcon
    },
    { NULL }
};

static BAD_ACTION g_Category =
{
    L"Diagnostics",
    L"Elevate BadApp, reset FTH, disable WER, check version",
    NULL
};

void BADAPP_EXPORT Diag_Init(void)
{
    BOOL bAdmin;
    LPCWSTR ExeOnly, FullPath, Tmp;

    ExeOnly = FullPath = Tmp = AppExecutable();

    while ((Tmp = wcspbrk_(ExeOnly, L"\\/")) != NULL)
    {
        ExeOnly = Tmp + 1;
    }

    AnalyzeShims(FullPath);
    AnalyzeWER(ExeOnly);
    AnalyzeFTHState(FullPath);
    AnalyzeZoneID(FullPath);
    AnalyzeGlobalFlags(ExeOnly);

    bAdmin = IsRunAsAdmin();
    if (bAdmin)
        g_Actions[1].iIcon = ApplicationIcon;
    Register_Category(&g_Category, g_Actions + (bAdmin ? 1 : 0));
}

