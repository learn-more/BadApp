/*
 * PROJECT:     BadApp
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Crashing functions
 * COPYRIGHT:   Copyright 2018 Mark Jansen (mark.jansen@reactos.org)
 */

#include "stdafx.h"

volatile int g_AvoidWarning = 0;

static
void DummyFunction(void)
{
    Output(L"Dummy (should not end up here!)");
}

static
void CallNullptrFN(void)
{
    Action Proc = DummyFunction;

    if (!g_AvoidWarning)
        Proc = NULL;

    Proc();
}

static
void ReadNullptrFN(void)
{
    PDWORD DwordPtr = (PDWORD)NULL;
    DWORD Value = *DwordPtr;
    Output(L"Value: %u", Value);
}

static
void WriteNullptrFN(void)
{
    PDWORD DwordPtr = (PDWORD)NULL;
    *DwordPtr = 0x112233;
}

static
void StackOverflowFN(void)
{
    Action recurse = DummyFunction;

    if (!g_AvoidWarning)
        recurse = StackOverflowFN;

    recurse();
}

static BAD_ACTION g_Crash[] =
{
    { L"Call nullptr", L"Crash by calling a nullptr", L"Crash by calling a nullptr", CallNullptrFN },
    { L"Read nullptr", L"Crash by reading from a nullptr", L"Crash by reading from a nullptr", ReadNullptrFN },
    { L"Write nullptr", L"Crash by writing to a nullptr", L"Crash by writing to a nullptr", WriteNullptrFN },
    { L"Stack overflow", L"Crash by causing a stack overflow", L"Crash by causing a stack overflow", StackOverflowFN },
    { NULL, NULL },
};

static BAD_ACTION g_CrashCategory =
{
    L"Crashes",
    L"Invoke crashes",
    L"Apply various techniques to crash BadApp.",
    NULL
};

void Crash_Init(void)
{
    Register_Category(&g_CrashCategory, g_Crash);
}
