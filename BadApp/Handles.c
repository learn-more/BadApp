/*
* PROJECT:     BadApp
* LICENSE:     MIT (https://spdx.org/licenses/MIT)
* PURPOSE:     Handle bugs
* COPYRIGHT:   Copyright 2018 Mark Jansen (mark.jansen@reactos.org)
*/

#include "stdafx.h"


void BADAPP_EXPORT SetInvalidFN(void)
{
    HANDLE hHandle = ((HANDLE)(LONG_PTR)-2);
    SetEvent(hHandle);
}

void BADAPP_EXPORT SetNullFN(void)
{
    HANDLE hHandle = ((HANDLE)(LONG_PTR)0);
    SetEvent(hHandle);
}

void BADAPP_EXPORT WaitForMultipleObjectsFN(void)
{
    DWORD nCount = 0;
    WaitForMultipleObjects(nCount, NULL, TRUE, 100);
}

void BADAPP_EXPORT WrongEventFN(void)
{
    HANDLE hSemaphore = CreateSemaphoreW(NULL, 0, 10, NULL);
    SetEvent(hSemaphore);
    CloseHandle(hSemaphore);
}


static BAD_ACTION g_Actions[] =
{
    {
        L"SetEvent on invalid",
        L"Call SetEvent with a handle value of -2.",
        SetInvalidFN,
        BadIcon
    },
    {
        L"SetEvent NULL handle",
        L"Call SetEvent with a NULL handle.",
        SetNullFN,
        BadIcon
    },
    {
        L"WaitForMultipleObjects",
        L"Pass WaitForMultipleObjects a NULL pointer as handle array",
        WaitForMultipleObjectsFN,
        BadIcon
    },
    {
        L"WrongEvent",
        L"Create a semaphore (CreateSemaphoreW), and use this to call SetEvent",
        WrongEventFN,
        BadIcon
    },
    { NULL },
};

static BAD_ACTION g_Category =
{
    L"Handles",
    L"Trigger various bugs related to invalid handles",
    NULL,
    NoIcon
};

void BADAPP_EXPORT Handles_Init(void)
{
    Register_Category(&g_Category, g_Actions);
}
