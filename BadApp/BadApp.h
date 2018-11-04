/*
 * PROJECT:     BadApp
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Shared functions
 * COPYRIGHT:   Copyright 2018 Mark Jansen (mark.jansen@reactos.org)
 */

#pragma once

#include "resource.h"

/* Export all symbols, to generate a nicer callstack without symbols */
#if 1
#define BADAPP_EXPORT    __declspec(dllexport)
#else
#define BADAPP_EXPORT
#endif


void BADAPP_EXPORT Gui_AddOutput(LPCWSTR Text);
void BADAPP_EXPORT Output(wchar_t const* const _Format, ...);
void BADAPP_EXPORT xwprintf(wchar_t *_Dest, size_t _Count, wchar_t const* const _Format, ...);
unsigned long BADAPP_EXPORT wcstoul_(const wchar_t* str, wchar_t** endptr, int base);
LPCWSTR BADAPP_EXPORT wcspbrk_(LPCWSTR Source, LPCWSTR Find);

typedef void (*Action)(void);

typedef enum _ACTION_ICON
{
    NoIcon,
    ApplicationIcon,    // IDI_APPLICATION
    BadIcon,            // IDI_HAND
    InformationIcon,    // IDI_ASTERISK
    ShieldIcon,         // IDI_SHIELD
    GithubIcon,         // IDI_GITHUB
    MaxIcons
} ACTION_ICON;

typedef struct _BAD_ACTION
{
    LPCWSTR Name;
    LPCWSTR Description;
    Action Execute;
    ACTION_ICON iIcon;
} BAD_ACTION;


BOOL BADAPP_EXPORT IsRunAsAdmin(void);
void BADAPP_EXPORT Register_Category(BAD_ACTION* Name, BAD_ACTION* Actions);

void BADAPP_EXPORT Crash_Init(void);
void BADAPP_EXPORT CriticalSection_Init(void);
void BADAPP_EXPORT Handles_Init(void);
void BADAPP_EXPORT Heap_Init(void);
void BADAPP_EXPORT Diag_Init(void);

