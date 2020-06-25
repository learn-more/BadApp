/*
 * PROJECT:     BadApp
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Crashing functions
 * COPYRIGHT:   Copyright 2018-2020 Mark Jansen (mark.jansen@reactos.org)
 */

#include "stdafx.h"
#include <signal.h>

void BADAPP_EXPORT CallNullptrFN(void)
{
    // Hide from the compiler that we are calling a nullptr
    Action Proc = (Action)GetProcAddress(0, "__this_function_does_not_exist_but_my_compiler_doesnt_know_that__");

    Proc();
    if (Proc == CallNullptrFN)
        Output(L"Done.");   // Prevent tail call optimalization
}

void BADAPP_EXPORT ReadNullptrFN(void)
{
    PDWORD DwordPtr = (PDWORD)NULL;
    DWORD Value = *DwordPtr;
    Output(L"Value: %u", Value);
}

void BADAPP_EXPORT WriteNullptrFN(void)
{
    PDWORD DwordPtr = (PDWORD)NULL;
    *DwordPtr = 0x112233;
}

void BADAPP_EXPORT StackOverflowFN(void)
{
    // Suppress warnings / hide from the compiler what we are doing here
    Action recurse = (Action)GetProcAddress(0, "StackOverflowFN");
    if (recurse == NULL)
        recurse = StackOverflowFN;

    recurse();
    if (recurse == CallNullptrFN)
        Output(L"Done.");   // Prevent tail call optimalization
}

void BADAPP_EXPORT SimulateAssertionFN(void)
{
    int (__cdecl* p_raise)(_In_ int _Signal);
    void (__cdecl* p__exit)(_In_ int _Code);

    HMODULE mod = GetModuleHandleW(L"msvcrt.dll");
    assert(mod != 0);

    if (mod)
    {
        p_raise = (int (__cdecl *)(int))GetProcAddress(mod, "raise");
        if (p_raise)
        {
            p__exit = (void (__cdecl *)(int))GetProcAddress(mod, "_exit");
            if (p__exit)
            {
                p_raise(SIGABRT);
                p__exit(3);
                // Unreachable
                assert(0);
            }
            else
            {
                Output(L"Unable to resolve msvcrt!_exit");
            }
        }
        else
        {
            Output(L"Unable to resolve msvcrt!raise");
        }
    }
    else
    {
        Output(L"Unable to get a reference to msvcrt.dll");
    }
}

static BAD_ACTION g_Actions[] =
{
    {
        L"Call nullptr",
        L"Crash by calling a nullptr",
        CallNullptrFN,
        BadIcon
    },
    {
        L"Read nullptr",
        L"Crash by reading from a nullptr",
        ReadNullptrFN,
        BadIcon
    },
    {
        L"Write nullptr",
        L"Crash by writing to a nullptr",
        WriteNullptrFN,
        BadIcon
    },
    {
        L"Stack overflow",
        L"Crash by causing a stack overflow",
        StackOverflowFN,
        BadIcon
    },
    {
        L"AssertionAbort",
        L"Simulate the 'abort' button on an assertion dialog",
        SimulateAssertionFN,
        BadIcon
    },
    { NULL, NULL },
};

static BAD_ACTION g_Category =
{
    L"Crashes",
    L"Apply various techniques to crash BadApp.",
    NULL
};

void BADAPP_EXPORT Crash_Init(void)
{
    Register_Category(&g_Category, g_Actions);
}
