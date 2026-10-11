/*
 * PROJECT:     BadApp
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Thread / synchronization bugs
 * COPYRIGHT:   Copyright 2018,2020 Mark Jansen (mark.jansen@reactos.org)
 */

#include "stdafx.h"

#define LEAKED_THREAD_COUNT 32


DWORD BADAPP_EXPORT WINAPI LeakThreadHandlesFN_ThreadProc(LPVOID pArg)
{
    UNREFERENCED_PARAMETER(pArg);
    return 0;
}

void BADAPP_EXPORT LeakThreadHandlesFN(void)
{
    DWORD n, created = 0;

    for (n = 0; n < LEAKED_THREAD_COUNT; ++n)
    {
        if (CreateThread(NULL, 0, LeakThreadHandlesFN_ThreadProc, NULL, 0, NULL))
            created++;                  /* Eventually we should close this */
    }

    Output(L"Leaked %u thread handle(s)", created);
}


DWORD BADAPP_EXPORT WINAPI TerminateThreadFN_ThreadProc(LPVOID pArg)
{
    volatile LONG* Stop = (volatile LONG*)pArg;

    while (!*Stop)
        Sleep(1);

    return 0;
}

void BADAPP_EXPORT TerminateThreadFN(void)
{
    volatile LONG Stop = 0;
    HANDLE hThread;

    hThread = CreateThread(NULL, 0, TerminateThreadFN_ThreadProc, (LPVOID)&Stop, 0, NULL);
    if (!hThread)
    {
        Output(L"CreateThread failed (%u)", GetLastError());
        return;
    }

    Sleep(50);
    TerminateThread(hThread, 0xDEAD);
    CloseHandle(hThread);
    Output(L"Killed the thread with TerminateThread");
}


DWORD BADAPP_EXPORT WINAPI AbandonedMutexFN_ThreadProc(LPVOID pArg)
{
    HANDLE hMutex = (HANDLE)pArg;

    WaitForSingleObject(hMutex, INFINITE);
    return 0;                           /* Exit while still owning the mutex */
}

void BADAPP_EXPORT AbandonedMutexFN(void)
{
    HANDLE hMutex = CreateMutexW(NULL, FALSE, NULL);
    HANDLE hThread;
    DWORD dwWait;

    if (!hMutex)
    {
        Output(L"CreateMutexW failed (%u)", GetLastError());
        return;
    }

    hThread = CreateThread(NULL, 0, AbandonedMutexFN_ThreadProc, hMutex, 0, NULL);
    if (hThread)
    {
        WaitForSingleObject(hThread, 5000);
        CloseHandle(hThread);
    }

    dwWait = WaitForSingleObject(hMutex, 5000);
    Output(L"Waiting on an abandoned mutex: %u%s", dwWait,
           (dwWait == WAIT_ABANDONED) ? L" (WAIT_ABANDONED)" : L"");

    /* We still own it, even if it was abandoned */
    if (dwWait == WAIT_OBJECT_0 || dwWait == WAIT_ABANDONED)
        ReleaseMutex(hMutex);
    CloseHandle(hMutex);
}


void BADAPP_EXPORT SelfDeadlockFN(void)
{
    HANDLE hSemaphore = CreateSemaphoreW(NULL, 1, 1, NULL);
    DWORD dwFirst, dwSecond;

    if (!hSemaphore)
    {
        Output(L"CreateSemaphoreW failed (%u)", GetLastError());
        return;
    }

    dwFirst = WaitForSingleObject(hSemaphore, 1000);
    dwSecond = WaitForSingleObject(hSemaphore, 3000);   /* This can never succeed */

    Output(L"First wait = %u, second wait = %u%s", dwFirst, dwSecond,
           (dwSecond == WAIT_TIMEOUT) ? L" (self deadlock)" : L"");

    if (dwFirst == WAIT_OBJECT_0)
        ReleaseSemaphore(hSemaphore, 1, NULL);
    CloseHandle(hSemaphore);
}


typedef struct _ABBA_DATA
{
    CRITICAL_SECTION Section1;
    CRITICAL_SECTION Section2;
    HANDLE hThread1Held;
    HANDLE hThread2Held;
    HANDLE hProceed;
} ABBA_DATA;

static ABBA_DATA g_Abba;
static BOOL g_AbbaTriggered;

/* Both threads take the two sections in the opposite order, so they lock up */
DWORD BADAPP_EXPORT WINAPI LockOrderInversionFN_Thread1Proc(LPVOID pArg)
{
    ABBA_DATA* Data = (ABBA_DATA*)pArg;

    EnterCriticalSection(&Data->Section1);
    SetEvent(Data->hThread1Held);
    WaitForSingleObject(Data->hProceed, 5000);

    EnterCriticalSection(&Data->Section2);      /* Blocks forever */
    LeaveCriticalSection(&Data->Section2);
    LeaveCriticalSection(&Data->Section1);
    return 0;
}

DWORD BADAPP_EXPORT WINAPI LockOrderInversionFN_Thread2Proc(LPVOID pArg)
{
    ABBA_DATA* Data = (ABBA_DATA*)pArg;

    EnterCriticalSection(&Data->Section2);
    SetEvent(Data->hThread2Held);
    WaitForSingleObject(Data->hProceed, 5000);

    EnterCriticalSection(&Data->Section1);      /* Blocks forever */
    LeaveCriticalSection(&Data->Section1);
    LeaveCriticalSection(&Data->Section2);
    return 0;
}

void BADAPP_EXPORT LockOrderInversionFN(void)
{
    HANDLE hThread1, hThread2, Handles[2];
    DWORD dwWait;

    if (g_AbbaTriggered)
    {
        Output(L"Already deadlocked, restart BadApp to run it again");
        return;
    }

    InitializeCriticalSection(&g_Abba.Section1);
    InitializeCriticalSection(&g_Abba.Section2);
    g_Abba.hThread1Held = CreateEvent(NULL, TRUE, FALSE, NULL);
    g_Abba.hThread2Held = CreateEvent(NULL, TRUE, FALSE, NULL);
    g_Abba.hProceed = CreateEvent(NULL, TRUE, FALSE, NULL);

    hThread1 = CreateThread(NULL, 0, LockOrderInversionFN_Thread1Proc, &g_Abba, 0, NULL);
    hThread2 = CreateThread(NULL, 0, LockOrderInversionFN_Thread2Proc, &g_Abba, 0, NULL);
    if (!hThread1 || !hThread2)
    {
        Output(L"CreateThread failed (%u)", GetLastError());
        if (hThread1)
            CloseHandle(hThread1);
        if (hThread2)
            CloseHandle(hThread2);
        return;
    }

    WaitForSingleObject(g_Abba.hThread1Held, 2000);
    WaitForSingleObject(g_Abba.hThread2Held, 2000);
    SetEvent(g_Abba.hProceed);

    Handles[0] = hThread1;
    Handles[1] = hThread2;
    dwWait = WaitForMultipleObjects(2, Handles, TRUE, 2000);
    if (dwWait == WAIT_TIMEOUT)
    {
        g_AbbaTriggered = TRUE;
        Output(L"Deadlock detected");
    }
    else
    {
        Output(L"No deadlock (%u)", dwWait);
    }
}


DWORD BADAPP_EXPORT WINAPI CrashInThreadFN_ThreadProc(LPVOID pArg)
{
    UNREFERENCED_PARAMETER(pArg);

    RaiseException(0xE0000001, 0, 0, NULL);
    return 1;
}

void BADAPP_EXPORT CrashInThreadFN(void)
{
    HANDLE hThread = CreateThread(NULL, 0, CrashInThreadFN_ThreadProc, NULL, 0, NULL);

    if (!hThread)
    {
        Output(L"CreateThread failed (%u)", GetLastError());
        return;
    }

    /* The unhandled exception usually kills the whole process first */
    WaitForSingleObject(hThread, 4000);
    CloseHandle(hThread);
}


static BAD_ACTION g_Actions[] =
{
    {
        L"Leak thread handles",
        L"Create a bunch of threads and never close their handles",
        LeakThreadHandlesFN,
        BadIcon
    },
    {
        L"TerminateThread",
        L"Kill a running thread instead of letting it exit",
        TerminateThreadFN,
        BadIcon
    },
    {
        L"Abandoned mutex",
        L"Exit a thread while it still owns a mutex",
        AbandonedMutexFN,
        BadIcon
    },
    {
        L"Self deadlock",
        L"Wait twice on a single-count semaphore from one thread",
        SelfDeadlockFN,
        BadIcon
    },
    {
        L"Lock order inversion",
        L"Two threads acquire the same two sections in a different order",
        LockOrderInversionFN,
        BadIcon
    },
    {
        L"Crash in thread",
        L"Throw an unhandled exception from a worker thread",
        CrashInThreadFN,
        BadIcon
    },
    { NULL },
};

static BAD_ACTION g_Category =
{
    L"Threads",
    L"Trigger various thread and synchronization bugs",
    NULL,
    NoIcon
};

void BADAPP_EXPORT Threads_Init(void)
{
    Register_Category(&g_Category, g_Actions);
}
