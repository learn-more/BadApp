/*
 * PROJECT:     BadApp
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Heap bugs
 * COPYRIGHT:   Copyright 2018 Mark Jansen (mark.jansen@reactos.org)
 */

#include "stdafx.h"

static HANDLE g_Heap1;
static HANDLE g_Heap2;

static
void NormalFreeFN(PVOID Allocation)
{
    HeapFree(g_Heap1, 0, Allocation);
}

static
void UseAfterFreeFN(PVOID Allocation)
{
    PBYTE ptr = (PBYTE)Allocation;
    HeapFree(g_Heap1, 0, Allocation);
    ptr[0] = 0x11;
}

static
void DoubleFreeFN(PVOID Allocation)
{
    HeapFree(g_Heap1, 0, Allocation);
    HeapFree(g_Heap1, 0, Allocation);
}

static
void WrongHeapFN(PVOID Allocation)
{
    HeapFree(g_Heap2, 0, Allocation);
}

static
void OverflowFN(PVOID Allocation)
{
    PDWORD ptr = (PDWORD)Allocation;
    ptr[0x10 / sizeof(DWORD)] = 0x11223344;
    HeapFree(g_Heap1, 0, Allocation);
}

void Heap_Execute(HeapAction Action)
{
    PVOID Allocation = HeapAlloc(g_Heap1, 0, 0x10);

    Output(L"Alloc(Heap1, 0x10)=%p", Allocation);

    switch (Action)
    {
    case NormalAllocation:
        Output(L"Normal free");
        Schedule(NormalFreeFN, Allocation);
        break;
    case UseAfterFree:
        Output(L"Use after free");
        Schedule(UseAfterFreeFN, Allocation);
        break;
    case DoubleFree:
        Output(L"Double free");
        Schedule(DoubleFreeFN, Allocation);
        break;
    case FreeWrongHeap:
        Output(L"Wrong heap");
        Schedule(WrongHeapFN, Allocation);
        break;
    case HeapOverflow:
        Output(L"Overflow");
        Schedule(OverflowFN, Allocation);
        break;
    default:
        assert(0);
    }
}

void Heap_Init()
{
    g_Heap1 = HeapCreate(0, 0, 0);
    g_Heap2 = HeapCreate(0, 0, 0);
    Output(L"Heap1: %p", g_Heap1);
    Output(L"Heap2: %p", g_Heap2);
}
