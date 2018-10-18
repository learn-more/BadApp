/*
 * PROJECT:     BadApp
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Entrypoint / UI
 * COPYRIGHT:   Copyright 2018 Mark Jansen (mark.jansen@reactos.org)
 */

#include "stdafx.h"
#include "version.h"
#include <Shellapi.h>
#define COBJMACROS
#include <Wincodec.h>

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

static UINT g_ExecMessage;
static HWND g_Dialog;
static HWND g_Listbox;
static BOOL g_UseThread = FALSE;


static int (__cdecl* p_vsnwprintf)(wchar_t *_Dest, size_t _Count, const wchar_t *_Format, va_list _Args);
static int vsnwprintf_(wchar_t *_Dest, size_t _Count, const wchar_t *_Format, va_list _Args)
{
    if (!p_vsnwprintf)
    {
        HMODULE mod = GetModuleHandleW(L"ntdll.dll");
        p_vsnwprintf = (int (__cdecl*)(wchar_t *, size_t, const wchar_t *, va_list))GetProcAddress(mod, "_vsnwprintf");
    }
    return p_vsnwprintf(_Dest, _Count, _Format, _Args);
}

void Output(wchar_t const* const _Format, ...)
{
    va_list _ArgList;
    va_start(_ArgList, _Format);

    wchar_t buf[1024];
    vsnwprintf_(buf, _countof(buf), _Format, _ArgList);
    va_end(_ArgList);
    OutputDebugStringW(buf);
    OutputDebugStringW(L"\r\n");

    int Index = (int)SendMessageW(g_Listbox, LB_ADDSTRING, (WPARAM)NULL, (LPARAM)buf);
    SendMessageW(g_Listbox, LB_SETCURSEL, Index, (LPARAM)NULL);
}

void xwprintf(wchar_t *_Dest, size_t _Count, wchar_t const* const _Format, ...)
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

typedef struct ThreadArg
{
    fn Func;
    PVOID Allocation;
} ThreadArg;

static DWORD WINAPI ThreadProc(LPVOID pArg)
{
    ThreadArg* Arg = (ThreadArg*)pArg;
    Arg->Func(Arg->Allocation);
    LocalFree(Arg);
    Output(L"Done.");
    return 0;
}

void Schedule(fn pfn, PVOID Allocation)
{
    RedrawWindow(g_Listbox, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    if (g_UseThread)
    {
        ThreadArg* arg;
        arg = LocalAlloc(LMEM_ZEROINIT , sizeof(*arg));
        arg->Func = pfn;
        arg->Allocation = Allocation;
        HANDLE hThread = CreateThread(NULL, 0, ThreadProc, arg, 0, NULL);
        if (hThread)
            CloseHandle(hThread);
        else
            Output(L"Failed to create thread");
    }
    else
    {
        PostMessageW(g_Dialog, g_ExecMessage, (WPARAM)Allocation, (LPARAM)pfn);
    }
}

static
void AddBitmapToMenuItem(HMENU hmenu, int iItem, BOOL fByPosition, HBITMAP hbmp)
{
    MENUITEMINFO mii = { sizeof(mii) };
    mii.fMask = MIIM_BITMAP;
    mii.hbmpItem = hbmp;
    SetMenuItemInfoW(hmenu, iItem, fByPosition, &mii);
}

static
HBITMAP CreateHBITMAP(int cx, int cy, void **ppvBits)
{
    HBITMAP hbmp;
    BITMAPINFO bmi = {0};

    HDC hdc = GetDC(NULL);

    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biCompression = BI_RGB;

    bmi.bmiHeader.biWidth = cx;
    bmi.bmiHeader.biHeight = cy;
    bmi.bmiHeader.biBitCount = 32;

    hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, ppvBits, NULL, 0);

    ReleaseDC(NULL, hdc);

    return hbmp;
}

static
HBITMAP GetBitmap(HICON hIcon)
{
    IWICImagingFactory* pFactory;
    IWICBitmap* pBitmap;
    HBITMAP hbmp;
    UINT cx, cy;
    BYTE *pbBuffer;

    HRESULT hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, &pFactory);
    if (!SUCCEEDED(hr))
        return NULL;

    hr = IWICImagingFactory_CreateBitmapFromHICON(pFactory, hIcon, &pBitmap);
    IUnknown_Release(pFactory);
    pFactory = NULL;

    if (!SUCCEEDED(hr))
        return NULL;

    hr = IWICBitmap_GetSize(pBitmap, &cx, &cy);
    if (!SUCCEEDED(hr))
    {
        IUnknown_Release(pBitmap);
        return NULL;
    }

    hbmp = CreateHBITMAP((int)cx, -(int)cy, &pbBuffer);
    if (hbmp)
    {
        const UINT cbStride = cx * sizeof(DWORD);
        const UINT cbBuffer = cy * cbStride;
        IWICBitmap_CopyPixels(pBitmap, NULL, cbStride, cbBuffer, pbBuffer);
    }

    IUnknown_Release(pBitmap);
    return hbmp;
}

typedef HRESULT (STDAPICALLTYPE *SHGetStockIconInfoProc)(SHSTOCKICONID siid, UINT uFlags, __inout SHSTOCKICONINFO *psii);
static SHGetStockIconInfoProc pSHGetStockIconInfo;
static HMODULE g_hShell;

static
HBITMAP GetStockBitmap()
{
    SHSTOCKICONINFO sii;
    HBITMAP hBitmap;

    if (!pSHGetStockIconInfo)
    {
        if (!g_hShell)
            g_hShell = LoadLibraryW(L"shell32.dll");
        pSHGetStockIconInfo = (SHGetStockIconInfoProc)GetProcAddress(g_hShell, "SHGetStockIconInfo");
        if (!pSHGetStockIconInfo)
            return NULL;
    }

    sii.cbSize = sizeof(sii);
    pSHGetStockIconInfo(SIID_SHIELD, SHGSI_ICON | SHGSI_SMALLICON, &sii);
    hBitmap = GetBitmap(sii.hIcon);
    DestroyIcon(sii.hIcon);
    return hBitmap;
}

static
void OnShowMenu(HWND hDlg)
{
    int Cmd;
    RECT rc;
    HMENU hMenu = LoadMenuW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDR_POPUPMENU));
    HMENU hPopup = GetSubMenu(hMenu, 0);

    HBITMAP hBitmap = GetStockBitmap();

    if (IsRunAsAdmin())
    {
        RemoveMenu(hPopup, ID_POPUP_RELAUNCHASADMIN, MF_BYCOMMAND);
    }
    else if (hBitmap)
    {
        AddBitmapToMenuItem(hPopup, ID_POPUP_RELAUNCHASADMIN, FALSE, hBitmap);
        AddBitmapToMenuItem(hPopup, ID_POPUP_RESETGLOBALFTHSTATE, FALSE, hBitmap);
    }

    GetWindowRect(GetDlgItem(hDlg, IDC_MENU), &rc);

    SetForegroundWindow(hDlg);
    Cmd = (int)TrackPopupMenu(hPopup, TPM_NONOTIFY | TPM_RETURNCMD, rc.right, rc.top, 0, hDlg, NULL);
    PostMessageW(hDlg, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
    if (hBitmap)
        DeleteObject(hBitmap);

    switch (Cmd)
    {
    case ID_POPUP_RELAUNCHASADMIN:
        if (RelaunchAsAdmin())
            EndDialog(hDlg, 0);
        break;
    case ID_POPUP_RESETGLOBALFTHSTATE:
        ResetFTHState();
        break;
    case ID_POPUP_ENABLEWER:
        EnableWER(TRUE);
        EnableWER(FALSE);
        break;
    case ID_POPUP_DISABLEWER:
        DisableWER(TRUE);
        DisableWER(FALSE);
        break;
    }
}


static
void OnInitDialog(HWND hDlg)
{
    WCHAR Buffer[120];
    g_Dialog = hDlg;
    g_Listbox = GetDlgItem(hDlg, IDC_LISTOUTPUT);
    Diag_Init();
    Heap_Init();

    xwprintf(Buffer, _countof(Buffer), L"BadApp %S%s", GIT_VERSION_STR, IsRunAsAdmin() ? L" (Administrator)" : L"");

    SetWindowTextW(hDlg, Buffer);
    CheckDlgButton(hDlg, IDC_DISP_MSG, BST_CHECKED);
}

static
INT_PTR CALLBACK WndProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (g_ExecMessage == message)
    {
        fn func = (fn)(lParam);
        func((PVOID)wParam);
        Output(L"Done.");
        return (INT_PTR)TRUE;
    }

    switch (message)
    {
    case WM_INITDIALOG:
        OnInitDialog(hDlg);
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        case IDC_HEAP_NORMAL:
        case IDC_HEAP_DOUBLE:
        case IDC_HEAP_OVERFLOW:
        case IDC_HEAP_UAF:
        case IDC_HEAP_WRONG_HEAP:
            Heap_Execute((HeapAction)(LOWORD(wParam) - IDC_HEAP_NORMAL));
            return (INT_PTR)TRUE;
        case IDC_CRASH_CALLNULL:
        case IDC_CRASH_READNULL:
        case IDC_CRASH_WRITENULL:
        case IDC_CRASH_STACKOVERFLOW:
            Crash_Execute((CrashAction)(LOWORD(wParam) - IDC_CRASH_CALLNULL));
            return (INT_PTR)TRUE;
        case IDC_MENU:
            OnShowMenu(hDlg);
            return (INT_PTR)TRUE;
        case IDC_DISP_MSG:
        case IDC_DISP_THREAD:
            g_UseThread = IsDlgButtonChecked(hDlg, IDC_DISP_THREAD) == BST_CHECKED;
            break;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    g_ExecMessage = RegisterWindowMessageW(L"BadApp_ExecLater");

    return (int)DialogBoxW(GetModuleHandleW(NULL), MAKEINTRESOURCE(IDD_MAINDLG), NULL, WndProc);
}

