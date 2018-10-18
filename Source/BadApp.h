/*
 * PROJECT:     BadApp
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Shared functions
 * COPYRIGHT:   Copyright 2018 Mark Jansen (mark.jansen@reactos.org)
 */

#pragma once

#include "resource.h"

typedef void(*fn)(PVOID data);
void Schedule(fn pfn, PVOID Allocation);

void Output(wchar_t const* const _Format, ...);
void xwprintf(wchar_t *_Dest, size_t _Count, wchar_t const* const _Format, ...);

void Diag_Init();
BOOL RelaunchAsAdmin();
BOOL IsRunAsAdmin();
void ResetFTHState();
void EnableWER(BOOL bAllUsers);
void DisableWER(BOOL bAllUsers);

typedef enum HeapAction
{
    NormalAllocation,
    UseAfterFree,
    DoubleFree,
    FreeWrongHeap,
    HeapOverflow,
} HeapAction;

void Heap_Execute(HeapAction Action);
void Heap_Init();

typedef enum CrashAction
{
    CallNullptr,
    ReadNullptr,
    WriteNullptr,
    StackOverflow
} CrashAction;

void Crash_Execute(CrashAction Action);

