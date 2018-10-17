/*
 * PROJECT:     BadApp
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Crashing functions
 * COPYRIGHT:   Copyright 2018 Mark Jansen (mark.jansen@reactos.org)
 */

#include "stdafx.h"

static
void CallNullptrFN(PVOID Ptr)
{
    union
    {
        PVOID Ptr;
        FARPROC Proc;
    } AvoidWarning;

    AvoidWarning.Ptr = Ptr;
    AvoidWarning.Proc();
}

static
void ReadNullptrFN(PVOID Ptr)
{
    PDWORD DwordPtr = (PDWORD)Ptr;
    DWORD Value = *DwordPtr;
    Output(L"Value: %u", Value);
}

static
void WriteNullptrFN(PVOID Ptr)
{
    PDWORD DwordPtr = (PDWORD)Ptr;
    *DwordPtr = 0x112233;
}

static
void StackOverflowFN(PVOID Ptr)
{
    union
    {
        PVOID Ptr;
        void (*Func)(void (*)(void*));
    } AvoidWarning;

    AvoidWarning.Ptr = Ptr;
    AvoidWarning.Func(StackOverflowFN);
}


void Crash_Execute(CrashAction Action)
{
    union
    {
        PVOID Ptr;
        fn Func;
    } AvoidWarning;

    switch (Action)
    {
    case CallNullptr:
        Output(L"Call nullptr");
        Schedule(CallNullptrFN, NULL);
        break;
    case ReadNullptr:
        Output(L"Read nullptr");
        Schedule(ReadNullptrFN, NULL);
        break;
    case WriteNullptr:
        Output(L"Write nullptr");
        Schedule(WriteNullptrFN, NULL);
        break;
    case StackOverflow:
        Output(L"Stack overflow");
        AvoidWarning.Func = StackOverflowFN;
        Schedule(StackOverflowFN, AvoidWarning.Ptr);
        break;
    default:
        assert(0);
        break;
    }
}
