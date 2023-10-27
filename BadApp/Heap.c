/*
 * PROJECT:     BadApp
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Heap bugs
 * COPYRIGHT:   Copyright 2018 Mark Jansen (mark.jansen@reactos.org)
 */

#include "stdafx.h"

static HANDLE g_Heap1;
static HANDLE g_Heap2;

#define ALLOCATION_SIZE     0x0d

PVOID BADAPP_EXPORT DoAllocation(void)
{
    PVOID Allocation = HeapAlloc(g_Heap1, 0, ALLOCATION_SIZE);
    Output(L"Alloc(Heap1, 0x%x)=%p", ALLOCATION_SIZE, Allocation);
    return Allocation;
}

void BADAPP_EXPORT NormalFree1FN(void)
{
    PVOID Allocation = DoAllocation();
    HeapFree(g_Heap1, 0, Allocation);
}

void BADAPP_EXPORT NormalFree2FN(void)
{
    PVOID Allocation = HeapAlloc(g_Heap2, 0, ALLOCATION_SIZE);
    Output(L"Alloc(Heap2, 0x%x)=%p", ALLOCATION_SIZE, Allocation);
    HeapFree(g_Heap2, 0, Allocation);
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
    ptr[ALLOCATION_SIZE] = 0x11;
    HeapFree(g_Heap1, 0, Allocation);
}

void BADAPP_EXPORT LeakFN(void)
{
    DoAllocation();
}

static BAD_ACTION g_Actions[] =
{
    {
        L"Normal alloc Heap1",
        L"Allocate and free memory using HeapAlloc and HeapFree. This can be used to trigger checks in the heap functions. This function operates on Heap1.",
        NormalFree1FN,
        NoIcon
    },
    {
        L"Normal alloc Heap2",
        L"Allocate and free memory using HeapAlloc and HeapFree. This can be used to trigger checks in the heap functions. This function operates on Heap1.",
        NormalFree2FN,
        NoIcon
    },
    {
        L"Use after free",
        L"Free memory using HeapFree, and modify it afterwards.",
        UseAfterFreeFN,
        BadIcon
    },
    {
        L"Double free",
        L"Free memory twice using HeapFree.",
        DoubleFreeFN,
        BadIcon
    },
    {
        L"Free wrong heap",
        L"Allocate memory using HeapAlloc from Heap1, then free it using HeapFree from Heap2.",
        WrongHeapFN,
        BadIcon
    },
    {
        L"Buffer overflow",
        L"Write 2 bytes past the allocated space, then free the memory using HeapFree.",
        OverflowFN,
        BadIcon
    },
    {
        L"Leak allocation",
        L"Allocate memory, but never free it.",
        LeakFN,
        BadIcon
    },
    { NULL },
};

static BAD_ACTION g_Category =
{
    L"Heap",
    L"Trigger various heap bugs",
    NULL,
    NoIcon
};

void BADAPP_EXPORT Heap_Init(void)
{
    g_Heap1 = HeapCreate(0, 0, 0);
    g_Heap2 = HeapCreate(0, 0, 0);

    Register_Category(&g_Category, g_Actions);
    Output(L"Heap1: %p", g_Heap1);
    Output(L"Heap2: %p", g_Heap2);
}


