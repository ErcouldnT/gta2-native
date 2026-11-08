#pragma once
#include "windows.h"
// Dialog / common-control vocabulary used by the network lobby UI (NetUi.cpp).
// The native port has no Win32 dialogs; these calls are stubbed in win32_shim.cpp.
#define BM_GETSTATE 0x00F2
#define BM_SETCHECK 0x00F1
#define BST_CHECKED 0x0001
#define BST_UNCHECKED 0x0000
#define CB_ADDSTRING 0x0143
#define CB_GETCURSEL 0x0147
#define CB_GETITEMDATA 0x0150
#define CB_SETCURSEL 0x014E
#define CB_SETITEMDATA 0x0151
#define CB_RESETCONTENT 0x014B
#define CBN_SELCHANGE 1
#define EM_GETLINECOUNT 0x00BA
#define EM_LINESCROLL 0x00B6
#define EM_SETSEL 0x00B1
#define EM_REPLACESEL 0x00C2
#define EM_LIMITTEXT 0x00C5
#define EN_CHANGE 0x0300
#define KF_MENUMODE 0x1000
#define LB_ADDSTRING 0x0180
#define LB_ERR (-1)
#define LB_ERRSPACE (-2)
#define LB_GETCURSEL 0x0188
#define LB_GETITEMDATA 0x0199
#define LB_GETTEXT 0x0189
#define LB_RESETCONTENT 0x0184
#define LB_SELECTSTRING 0x018C
#define LB_SETCURSEL 0x0186
#define LB_SETITEMDATA 0x019A
#define LB_GETCOUNT 0x018B
#define LB_DELETESTRING 0x0182
#define LBN_DBLCLK 2
#define LBN_SELCHANGE 1
#define LVFI_STRING 0x0002
#define LVIF_TEXT 0x0001
#define LVIF_PARAM 0x0004
#define LVM_FIRST 0x1000
#define LVM_DELETEALLITEMS (LVM_FIRST + 9)
#define LVM_DELETEITEM (LVM_FIRST + 8)
#define LVM_FINDITEMA (LVM_FIRST + 13)
#define LVM_GETITEMCOUNT (LVM_FIRST + 4)
#define LVM_GETITEMTEXTA (LVM_FIRST + 45)
#define LVM_GETNEXTITEM (LVM_FIRST + 12)
#define LVM_INSERTITEMA (LVM_FIRST + 7)
#define LVM_SETITEMTEXTA (LVM_FIRST + 46)
#define LVNI_SELECTED 0x0002
#define TBM_GETPOS (WM_USER)
#define TBM_SETRANGE (WM_USER + 6)
#define TBM_SETPOS (WM_USER + 5)
#define WM_HSCROLL 0x0114
#define WM_VSCROLL 0x0115
typedef struct tagLVITEMA
{
    UINT mask;
    int iItem;
    int iSubItem;
    UINT state;
    UINT stateMask;
    LPSTR pszText;
    int cchTextMax;
    int iImage;
    LPARAM lParam;
    int iIndent;
} LVITEMA, LVITEM, *LPLVITEMA;
typedef struct tagLVFINDINFOA
{
    UINT flags;
    LPCSTR psz;
    LPARAM lParam;
    POINT pt;
    UINT vkDirection;
} LVFINDINFOA, LVFINDINFO;
typedef BOOL (*DLGPROC)(HWND, UINT, WPARAM, LPARAM);
typedef void (*TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);
HWND GetDlgItem(HWND, int);
int GetDlgCtrlID(HWND);
BOOL SetDlgItemTextA(HWND, int, LPCSTR);
UINT GetDlgItemTextA(HWND, int, LPSTR, int);
LRESULT SendDlgItemMessageA(HWND, int, UINT, WPARAM, LPARAM);
INT_PTR DialogBoxParamA(HINSTANCE, LPCSTR, HWND, DLGPROC, LPARAM);
BOOL EndDialog(HWND, INT_PTR);
HWND GetParent(HWND);
LRESULT CallWindowProcA(WNDPROC, HWND, UINT, WPARAM, LPARAM);
UINT_PTR SetTimer(HWND, UINT_PTR, UINT, TIMERPROC);
BOOL KillTimer(HWND, UINT_PTR);
BOOL EnableWindow(HWND, BOOL);
void InitCommonControls(void);
#define SetDlgItemText SetDlgItemTextA
#define GetDlgItemText GetDlgItemTextA
#define SendDlgItemMessage SendDlgItemMessageA
#define DialogBoxParam DialogBoxParamA
#define CallWindowProc CallWindowProcA
#define DialogBox(i, t, p, f) DialogBoxParamA(i, t, p, f, 0)
