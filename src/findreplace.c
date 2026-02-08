/*
 * Find and Replace dialog implementation for Notepad+
 */

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "findreplace.h"
#include "resource.h"
#include "window.h"
#include "Scintilla.h"
#include "tabs.h"
#include "themes.h"

static FindReplaceState g_findReplace = {0};
static char* g_searchHistory[MAX_SEARCH_HISTORY] = {0};
static int g_searchHistoryCount = 0;
static HBRUSH g_findDlgBgBrush = NULL;
static HBRUSH g_replaceDlgBgBrush = NULL;

BOOL InitializeFindReplace(void)
{
    memset(&g_findReplace, 0, sizeof(FindReplaceState));
    return TRUE;
}

void CleanupFindReplace(void)
{
    for (int i = 0; i < g_searchHistoryCount; i++) {
        if (g_searchHistory[i]) { free(g_searchHistory[i]); g_searchHistory[i] = NULL; }
    }
    g_searchHistoryCount = 0;
    
    if (g_findReplace.hwndFind) {
        DestroyWindow(g_findReplace.hwndFind);
        g_findReplace.hwndFind = NULL;
    }
    if (g_findReplace.hwndReplace) {
        DestroyWindow(g_findReplace.hwndReplace);
        g_findReplace.hwndReplace = NULL;
    }
}

/* Find dialog procedure */
static INT_PTR CALLBACK FindDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (msg) {
        case WM_INITDIALOG:
            {
                /* Set find text if available */
                SetDlgItemText(hwnd, IDC_FIND_COMBO, g_findReplace.findText);
                
                /* Set checkboxes */
                CheckDlgButton(hwnd, IDC_MATCH_CASE, (g_findReplace.flags & 0x01) ? BST_CHECKED : BST_UNCHECKED);
                CheckDlgButton(hwnd, IDC_WHOLE_WORD, (g_findReplace.flags & 0x02) ? BST_CHECKED : BST_UNCHECKED);
                
                /* Focus on find text box */
                SetFocus(GetDlgItem(hwnd, IDC_FIND_COMBO));
                SendDlgItemMessage(hwnd, IDC_FIND_COMBO, EM_SETSEL, 0, -1);
                
                return FALSE;
            }
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_FIND_NEXT: /* Find Next button */
                    {
                        /* Get find text */
                        GetDlgItemText(hwnd, IDC_FIND_COMBO, g_findReplace.findText, MAX_FIND_TEXT_LENGTH);
                        
                        /* Get flags */
                        g_findReplace.flags = 0;
                        if (IsDlgButtonChecked(hwnd, IDC_MATCH_CASE) == BST_CHECKED) g_findReplace.flags |= 0x01;
                        if (IsDlgButtonChecked(hwnd, IDC_WHOLE_WORD) == BST_CHECKED) g_findReplace.flags |= 0x02;
                        
                        /* Set search direction */
                        g_findReplace.searchDirection = SEARCH_DIRECTION_DOWN;
                        
                        /* Perform search */
                        if (!FindNext()) {
                            MessageBox(hwnd, "Cannot find the text specified.", "Find", MB_OK | MB_ICONINFORMATION);
                        }
                        return TRUE;
                    }
                    
                case IDCANCEL:
                case IDC_CLOSE_BUTTON: /* Close button */
                    ShowWindow(hwnd, SW_HIDE);
                    return TRUE;
            }
            break;
            
        case WM_CTLCOLORSTATIC:
            {
                /* Theme-aware static control coloring */
                ThemeColors* colors = GetThemeColors();
                if (colors) {
                    HDC hdcStatic = (HDC)wParam;
                    SetTextColor(hdcStatic, colors->statusbarFg);
                    SetBkColor(hdcStatic, colors->windowBg);
                    
                    /* Create or update background brush */
                    if (!g_findDlgBgBrush) {
                        g_findDlgBgBrush = CreateSolidBrush(colors->windowBg);
                    }
                    return (INT_PTR)g_findDlgBgBrush;
                }
            }
            break;
            
        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return TRUE;
            
        case WM_DESTROY:
            /* Clean up background brush */
            if (g_findDlgBgBrush) {
                DeleteObject(g_findDlgBgBrush);
                g_findDlgBgBrush = NULL;
            }
            return TRUE;
    }
    
    return FALSE;
}

/* Replace dialog procedure */
static INT_PTR CALLBACK ReplaceDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (msg) {
        case WM_INITDIALOG:
            {
                /* Set find and replace text if available */
                SetDlgItemText(hwnd, IDC_FIND_COMBO, g_findReplace.findText);
                SetDlgItemText(hwnd, IDC_REPLACE_COMBO, g_findReplace.replaceText);
                
                /* Set checkboxes */
                CheckDlgButton(hwnd, IDC_MATCH_CASE, (g_findReplace.flags & 0x01) ? BST_CHECKED : BST_UNCHECKED);
                CheckDlgButton(hwnd, IDC_WHOLE_WORD, (g_findReplace.flags & 0x02) ? BST_CHECKED : BST_UNCHECKED);
                
                /* Focus on find text box */
                SetFocus(GetDlgItem(hwnd, IDC_FIND_COMBO));
                SendDlgItemMessage(hwnd, IDC_FIND_COMBO, EM_SETSEL, 0, -1);
                
                return FALSE;
            }
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_FIND_NEXT: /* Find Next button */
                    {
                        /* Get find text */
                        GetDlgItemText(hwnd, IDC_FIND_COMBO, g_findReplace.findText, MAX_FIND_TEXT_LENGTH);
                        GetDlgItemText(hwnd, IDC_REPLACE_COMBO, g_findReplace.replaceText, MAX_REPLACE_TEXT_LENGTH);
                        
                        /* Get flags */
                        g_findReplace.flags = 0;
                        if (IsDlgButtonChecked(hwnd, IDC_MATCH_CASE) == BST_CHECKED) g_findReplace.flags |= 0x01;
                        if (IsDlgButtonChecked(hwnd, IDC_WHOLE_WORD) == BST_CHECKED) g_findReplace.flags |= 0x02;
                        
                        /* Set search direction */
                        g_findReplace.searchDirection = SEARCH_DIRECTION_DOWN;
                        
                        /* Perform search */
                        if (!FindNext()) {
                            MessageBox(hwnd, "Cannot find the text specified.", "Replace", MB_OK | MB_ICONINFORMATION);
                        }
                        return TRUE;
                    }
                    
                case IDCANCEL:
                case IDC_CLOSE_BUTTON: /* Close button */
                    ShowWindow(hwnd, SW_HIDE);
                    return TRUE;
            }
            break;
            
        case WM_CTLCOLORSTATIC:
            {
                /* Theme-aware static control coloring */
                ThemeColors* colors = GetThemeColors();
                if (colors) {
                    HDC hdcStatic = (HDC)wParam;
                    SetTextColor(hdcStatic, colors->statusbarFg);
                    SetBkColor(hdcStatic, colors->windowBg);
                    
                    /* Create or update background brush */
                    if (!g_replaceDlgBgBrush) {
                        g_replaceDlgBgBrush = CreateSolidBrush(colors->windowBg);
                    }
                    return (INT_PTR)g_replaceDlgBgBrush;
                }
            }
            break;
            
        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return TRUE;
            
        case WM_DESTROY:
            /* Clean up background brush */
            if (g_replaceDlgBgBrush) {
                DeleteObject(g_replaceDlgBgBrush);
                g_replaceDlgBgBrush = NULL;
            }
            return TRUE;
    }
    
    return FALSE;
}

static LPDLGTEMPLATE CreateFindReplaceTemplate(BOOL isReplace)
{
    /* Create a simple dialog template in memory */
    WORD *p, *pdlgtemplate;
    int nchar;
    DWORD lStyle;
    
    pdlgtemplate = p = (PWORD)GlobalAlloc(GMEM_ZEROINIT, 2048);
    
    /* DLGTEMPLATE header */
    lStyle = DS_SETFONT | DS_MODALFRAME | DS_FIXEDSYS | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
    
    *p++ = LOWORD(lStyle);
    *p++ = HIWORD(lStyle);
    *p++ = 0;          /* LOWORD (lExtendedStyle) */
    *p++ = 0;          /* HIWORD (lExtendedStyle) */
    *p++ = isReplace ? 13 : 10;  /* NumberOfItems */
    *p++ = 10;         /* x */
    *p++ = 10;         /* y */
    *p++ = 280;        /* cx */
    *p++ = isReplace ? 120 : 100; /* cy */
    *p++ = 0;          /* Menu */
    *p++ = 0;          /* Class */
    
    /* Title */
    nchar = MultiByteToWideChar(CP_ACP, 0, isReplace ? "Replace" : "Find", -1, (LPWSTR)p, 50);
    p += nchar;
    
    /* Font */
    *p++ = 8;          /* Font size */
    nchar = MultiByteToWideChar(CP_ACP, 0, "MS Shell Dlg", -1, (LPWSTR)p, 50);
    p += nchar;
    
    /* Align to DWORD boundary */
    p = (PWORD)(((LONG_PTR)p + 3) & ~3);
    
    /* "Find what:" label */
    lStyle = WS_CHILD | WS_VISIBLE | SS_LEFT;
    *p++ = LOWORD(lStyle); *p++ = HIWORD(lStyle);
    *p++ = 0; *p++ = 0;
    *p++ = 5; *p++ = 10; *p++ = 50; *p++ = 10;
    *p++ = 1000;
    *p++ = 0xFFFF; *p++ = 0x0082;  /* Static class */
    nchar = MultiByteToWideChar(CP_ACP, 0, "Find what:", -1, (LPWSTR)p, 50);
    p += nchar;
    *p++ = 0;  /* No creation data */
    p = (PWORD)(((LONG_PTR)p + 3) & ~3);
    
    /* Find text edit box */
    lStyle = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL;
    *p++ = LOWORD(lStyle); *p++ = HIWORD(lStyle);
    *p++ = 0; *p++ = 0;
    *p++ = 60; *p++ = 8; *p++ = 150; *p++ = 14;
    *p++ = 1001;
    *p++ = 0xFFFF; *p++ = 0x0081;  /* Edit class */
    *p++ = 0;
    *p++ = 0;
    p = (PWORD)(((LONG_PTR)p + 3) & ~3);

    if (isReplace) {
        /* "Replace with:" label */
        lStyle = WS_CHILD | WS_VISIBLE | SS_LEFT;
        *p++ = LOWORD(lStyle); *p++ = HIWORD(lStyle);
        *p++ = 0; *p++ = 0;
        *p++ = 5; *p++ = 28; *p++ = 50; *p++ = 10;
        *p++ = 1007;
        *p++ = 0xFFFF; *p++ = 0x0082;  /* Static class */
        nchar = MultiByteToWideChar(CP_ACP, 0, "Replace with:", -1, (LPWSTR)p, 50);
        p += nchar;
        *p++ = 0;
        p = (PWORD)(((LONG_PTR)p + 3) & ~3);
        
        /* Replace text edit box */
        lStyle = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL;
        *p++ = LOWORD(lStyle); *p++ = HIWORD(lStyle);
        *p++ = 0; *p++ = 0;
        *p++ = 60; *p++ = 26; *p++ = 150; *p++ = 14;
        *p++ = 1002;
        *p++ = 0xFFFF; *p++ = 0x0081;  /* Edit class */
        *p++ = 0;
        *p++ = 0;
        p = (PWORD)(((LONG_PTR)p + 3) & ~3);
    }
    
    /* Match case checkbox */
    lStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX;
    *p++ = LOWORD(lStyle); *p++ = HIWORD(lStyle);
    *p++ = 0; *p++ = 0;
    *p++ = 5; *p++ = isReplace ? 50 : 30; *p++ = 100; *p++ = 12;
    *p++ = 1003;
    *p++ = 0xFFFF; *p++ = 0x0080;  /* Button class */
    nchar = MultiByteToWideChar(CP_ACP, 0, "Match case", -1, (LPWSTR)p, 50);
    p += nchar;
    *p++ = 0;
    p = (PWORD)(((LONG_PTR)p + 3) & ~3);
    
    /* Whole word checkbox */
    lStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX;
    *p++ = LOWORD(lStyle); *p++ = HIWORD(lStyle);
    *p++ = 0; *p++ = 0;
    *p++ = 5; *p++ = isReplace ? 65 : 45; *p++ = 100; *p++ = 12;
    *p++ = IDC_WHOLE_WORD;
    *p++ = 0xFFFF; *p++ = 0x0080;  /* Button class */
    nchar = MultiByteToWideChar(CP_ACP, 0, "Whole word", -1, (LPWSTR)p, 50);
    p += nchar;
    *p++ = 0;
    p = (PWORD)(((LONG_PTR)p + 3) & ~3);

    /* Direction label */
    lStyle = WS_CHILD | WS_VISIBLE | SS_LEFT;
    *p++ = LOWORD(lStyle); *p++ = HIWORD(lStyle);
    *p++ = 0; *p++ = 0;
    *p++ = 120; *p++ = isReplace ? 50 : 30; *p++ = 50; *p++ = 10;
    *p++ = IDC_DIR_LABEL;
    *p++ = 0xFFFF; *p++ = 0x0082;  /* Static class */
    nchar = MultiByteToWideChar(CP_ACP, 0, "Direction", -1, (LPWSTR)p, 50);
    p += nchar;
    *p++ = 0;
    p = (PWORD)(((LONG_PTR)p + 3) & ~3);

    /* Direction Up radio */
    lStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON;
    *p++ = LOWORD(lStyle); *p++ = HIWORD(lStyle);
    *p++ = 0; *p++ = 0;
    *p++ = 120; *p++ = isReplace ? 62 : 42; *p++ = 40; *p++ = 12;
    *p++ = IDC_DIRECTION_UP;
    *p++ = 0xFFFF; *p++ = 0x0080;  /* Button class */
    nchar = MultiByteToWideChar(CP_ACP, 0, "Up", -1, (LPWSTR)p, 50);
    p += nchar;
    *p++ = 0;
    p = (PWORD)(((LONG_PTR)p + 3) & ~3);

    /* Direction Down radio */
    lStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON;
    *p++ = LOWORD(lStyle); *p++ = HIWORD(lStyle);
    *p++ = 0; *p++ = 0;
    *p++ = 160; *p++ = isReplace ? 62 : 42; *p++ = 50; *p++ = 12;
    *p++ = IDC_DIRECTION_DOWN;
    *p++ = 0xFFFF; *p++ = 0x0080;  /* Button class */
    nchar = MultiByteToWideChar(CP_ACP, 0, "Down", -1, (LPWSTR)p, 50);
    p += nchar;
    *p++ = 0;
    p = (PWORD)(((LONG_PTR)p + 3) & ~3);
    
    /* Find Next button */
    lStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON;
    *p++ = LOWORD(lStyle); *p++ = HIWORD(lStyle);
    *p++ = 0; *p++ = 0;
    *p++ = 220; *p++ = 8; *p++ = 55; *p++ = 14;
    *p++ = IDC_FIND_NEXT;
    *p++ = 0xFFFF; *p++ = 0x0080;  /* Button class */
    nchar = MultiByteToWideChar(CP_ACP, 0, "Find Next", -1, (LPWSTR)p, 50);
    p += nchar;
    *p++ = 0;
    p = (PWORD)(((LONG_PTR)p + 3) & ~3);
    
    /* Close button */
    lStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;
    *p++ = LOWORD(lStyle); *p++ = HIWORD(lStyle);
    *p++ = 0; *p++ = 0;
    *p++ = 220; *p++ = isReplace ? 46 : 27; *p++ = 55; *p++ = 14;
    *p++ = IDC_CLOSE_BUTTON;
    *p++ = 0xFFFF; *p++ = 0x0080;  /* Button class */
    nchar = MultiByteToWideChar(CP_ACP, 0, "Close", -1, (LPWSTR)p, 50);
    p += nchar;
    *p++ = 0;
    p = (PWORD)(((LONG_PTR)p + 3) & ~3);

    /* Status label */
    lStyle = WS_CHILD | WS_VISIBLE | SS_LEFT;
    *p++ = LOWORD(lStyle); *p++ = HIWORD(lStyle);
    *p++ = 0; *p++ = 0;
    *p++ = 5; *p++ = isReplace ? 100 : 80; *p++ = 200; *p++ = 10;
    *p++ = IDC_FIND_COUNT;
    *p++ = 0xFFFF; *p++ = 0x0082;  /* Static class */
    *p++ = 0;
    *p++ = 0;
    p = (PWORD)(((LONG_PTR)p + 3) & ~3);

    if (isReplace) {
        /* Replace button */
        lStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;
        *p++ = LOWORD(lStyle); *p++ = HIWORD(lStyle);
        *p++ = 0; *p++ = 0;
        *p++ = 220; *p++ = 27; *p++ = 55; *p++ = 14;
        *p++ = IDC_REPLACE;
        *p++ = 0xFFFF; *p++ = 0x0080;  /* Button class */
        nchar = MultiByteToWideChar(CP_ACP, 0, "Replace", -1, (LPWSTR)p, 50);
        p += nchar;
        *p++ = 0;
        p = (PWORD)(((LONG_PTR)p + 3) & ~3);

        /* Replace All button */
        lStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;
        *p++ = LOWORD(lStyle); *p++ = HIWORD(lStyle);
        *p++ = 0; *p++ = 0;
        *p++ = 5; *p++ = 80; *p++ = 55; *p++ = 14;
        *p++ = IDC_REPLACE_ALL;
        *p++ = 0xFFFF; *p++ = 0x0080;  /* Button class */
        nchar = MultiByteToWideChar(CP_ACP, 0, "Replace All", -1, (LPWSTR)p, 50);
        p += nchar;
        *p++ = 0;
        p = (PWORD)(((LONG_PTR)p + 3) & ~3);
    }

    return (LPDLGTEMPLATE)pdlgtemplate;
}

BOOL ShowFindDialog(HWND parent)
{
    if (g_findReplace.hwndFind) {
        ShowWindow(g_findReplace.hwndFind, SW_SHOW);
        SetFocus(g_findReplace.hwndFind);
        return TRUE;
    }

    LPDLGTEMPLATE templ = CreateFindReplaceTemplate(FALSE);
    g_findReplace.hwndFind = CreateDialogIndirect(GetModuleHandle(NULL), templ, parent, FindDialogProc);
    GlobalFree(templ);

    if (g_findReplace.hwndFind) {
        ShowWindow(g_findReplace.hwndFind, SW_SHOW);
        return TRUE;
    }
    return FALSE;
}

BOOL ShowReplaceDialog(HWND parent)
{
    if (g_findReplace.hwndReplace) {
        ShowWindow(g_findReplace.hwndReplace, SW_SHOW);
        SetFocus(g_findReplace.hwndReplace);
        return TRUE;
    }

    LPDLGTEMPLATE templ = CreateFindReplaceTemplate(TRUE);
    g_findReplace.hwndReplace = CreateDialogIndirect(GetModuleHandle(NULL), templ, parent, ReplaceDialogProc);
    GlobalFree(templ);

    if (g_findReplace.hwndReplace) {
        ShowWindow(g_findReplace.hwndReplace, SW_SHOW);
        return TRUE;
    }
    return FALSE;
}

void CreateFindControls(HWND hwnd) { UNREFERENCED_PARAMETER(hwnd); }
void CreateReplaceControls(HWND hwnd) { UNREFERENCED_PARAMETER(hwnd); }

BOOL FindNext(void)
{
    if (g_findReplace.findText[0] == '\0') {
        return FALSE;
    }
    
    /* Get current tab */
    int tabIndex = GetSelectedTab();
    if (tabIndex < 0) {
        return FALSE;
    }
    
    TabInfo* tab = GetTab(tabIndex);
    if (!tab || !tab->editorHandle) {
        return FALSE;
    }
    
    HWND editor = tab->editorHandle;
    
    /* Set search flags */
    int searchFlags = 0;
    if (g_findReplace.flags & 0x01) searchFlags |= SCFIND_MATCHCASE;
    if (g_findReplace.flags & 0x02) searchFlags |= SCFIND_WHOLEWORD;
    if (g_findReplace.flags & 0x04) searchFlags |= SCFIND_REGEXP;
    
    SendMessage(editor, SCI_SETSEARCHFLAGS, searchFlags, 0);
    
    /* Get current position and selection */
    int currentPos = (int)SendMessage(editor, SCI_GETCURRENTPOS, 0, 0);
    int anchor = (int)SendMessage(editor, SCI_GETANCHOR, 0, 0);
    int textLength = (int)SendMessage(editor, SCI_GETLENGTH, 0, 0);
    
    /* Determine search range based on direction */
    int searchStart, searchEnd;
    if (g_findReplace.searchDirection == SEARCH_DIRECTION_DOWN) {
        /* Search from current position or end of selection */
        searchStart = (currentPos > anchor) ? currentPos : anchor;
        searchEnd = textLength;
    } else {
        /* Search from current position or start of selection backwards */
        searchStart = (currentPos < anchor) ? currentPos : anchor;
        searchEnd = 0;
    }
    
    /* Perform search */
    SendMessage(editor, SCI_SETTARGETSTART, searchStart, 0);
    SendMessage(editor, SCI_SETTARGETEND, searchEnd, 0);
    
    int pos = (int)SendMessage(editor, SCI_SEARCHINTARGET, strlen(g_findReplace.findText), 
                         (LPARAM)g_findReplace.findText);
    
    /* If not found, wrap around */
    if (pos == -1) {
        if (g_findReplace.searchDirection == SEARCH_DIRECTION_DOWN) {
            SendMessage(editor, SCI_SETTARGETSTART, 0, 0);
            SendMessage(editor, SCI_SETTARGETEND, searchStart, 0);
        } else {
            SendMessage(editor, SCI_SETTARGETSTART, textLength, 0);
            SendMessage(editor, SCI_SETTARGETEND, 0, 0);
        }
        
        pos = (int)SendMessage(editor, SCI_SEARCHINTARGET, strlen(g_findReplace.findText),
                         (LPARAM)g_findReplace.findText);
    }
    
    /* Select found text */
    if (pos != -1) {
        int targetStart = (int)SendMessage(editor, SCI_GETTARGETSTART, 0, 0);
        int targetEnd = (int)SendMessage(editor, SCI_GETTARGETEND, 0, 0);
        
        SendMessage(editor, SCI_SETSEL, targetStart, targetEnd);
        SendMessage(editor, SCI_SCROLLCARET, 0, 0);
        
        return TRUE;
    }
    
    return FALSE;
}

BOOL FindPrevious(void)
{
    /* Temporarily set direction to up */
    SearchDirection oldDirection = g_findReplace.searchDirection;
    g_findReplace.searchDirection = SEARCH_DIRECTION_UP;
    
    BOOL result = FindNext();
    
    /* Restore direction */
    g_findReplace.searchDirection = oldDirection;
    
    return result;
}

BOOL Replace(void) { return FALSE; }
int ReplaceAll(void) { return 0; }
int CountMatches(void) { return 0; }

void AddToSearchHistory(const char* text)
{
    if (!text || text[0] == '\0') return;
    for (int i = 0; i < g_searchHistoryCount; i++) {
        if (strcmp(g_searchHistory[i], text) == 0) return;
    }
    if (g_searchHistoryCount < MAX_SEARCH_HISTORY) {
        g_searchHistory[g_searchHistoryCount++] = _strdup(text);
    }
}

void ClearSearchHistory(void)
{
    for (int i = 0; i < g_searchHistoryCount; i++) {
        if (g_searchHistory[i]) { free(g_searchHistory[i]); g_searchHistory[i] = NULL; }
    }
    g_searchHistoryCount = 0;
}

const char* GetFindText(void) { return g_findReplace.findText; }
FindReplaceState* GetFindReplaceState(void) { return &g_findReplace; }

/* Set find text for F3/Shift+F3 quick search */
void SetFindText(const char* text)
{
    if (text && text[0] != '\0') {
        strncpy(g_findReplace.findText, text, MAX_FIND_TEXT_LENGTH - 1);
        g_findReplace.findText[MAX_FIND_TEXT_LENGTH - 1] = '\0';
        g_findReplace.searchDirection = SEARCH_DIRECTION_DOWN;
    }
}
