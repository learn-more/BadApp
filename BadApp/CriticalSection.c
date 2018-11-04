/*
* PROJECT:     BadApp
* LICENSE:     MIT (https://spdx.org/licenses/MIT)
* PURPOSE:     CriticalSection bugs
* COPYRIGHT:   Copyright 2018 Mark Jansen (mark.jansen@reactos.org)
*/

#include "stdafx.h"

HANDLE BADAPP_EXPORT CSHeap(void)
{
    return GetProcessHeap();
}


LPCRITICAL_SECTION BADAPP_EXPORT CreateAndInitializeSection(void)
{
    HANDLE hHeap = CSHeap();
    LPCRITICAL_SECTION lpSection = HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(*lpSection));

    InitializeCriticalSection(lpSection);
    return lpSection;
}


DWORD BADAPP_EXPORT WINAPI TerminateFN_ThreadProc(LPVOID pArg)
{
    LPCRITICAL_SECTION lpSection = CreateAndInitializeSection();
    UNREFERENCED_PARAMETER(pArg);

    EnterCriticalSection(lpSection);

    ExitThread(0);
}

void BADAPP_EXPORT TerminateFN(void)
{
    HANDLE hThread;

    hThread = CreateThread(NULL, 0, TerminateFN_ThreadProc, NULL, 0, NULL);
    WaitForSingleObject(hThread, 2000);
    CloseHandle(hThread);
}

void BADAPP_EXPORT FreeFN(void)
{
    LPCRITICAL_SECTION lpSection = CreateAndInitializeSection();

    HeapFree(CSHeap(), 0, lpSection);
}

void BADAPP_EXPORT InitTwiceFN(void)
{
    LPCRITICAL_SECTION lpSection = CreateAndInitializeSection();

    /* Initialize for the second time */
    InitializeCriticalSection(lpSection);

    DeleteCriticalSection(lpSection);

    HeapFree(CSHeap(), 0, lpSection);
}


typedef struct _EventData
{
    LPCRITICAL_SECTION lpSection;
    HANDLE hLockHeld;
    HANDLE hExitThread;
} EventData;

DWORD BADAPP_EXPORT WINAPI DeleteHeldSectionFN_ThreadProc(LPVOID pArg)
{
    DWORD dwWait;
    EventData Data = *(EventData*)pArg;

    EnterCriticalSection(Data.lpSection);
    SetEvent(Data.hLockHeld);
    dwWait = WaitForSingleObject(Data.hExitThread, 10 * 1000);

    if (dwWait != WAIT_OBJECT_0)
        Output(L"Failed to wait for the exit event (%u)", dwWait);

    return 0;
}

void BADAPP_EXPORT DeleteHeldSectionFN(void)
{
    EventData Evt;
    HANDLE hThread;
    DWORD dwWait;

    Evt.lpSection = CreateAndInitializeSection();
    Evt.hLockHeld = CreateEvent(NULL, TRUE, FALSE, NULL);
    Evt.hExitThread = CreateEvent(NULL, TRUE, FALSE, NULL);

    hThread = CreateThread(NULL, 0, DeleteHeldSectionFN_ThreadProc, (LPVOID)&Evt, 0, NULL);
    dwWait = WaitForSingleObject(Evt.hLockHeld, 2 * 1000);
    if (dwWait != WAIT_OBJECT_0)
        Output(L"Failed to wait for the section to be entered (%u)", dwWait);

    DeleteCriticalSection(Evt.lpSection);
    SetEvent(Evt.hExitThread);

    dwWait = WaitForSingleObject(hThread, 2 * 1000);
    if (dwWait != WAIT_OBJECT_0)
        Output(L"Failed to wait for the thread to be done (%u)", dwWait);

    CloseHandle(hThread);
    CloseHandle(Evt.hLockHeld);
    CloseHandle(Evt.hExitThread);

    HeapFree(CSHeap(), 0, Evt.lpSection);
}

void BADAPP_EXPORT ReleaseMultipleFN(void)
{
    LPCRITICAL_SECTION lpSection = CreateAndInitializeSection();

    EnterCriticalSection(lpSection);
    LeaveCriticalSection(lpSection);
    LeaveCriticalSection(lpSection);

    DeleteCriticalSection(lpSection);
    HeapFree(CSHeap(), 0, lpSection);
}

void BADAPP_EXPORT UseNoInitFN(void)
{
    CRITICAL_SECTION Section = { NULL };

    EnterCriticalSection(&Section);
}

void BADAPP_EXPORT VirtualFreeFN(void)
{
    LPCRITICAL_SECTION lpSection = VirtualAlloc(NULL, sizeof(*lpSection), MEM_COMMIT, PAGE_READWRITE);

    InitializeCriticalSection(lpSection);

    VirtualFree(lpSection, 0, MEM_RELEASE);
}

typedef struct _DUM_PEB
{
    INT_PTR Flags;
    PVOID Mutant;
    PVOID ImageBaseAddress;
    PVOID Ldr;
    PVOID ProcessParameters;
    PVOID SubSystemData;
    HANDLE ProcessHeap;
    RTL_CRITICAL_SECTION* FastPebLock;
} DUM_PEB;

#ifdef _WIN64
C_ASSERT(offsetof(DUM_PEB, FastPebLock) == 0x38);
#else
C_ASSERT(offsetof(DUM_PEB, FastPebLock) == 0x1c);
#endif

void BADAPP_EXPORT PrivateSectionFN(void)
{
    DUM_PEB* peb;

#ifdef _WIN64
    peb = (DUM_PEB*)__readgsqword(0x60);
#else
    peb = (DUM_PEB*)__readfsdword(0x30);
#endif

    EnterCriticalSection(peb->FastPebLock);
    LeaveCriticalSection(peb->FastPebLock);
}

static BAD_ACTION g_Actions[] =
{
    {
        L"Terminate thread",
        L"Terminate a thread holding a critical section",
        TerminateFN,
        BadIcon
    },
    {
        L"Free without delete",
        L"Free an allocated critical section without deleting it.",
        FreeFN,
        BadIcon
    },
    {
        L"Initialize twice",
        L"Initialize a critical section twice.",
        InitTwiceFN,
        BadIcon
    },
    {
        L"Delete locked section",
        L"Delete a critical section while it is being held.",
        DeleteHeldSectionFN,
        BadIcon
    },
    {
        L"Release multiple",
        L"Release a critical section multiple times.",
        ReleaseMultipleFN,
        BadIcon
    },
    {
        L"Use without init",
        L"Use a critical section without initializing it.",
        UseNoInitFN,
        BadIcon
    },
    {
        L"Call VirtualFree",
        L"Call VirtualFree on a critical section without releasing it.",
        VirtualFreeFN,
        BadIcon
    },
    {
        L"Private section",
        L"Use an ntdll private section (ntdll!FastPebLock)",
        PrivateSectionFN,
        BadIcon
    },

    { NULL },
};

static BAD_ACTION g_Category =
{
    L"Critical Section",
    L"Trigger various bugs related to critical sections",
    NULL,
    NoIcon
};

void BADAPP_EXPORT CriticalSection_Init(void)
{
    Register_Category(&g_Category, g_Actions);
}
