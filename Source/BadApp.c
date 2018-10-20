/*
 * PROJECT:     BadApp
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Entrypoint / UI
 * COPYRIGHT:   Copyright 2018 Mark Jansen (mark.jansen@reactos.org)
 */

#include "stdafx.h"
#include "version.h"
#include <CommCtrl.h>
#pragma comment(lib, "comctl32.lib")


typedef struct _BAD_CATEGORY
{
    BAD_ACTION Info;
    HTREEITEM hItem;
    BAD_ACTION* Actions;
} BAD_CATEGORY;

typedef enum _BAD_CALLCONTEXT
{
    InvalidCC = CB_ERR,
    DirectCC = 0,
    PostMessageCC,
    CreateThreadCC,
} BAD_CALLCONTEXT;

#define MAX_NUMBER_OF_CATEGORY  5
static BAD_CATEGORY g_Category[MAX_NUMBER_OF_CATEGORY + 1] = {0};
static BAD_CALLCONTEXT g_CallContext = DirectCC;
static ULONG g_ExecMessage;
static HWND g_hTreeView;
static HIMAGELIST g_hTreeViewImagelist;
static HWND g_hEdit;
static HWND g_hGripper;
static HWND g_hCombo;

#define POS_FIXED       1
#define POS_PERCENT     2
#define POS_FOLLOW_SM   3
#define POS_FOLLOW_OBJH 4

#define FIXED_AT(n)         { POS_FIXED, n, 0, NULL }
#define PERCENTAGE(n)       { POS_PERCENT, n, 0, NULL }
#define FOLLOW_SM(n,m)        { POS_FOLLOW_SM, n, m, NULL }
#define FOLLOW_OBJH(n, h)      { POS_FOLLOW_OBJH, n, 0, h }

typedef struct _RESIZE_COORD
{
    BYTE Type;
    LONG Value;
    LONG Max;
    HWND* hOther;
} RESIZE_COORD;

static
const
struct _RESIZE_STRUCT
{
    HWND* hWindow;
    RESIZE_COORD Left;
    RESIZE_COORD Top;
    RESIZE_COORD Right;
    RESIZE_COORD Bottom;
} g_Resize[] =
{
    { &g_hTreeView, FIXED_AT(0), FIXED_AT(0), FIXED_AT(200), PERCENTAGE(100) },
    { &g_hEdit, FIXED_AT(200), FIXED_AT(0), PERCENTAGE(100), FIXED_AT(100) },
    { &g_hCombo, FIXED_AT(200), FOLLOW_OBJH(0, &g_hCombo), FOLLOW_SM(SM_CXVSCROLL,400), PERCENTAGE(100) },
    { &g_hGripper, FOLLOW_SM(SM_CXVSCROLL,0), FOLLOW_SM(SM_CYVSCROLL,0), PERCENTAGE(100), PERCENTAGE(100) },
};

LONG BADAPP_EXPORT DoCoord(HWND hObject, LONG MaxValue, const RESIZE_COORD* coord)
{
    if (coord->Type == POS_FIXED)
        return coord->Value;
    if (coord->Type == POS_PERCENT)
        return coord->Value * MaxValue / 100;
    if (coord->Type == POS_FOLLOW_SM)
    {
        LONG Value = MaxValue - GetSystemMetrics(coord->Value);
        if (coord->Max && Value > coord->Max)
            return coord->Max;
        return Value;
    }
    if (coord->Type == POS_FOLLOW_OBJH)
    {
        RECT rc;
        GetWindowRect(hObject, &rc);
        return MaxValue - (rc.bottom - rc.top) - coord->Value;
    }
    assert(0);
    return 0;
}

void BADAPP_EXPORT OnSize(HWND hWnd, WPARAM wParam)
{
    HDWP hdwp;
    RECT rcClient;
    DWORD n, dwBaseFlags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER;

    GetClientRect(hWnd, &rcClient);
    assert(rcClient.left == 0);
    assert(rcClient.top == 0);

    hdwp = BeginDeferWindowPos(_countof(g_Resize));

    for (n = 0; n < _countof(g_Resize); ++n)
    {
        RECT rc, oldRC;
        DWORD dwFlags;
        HWND hObject = *g_Resize[n].hWindow;

        rc.left = DoCoord(hObject, rcClient.right, &g_Resize[n].Left);
        rc.top = DoCoord(hObject, rcClient.bottom, &g_Resize[n].Top);
        rc.right = DoCoord(hObject, rcClient.right, &g_Resize[n].Right);
        rc.bottom = DoCoord(hObject, rcClient.bottom, &g_Resize[n].Bottom);
        GetWindowRect(hObject, &oldRC);

        dwFlags = dwBaseFlags;
        if ((rc.right - rc.left) != (oldRC.right - oldRC.left) ||
            (rc.bottom - rc.top) != (oldRC.bottom - oldRC.top))
        {
            dwFlags |= SWP_NOCOPYBITS;
        }

        if (hdwp)
            hdwp = DeferWindowPos(hdwp, hObject, NULL, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, dwFlags);
    }

    EndDeferWindowPos(hdwp);
    ShowWindow(g_hGripper, (wParam == SIZE_MAXIMIZED) ? SW_HIDE : SW_SHOW);
}

HTREEITEM BADAPP_EXPORT InsertTV(HWND hwndTV, BAD_ACTION* Item, HTREEITEM Parent, HTREEITEM InsertAfter)
{
    TVINSERTSTRUCTW tvins = {0};
    HTREEITEM hItem;
    tvins.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
    tvins.item.pszText = (LPWSTR)Item->Name;
    tvins.item.lParam = (LPARAM)Item;
    tvins.hParent = Parent;
    tvins.hInsertAfter = InsertAfter;
    tvins.item.state = INDEXTOSTATEIMAGEMASK(Item->iIcon);
    tvins.item.stateMask = TVIS_STATEIMAGEMASK;

    hItem = (HTREEITEM)SendMessageW(hwndTV, TVM_INSERTITEMW, 0, (LPARAM)&tvins);
    return hItem;
}

void BADAPP_EXPORT CreateTV(HWND hWnd, HINSTANCE hInstance)
{
    DWORD n, j;
    DWORD dwTvStyle = TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_INFOTIP;
    HTREEITEM hPrevCat = TVI_FIRST;
    LPWSTR Icons[MaxIcons] = {
        IDI_APPLICATION,    // NoIcon, but the treeview state handles index '0' as not assigned..
        IDI_APPLICATION,    // ApplicationIcon
        IDI_HAND,           // BadIcon
        IDI_ASTERISK,       // InformationIcon
        IDI_SHIELD,         // ShieldIcon
    };
    HICON hIcon;

    g_hTreeView = CreateWindowExW(0, WC_TREEVIEW, L"Hello :)", WS_VISIBLE | WS_CHILD | WS_BORDER | dwTvStyle,
                                  0, 0, 0, 0, hWnd, NULL, hInstance, NULL);
    SendMessageW(g_hTreeView, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), (LPARAM)TRUE);

    g_hTreeViewImagelist = ImageList_Create(16, 16, ILC_COLOR32, 1, 1);
    for (n = 0; n < _countof(Icons); ++n)
    {
        hIcon = LoadImage(NULL, Icons[n], IMAGE_ICON, 16, 16, LR_SHARED | LR_CREATEDIBSECTION);
        ImageList_AddIcon(g_hTreeViewImagelist, hIcon);
        DestroyIcon(hIcon);
    }

    SendMessageW(g_hTreeView, TVM_SETIMAGELIST, TVSIL_STATE, (LPARAM)g_hTreeViewImagelist);
    for (n = 0; g_Category[n].Actions; ++n)
    {
        hPrevCat = InsertTV(g_hTreeView, &g_Category[n].Info, TVI_ROOT, hPrevCat);
        g_Category[n].hItem = hPrevCat;

        BAD_ACTION* Action = g_Category[n].Actions;
        HTREEITEM hPrevAction = TVI_FIRST;
        for (j = 0; Action[j].Name; ++j)
        {
            hPrevAction = InsertTV(g_hTreeView, &Action[j], hPrevCat, hPrevAction);
        }
    }
}

void BADAPP_EXPORT OnCreate(HWND hWnd)
{
    HINSTANCE hInstance = GetModuleHandleW(NULL);

    CreateTV(hWnd, hInstance);

    g_hEdit = CreateWindowExW(WS_EX_NOPARENTNOTIFY | WS_EX_CLIENTEDGE, WC_EDITW, NULL,
                              WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                              0, 0, 0, 0, hWnd, NULL, hInstance, NULL);
    SendMessageW(g_hEdit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), (LPARAM)TRUE);

    g_hGripper = CreateWindowExW(0, L"Scrollbar", L"size", WS_CHILD | WS_VISIBLE | SBS_SIZEGRIP,
                                 0, 0, 0, 0, hWnd, NULL, hInstance, NULL);
    SendMessageW(g_hGripper, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), (LPARAM)TRUE);
    SetWindowPos(g_hGripper, HWND_TOP, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_SHOWWINDOW);

    g_hCombo = CreateWindowExW(0, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE,
                               0, 0, 0, 200, hWnd, NULL, hInstance, NULL);
    SendMessageW(g_hCombo, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), (LPARAM)TRUE);

    SendMessageW(g_hCombo, CB_ADDSTRING, 0, (LPARAM)L"Direct call");    // DirectCC
    SendMessageW(g_hCombo, CB_ADDSTRING, 0, (LPARAM)L"PostMessage");    // WindowMessageCC
    SendMessageW(g_hCombo, CB_ADDSTRING, 0, (LPARAM)L"CreateThread");   // NewThreadCC
    SendMessageW(g_hCombo, CB_SETCURSEL, (WPARAM)g_CallContext, 0);
}

/* This is only here to show up in the callstack */
void BADAPP_EXPORT OnDirectCall(BAD_ACTION* Action)
{
    Action->Execute();
}

DWORD BADAPP_EXPORT WINAPI ThreadProc(LPVOID pArg)
{
    BAD_ACTION* Action = (BAD_ACTION*)pArg;
    Action->Execute();
    return 0;
}

void BADAPP_EXPORT OnExecute(HWND hWnd, BAD_ACTION* Action)
{
    HANDLE hThread;

    switch (g_CallContext)
    {
    case DirectCC:
        OnDirectCall(Action);
        break;
    case PostMessageCC:
        PostMessageW(hWnd, g_ExecMessage, 0L, (LPARAM)Action);
        break;
    case CreateThreadCC:
        hThread = CreateThread(NULL, 0, ThreadProc, Action, 0, NULL);
        if (hThread)
            CloseHandle(hThread);
        else
            Output(L"Failed to create thread");
        break;
    default:
        assert(0);
        break;
    }
}

void BADAPP_EXPORT OnTreeviewNotify(HWND hWnd, LPNMTREEVIEWW lTreeview)
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
            assert(0);
            return;
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
            OnExecute(hWnd, Action);
        }
        break;
    }
}

LRESULT BADAPP_EXPORT CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (g_ExecMessage == Msg)
    {
        BAD_ACTION* Action = (BAD_ACTION*)lParam;
        Action->Execute();
        return 0L;
    }

    switch (Msg)
    {
    case WM_CREATE:
        OnCreate(hWnd);
        break;
    case WM_SIZE:
        OnSize(hWnd, wParam);
        break;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)lParam)->ptMinTrackSize.x = 400;
        ((MINMAXINFO *)lParam)->ptMinTrackSize.y = 300;
        break;
    case WM_COMMAND:
        if (lParam == (LPARAM)g_hCombo && HIWORD(wParam) == CBN_SELCHANGE)
        {
            g_CallContext = (BAD_CALLCONTEXT)SendMessageW(g_hCombo, CB_GETCURSEL, 0L, 0L);
            if (g_CallContext == InvalidCC)
                g_CallContext = DirectCC;
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

BOOL BADAPP_EXPORT RegisterWndClass(HINSTANCE hInstance, LPCWSTR ClassName)
{
    WCHAR Buffer[50];
    WNDCLASSEX wc = {0};

    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadImageW(hInstance, MAKEINTRESOURCE(IDI_BADAPP), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = ClassName;

    if (!RegisterClassExW(&wc))
    {
        xwprintf(Buffer, _countof(Buffer), L"Window Registration Failed: %u", GetLastError());
        MessageBoxW(NULL, Buffer, L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return FALSE;
    }

    return TRUE;
}

HWND BADAPP_EXPORT CreateBadWindow(HINSTANCE hInstance, LPCWSTR ClassName)
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
        xwprintf(Buffer, _countof(Buffer), L"Window Creation Failed: %u", GetLastError());
        MessageBoxW(NULL, Buffer, L"Error!", MB_ICONEXCLAMATION | MB_OK);
    }

    return hWnd;
}


void BADAPP_EXPORT Register_Category(BAD_ACTION* Name, BAD_ACTION* Actions)
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

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    HWND hWnd;
    MSG Msg;
    BOOL bRet;
    const LPCWSTR lpClassName = L"BadAppClass";

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    hInstance = GetModuleHandleW(NULL);
    g_ExecMessage = RegisterWindowMessageW(L"BadAppExecLaterMSG");
    InitCommonControls();

    Crash_Init();
    Heap_Init();
    Diag_Init();

    if (!RegisterWndClass(hInstance, lpClassName))
    {
        ExitProcess(GetLastError());
    }

    hWnd = CreateBadWindow(hInstance, lpClassName);
    if (!hWnd)
    {
        ExitProcess(GetLastError());
    }

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    while((bRet = GetMessageW(&Msg, NULL, 0, 0)) != 0)
    {
        if (bRet == -1)
        {
            DestroyWindow(hWnd);
            ExitProcess(GetLastError());
        }
        TranslateMessage(&Msg);
        DispatchMessageW(&Msg);
    }
    ExitProcess((UINT)Msg.wParam);
}

