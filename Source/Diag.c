/*
 * PROJECT:     BadApp
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Diagnostic / support functions
 * COPYRIGHT:   Copyright 2018 Mark Jansen (mark.jansen@reactos.org)
 */

#include "stdafx.h"
#include <shellapi.h>
#include <Objbase.h>

static PCWSTR AppExecutable()
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

static
void AnalyzeShims(LPCWSTR Executable)
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
#if 0

static
BOOL ShowChar(WCHAR ch)
{
    if (ch >= 0x20 && ch <= 0x7e)
    {
        return TRUE;
    }
    return FALSE;
}

#define CHARS_PER_LINE 16

static
void AnalyzeRegistry(LPCWSTR Executable)
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
#endif

BOOL RelaunchAsAdmin()
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
BOOL IsRunAsAdmin()
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

static
void ResetFTHState(void)
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

void EnableWER(BOOL bAllUsers)
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

void DisableWER(BOOL bAllUsers)
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


static BOOL IsWerDisabled(LPCWSTR Executable, BOOL bAllUsers)
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

static
LPCWSTR wcspbrk_(LPCWSTR Source, LPCWSTR Find)
{
    for (;*Source; ++Source)
    {
        LPCWSTR FindCur;
        for (FindCur = Find; *FindCur; ++FindCur)
        {
            if (*FindCur == *Source)
                return Source;
        }
    }
    return NULL;
}

static void AnalyzeWER(LPCWSTR FullPath)
{
    LPCWSTR ExeOnly = FullPath;
    BOOL bCurrentUser, bAllUsers;
    while ((FullPath = wcspbrk_(ExeOnly, L"\\/")) != NULL)
    {
        ExeOnly = FullPath + 1;
    }

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

static
void RelaunchFN(void)
{
    if (RelaunchAsAdmin())
        PostQuitMessage(0);
}

static
void EnableWerFN(void)
{
    EnableWER(TRUE);
    EnableWER(FALSE);
}

static
void DisableWerFN(void)
{
    DisableWER(TRUE);
    DisableWER(FALSE);
}

static BAD_ACTION g_Diag[] =
{
    {
        L"Relaunch as admin",
        L"Relaunch BadApp as administrator",
        L"Relaunch BadApp as administrator. This allows the modification of some properties in HKLM.",
        RelaunchFN,
        ShieldIcon
    },
    {
        L"Reset FTH State",
        L"Reset the global FTH State",
        L"Reset the list of applications tracked by FTH. This also removes FTH from all applications, so certain applications might start to malfuction (again).",
        ResetFTHState,
        ShieldIcon
    },
    {
        L"Enable WER",
        L"Enable WER for BadApp",
        L"Enable Windows Error Reporting for BadApp. First try to set this for all users, then for the current user.",
        EnableWerFN,
        ApplicationIcon
    },
    {
        L"Disable WER",
        L"Disable WER for BadApp",
        L"Disable Windows Error Reporting for BadApp. First try to set this for all users, then for the current user.",
        DisableWerFN,
        ApplicationIcon
    },
    { NULL }
};

static BAD_ACTION g_DiagCategory =
{
    L"Diagnostics",
    L"Various diagnostic functions",
    L"Elevate BadApp, reset FTH, disable WER.",
    NULL
};

void Diag_Init(void)
{
    AnalyzeShims(AppExecutable());
    AnalyzeWER(AppExecutable());

    Register_Category(&g_DiagCategory, g_Diag + (IsRunAsAdmin() ? 1 : 0));
    //AnalyzeRegistry(AppExecutable());
}

