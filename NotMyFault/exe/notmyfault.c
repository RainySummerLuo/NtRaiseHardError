//======================================================================
//
// NotMyFault.c
//
// Copyright (C) 2002 Mark Russinovich
// Sysinternals - www.sysinternals.com
//
// Simple interface to myfault device driver.
// 
//======================================================================
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include "resource.h"
#include "ioctlcmd.h"
#include "notmyfault.h"

COLORREF BsodFg = RGB(0xFF, 0xFF, 0xFF);
COLORREF BsodBg = RGB(0xFF, 0, 0);

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


//----------------------------------------------------------------------
//
// Abort
//
// Exit with a fatal error.
//
//----------------------------------------------------------------------
LONG Abort(HWND hWnd, char* Msg, DWORD Error)
{
    LPVOID lpMsgBuf;
    char errmsg[MAX_PATH * 2];
    DWORD error = GetLastError();

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL, Error,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&lpMsgBuf, 0, NULL);
    UnloadDeviceDriver(SYS_NAME);
    sprintf(errmsg, "%s: %s", Msg, lpMsgBuf);
    if ((Error == ERROR_INVALID_HANDLE || Error == ERROR_ACCESS_DENIED ||
        Error == ERROR_FILE_NOT_FOUND))
        wsprintf(errmsg, "%s\nMake sure that you are an administrator and that NotMyFault is "
                 "not already running.", errmsg);
    MessageBox(hWnd, errmsg, "NotMyFault", MB_OK | MB_ICONERROR);
    PostQuitMessage(1);
    LocalFree(lpMsgBuf);
    return (DWORD)-1;
}


//----------------------------------------------------------------------
//
// CenterWindow
//
// Centers the Window on the screen.
//
//----------------------------------------------------------------------
VOID CenterWindow(HWND hDlg)
{
    RECT aRt;

    // center the dialog box
    GetWindowRect(hDlg, &aRt);
    OffsetRect(&aRt, -aRt.left, -aRt.top);
    MoveWindow(hDlg,
               ((GetSystemMetrics(SM_CXSCREEN) -
                   aRt.right) / 2 + 4) & ~7,
               (GetSystemMetrics(SM_CYSCREEN) -
                   aRt.bottom) / 2,
               aRt.right, aRt.bottom, 0);
}

//----------------------------------------------------------------------
//
// BsodColorsCallback
//
//----------------------------------------------------------------------
UINT_PTR CALLBACK BsodColorsCallback(HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    static COLORREF newFg, newBg;
    static UINT wm_colorOkString, wm_setRgbString;
    HBRUSH hBack;

    switch (uiMsg)
    {
    case WM_INITDIALOG:
        newFg = BsodFg;
        newBg = BsodBg;
        wm_colorOkString = RegisterWindowMessage(COLOROKSTRING);
        wm_setRgbString = RegisterWindowMessage(SETRGBSTRING);
        CheckRadioButton(hDlg, IDC_RADIOFG, IDC_RADIOBG, IDC_RADIOFG);
        SendMessage(hDlg, wm_setRgbString, 0, newFg);
        SetFocus(GetDlgItem(hDlg, IDC_DONE));
        break;

    case WM_CTLCOLORSTATIC:
        if ((HWND)lParam == GetDlgItem(hDlg,IDC_PREVIEW))
        {
            SetBkColor((HDC)wParam, newBg);
            SetTextColor((HDC)wParam, newFg);
            hBack = CreateSolidBrush(newBg);
            return (BOOL)hBack;
        }
        break;

    case WM_COMMAND:
        if (wParam == IDC_DONE)
        {
            BsodFg = newFg;
            BsodBg = newBg;
            PostMessage(hDlg, WM_COMMAND, IDABORT, 1);
            return FALSE;
        }
        break;

    default:
        if (uiMsg == wm_colorOkString)
        {
            CHOOSECOLOR* choose = (CHOOSECOLOR*)lParam;
            if (IsDlgButtonChecked(hDlg, IDC_RADIOBG))
            {
                newBg = choose->rgbResult;
                InvalidateRect(GetDlgItem(hDlg,IDC_PREVIEW), NULL, TRUE);
                //SendMessage( hDlg, wm_setRgbString, 0, newFg );
                return TRUE;
            }
            else
            {
                newFg = choose->rgbResult;
                InvalidateRect(GetDlgItem(hDlg,IDC_PREVIEW), NULL, TRUE);
                //SendMessage( hDlg, wm_setRgbString, 0, newBg );
                return TRUE;
            }
        }
        break;
    }
    return 0;
}


//----------------------------------------------------------------------
//
//  StartMyFaultDriver
//
// Loads and starts the driver.
//
//----------------------------------------------------------------------
LONG StartMyFaultDriver(HWND hDlg)
{
    char driverPath[MAX_PATH];
    char systemRoot[MAX_PATH];
    char path[MAX_PATH];
    WIN32_FIND_DATA findData;
    HANDLE findHandle;
    char* file;
    DWORD error;
    char msgbuf[MAX_PATH * 2];

    //
    // Load the myfault driver
    //
    GetCurrentDirectory(sizeof path, path);
    sprintf(path + lstrlen(path), "\\%s", SYS_FILE);

    findHandle = FindFirstFile(path, &findData);
    if (findHandle == INVALID_HANDLE_VALUE)
    {
        if (!SearchPath(NULL, SYS_FILE, NULL, sizeof(path), path, &file))
        {
            sprintf(msgbuf, "%s was not found.", SYS_FILE);
            return Abort(hDlg, msgbuf, GetLastError());
        }
    }
    else FindClose(findHandle);

    if (!GetEnvironmentVariable("SYSTEMROOT", systemRoot, sizeof(systemRoot)))
    {
        strcpy(msgbuf, "Could not resolve SYSTEMROOT environment variable");
        return Abort(hDlg, msgbuf, GetLastError());
    }
    sprintf(driverPath, "%s\\system32\\drivers\\myfault.sys", systemRoot);
    SetFileAttributes(driverPath, FILE_ATTRIBUTE_NORMAL);
    CopyFile(path, driverPath, FALSE);
    if (!LoadDeviceDriver(SYS_NAME, driverPath, &SysHandle, &error))
    {
        if (!CopyFile(path, driverPath, FALSE))
        {
            sprintf(msgbuf, "Unable to copy %s to %s\n\n"
                    "Make sure that %s is in the current directory.",
                    SYS_NAME, driverPath, SYS_FILE);
            return Abort(hDlg, msgbuf, GetLastError());
        }
        SetFileAttributes(driverPath, FILE_ATTRIBUTE_NORMAL);
        if (!LoadDeviceDriver(SYS_NAME, driverPath, &SysHandle, &error))
        {
            UnloadDeviceDriver(SYS_NAME);
            if (!LoadDeviceDriver(SYS_NAME, driverPath, &SysHandle, &error))
            {
                sprintf(msgbuf, "Error loading %s:", path);
                DeleteFile(driverPath);
                return Abort(hDlg, msgbuf, error);
            }
        }
    }
    return TRUE;
}

//----------------------------------------------------------------------
//
// IoctlThreadProc
//
//----------------------------------------------------------------------
void IoctlThreadProc(PVOID Context)
{
    DWORD nb;
    DeviceIoControl(SysHandle, (DWORD)Context, NULL, 0, NULL, 0, &nb, NULL);
}


//---------------------------------------------------------------------
//
// LeakPool
//
//---------------------------------------------------------------------
void LeakPool(UINT PoolType, DWORD allocSize)
{
    DWORD maxAlloc, bytesAllocated, nb;
    DWORD tickCount = GetTickCount();

    maxAlloc = allocSize;
    bytesAllocated = 0;
    while (bytesAllocated < maxAlloc && tickCount - GetTickCount() < 1000)
    {
        if (!DeviceIoControl(SysHandle,
                             PoolType ? IOCTL_LEAK_NONPAGED : IOCTL_LEAK_PAGED, &allocSize, sizeof(allocSize),
                             NULL, 0, &nb, NULL))
        {
            // can't even allocate 1 byte
            if (allocSize == 1) break;

            allocSize /= 2;
            if (allocSize == 0) allocSize = 1;
        }
        else
        {
            bytesAllocated += allocSize;
        }
    }

    // one more try going from 2 to 8192
    if (bytesAllocated < maxAlloc)
    {
        allocSize = 8192;
        while (allocSize > 1 && bytesAllocated < maxAlloc
            && tickCount - GetTickCount() < 1000)
        {
            while (DeviceIoControl(SysHandle,
                                   PoolType ? IOCTL_LEAK_NONPAGED : IOCTL_LEAK_PAGED, &allocSize, sizeof(allocSize),
                                   NULL, 0, &nb, NULL) && bytesAllocated < maxAlloc)
            {
                bytesAllocated += allocSize;
            }
            allocSize /= 2;
        }
    }
}


//----------------------------------------------------------------------
//
// MainDialog
//
// This is the main window.
//
//----------------------------------------------------------------------
LRESULT APIENTRY MainDialog(HWND hDlg, UINT message, UINT wParam,
                            LONG lParam)
{
    char label[MAX_PATH];
    SYSTEM_INFO sysInfo;
    DWORD i, nb, ioctl;
    DWORD allocSize, maxAlloc;
    static BOOLEAN leakPaged = FALSE;
    static BOOLEAN leakNonpaged = FALSE;
    CHOOSECOLOR colorArgs;
    static DWORD rgbCurrent;
    static COLORREF acrCustClr[16];

    switch (message)
    {
    case WM_INITDIALOG:

        //
        // Start driver
        //
        if (!StartMyFaultDriver(hDlg))
        {
            return FALSE;
        }

    //
    // We can delete the driver and its Registry key now that its loaded
    //
        CheckDlgButton(hDlg, IDC_IRQL, BST_CHECKED);
        CenterWindow(hDlg);
        SetDlgItemText(hDlg, IDC_LEAKMB, "1000");
        break;

    case WM_TIMER:

        GetDlgItemText(hDlg, IDC_LEAKMB, label, _countof(label));
        allocSize = maxAlloc = (atoi(label) * 1024);
        LeakPool(wParam, allocSize);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:

            if (IsDlgButtonChecked(hDlg, IDC_BUFFEROVERFLOW) == BST_CHECKED)
            {
                ioctl = IOCTL_BUFFER_OVERFLOW;
            }
            else if (IsDlgButtonChecked(hDlg, IDC_WILDPOINTER) == BST_CHECKED)
            {
                ioctl = IOCTL_WILD_POINTER;
            }
            else if (IsDlgButtonChecked(hDlg, IDC_DEADLOCK) == BST_CHECKED)
            {
                ioctl = IOCTL_DEADLOCK;
            }
            else if (IsDlgButtonChecked(hDlg, IDC_HANG) == BST_CHECKED)
            {
                ioctl = IOCTL_HANG;
            }
            else if (IsDlgButtonChecked(hDlg, IDC_STACKTRASH) == BST_CHECKED)
            {
                ioctl = IOCTL_TRASH_STACK;
            }
            else if (IsDlgButtonChecked(hDlg, IDC_PAGEFAULT) == BST_CHECKED)
            {
                ioctl = IOCTL_PAGE_FAULT;
            }
            else if (IsDlgButtonChecked(hDlg, IDC_IRQL) == BST_CHECKED)
            {
                ioctl = IOCTL_IRQL;
            }
            else if (IsDlgButtonChecked(hDlg, IDC_HANGIRP) == BST_CHECKED)
            {
                _beginthread(IoctlThreadProc, 0, (PVOID)IOCTL_HANG_IRP);
                break;
            }

        //
        // Execute hang and deadlock on each CPU
        //
            if (ioctl == IOCTL_HANG || ioctl == IOCTL_DEADLOCK)
            {
                GetSystemInfo(&sysInfo);
                for (i = 0; i < sysInfo.dwNumberOfProcessors; i++)
                {
                    DeviceIoControl(SysHandle, ioctl, NULL, 0, NULL, 0, &nb, NULL);
                }
            }
            else
            {
                DeviceIoControl(SysHandle, ioctl, NULL, 0, NULL, 0, &nb, NULL);
            }
            break;

        case IDC_LEAK_PAGE:

            if (leakPaged)
            {
                KillTimer(hDlg, 0);
                SetDlgItemText(hDlg, IDC_LEAK_PAGE, "Leak &Paged");
            }
            else
            {
                SetTimer(hDlg, 0, 1000, NULL);
                SetDlgItemText(hDlg, IDC_LEAK_PAGE, "Stop &Paged");
            }
            leakPaged = !leakPaged;
            break;

        case IDC_LEAK_NONPAGE:

            if (leakNonpaged)
            {
                KillTimer(hDlg, 1);
                SetDlgItemText(hDlg, IDC_LEAK_NONPAGE, "Leak &Nonpaged");
            }
            else
            {
                SetTimer(hDlg, 1, 1000, NULL);
                SetDlgItemText(hDlg, IDC_LEAK_NONPAGE, "Stop &Nonpaged");
            }
            leakNonpaged = !leakNonpaged;
            break;

        case IDCOLOR:
            {
                COLORREF CustomColors[16];
                int i;
                for (i = 0; i < 16; i++)
                {
                    CustomColors[i] = RGB(255, 255, 255);
                }
                colorArgs.lStructSize = sizeof colorArgs;
                colorArgs.Flags = CC_RGBINIT | CC_ENABLEHOOK | CC_ENABLETEMPLATE | CC_FULLOPEN;
                colorArgs.hwndOwner = hDlg;
                colorArgs.hInstance = (HWND)GetModuleHandle(NULL);
                colorArgs.rgbResult = RGB(0, 0, 0);
                colorArgs.lpCustColors = CustomColors;
                colorArgs.lCustData = 0;
                colorArgs.rgbResult = BsodFg;
                colorArgs.lpTemplateName = "BSODCOLORS";
                colorArgs.lpfnHook = BsodColorsCallback;
                if (ChooseColor(&colorArgs) == TRUE)
                {
                    LARGE_INTEGER Color;
                    Color.LowPart = RGB(GetRValue(BsodBg)/4,
                                        GetGValue(BsodBg)/4,
                                        GetBValue(BsodBg)/4);
                    Color.HighPart = RGB(GetRValue(BsodFg)/4,
                                         GetGValue(BsodFg)/4,
                                         GetBValue(BsodFg)/4);
                    DeviceIoControl(SysHandle, IOCTL_BSOD_COLOR, &Color, sizeof(LARGE_INTEGER), NULL, 0, &nb, NULL);
                }
            }
            break;

        case IDCANCEL:

            //
            // Cancel
            //
            EndDialog(hDlg, 0);
            PostQuitMessage(0);
            break ;
        }
        break;

    case WM_CLOSE:
        EndDialog(hDlg, 0);
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hDlg, message, wParam, lParam);
}


//----------------------------------------------------------------------
//
// WinMain
//
// Initialize a dialog window class and pop the autologon dialog.
//
//----------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow)
{
    static TCHAR szAppName[] = TEXT("NOTMYFAULT");
    MSG msg;
    HWND hMainDlg;
    WNDCLASSEX wndclass;
    PWSTR* cmdLine;
    int numArgs, i;
    DWORD nb;

    cmdLine = CommandLineToArgvW(GetCommandLineW(), &numArgs);
    for (i = 0; i < numArgs; i++)
    {
        if (cmdLine[i][0] == '/' ||
            cmdLine[i][0] == '-')
        {
            if (!_wcsicmp(&cmdLine[i][1], L"crash"))
            {
                if (StartMyFaultDriver(NULL))
                {
                    DeviceIoControl(SysHandle, IOCTL_IRQL, NULL, 0, NULL, 0, &nb, NULL);
                }
            }
            else
            {
                MessageBox(NULL, "Usage: notmyfault [/crash]\n"
                           "/crash    Crashes the system.", "NotMyFault", MB_ICONERROR);
                return -1;
            }
        }
    }

    //
    // Create the main window class
    //
    wndclass.cbSize = sizeof(WNDCLASSEX);
    wndclass.style = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc = (WNDPROC)MainDialog;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = DLGWINDOWEXTRA;
    wndclass.hInstance = hInstance;
    wndclass.hIcon = LoadIcon(hInstance, "APPICON");
    wndclass.hIconSm = LoadIcon(hInstance, "APPICON");
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = szAppName;
    RegisterClassEx(&wndclass);

    //
    // Create the dialog
    //
    hMainDlg = CreateDialog(hInstance, "NOTMYFAULT", NULL, (DLGPROC)MainDialog);
    ShowWindow(hMainDlg, nCmdShow);

    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!IsDialogMessage(hMainDlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}
