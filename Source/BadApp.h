/*
 * PROJECT:     BadApp
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Shared functions
 * COPYRIGHT:   Copyright 2018 Mark Jansen (mark.jansen@reactos.org)
 */

#pragma once

#include "resource.h"

void Output(wchar_t const* const _Format, ...);
void xwprintf(wchar_t *_Dest, size_t _Count, wchar_t const* const _Format, ...);

typedef void (*Action)(void);

typedef struct _BAD_ACTION
{
    LPCWSTR Name;
    LPCWSTR Tooltip;
    LPCWSTR Description;
    Action Execute;
} BAD_ACTION;


BOOL IsRunAsAdmin(void);
void Register_Category(BAD_ACTION* Name, BAD_ACTION* Actions);

void Crash_Init(void);
void Heap_Init(void);
void Diag_Init(void);

