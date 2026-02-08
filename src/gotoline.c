/*
 * Go to Line dialog implementation for Notepad+
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "resource.h"
#include "tabs.h"
#include "Scintilla.h"
#include "themes.h"

/* Dialog window class */
#define GOTOLINE_CLASS_NAME "NotepadPlusGoToLineDialog"

/* Dialog dimensions */
#define DIALOG_WIDTH 280
#define DIALOG_HEIGHT 120

/* Control positions */
#define MARGIN 10
#define LABEL_HEIGHT 16
#define EDIT_HEIGHT 22
#define BUTTON_WIDTH 75
#define BUTTON_HEIGHT 25

/* Static variables for the dialog */
static HWND g_dialogHwnd = NULL;
static HWND g_editHwnd = NULL;
static HWND g_labelHwnd = NULL;
static HWND g_okButton = NULL;
static HWND g_cancelButton = NULL;
static HWND g_editorHandle = NULL;
static int g_totalLines = 0;
static WNDPROC g_origEditProc = NULL;
static HBRUSH g_dialogBgBrush = NULL;

/* Forward declarations */
static LRESULT CALLBACK GoToLineWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* Subclass procedure for edit control to handle Enter key */
static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            /* Simulate OK button click */
            SendMessage(g_dialogHwnd, WM_COMMAND, MAKEWPARAM(IDC_GOTOLINE_OK, BN_CLICKED), (LPARAM)g_okButton);
            return 0;
        } else if (wParam == VK_ESCAPE) {
            /* Close dialog */
            SendMessage(g_dialogHwnd, WM_CLOSE, 0, 0);
            return 0;
        }
    }
    return CallWindowProc(g_origEditProc, hwnd, msg, wParam, lParam);
}

/* Dialog window procedure */
static LRESULT CALLBACK GoToLineWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_CREATE:
            return 0;
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_GOTOLINE_OK:
                case IDOK:
                    {
                        char lineStr[32];
                        GetWindowText(g_editHwnd, lineStr, sizeof(lineStr));
                        
                        int lineNum = atoi(lineStr);
                        
                        if (lineNum < 1 || lineNum > g_totalLines) {
                            char msg[100];
                            sprintf(msg, "Please enter a line number between 1 and %d", g_totalLines);
                            MessageBox(hwnd, msg, "Go To Line", MB_OK | MB_ICONWARNING);
                            SetFocus(g_editHwnd);
                            SendMessage(g_editHwnd, EM_SETSEL, 0, -1);
                            return 0;
                        }
                        
                        /* Go to the line */
                        if (g_editorHandle) {
                            SendMessage(g_editorHandle, SCI_GOTOLINE, lineNum - 1, 0);
                            SendMessage(g_editorHandle, SCI_SCROLLCARET, 0, 0);
                            /* Set focus back to the editor */
                            SetFocus(g_editorHandle);
                        }
                        
                        DestroyWindow(hwnd);
                        g_dialogHwnd = NULL;
                        return 0;
                    }
                    
                case IDC_GOTOLINE_CANCEL:
                case IDCANCEL:
                    /* Set focus back to the editor before closing */
                    if (g_editorHandle) {
                        SetFocus(g_editorHandle);
                    }
                    DestroyWindow(hwnd);
                    g_dialogHwnd = NULL;
                    return 0;
            }
            break;
            
        case WM_CLOSE:
            /* Set focus back to the editor before closing */
            if (g_editorHandle) {
                SetFocus(g_editorHandle);
            }
            DestroyWindow(hwnd);
            g_dialogHwnd = NULL;
            return 0;
            
        case WM_CTLCOLORSTATIC:
            {
                /* Theme-aware static control coloring */
                ThemeColors* colors = GetThemeColors();
                if (colors) {
                    HDC hdcStatic = (HDC)wParam;
                    SetTextColor(hdcStatic, colors->statusbarFg);
                    SetBkColor(hdcStatic, colors->windowBg);
                    
                    /* Create or update background brush */
                    if (!g_dialogBgBrush) {
                        g_dialogBgBrush = CreateSolidBrush(colors->windowBg);
                    }
                    return (LRESULT)g_dialogBgBrush;
                }
            }
            break;
            
        case WM_DESTROY:
            /* Restore original edit window procedure */
            if (g_editHwnd && g_origEditProc) {
                SetWindowLongPtr(g_editHwnd, GWLP_WNDPROC, (LONG_PTR)g_origEditProc);
                g_origEditProc = NULL;
            }
            /* Clean up background brush */
            if (g_dialogBgBrush) {
                DeleteObject(g_dialogBgBrush);
                g_dialogBgBrush = NULL;
            }
            g_dialogHwnd = NULL;
            g_editHwnd = NULL;
            g_labelHwnd = NULL;
            g_okButton = NULL;
            g_cancelButton = NULL;
            return 0;
            
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                /* Use WM_CLOSE to properly restore focus */
                SendMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            break;
            
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/* Register dialog window class */
static BOOL RegisterGoToLineClass(void)
{
    static BOOL registered = FALSE;
    
    if (registered) {
        return TRUE;
    }
    
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = GoToLineWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = GOTOLINE_CLASS_NAME;
    wc.hIconSm = NULL;
    
    if (!RegisterClassEx(&wc)) {
        return FALSE;
    }
    
    registered = TRUE;
    return TRUE;
}

/* Show Go to Line dialog */
BOOL ShowGoToLineDialog(HWND parent, HWND editorHandle)
{
    if (!editorHandle) {
        return FALSE;
    }
    
    /* If dialog already exists, just bring it to front */
    if (g_dialogHwnd && IsWindow(g_dialogHwnd)) {
        SetForegroundWindow(g_dialogHwnd);
        SetFocus(g_editHwnd);
        return TRUE;
    }
    
    /* Register window class */
    if (!RegisterGoToLineClass()) {
        MessageBox(parent, "Failed to register Go To Line dialog class", "Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    
    /* Store editor handle */
    g_editorHandle = editorHandle;
    
    /* Get total lines */
    g_totalLines = (int)SendMessage(editorHandle, SCI_GETLINECOUNT, 0, 0);
    
    /* Get current line */
    int currentPos = (int)SendMessage(editorHandle, SCI_GETCURRENTPOS, 0, 0);
    int currentLine = (int)SendMessage(editorHandle, SCI_LINEFROMPOSITION, currentPos, 0);
    
    /* Get parent window position to center dialog */
    RECT parentRect;
    GetWindowRect(parent, &parentRect);
    int parentWidth = parentRect.right - parentRect.left;
    int parentHeight = parentRect.bottom - parentRect.top;
    int dialogX = parentRect.left + (parentWidth - DIALOG_WIDTH) / 2;
    int dialogY = parentRect.top + (parentHeight - DIALOG_HEIGHT) / 2;
    
    /* Create dialog window */
    g_dialogHwnd = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        GOTOLINE_CLASS_NAME,
        "Go To Line",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        dialogX, dialogY, DIALOG_WIDTH, DIALOG_HEIGHT,
        parent,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!g_dialogHwnd) {
        MessageBox(parent, "Failed to create Go To Line dialog", "Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    
    /* Get default GUI font */
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    
    /* Create label */
    char labelText[100];
    sprintf(labelText, "Line number (1 - %d):", g_totalLines);
    
    g_labelHwnd = CreateWindowEx(
        0,
        "STATIC",
        labelText,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        MARGIN, MARGIN, DIALOG_WIDTH - 2 * MARGIN - 20, LABEL_HEIGHT,
        g_dialogHwnd,
        (HMENU)IDC_GOTOLINE_INFO,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(g_labelHwnd, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    /* Create edit control */
    g_editHwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_NUMBER | ES_AUTOHSCROLL,
        MARGIN, MARGIN + LABEL_HEIGHT + 5, DIALOG_WIDTH - 2 * MARGIN - 20, EDIT_HEIGHT,
        g_dialogHwnd,
        (HMENU)IDC_GOTOLINE_EDIT,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(g_editHwnd, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    /* Subclass edit control to handle Enter key */
    g_origEditProc = (WNDPROC)SetWindowLongPtr(g_editHwnd, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
    
    /* Set current line in edit box */
    char lineStr[32];
    sprintf(lineStr, "%d", currentLine + 1);
    SetWindowText(g_editHwnd, lineStr);
    
    /* Calculate button positions */
    int buttonY = MARGIN + LABEL_HEIGHT + 5 + EDIT_HEIGHT + 15;
    int totalButtonWidth = 2 * BUTTON_WIDTH + 10;
    int buttonStartX = (DIALOG_WIDTH - 20 - totalButtonWidth) / 2;
    
    /* Create OK button */
    g_okButton = CreateWindowEx(
        0,
        "BUTTON",
        "OK",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        buttonStartX, buttonY, BUTTON_WIDTH, BUTTON_HEIGHT,
        g_dialogHwnd,
        (HMENU)IDC_GOTOLINE_OK,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(g_okButton, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    /* Create Cancel button */
    g_cancelButton = CreateWindowEx(
        0,
        "BUTTON",
        "Cancel",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        buttonStartX + BUTTON_WIDTH + 10, buttonY, BUTTON_WIDTH, BUTTON_HEIGHT,
        g_dialogHwnd,
        (HMENU)IDC_GOTOLINE_CANCEL,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(g_cancelButton, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    /* Show the dialog */
    ShowWindow(g_dialogHwnd, SW_SHOW);
    UpdateWindow(g_dialogHwnd);
    
    /* Select all text in edit box and set focus */
    SendMessage(g_editHwnd, EM_SETSEL, 0, -1);
    SetFocus(g_editHwnd);
    
    return TRUE;
}
