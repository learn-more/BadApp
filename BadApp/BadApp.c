/*
 * PROJECT:     BadApp
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Entrypoint / UI
 * COPYRIGHT:   Copyright 2018 Mark Jansen (mark.jansen@reactos.org)
 */

#include "stdafx.h"
#include "version.h"
#include <CommCtrl.h>
#include <shellapi.h>
#include <Objbase.h>


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
    RtlCreateUserThreadCC,
} BAD_CALLCONTEXT;

#define MAX_NUMBER_OF_CATEGORY  5
static BAD_CATEGORY g_Category[MAX_NUMBER_OF_CATEGORY + 1] = {0};
static BAD_CALLCONTEXT g_CallContext = DirectCC;
static ULONG g_ExecMessage;
static HWND g_hTreeView;
static HIMAGELIST g_hTreeViewImagelist;
static HWND g_hDescriptionEdit;
static HWND g_hOutputEdit;
static HWND g_hGripper;
static HWND g_hCombo;

#define POS_FIXED       1
#define POS_PERCENT     2
#define POS_FOLLOW_SM   3
#define POS_FOLLOW_OBJH 4

#define FIXED_AT(n)             { .u.s.Value = n, .u.s.Max = 0, .Type = POS_FIXED }
#define PERCENTAGE(n)           { .u.s.Value = n, .u.s.Max = 0, .Type = POS_PERCENT }
#define FOLLOW_SM(n,m)          { .u.s.Value = n, .u.s.Max = m, .Type = POS_FOLLOW_SM }
#define FOLLOW_OBJH(h)          { .u.hOther = h, .Type = POS_FOLLOW_OBJH }

typedef struct _RESIZE_COORD
{
    union
    {
        struct
        {
            WORD Value;
            WORD Max;
        } s;
        HWND* hOther;
    } u;
    BYTE Type;
} RESIZE_COORD;

#define TV_WIDTH    200

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
    { &g_hTreeView, FIXED_AT(0), FIXED_AT(0), FIXED_AT(TV_WIDTH), PERCENTAGE(100) },
    { &g_hDescriptionEdit, FIXED_AT(TV_WIDTH), FIXED_AT(0), PERCENTAGE(100), FIXED_AT(100) },
    { &g_hCombo, FIXED_AT(TV_WIDTH), FOLLOW_OBJH(&g_hCombo), FOLLOW_SM(SM_CXVSCROLL,TV_WIDTH + 200), PERCENTAGE(100) },
    { &g_hOutputEdit, FIXED_AT(TV_WIDTH), FIXED_AT(100), PERCENTAGE(100), FOLLOW_OBJH(&g_hCombo) },
    { &g_hGripper, FOLLOW_SM(SM_CXVSCROLL,0), FOLLOW_SM(SM_CYVSCROLL,0), PERCENTAGE(100), PERCENTAGE(100) },
};

LONG BADAPP_EXPORT DoCoord(LONG MaxValue, const RESIZE_COORD* coord)
{
    if (coord->Type == POS_FIXED)
        return coord->u.s.Value;
    if (coord->Type == POS_PERCENT)
        return coord->u.s.Value * MaxValue / 100;
    if (coord->Type == POS_FOLLOW_SM)
    {
        LONG Value = MaxValue - GetSystemMetrics(coord->u.s.Value);
        if (coord->u.s.Max && Value > coord->u.s.Max)
            return coord->u.s.Max;
        return Value;
    }
    if (coord->Type == POS_FOLLOW_OBJH)
    {
        RECT rc;
        GetWindowRect(*coord->u.hOther, &rc);
        return MaxValue - (rc.bottom - rc.top);
    }
    assert(0);
    return 0;
}

void BADAPP_EXPORT OnSize(HWND hWnd, WPARAM wParam)
{
    HDWP hdwp;
    RECT rcClient;
    DWORD n, dwFlags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOCOPYBITS;

    GetClientRect(hWnd, &rcClient);
    assert(rcClient.left == 0);
    assert(rcClient.top == 0);

    hdwp = BeginDeferWindowPos(_countof(g_Resize));

    for (n = 0; n < _countof(g_Resize); ++n)
    {
        RECT rc;
        HWND hChild = *g_Resize[n].hWindow;

        rc.left = DoCoord(rcClient.right, &g_Resize[n].Left);
        rc.top = DoCoord(rcClient.bottom, &g_Resize[n].Top);
        rc.right = DoCoord(rcClient.right, &g_Resize[n].Right);
        rc.bottom = DoCoord(rcClient.bottom, &g_Resize[n].Bottom);

        if (hdwp)
            hdwp = DeferWindowPos(hdwp, hChild, NULL, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, dwFlags);
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

    hItem = (HTREEITEM)SendMessageW(hwndTV, TVM_INSERTITEMW, 0L, (LPARAM)&tvins);
    return hItem;
}

void BADAPP_EXPORT FillTV()
{
    DWORD n, j;
    HTREEITEM hPrevCat = TVI_FIRST;
    LPWSTR Icons[MaxIcons] = {
        IDI_APPLICATION,    // NoIcon, but the treeview state handles index '0' as not assigned..
        IDI_APPLICATION,    // ApplicationIcon
        IDI_HAND,           // BadIcon
        IDI_ASTERISK,       // InformationIcon
        IDI_SHIELD,         // ShieldIcon
    };
    HICON hIcon;

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
    DWORD dwTvStyle = TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_INFOTIP;
    HINSTANCE hInstance = GetModuleHandleW(NULL);
    LOGFONTW lf = {0};
    HFONT hFont;

    /* Treeview for all actions */
    g_hTreeView = CreateWindowExW(0, WC_TREEVIEW, L"Hello :)", WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | dwTvStyle,
                                  0, 0, 0, 0, hWnd, NULL, hInstance, NULL);
    SendMessageW(g_hTreeView, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), (LPARAM)FALSE);

    /* Show description of the selected node */
    g_hDescriptionEdit = CreateWindowExW(WS_EX_NOPARENTNOTIFY | WS_EX_CLIENTEDGE, WC_EDITW, NULL,
                                         WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                                         0, 0, 0, 0, hWnd, NULL, hInstance, NULL);
    SendMessageW(g_hDescriptionEdit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), (LPARAM)FALSE);

    /* Show output */
    g_hOutputEdit = CreateWindowExW(WS_EX_NOPARENTNOTIFY | WS_EX_CLIENTEDGE, WC_EDITW, NULL,
                                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                                    0, 0, 0, 0, hWnd, NULL, hInstance, NULL);
    lf.lfHeight = -11;
    lf.lfWeight = FW_NORMAL;
    StringCchCopyW(lf.lfFaceName, _countof(lf.lfFaceName), L"Courier New");
    hFont = CreateFontIndirectW(&lf);
    SendMessageW(g_hOutputEdit, WM_SETFONT, (WPARAM)hFont, (LPARAM)FALSE);

    /* Resize gripper */
    g_hGripper = CreateWindowExW(0, L"Scrollbar", L"size", WS_CHILD | WS_VISIBLE | SBS_SIZEGRIP,
                                 0, 0, 0, 0, hWnd, NULL, hInstance, NULL);
    SendMessageW(g_hGripper, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), (LPARAM)FALSE);
    SetWindowPos(g_hGripper, HWND_TOP, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_SHOWWINDOW);

    g_hCombo = CreateWindowExW(0, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE | WS_TABSTOP,
                               0, 0, 0, 200, hWnd, NULL, hInstance, NULL);
    SendMessageW(g_hCombo, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), (LPARAM)FALSE);

    SendMessageW(g_hCombo, CB_ADDSTRING, 0L, (LPARAM)L"Direct call");        // DirectCC
    SendMessageW(g_hCombo, CB_ADDSTRING, 0L, (LPARAM)L"PostMessage");        // WindowMessageCC
    SendMessageW(g_hCombo, CB_ADDSTRING, 0L, (LPARAM)L"CreateThread");       // NewThreadCC
    SendMessageW(g_hCombo, CB_ADDSTRING, 0L, (LPARAM)L"RtlCreateUserThread");// RtlCreateUserThreadCC
    SendMessageW(g_hCombo, CB_SETCURSEL, (WPARAM)g_CallContext, 0L);

    /* Register all handlers */
    Crash_Init();
    Handles_Init();
    Heap_Init();
    Diag_Init();

    /* Dump handlers in treeview */
    FillTV();
}

/* This is only here to show up in the callstack */
void BADAPP_EXPORT OnDirectCall(BAD_ACTION* Action)
{
    Action->Execute();
    Output(L"'%s' done.", Action->Name);
}

DWORD BADAPP_EXPORT WINAPI ThreadProcK32(LPVOID pArg)
{
    BAD_ACTION* Action = (BAD_ACTION*)pArg;
    Action->Execute();
    Output(L"'%s' from ThreadProcK32 done.", Action->Name);
    return 0;
}

DWORD BADAPP_EXPORT WINAPI ThreadProcNt(LPVOID pArg)
{
    BAD_ACTION* Action = (BAD_ACTION*)pArg;
    Action->Execute();
    Output(L"'%s' from ThreadProcNt done.", Action->Name);
    return 0;
}

typedef ULONG (NTAPI *tRtlCreateUserThread)(HANDLE ProcessHandle, PSECURITY_DESCRIPTOR SecurityDescriptor,
                                            BOOLEAN CreateSuspended, ULONG StackZeroBits, SIZE_T StackReserve,
                                            SIZE_T StackCommit, PTHREAD_START_ROUTINE StartAddress,
                                            PVOID Parameter, PHANDLE ThreadHandle, PVOID ClientId);
static tRtlCreateUserThread RtlCreateUserThread;


void BADAPP_EXPORT OnExecute(HWND hWnd, BAD_ACTION* Action)
{
    HANDLE hThread;

    switch (g_CallContext)
    {
    case DirectCC:
        Output(L"Calling '%s'", Action->Name);
        OnDirectCall(Action);
        break;
    case PostMessageCC:
        Output(L"Posting '%s'", Action->Name);
        PostMessageW(hWnd, g_ExecMessage, 0L, (LPARAM)Action);
        break;
    case CreateThreadCC:
        Output(L"CreateThread '%s'", Action->Name);
        hThread = CreateThread(NULL, 0, ThreadProcK32, Action, 0, NULL);
        if (hThread)
            CloseHandle(hThread);
        else
            Output(L"Failed to create thread");
        break;
    case RtlCreateUserThreadCC:
        if (!RtlCreateUserThread)
        {
            RtlCreateUserThread = (tRtlCreateUserThread)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlCreateUserThread");
        }
        Output(L"RtlCreateUserThread '%s'", Action->Name);
        RtlCreateUserThread(((HANDLE)(LONG_PTR)-1), NULL, 0, 0, 0, 0, ThreadProcNt, Action, NULL, NULL);
        break;
    default:
        assert(0);
        break;
    }
}

void BADAPP_EXPORT OnExecuteTVSelection(HWND hWnd)
{
    TVITEMW tvi = {0};
    BAD_ACTION* Action;

    tvi.mask = TVIF_PARAM;
    tvi.hItem = (HTREEITEM)SendMessageW(g_hTreeView, TVM_GETNEXTITEM, TVGN_CARET, 0L);
    if (tvi.hItem && SendMessageW(g_hTreeView, TVM_GETITEMW, 0L, (LPARAM)&tvi))
    {
        Action = (BAD_ACTION*)tvi.lParam;
        if (Action->Execute)
        {
            OnExecute(hWnd, Action);
        }
    }
}

void BADAPP_EXPORT OnTreeviewNotify(HWND hWnd, LPNMTREEVIEWW lTreeview)
{
    LPNMTVGETINFOTIPW pTip;
    LPNMTVKEYDOWN pKey;
    BAD_ACTION* Action;

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
            SetWindowTextW(g_hDescriptionEdit, Action->Description);
        else
            SetWindowTextW(g_hDescriptionEdit, L"");
        break;
    case TVN_GETINFOTIP:
        pTip = (LPNMTVGETINFOTIPW)lTreeview;
        Action = (BAD_ACTION*)pTip->lParam;
        if (Action->Description && Action->Description[0])
            StringCchCopyW(pTip->pszText, pTip->cchTextMax, Action->Description);
        break;
    case TVN_KEYDOWN:
        pKey = (LPNMTVKEYDOWN)lTreeview;
        if (pKey->wVKey == VK_RETURN || pKey->wVKey == VK_EXECUTE)
        {
            OnExecuteTVSelection(hWnd);
        }
        else if (pKey->wVKey == VK_SPACE)
        {
            HTREEITEM hItem = (HTREEITEM)SendMessageW(g_hTreeView, TVM_GETNEXTITEM, TVGN_CARET, 0L);
            if (hItem)
                SendMessageW(g_hTreeView, TVM_EXPAND, TVE_TOGGLE, (LPARAM)hItem);
        }
        break;
    case NM_DBLCLK:
        OnExecuteTVSelection(hWnd);
        break;
    }
}

LRESULT BADAPP_EXPORT CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (g_ExecMessage == Msg)
    {
        BAD_ACTION* Action = (BAD_ACTION*)lParam;
        Action->Execute();
        Output(L"'%s' done.", Action->Name);
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

void BADAPP_EXPORT Gui_AddOutput(LPCWSTR Text)
{
    SendMessageW(g_hOutputEdit, EM_SETSEL, INT_MAX, INT_MAX);
    SendMessageW(g_hOutputEdit, EM_REPLACESEL, FALSE, (LPARAM)Text);
    SendMessageW(g_hOutputEdit, EM_SCROLLCARET, 0L, 0L);
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

void BADAPP_EXPORT OnInitAboutDlg(HWND hDlg)
{
    HWND hWndParent;
    RECT rc;
    int w, h;

    hWndParent = GetParent(hDlg);
    GetWindowRect(hDlg, &rc);
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    GetWindowRect(hWndParent, &rc);
    SetWindowPos(hDlg, HWND_TOP,
                 rc.left + ((rc.right - rc.left) - w) / 2,
                 rc.top + ((rc.bottom - rc.top) - h) / 2,
                 0, 0, SWP_NOSIZE);

    SetDlgItemTextA(hDlg, IDC_VERSION, "<a href=\"https://learn-more.github.io/BadApp/\">BadApp</a> " GIT_VERSION_STR);
}

INT_PTR BADAPP_EXPORT CALLBACK AboutProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

    switch (uMsg)
    {
    case WM_INITDIALOG:
        OnInitAboutDlg(hDlg);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
        case IDOK:
            EndDialog(hDlg, 0);
            break;
        }
        break;
    case WM_NOTIFY:
        switch (((LPNMHDR)lParam)->code)
        {
        case NM_CLICK:
        case NM_RETURN:
        {
            PNMLINK pNMLink = (PNMLINK)lParam;
            if (pNMLink->hdr.idFrom == IDC_VERSION || pNMLink->hdr.idFrom == IDC_CREDITS)
            {
                SHELLEXECUTEINFOW shExInfo = { sizeof(shExInfo) };
                shExInfo.lpVerb = L"open";
                shExInfo.hwnd = hDlg;
                shExInfo.fMask = SEE_MASK_UNICODE | SEE_MASK_NOZONECHECKS | SEE_MASK_NOASYNC;
                shExInfo.lpFile = pNMLink->item.szUrl;
                shExInfo.nShow = SW_SHOW;

                CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
                ShellExecuteExW(&shExInfo);
                CoUninitialize();
            }
        }
            break;
        }
    default:
        break;
    }
    return FALSE;
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
        if (!IsDialogMessageW(hWnd, &Msg))
        {
            TranslateMessage(&Msg);
            DispatchMessageW(&Msg);
        }

        /* Just spy for messages here. This way we also see it when a child control has focus */
        if (Msg.message == WM_KEYUP && Msg.wParam == VK_F1)
        {
            DialogBoxW(hInstance, MAKEINTRESOURCEW(IDD_ABOUTBOX), hWnd, AboutProc);
        }
    }
    ExitProcess((UINT)Msg.wParam);
}

