/*
 * Preferences dialog implementation for Notepad+
 * Uses panels (bordered containers) for grouping settings
 */

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "preferences.h"
#include "resource.h"
#include "config.h"
#include "registry_config.h"
#include "tabs.h"
#include "statusbar.h"
#include "Scintilla.h"
#include "themes.h"
#include "shellintegrate.h"

/* Dialog dimensions */
#define DIALOG_WIDTH 600
#define DIALOG_HEIGHT 580

/* Control dimensions */
#define MARGIN 14
#define COL_WIDTH 260
#define COL_GAP 18
#define PANEL_PADDING 12
#define LABEL_HEIGHT 18
#define CONTROL_HEIGHT 20
#define BUTTON_WIDTH 80
#define BUTTON_HEIGHT 26
#define PANEL_GAP 12
#define ITEM_GAP 5

/* Zoom range */
#define ZOOM_MIN -10
#define ZOOM_MAX 20

/* Static variables */
static HWND g_dialogHwnd = NULL;
static HWND g_parentHwnd = NULL;
static AppConfig g_originalConfig;
static HFONT g_hFont = NULL;
static HFONT g_hBoldFont = NULL;
static HBRUSH g_panelBrush = NULL;

/* Control handles - Behavior */
static HWND g_singleInstanceCheck = NULL;
static HWND g_confirmExitCheck = NULL;
static HWND g_restoreSessionCheck = NULL;
static HWND g_saveOnExitCheck = NULL;

/* Control handles - Editor */
static HWND g_tabWidthEdit = NULL;
static HWND g_useSpacesCheck = NULL;
static HWND g_autoIndentCheck = NULL;
static HWND g_showWhitespaceCheck = NULL;
static HWND g_highlightLineCheck = NULL;

/* Control handles - New Tab Defaults */
static HWND g_defaultCodeFoldingCheck = NULL;
static HWND g_defaultBracketMatchingCheck = NULL;
static HWND g_defaultChangeHistoryCheck = NULL;

/* Control handles - Font */
static HWND g_fontLabel = NULL;
static HWND g_fontEdit = NULL;
static HWND g_fontButton = NULL;
static HWND g_fontSizeLabel = NULL;
static HWND g_fontSizeEdit = NULL;

/* Control handles - View */
static HWND g_showLineNumbersCheck = NULL;
static HWND g_wordWrapCheck = NULL;
static HWND g_showStatusBarCheck = NULL;
static HWND g_zoomSlider = NULL;
static HWND g_zoomValue = NULL;

/* Control handles - Find defaults */
static HWND g_matchCaseCheck = NULL;
static HWND g_wholeWordCheck = NULL;

/* Control handles - Theme */
static HWND g_themeCombo = NULL;

/* Control handles - View Settings */
static HWND g_applyToAllTabsCheck = NULL;

/* Control handles - Shell Integration */
static HWND g_shellCtxStatusLabel = NULL;
static HWND g_shellCtxButton = NULL;
static HWND g_shellFileStatusLabel = NULL;
static HWND g_shellFileButton = NULL;
static HWND g_shellAdminLabel = NULL;

/* Panel handles for cleanup */
static HWND g_panels[7] = {NULL};

/* Preferences dialog window class */
#define PREFS_CLASS_NAME "NotepadPlusPreferencesClass"
#define PANEL_CLASS_NAME "NotepadPlusPanelClass"
static BOOL g_prefsClassRegistered = FALSE;
static BOOL g_panelClassRegistered = FALSE;

/* Forward declarations */
static LRESULT CALLBACK PreferencesDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL RegisterPreferencesClass(void);
static BOOL RegisterPanelClass(void);
static void CreateControls(HWND parent);
static void LoadCurrentSettings(void);
static void ApplyPreferences(void);
static void UpdateZoomDisplay(int zoom);
static void ShowFontChooser(HWND parent);
static void ApplyFontToAllEditors(const char* fontName, int fontSize);
static void UpdateShellIntegrationStatus(void);
static void HandleShellContextMenuButton(void);
static void HandleShellFileAssocButton(void);

/* Apply font to all editors */
static void ApplyFontToAllEditors(const char* fontName, int fontSize)
{
    int tabCount = GetTabCount();
    for (int i = 0; i < tabCount; i++) {
        TabInfo* tab = GetTab(i);
        if (tab && tab->editorHandle) {
            SendMessage(tab->editorHandle, SCI_STYLESETFONT, STYLE_DEFAULT, (LPARAM)fontName);
            SendMessage(tab->editorHandle, SCI_STYLESETSIZE, STYLE_DEFAULT, fontSize);
            SendMessage(tab->editorHandle, SCI_STYLECLEARALL, 0, 0);
        }
    }
}

/* Show font chooser dialog */
static void ShowFontChooser(HWND parent)
{
    AppConfig* config = GetConfig();
    if (!config) return;
    
    LOGFONT lf = {0};
    CHOOSEFONT cf = {0};
    
    /* Initialize LOGFONT with current font */
    strncpy(lf.lfFaceName, config->fontName, LF_FACESIZE - 1);
    lf.lfFaceName[LF_FACESIZE - 1] = '\0';
    
    /* Set font height based on current size (negative for character height) */
    HDC hdc = GetDC(NULL);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(NULL, hdc);
    lf.lfHeight = -MulDiv(config->fontSize, dpi, 72);
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = FF_MODERN | FIXED_PITCH;
    
    /* Initialize CHOOSEFONT */
    cf.lStructSize = sizeof(CHOOSEFONT);
    cf.hwndOwner = parent;
    cf.lpLogFont = &lf;
    cf.Flags = CF_SCREENFONTS | CF_FIXEDPITCHONLY | CF_INITTOLOGFONTSTRUCT | CF_FORCEFONTEXIST;
    cf.nFontType = SCREEN_FONTTYPE;
    
    /* Show font dialog */
    if (ChooseFont(&cf)) {
        /* Copy selected font name */
        strncpy(config->fontName, lf.lfFaceName, sizeof(config->fontName) - 1);
        config->fontName[sizeof(config->fontName) - 1] = '\0';
        
        /* Calculate point size from height */
        config->fontSize = MulDiv(-lf.lfHeight, 72, dpi);
        if (config->fontSize < 6) config->fontSize = 6;
        if (config->fontSize > 72) config->fontSize = 72;
        
        /* Update UI controls */
        SetWindowText(g_fontEdit, config->fontName);
        char buf[16];
        sprintf(buf, "%d", config->fontSize);
        SetWindowText(g_fontSizeEdit, buf);
        
        /* Apply to all editors */
        ApplyFontToAllEditors(config->fontName, config->fontSize);
    }
}

/* Create system font */
static HFONT CreateSystemFont(void)
{
    NONCLIENTMETRICS ncm;
    ncm.cbSize = sizeof(NONCLIENTMETRICS);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
    return CreateFontIndirect(&ncm.lfMessageFont);
}

/* Create bold font for section headers */
static HFONT CreateBoldSystemFont(void)
{
    NONCLIENTMETRICS ncm;
    ncm.cbSize = sizeof(NONCLIENTMETRICS);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
    ncm.lfMessageFont.lfWeight = FW_BOLD;
    return CreateFontIndirect(&ncm.lfMessageFont);
}

/* Update zoom value display */
static void UpdateZoomDisplay(int zoom)
{
    if (g_zoomValue) {
        char text[32];
        sprintf(text, "%+d", zoom);
        SetWindowText(g_zoomValue, text);
        InvalidateRect(g_zoomValue, NULL, TRUE);
        UpdateWindow(g_zoomValue);
    }
}

/* Apply zoom to all editors */
static void ApplyZoomToAllEditors(int zoom)
{
    int tabCount = GetTabCount();
    for (int i = 0; i < tabCount; i++) {
        TabInfo* tab = GetTab(i);
        if (tab && tab->editorHandle) {
            SendMessage(tab->editorHandle, SCI_SETZOOM, zoom, 0);
        }
    }
    UpdateZoomLevel(zoom);
}

/* Load current settings into dialog controls */
static void LoadCurrentSettings(void)
{
    AppConfig* config = GetConfig();
    if (!config) return;
    
    SendMessage(g_singleInstanceCheck, BM_SETCHECK, config->singleInstance ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_confirmExitCheck, BM_SETCHECK, config->confirmExit ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_restoreSessionCheck, BM_SETCHECK, config->restoreSession ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_saveOnExitCheck, BM_SETCHECK, config->saveOnExit ? BST_CHECKED : BST_UNCHECKED, 0);
    
    char buf[16];
    sprintf(buf, "%d", config->tabWidth);
    SetWindowText(g_tabWidthEdit, buf);
    SendMessage(g_useSpacesCheck, BM_SETCHECK, config->useSpaces ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_autoIndentCheck, BM_SETCHECK, config->autoIndent ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_showWhitespaceCheck, BM_SETCHECK, config->showWhitespace ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_highlightLineCheck, BM_SETCHECK, config->highlightCurrentLine ? BST_CHECKED : BST_UNCHECKED, 0);
    
    SendMessage(g_showLineNumbersCheck, BM_SETCHECK, config->showLineNumbers ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_wordWrapCheck, BM_SETCHECK, config->wordWrap ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_showStatusBarCheck, BM_SETCHECK, config->showStatusBar ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_zoomSlider, TBM_SETPOS, TRUE, config->zoomLevel - ZOOM_MIN);
    UpdateZoomDisplay(config->zoomLevel);
    
    SendMessage(g_matchCaseCheck, BM_SETCHECK, config->matchCase ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_wholeWordCheck, BM_SETCHECK, config->wholeWord ? BST_CHECKED : BST_UNCHECKED, 0);
    
    SendMessage(g_themeCombo, CB_SETCURSEL, config->theme, 0);
    
    SendMessage(g_defaultCodeFoldingCheck, BM_SETCHECK, config->codeFoldingEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_defaultBracketMatchingCheck, BM_SETCHECK, config->bracketMatching ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_defaultChangeHistoryCheck, BM_SETCHECK, config->changeHistoryEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    
    /* Font settings */
    SetWindowText(g_fontEdit, config->fontName);
    sprintf(buf, "%d", config->fontSize);
    SetWindowText(g_fontSizeEdit, buf);
    
    SendMessage(g_applyToAllTabsCheck, BM_SETCHECK, BST_UNCHECKED, 0);
    
    /* Load shell integration status */
    UpdateShellIntegrationStatus();
}

/* Apply settings from dialog controls */
static void ApplyPreferences(void)
{
    AppConfig* config = GetConfig();
    if (!config) return;
    
    config->singleInstance = (SendMessage(g_singleInstanceCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    config->confirmExit = (SendMessage(g_confirmExitCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    config->restoreSession = (SendMessage(g_restoreSessionCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    config->saveOnExit = (SendMessage(g_saveOnExitCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    
    char buf[16];
    GetWindowText(g_tabWidthEdit, buf, sizeof(buf));
    config->tabWidth = atoi(buf);
    if (config->tabWidth < 1) config->tabWidth = 4;
    if (config->tabWidth > 16) config->tabWidth = 16;
    
    config->useSpaces = (SendMessage(g_useSpacesCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    config->autoIndent = (SendMessage(g_autoIndentCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    config->showWhitespace = (SendMessage(g_showWhitespaceCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    config->highlightCurrentLine = (SendMessage(g_highlightLineCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    
    config->showLineNumbers = (SendMessage(g_showLineNumbersCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    config->wordWrap = (SendMessage(g_wordWrapCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    config->showStatusBar = (SendMessage(g_showStatusBarCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    int pos = (int)SendMessage(g_zoomSlider, TBM_GETPOS, 0, 0);
    config->zoomLevel = pos + ZOOM_MIN;
    
    config->matchCase = (SendMessage(g_matchCaseCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    config->wholeWord = (SendMessage(g_wholeWordCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    
    config->codeFoldingEnabled = (SendMessage(g_defaultCodeFoldingCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    config->bracketMatching = (SendMessage(g_defaultBracketMatchingCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    config->changeHistoryEnabled = (SendMessage(g_defaultChangeHistoryCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    
    /* Font settings */
    GetWindowText(g_fontEdit, config->fontName, sizeof(config->fontName));
    GetWindowText(g_fontSizeEdit, buf, sizeof(buf));
    config->fontSize = atoi(buf);
    if (config->fontSize < 6) config->fontSize = 6;
    if (config->fontSize > 72) config->fontSize = 72;
    
    int themeIdx = (int)SendMessage(g_themeCombo, CB_GETCURSEL, 0, 0);
    if (themeIdx >= 0) config->theme = themeIdx;
    
    BOOL applyToAllTabs = (SendMessage(g_applyToAllTabsCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    
    ApplyZoomToAllEditors(config->zoomLevel);
    SetTheme((Theme)config->theme);
    extern void ApplyThemeToAllEditors(void);
    ApplyThemeToAllEditors();
    extern void ShowStatusBar(BOOL show);
    ShowStatusBar(config->showStatusBar);
    
    if (applyToAllTabs) {
        int tabCount = GetTabCount();
        for (int i = 0; i < tabCount; i++) {
            TabInfo* tab = GetTab(i);
            if (tab && tab->editorHandle) {
                SendMessage(tab->editorHandle, SCI_SETTABWIDTH, config->tabWidth, 0);
                SendMessage(tab->editorHandle, SCI_SETUSETABS, !config->useSpaces, 0);
                SendMessage(tab->editorHandle, SCI_SETVIEWWS, config->showWhitespace ? SCWS_VISIBLEALWAYS : SCWS_INVISIBLE, 0);
                SendMessage(tab->editorHandle, SCI_SETCARETLINEVISIBLE, config->highlightCurrentLine, 0);
                
                if (config->showLineNumbers) {
                    SendMessage(tab->editorHandle, SCI_SETMARGINWIDTHN, 0, 40);
                } else {
                    SendMessage(tab->editorHandle, SCI_SETMARGINWIDTHN, 0, 0);
                }
                
                SendMessage(tab->editorHandle, SCI_SETWRAPMODE, config->wordWrap ? SC_WRAP_WORD : SC_WRAP_NONE, 0);
                
                tab->wordWrap = config->wordWrap;
                tab->showLineNumbers = config->showLineNumbers;
                tab->showWhitespace = config->showWhitespace;
                tab->autoIndent = config->autoIndent;
            }
        }
    } else {
        int activeTab = GetSelectedTab();
        if (activeTab >= 0) {
            TabInfo* tab = GetTab(activeTab);
            if (tab && tab->editorHandle) {
                SendMessage(tab->editorHandle, SCI_SETTABWIDTH, config->tabWidth, 0);
                SendMessage(tab->editorHandle, SCI_SETUSETABS, !config->useSpaces, 0);
                SendMessage(tab->editorHandle, SCI_SETVIEWWS, config->showWhitespace ? SCWS_VISIBLEALWAYS : SCWS_INVISIBLE, 0);
                SendMessage(tab->editorHandle, SCI_SETCARETLINEVISIBLE, config->highlightCurrentLine, 0);
                
                if (config->showLineNumbers) {
                    SendMessage(tab->editorHandle, SCI_SETMARGINWIDTHN, 0, 40);
                } else {
                    SendMessage(tab->editorHandle, SCI_SETMARGINWIDTHN, 0, 0);
                }
                
                SendMessage(tab->editorHandle, SCI_SETWRAPMODE, config->wordWrap ? SC_WRAP_WORD : SC_WRAP_NONE, 0);
                
                tab->wordWrap = config->wordWrap;
                tab->showLineNumbers = config->showLineNumbers;
                tab->showWhitespace = config->showWhitespace;
                tab->autoIndent = config->autoIndent;
            }
        }
    }
    
    SaveConfig();
}

/* Update shell integration status display */
static void UpdateShellIntegrationStatus(void)
{
    BOOL isAdmin = IsRunningAsAdministrator();
    BOOL ctxInstalled = IsContextMenuInstalled();
    BOOL fileAssocRegistered = AreFileAssociationsRegistered();
    
    /* Update context menu status */
    if (ctxInstalled) {
        SetWindowText(g_shellCtxStatusLabel, "Installed");
        SetWindowText(g_shellCtxButton, "Remove");
    } else {
        SetWindowText(g_shellCtxStatusLabel, "Not installed");
        SetWindowText(g_shellCtxButton, "Install");
    }
    
    /* Update file associations status */
    if (fileAssocRegistered) {
        SetWindowText(g_shellFileStatusLabel, "Registered");
        SetWindowText(g_shellFileButton, "Unregister");
    } else {
        SetWindowText(g_shellFileStatusLabel, "Not registered");
        SetWindowText(g_shellFileButton, "Register");
    }
    
    /* Update admin notice visibility */
    if (isAdmin) {
        ShowWindow(g_shellAdminLabel, SW_HIDE);
    } else {
        ShowWindow(g_shellAdminLabel, SW_SHOW);
    }
}

/* Handle context menu button click */
static void HandleShellContextMenuButton(void)
{
    /* Check if we have admin privileges */
    if (!IsRunningAsAdministrator()) {
        /* Ask user if they want to restart as admin */
        int result = MessageBox(g_dialogHwnd, 
            "Administrator privileges are required to modify the Windows registry.\n\n"
            "Would you like to restart Notepad+ as administrator?",
            "Administrator Required", 
            MB_ICONQUESTION | MB_YESNO);
        
        if (result == IDYES) {
            RequestAdministratorPrivileges();
        }
        return;
    }
    
    /* Check current state and toggle */
    if (IsContextMenuInstalled()) {
        /* Uninstall */
        if (UninstallContextMenu()) {
            MessageBox(g_dialogHwnd, 
                "Context menu entries have been removed successfully.",
                "Success", MB_ICONINFORMATION | MB_OK);
        }
    } else {
        /* Install */
        if (InstallContextMenu()) {
            MessageBox(g_dialogHwnd, 
                "Context menu entries have been installed successfully.\n\n"
                "You can now right-click on files, folders, or the desktop background "
                "to open them with Notepad+.",
                "Success", MB_ICONINFORMATION | MB_OK);
        }
    }
    
    /* Update status display */
    UpdateShellIntegrationStatus();
}

/* Handle file associations button click */
static void HandleShellFileAssocButton(void)
{
    /* Check if we have admin privileges */
    if (!IsRunningAsAdministrator()) {
        /* Ask user if they want to restart as admin */
        int result = MessageBox(g_dialogHwnd, 
            "Administrator privileges are required to modify the Windows registry.\n\n"
            "Would you like to restart Notepad+ as administrator?",
            "Administrator Required", 
            MB_ICONQUESTION | MB_YESNO);
        
        if (result == IDYES) {
            RequestAdministratorPrivileges();
        }
        return;
    }
    
    /* Check current state and toggle */
    if (AreFileAssociationsRegistered()) {
        /* Unregister */
        if (UnregisterFileAssociations()) {
            MessageBox(g_dialogHwnd, 
                "File associations have been removed successfully.",
                "Success", MB_ICONINFORMATION | MB_OK);
        }
    } else {
        /* Register */
        if (RegisterFileAssociations()) {
            MessageBox(g_dialogHwnd, 
                "File associations have been registered successfully.\n\n"
                "Notepad+ will now appear in the 'Open With' list for supported file types.",
                "Success", MB_ICONINFORMATION | MB_OK);
        }
    }
    
    /* Update status display */
    UpdateShellIntegrationStatus();
}

/* Panel window procedure */
static LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_ERASEBKGND:
        {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, g_panelBrush);
            return 1;
        }
        
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
            SetBkMode((HDC)wParam, TRANSPARENT);
            return (LRESULT)g_panelBrush;
            
        case WM_COMMAND:
            /* Forward command messages to parent dialog */
            SendMessage(GetParent(hwnd), msg, wParam, lParam);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/* Create a panel with title */
static HWND CreatePanel(HWND parent, const char* title, int x, int y, int w, int h, int idx)
{
    HWND panel = CreateWindowEx(WS_EX_CONTROLPARENT, PANEL_CLASS_NAME, "",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, w, h, parent, NULL, GetModuleHandle(NULL), NULL);
    
    g_panels[idx] = panel;
    
    HWND titleLabel = CreateWindowEx(0, "STATIC", title,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        PANEL_PADDING, 6, w - 2 * PANEL_PADDING, LABEL_HEIGHT, panel, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(titleLabel, WM_SETFONT, (WPARAM)g_hBoldFont, TRUE);
    
    return panel;
}

/* Create all controls with two-column layout using panels */
static void CreateControls(HWND parent)
{
    int col1X = MARGIN;
    int col2X = MARGIN + COL_WIDTH + COL_GAP;
    int checkWidth = COL_WIDTH - 2 * PANEL_PADDING - 8;
    HWND panel, label;
    int py;
    
    /* Behavior Panel */
    int behaviorH = 125;
    panel = CreatePanel(parent, "Behavior", col1X, MARGIN, COL_WIDTH, behaviorH, 0);
    py = 28;
    
    g_singleInstanceCheck = CreateWindowEx(0, "BUTTON", "Single instance mode",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        PANEL_PADDING, py, checkWidth, CONTROL_HEIGHT, panel, (HMENU)IDC_PREFS_SINGLE_INSTANCE,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_singleInstanceCheck, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    py += CONTROL_HEIGHT + ITEM_GAP;
    
    g_confirmExitCheck = CreateWindowEx(0, "BUTTON", "Confirm on exit",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        PANEL_PADDING, py, checkWidth, CONTROL_HEIGHT, panel, (HMENU)IDC_PREFS_CONFIRM_EXIT,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_confirmExitCheck, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    py += CONTROL_HEIGHT + ITEM_GAP;
    
    g_restoreSessionCheck = CreateWindowEx(0, "BUTTON", "Restore session on startup",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        PANEL_PADDING, py, checkWidth, CONTROL_HEIGHT, panel, (HMENU)IDC_PREFS_RESTORE_SESSION,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_restoreSessionCheck, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    py += CONTROL_HEIGHT + ITEM_GAP;
    
    g_saveOnExitCheck = CreateWindowEx(0, "BUTTON", "Save session on exit",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        PANEL_PADDING, py, checkWidth, CONTROL_HEIGHT, panel, (HMENU)IDC_PREFS_SAVE_ON_EXIT,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_saveOnExitCheck, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    /* Editor Panel */
    int editorY = MARGIN + behaviorH + PANEL_GAP;
    int editorH = 155;
    panel = CreatePanel(parent, "Editor", col1X, editorY, COL_WIDTH, editorH, 1);
    py = 28;
    
    label = CreateWindowEx(0, "STATIC", "Tab width:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        PANEL_PADDING, py + 2, 65, LABEL_HEIGHT, panel, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(label, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    g_tabWidthEdit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "4",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_CENTER,
        PANEL_PADDING + 70, py, 35, CONTROL_HEIGHT, panel, (HMENU)IDC_PREFS_TAB_WIDTH_EDIT,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_tabWidthEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessage(g_tabWidthEdit, EM_SETLIMITTEXT, 2, 0);
    py += CONTROL_HEIGHT + ITEM_GAP;
    
    g_useSpacesCheck = CreateWindowEx(0, "BUTTON", "Use spaces instead of tabs",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        PANEL_PADDING, py, checkWidth, CONTROL_HEIGHT, panel, (HMENU)IDC_PREFS_USE_SPACES,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_useSpacesCheck, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    py += CONTROL_HEIGHT + ITEM_GAP;
    
    g_autoIndentCheck = CreateWindowEx(0, "BUTTON", "Auto indent",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        PANEL_PADDING, py, checkWidth, CONTROL_HEIGHT, panel, (HMENU)IDC_PREFS_AUTO_INDENT,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_autoIndentCheck, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    py += CONTROL_HEIGHT + ITEM_GAP;
    
    g_showWhitespaceCheck = CreateWindowEx(0, "BUTTON", "Show whitespace",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        PANEL_PADDING, py, checkWidth, CONTROL_HEIGHT, panel, (HMENU)IDC_PREFS_SHOW_WHITESPACE,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_showWhitespaceCheck, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    py += CONTROL_HEIGHT + ITEM_GAP;
    
    g_highlightLineCheck = CreateWindowEx(0, "BUTTON", "Highlight current line",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        PANEL_PADDING, py, checkWidth, CONTROL_HEIGHT, panel, (HMENU)IDC_PREFS_HIGHLIGHT_LINE,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_highlightLineCheck, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    /* Font Panel */
    int fontY = editorY + editorH + PANEL_GAP;
    int fontH = 95;
    panel = CreatePanel(parent, "Font", col1X, fontY, COL_WIDTH, fontH, 5);
    py = 28;
    
    g_fontLabel = CreateWindowEx(0, "STATIC", "Font:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        PANEL_PADDING, py + 2, 35, LABEL_HEIGHT, panel, (HMENU)IDC_PREFS_FONT_LABEL,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_fontLabel, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    g_fontEdit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_READONLY,
        PANEL_PADDING + 40, py, 110, CONTROL_HEIGHT, panel, (HMENU)IDC_PREFS_FONT_EDIT,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_fontEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    g_fontButton = CreateWindowEx(0, "BUTTON", "Choose...",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        PANEL_PADDING + 155, py, 65, CONTROL_HEIGHT, panel, (HMENU)IDC_PREFS_FONT_BUTTON,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_fontButton, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    py += CONTROL_HEIGHT + ITEM_GAP + 4;
    
    g_fontSizeLabel = CreateWindowEx(0, "STATIC", "Size:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        PANEL_PADDING, py + 2, 35, LABEL_HEIGHT, panel, (HMENU)IDC_PREFS_FONTSIZE_LABEL,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_fontSizeLabel, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    g_fontSizeEdit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_CENTER,
        PANEL_PADDING + 40, py, 40, CONTROL_HEIGHT, panel, (HMENU)IDC_PREFS_FONTSIZE_EDIT,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_fontSizeEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessage(g_fontSizeEdit, EM_SETLIMITTEXT, 2, 0);
    
    /* View Panel */
    int viewH = 155;
    panel = CreatePanel(parent, "View", col2X, MARGIN, COL_WIDTH, viewH, 2);
    py = 28;
    
    g_showLineNumbersCheck = CreateWindowEx(0, "BUTTON", "Show line numbers",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        PANEL_PADDING, py, checkWidth, CONTROL_HEIGHT, panel, (HMENU)IDC_PREFS_SHOW_LINE_NUMBERS,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_showLineNumbersCheck, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    py += CONTROL_HEIGHT + ITEM_GAP;
    
    g_wordWrapCheck = CreateWindowEx(0, "BUTTON", "Word wrap",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        PANEL_PADDING, py, checkWidth, CONTROL_HEIGHT, panel, (HMENU)IDC_PREFS_WORD_WRAP,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_wordWrapCheck, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    py += CONTROL_HEIGHT + ITEM_GAP;
    
    g_showStatusBarCheck = CreateWindowEx(0, "BUTTON", "Show status bar",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        PANEL_PADDING, py, checkWidth, CONTROL_HEIGHT, panel, (HMENU)IDC_PREFS_SHOW_STATUSBAR,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_showStatusBarCheck, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    py += CONTROL_HEIGHT + ITEM_GAP + 6;
    
    label = CreateWindowEx(0, "STATIC", "Zoom:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        PANEL_PADDING, py + 3, 42, LABEL_HEIGHT, panel, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(label, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    g_zoomSlider = CreateWindowEx(0, TRACKBAR_CLASS, "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_AUTOTICKS,
        PANEL_PADDING + 45, py, checkWidth - 85, 26, panel, (HMENU)IDC_PREFS_ZOOM_SLIDER,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_zoomSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, ZOOM_MAX - ZOOM_MIN));
    SendMessage(g_zoomSlider, TBM_SETTICFREQ, 5, 0);
    
    g_zoomValue = CreateWindowEx(0, "STATIC", "+0",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        PANEL_PADDING + checkWidth - 35, py + 3, 35, LABEL_HEIGHT, panel, (HMENU)IDC_PREFS_ZOOM_VALUE,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_zoomValue, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    /* Find Defaults Panel */
    int findY = MARGIN + viewH + PANEL_GAP;
    int findH = 75;
    panel = CreatePanel(parent, "Find Defaults", col2X, findY, COL_WIDTH, findH, 3);
    py = 28;
    
    g_matchCaseCheck = CreateWindowEx(0, "BUTTON", "Match case",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        PANEL_PADDING, py, checkWidth, CONTROL_HEIGHT, panel, (HMENU)IDC_PREFS_MATCH_CASE,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_matchCaseCheck, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    py += CONTROL_HEIGHT + ITEM_GAP;
    
    g_wholeWordCheck = CreateWindowEx(0, "BUTTON", "Whole word only",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        PANEL_PADDING, py, checkWidth, CONTROL_HEIGHT, panel, (HMENU)IDC_PREFS_WHOLE_WORD,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_wholeWordCheck, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    /* Appearance Panel */
    int appearY = findY + findH + PANEL_GAP;
    int appearH = 65;
    panel = CreatePanel(parent, "Appearance", col2X, appearY, COL_WIDTH, appearH, 4);
    py = 28;
    
    label = CreateWindowEx(0, "STATIC", "Theme:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        PANEL_PADDING, py + 3, 48, LABEL_HEIGHT, panel, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(label, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    g_themeCombo = CreateWindowEx(0, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
        PANEL_PADDING + 52, py, 110, 200, panel, (HMENU)IDC_PREFS_THEME_COMBO,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_themeCombo, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessage(g_themeCombo, CB_ADDSTRING, 0, (LPARAM)"Light");
    SendMessage(g_themeCombo, CB_ADDSTRING, 0, (LPARAM)"Dark");
    
    /* Shell Integration Panel */
    int shellY = appearY + appearH + PANEL_GAP;
    int shellH = 115;
    panel = CreatePanel(parent, "Shell Integration", col2X, shellY, COL_WIDTH, shellH, 6);
    py = 28;
    
    /* Context Menu Status */
    label = CreateWindowEx(0, "STATIC", "Context menu:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        PANEL_PADDING, py + 2, 80, LABEL_HEIGHT, panel, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(label, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    g_shellCtxStatusLabel = CreateWindowEx(0, "STATIC", "Not installed",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        PANEL_PADDING + 85, py + 2, 70, LABEL_HEIGHT, panel, (HMENU)IDC_PREFS_SHELL_CTX_STATUS,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_shellCtxStatusLabel, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    g_shellCtxButton = CreateWindowEx(0, "BUTTON", "Install",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        PANEL_PADDING + 160, py, 70, CONTROL_HEIGHT, panel, (HMENU)IDC_PREFS_SHELL_CTX_BUTTON,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_shellCtxButton, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    py += CONTROL_HEIGHT + ITEM_GAP + 2;
    
    /* File Associations Status */
    label = CreateWindowEx(0, "STATIC", "File types:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        PANEL_PADDING, py + 2, 80, LABEL_HEIGHT, panel, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(label, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    g_shellFileStatusLabel = CreateWindowEx(0, "STATIC", "Not registered",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        PANEL_PADDING + 85, py + 2, 70, LABEL_HEIGHT, panel, (HMENU)IDC_PREFS_SHELL_FILE_STATUS,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_shellFileStatusLabel, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    g_shellFileButton = CreateWindowEx(0, "BUTTON", "Register",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        PANEL_PADDING + 160, py, 70, CONTROL_HEIGHT, panel, (HMENU)IDC_PREFS_SHELL_FILE_BUTTON,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_shellFileButton, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    py += CONTROL_HEIGHT + ITEM_GAP + 4;
    
    /* Admin notice */
    g_shellAdminLabel = CreateWindowEx(0, "STATIC", "Administrator privileges required",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        PANEL_PADDING, py, checkWidth, LABEL_HEIGHT, panel, (HMENU)IDC_PREFS_SHELL_ADMIN_MSG,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_shellAdminLabel, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    /* Buttons at bottom */
    int buttonY = DIALOG_HEIGHT - MARGIN - BUTTON_HEIGHT - 40;
    int buttonX = DIALOG_WIDTH - MARGIN - 2 * (BUTTON_WIDTH + 10) - 20;
    
    HWND okButton = CreateWindowEx(0, "BUTTON", "OK",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        buttonX, buttonY, BUTTON_WIDTH, BUTTON_HEIGHT,
        parent, (HMENU)IDC_PREFS_OK,
        GetModuleHandle(NULL), NULL);
    SendMessage(okButton, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    HWND cancelButton = CreateWindowEx(0, "BUTTON", "Cancel",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        buttonX + BUTTON_WIDTH + 10, buttonY, BUTTON_WIDTH, BUTTON_HEIGHT,
        parent, (HMENU)IDC_PREFS_CANCEL,
        GetModuleHandle(NULL), NULL);
    SendMessage(cancelButton, WM_SETFONT, (WPARAM)g_hFont, TRUE);
}

/* Dialog window procedure */
static LRESULT CALLBACK PreferencesDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            HPEN hPen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
            HPEN hOldPen = SelectObject(hdc, hPen);
            HBRUSH hOldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            
            for (int i = 0; i < 7; i++) {
                if (g_panels[i]) {
                    RECT rc;
                    GetWindowRect(g_panels[i], &rc);
                    MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&rc, 2);
                    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
                }
            }
            
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hPen);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_HSCROLL:
            if ((HWND)lParam == g_zoomSlider) {
                int pos = (int)SendMessage(g_zoomSlider, TBM_GETPOS, 0, 0);
                int zoom = pos + ZOOM_MIN;
                UpdateZoomDisplay(zoom);
                AppConfig* config = GetConfig();
                if (config) config->zoomLevel = zoom;
                ApplyZoomToAllEditors(zoom);
            }
            break;
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_PREFS_OK:
                case IDOK:
                    ApplyPreferences();
                    DestroyWindow(hwnd);
                    return 0;
                    
                case IDC_PREFS_CANCEL:
                case IDCANCEL:
                    ApplyZoomToAllEditors(g_originalConfig.zoomLevel);
                    DestroyWindow(hwnd);
                    return 0;
                    
                case IDC_PREFS_FONT_BUTTON:
                    ShowFontChooser(hwnd);
                    return 0;
                    
                case IDC_PREFS_SHELL_CTX_BUTTON:
                    HandleShellContextMenuButton();
                    return 0;
                    
                case IDC_PREFS_SHELL_FILE_BUTTON:
                    HandleShellFileAssocButton();
                    return 0;
            }
            break;
            
        case WM_CLOSE:
            ApplyZoomToAllEditors(g_originalConfig.zoomLevel);
            DestroyWindow(hwnd);
            return 0;
            
        case WM_DESTROY:
        {
            if (g_parentHwnd && IsWindow(g_parentHwnd)) {
                EnableWindow(g_parentHwnd, TRUE);
                SetForegroundWindow(g_parentHwnd);
            }
            
            if (g_hFont) { DeleteObject(g_hFont); g_hFont = NULL; }
            if (g_hBoldFont) { DeleteObject(g_hBoldFont); g_hBoldFont = NULL; }
            if (g_panelBrush) { DeleteObject(g_panelBrush); g_panelBrush = NULL; }
            
            g_dialogHwnd = NULL;
            g_parentHwnd = NULL;
            return 0;
        }
            
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
            SetBkMode((HDC)wParam, TRANSPARENT);
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/* Register panel window class */
static BOOL RegisterPanelClass(void)
{
    if (g_panelClassRegistered) return TRUE;
    
    WNDCLASS wc = {0};
    wc.lpfnWndProc = PanelProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = PANEL_CLASS_NAME;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    
    if (!RegisterClass(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return FALSE;
    }
    g_panelClassRegistered = TRUE;
    return TRUE;
}

/* Register preferences dialog window class */
static BOOL RegisterPreferencesClass(void)
{
    if (g_prefsClassRegistered) return TRUE;
    
    WNDCLASS wc = {0};
    wc.lpfnWndProc = PreferencesDialogProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    wc.lpszClassName = PREFS_CLASS_NAME;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    
    if (!RegisterClass(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return FALSE;
    }
    g_prefsClassRegistered = TRUE;
    return TRUE;
}

/* Show preferences dialog */
BOOL ShowPreferencesDialog(HWND parent)
{
    if (g_dialogHwnd && IsWindow(g_dialogHwnd)) {
        SetForegroundWindow(g_dialogHwnd);
        return TRUE;
    }
    
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);
    
    if (!RegisterPanelClass()) return FALSE;
    if (!RegisterPreferencesClass()) return FALSE;
    
    g_hFont = CreateSystemFont();
    g_hBoldFont = CreateBoldSystemFont();
    g_panelBrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    
    if (!g_hFont || !g_hBoldFont || !g_panelBrush) {
        if (g_hFont) DeleteObject(g_hFont);
        if (g_hBoldFont) DeleteObject(g_hBoldFont);
        if (g_panelBrush) DeleteObject(g_panelBrush);
        g_hFont = NULL; g_hBoldFont = NULL; g_panelBrush = NULL;
        return FALSE;
    }
    
    g_parentHwnd = parent;
    
    AppConfig* config = GetConfig();
    if (config) memcpy(&g_originalConfig, config, sizeof(AppConfig));
    
    RECT parentRect;
    GetWindowRect(parent, &parentRect);
    int x = parentRect.left + (parentRect.right - parentRect.left - DIALOG_WIDTH) / 2;
    int y = parentRect.top + (parentRect.bottom - parentRect.top - DIALOG_HEIGHT) / 2;
    
    g_dialogHwnd = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        PREFS_CLASS_NAME, "Preferences",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        x, y, DIALOG_WIDTH, DIALOG_HEIGHT,
        parent, NULL, GetModuleHandle(NULL), NULL
    );
    
    if (!g_dialogHwnd) {
        if (g_hFont) DeleteObject(g_hFont);
        if (g_hBoldFont) DeleteObject(g_hBoldFont);
        if (g_panelBrush) DeleteObject(g_panelBrush);
        g_hFont = NULL; g_hBoldFont = NULL; g_panelBrush = NULL;
        return FALSE;
    }
    
    CreateControls(g_dialogHwnd);
    LoadCurrentSettings();
    
    EnableWindow(parent, FALSE);
    ShowWindow(g_dialogHwnd, SW_SHOW);
    UpdateWindow(g_dialogHwnd);
    SetFocus(g_dialogHwnd);
    
    return TRUE;
}
