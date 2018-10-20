/*
 * PROJECT:     BadApp
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Heap bugs
 * COPYRIGHT:   Copyright 2018 Mark Jansen (mark.jansen@reactos.org)
 */

#include "stdafx.h"

static HANDLE g_Heap1;
static HANDLE g_Heap2;

PVOID BADAPP_EXPORT DoAllocation(void)
{
    PVOID Allocation = HeapAlloc(g_Heap1, 0, 0x10);
    //Output(L"Alloc(Heap1, 0x10)=%p", Allocation);
    return Allocation;
}

void BADAPP_EXPORT NormalFreeFN(void)
{
    PVOID Allocation = DoAllocation();
    HeapFree(g_Heap1, 0, Allocation);
}

void BADAPP_EXPORT UseAfterFreeFN(void)
{
    PVOID Allocation = DoAllocation();
    PBYTE ptr = (PBYTE)Allocation;
    HeapFree(g_Heap1, 0, Allocation);
    ptr[0] = 0x11;
}

void BADAPP_EXPORT DoubleFreeFN(void)
{
    PVOID Allocation = DoAllocation();
    HeapFree(g_Heap1, 0, Allocation);
    HeapFree(g_Heap1, 0, Allocation);
}

void BADAPP_EXPORT WrongHeapFN(void)
{
    PVOID Allocation = DoAllocation();
    HeapFree(g_Heap2, 0, Allocation);
}

void BADAPP_EXPORT OverflowFN(void)
{
    PVOID Allocation = DoAllocation();
    PBYTE ptr = (PBYTE)Allocation;
    ptr[0x10] = 0x11;
    ptr[0x11] = 0x22;
    HeapFree(g_Heap1, 0, Allocation);
}

static BAD_ACTION g_Heap[] =
{
    {
        L"Normal alloc",
        L"Allocate and free memory",
        L"Allocate and free memory using HeapAlloc and HeapFree. This can be used to trigger checks in the heap functions.",
        NormalFreeFN,
        NoIcon
    },
    {
        L"Use after free",
        L"Modify memory after it has been freed",
        L"Free memory using HeapFree, and modify it afterwards.",
        UseAfterFreeFN,
        BadIcon
    },
    {
        L"Double free",
        L"Free memory twice",
        L"Free memory twice using HeapFree.",
        DoubleFreeFN,
        BadIcon
    },
    {
        L"Free wrong heap",
        L"Free allocation from another heap as it was allocated from",
        L"Allocate memory using HeapAlloc from Heap1, then free it using HeapFree from Heap2.",
        WrongHeapFN,
        BadIcon
    },
    {
        L"Buffer overflow",
        L"Write more data than was allocated",
        L"Write 2 bytes past the allocated space, then free the memory using HeapFree.",
        OverflowFN,
        BadIcon
    },
    { NULL },
};

static BAD_ACTION g_HeapCategory =
{
    L"Heap",
    L"Various heap bugs",
    L"Trigger various heap bugs",
    NULL,
    NoIcon
};

void BADAPP_EXPORT Heap_Init(void)
{
    g_Heap1 = HeapCreate(0, 0, 0);
    g_Heap2 = HeapCreate(0, 0, 0);

    Register_Category(&g_HeapCategory, g_Heap);
}

