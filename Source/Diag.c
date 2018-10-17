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

static
BOOL ShowChar(WCHAR ch)
{
    if (iswprint(ch))
    {
        return ch != '\r' && ch != '\n' && ch != '\t';
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
                        StringCchPrintfW(Tmp, _countof(Tmp), L" %02X", Data[n]);
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

BOOL IsRunAsAdmin()
{
    BOOL fIsRunAsAdmin = FALSE;
    PSID pAdministratorsGroup = NULL;

    // Allocate and initialize a SID of the administrators group.
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (!AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdministratorsGroup))
        return fIsRunAsAdmin;

    if (!CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin))
        fIsRunAsAdmin = FALSE;

    if (pAdministratorsGroup)
        FreeSid(pAdministratorsGroup);

    return fIsRunAsAdmin;
}

void ResetFTHState()
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

static void AnalyzeWER(LPCWSTR FullPath)
{
    LPCWSTR ExeOnly = FullPath;
    BOOL bCurrentUser, bAllUsers;
    while ((FullPath = wcspbrk(ExeOnly, L"\\/")) != NULL)
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

void Diag_Init()
{
    AnalyzeShims(AppExecutable());
    AnalyzeRegistry(AppExecutable());
    AnalyzeWER(AppExecutable());
}

