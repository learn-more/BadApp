/*
 * PROJECT:     BadApp
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Entrypoint / UI
 * COPYRIGHT:   Copyright 2018 Mark Jansen (mark.jansen@reactos.org)
 */

#include "stdafx.h"
#include "version.h"
#include <Shellapi.h>
#include <CommCtrl.h>
#define COBJMACROS
#include <Wincodec.h>
#pragma comment(lib, "comctl32.lib")

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


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

    //int Index = (int)SendMessageW(g_Listbox, LB_ADDSTRING, 0L, (LPARAM)buf);
    //SendMessageW(g_Listbox, LB_SETCURSEL, Index, 0L);
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

typedef struct _BAD_CATEGORY
{
    BAD_ACTION Info;
    HTREEITEM hItem;
    BAD_ACTION* Actions;
} BAD_CATEGORY;

#define MAX_NUMBER_OF_CATEGORY  5
BAD_CATEGORY g_Category[MAX_NUMBER_OF_CATEGORY + 1] = {0};

void Register_Category(BAD_ACTION* Name, BAD_ACTION* Actions)
{
    DWORD n;

    assert(Name->Execute == NULL);

    for (n = 0; Actions[n].Name; ++n)
    {
        assert(Actions[n].Execute != NULL);
    }

    for (n = 0; n < MAX_NUMBER_OF_CATEGORY; ++n)
    {
        if (!g_Category[n].Actions)
        {
            g_Category[n].Info = *Name;
            g_Category[n].Actions = Actions;
            return;
        }
    }

    Output(L"Increase MAX_NUMBER_OF_CATEGORY");
    assert(0);
}

static
HTREEITEM InsertTV(HWND hwndTV, BAD_ACTION* Item, HTREEITEM Parent, HTREEITEM InsertAfter)
{
    TVINSERTSTRUCTW tvins = {0};
    HTREEITEM hItem;
    tvins.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvins.item.pszText = (LPWSTR)Item->Name;
    tvins.item.lParam = (LPARAM)Item;
    tvins.hParent = Parent;
    tvins.hInsertAfter = InsertAfter;
    hItem = (HTREEITEM)SendMessageW(hwndTV, TVM_INSERTITEMW, 0, (LPARAM)&tvins);
    return hItem;
}

static HWND g_hTreeView;
static HWND g_hEdit;
static HWND g_hGripper;

#define TV_WIDTH    200

static
void OnSize(HWND hWnd)
{
    HDWP hdwp;
    RECT rc;
    LONG width;
    DWORD dwFlags = SWP_NOCOPYBITS | SWP_NOZORDER | SWP_NOACTIVATE;

    GetClientRect(hWnd, &rc);
    width = (rc.right - rc.left) - TV_WIDTH;

    hdwp = BeginDeferWindowPos(3);

    if (hdwp)
        hdwp = DeferWindowPos(hdwp, g_hTreeView, NULL, rc.left, rc.top, TV_WIDTH, rc.bottom - rc.top, dwFlags);

    rc.left += TV_WIDTH;

    if (hdwp)
        hdwp = DeferWindowPos(hdwp, g_hEdit, NULL, rc.left, rc.top, width, (rc.bottom - rc.top) / 2, dwFlags);

    /* Combo box */
    /* Execute button */
    /* Output / log window? */

    if (hdwp)
    {
        int cx = GetSystemMetrics(SM_CXVSCROLL);
        int cy = GetSystemMetrics(SM_CYHSCROLL);
        hdwp = DeferWindowPos(hdwp, g_hGripper, HWND_TOP, rc.right - cx, rc.bottom - cy, cx, cy, dwFlags);
    }

    EndDeferWindowPos(hdwp);
}

static
void CreateTV(HWND hWnd, HINSTANCE hInstance)
{
    DWORD n, j;
    DWORD dwTvStyle = TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_INFOTIP;
    HTREEITEM hPrevCat = (HTREEITEM)TVI_FIRST;

    g_hTreeView = CreateWindowExW(0, WC_TREEVIEW, L"Hello :)", WS_VISIBLE | WS_CHILD | WS_BORDER | dwTvStyle,
                                  0, 0, 0, 0, hWnd, NULL, hInstance, NULL);
    SendMessage(g_hTreeView, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), (LPARAM)TRUE);

    for (n = 0; g_Category[n].Actions; ++n)
    {
        hPrevCat = InsertTV(g_hTreeView, &g_Category[n].Info, TVI_ROOT, hPrevCat);
        g_Category[n].hItem = hPrevCat;

        BAD_ACTION* Action = g_Category[n].Actions;
        HTREEITEM hPrevAction = (HTREEITEM)TVI_FIRST;
        for (j = 0; Action[j].Name; ++j)
        {
            hPrevAction = InsertTV(g_hTreeView, &Action[j], hPrevCat, hPrevAction);
        }
    }
}

static
void OnCreate(HWND hWnd)
{
    HINSTANCE hInstance = GetModuleHandleW(NULL);

    CreateTV(hWnd, hInstance);

    g_hEdit = CreateWindowExW(WS_EX_NOPARENTNOTIFY | WS_EX_CLIENTEDGE, WC_EDIT, NULL,
                              WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                              0, 0, 0, 0, hWnd, NULL, hInstance, NULL);
    SendMessage(g_hEdit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), (LPARAM)TRUE);

    g_hGripper = CreateWindowExW(0, L"Scrollbar", L"size", WS_CHILD | WS_VISIBLE | SBS_SIZEGRIP,
                                 0, 0, 0, 0, hWnd, NULL, hInstance, NULL);

}

static
void OnTreeviewNotify(HWND hWnd, LPNMTREEVIEWW lTreeview)
{
    LPNMTVGETINFOTIPW pTip;
    BAD_ACTION* Action;
    TVITEMW tvi;

    switch (lTreeview->hdr.code)
    {
    case TVN_SELCHANGEDW:
        if (!(lTreeview->itemNew.mask & TVIF_PARAM))
        {
            Output(L"Got tvitem without PARAM!");
            DebugBreak();
        }
        Action = (BAD_ACTION*)lTreeview->itemNew.lParam;
        if (Action->Description && Action->Description[0])
            SetWindowTextW(g_hEdit, Action->Description);
        else
            SetWindowTextW(g_hEdit, L"");
        break;
    case TVN_GETINFOTIP:
        pTip = (LPNMTVGETINFOTIPW)lTreeview;
        Action = (BAD_ACTION*)pTip->lParam;
        if (Action->Tooltip && Action->Tooltip[0])
            StringCchCopyW(pTip->pszText, pTip->cchTextMax, Action->Tooltip);
        break;
    case NM_DBLCLK:
        ZeroMemory(&tvi, sizeof(tvi));
        tvi.mask = TVIF_PARAM;
        tvi.hItem = (HTREEITEM)SendMessageW(g_hTreeView, TVM_GETNEXTITEM, TVGN_CARET, 0L);
        if (tvi.hItem && SendMessageW(g_hTreeView, TVM_GETITEMW, 0L, (LPARAM)&tvi))
        {
            Action = (BAD_ACTION*)tvi.lParam;
            Action->Execute();
        }
        break;
    }
}

static
INT_PTR CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    switch (Msg)
    {
    case WM_CREATE:
        OnCreate(hWnd);
        break;
    case WM_SIZE:
        OnSize(hWnd);
        break;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)lParam)->ptMinTrackSize.x = 400;
        ((MINMAXINFO *)lParam)->ptMinTrackSize.y = 300;
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
            //case IDM_ABOUT:
            //    DialogBoxW(hInst, MAKEINTRESOURCEW(IDD_ABOUTBOX), hWnd, About);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_NOTIFY:
        if (((LPNMHDR)lParam)->hwndFrom == g_hTreeView)
            OnTreeviewNotify(hWnd, (LPNMTREEVIEWW)lParam);
        break;
    default:
        break;
    }
    return DefWindowProcW(hWnd, Msg, wParam, lParam);
}

static
BOOL RegisterWndClass(HINSTANCE hInstance, LPCWSTR ClassName)
{
    WNDCLASSEX wc = {0};

    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BADAPP));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = ClassName;
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BADAPP));

    if (!RegisterClassExW(&wc))
    {
        MessageBoxW(NULL, L"Window Registration Failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return FALSE;
    }

    return TRUE;
}

static
HWND CreateBadWindow(HINSTANCE hInstance, LPCWSTR ClassName)
{
    HWND hWnd;
    WCHAR Buffer[50];

    xwprintf(Buffer, _countof(Buffer),
             L"BadApp %S%s",
             GIT_VERSION_STR,
             IsRunAsAdmin() ? L" (Administrator)" : L"");

    hWnd = CreateWindowExW(WS_EX_CLIENTEDGE, ClassName, Buffer, WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, 600, 400, NULL, NULL, hInstance, NULL);

    if (hWnd == NULL)
    {
        MessageBoxW(NULL, L"Window Creation Failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
    }

    return hWnd;
}


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    HWND hWnd;
    MSG Msg;
    BOOL bRet;
    const LPCWSTR lpClassName = L"BadAppClass";

    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    hInstance = GetModuleHandleW(NULL);
    InitCommonControls();

    Crash_Init();
    Heap_Init();
    Diag_Init();

    if (!RegisterWndClass(hInstance, lpClassName))
    {
        ExitProcess(-1);
    }

    if (!(hWnd = CreateBadWindow(hInstance, lpClassName)))
    {
        ExitProcess(-2);
    }

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    while((bRet = GetMessageW(&Msg, NULL, 0, 0)) != 0)
    {
        if (bRet == -1)
        {
            DestroyWindow(hWnd);
            ExitProcess(-3);
        }
        TranslateMessage(&Msg);
        DispatchMessageW(&Msg);
    }
    ExitProcess(Msg.wParam);
}

