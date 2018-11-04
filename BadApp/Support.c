/*
* PROJECT:     BadApp
* LICENSE:     MIT (https://spdx.org/licenses/MIT)
* PURPOSE:     Various support functions
* COPYRIGHT:   Copyright 2018 Mark Jansen (mark.jansen@reactos.org)
*/

#include "stdafx.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

static int (__cdecl* p_vsnwprintf)(wchar_t *_Dest, size_t _Count, const wchar_t *_Format, va_list _Args);
int BADAPP_EXPORT vsnwprintf_(wchar_t *_Dest, size_t _Count, const wchar_t *_Format, va_list _Args)
{
    if (!p_vsnwprintf)
    {
        HMODULE mod = GetModuleHandleW(L"ntdll.dll");
        p_vsnwprintf = (int (__cdecl*)(wchar_t *, size_t, const wchar_t *, va_list))GetProcAddress(mod, "_vsnwprintf");
    }
    return p_vsnwprintf(_Dest, _Count, _Format, _Args);
}

void BADAPP_EXPORT Output(wchar_t const* const _Format, ...)
{
    va_list _ArgList;
    va_start(_ArgList, _Format);

    wchar_t buf[1024];
    vsnwprintf_(buf, _countof(buf), _Format, _ArgList);
    va_end(_ArgList);
    StringCchCatW(buf, _countof(buf), L"\r\n");
    OutputDebugStringW(buf);

    Gui_AddOutput(buf);
}

void BADAPP_EXPORT xwprintf(wchar_t *_Dest, size_t _Count, wchar_t const* const _Format, ...)
{
    va_list _ArgList;
    va_start(_ArgList, _Format);

    vsnwprintf_(_Dest, _Count, _Format, _ArgList);
    va_end(_ArgList);
}

void* memset(void *s, int c, size_t len)
{
    unsigned char *dst = s;
    while (len > 0) {
        *dst = (unsigned char) c;
        dst++;
        len--;
    }
    return s;
}

unsigned long (__cdecl *p_wcstoul)(const wchar_t* str, wchar_t** endptr, int base);
unsigned long BADAPP_EXPORT wcstoul_(const wchar_t* str, wchar_t** endptr, int base)
{
    if (!p_wcstoul)
        p_wcstoul = (unsigned long (__cdecl *)(const wchar_t *,wchar_t **,int))GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wcstoul");

    return p_wcstoul(str, endptr, base);
}

LPCWSTR BADAPP_EXPORT wcspbrk_(LPCWSTR Source, LPCWSTR Find)
{
    for (;*Source; ++Source)
    {
        LPCWSTR FindCur;
        for (FindCur = Find; *FindCur; ++FindCur)
        {
            if (*FindCur == *Source)
                return Source;
        }
    }
    return NULL;
}

