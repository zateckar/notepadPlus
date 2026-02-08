/*
 * Theme system implementation for Notepad+
 * Supports dark and light themes
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "themes.h"
#include "resource.h"
#include "tabs.h"
#include "toolbar.h"
#include "statusbar.h"
#include "window.h"
#include "Scintilla.h"
#include "syntax.h"

static Theme g_theme = THEME_LIGHT;
static ThemeColors g_themeColors = {0};

static void LoadLightThemeColors(void)
{
    /* Modern Windows 11-style light theme palette */
    
    /* Editor colors - clean white with subtle accents */
    g_themeColors.editorBg = RGB(255, 255, 255);
    g_themeColors.editorFg = RGB(28, 28, 28);
    g_themeColors.editorSelBg = RGB(0, 103, 192);        /* Windows accent blue */
    g_themeColors.editorSelFg = RGB(255, 255, 255);
    g_themeColors.editorLineNumBg = RGB(250, 250, 250);
    g_themeColors.editorLineNumFg = RGB(120, 120, 120);
    g_themeColors.editorCaretLineBg = RGB(245, 245, 245);
    
    /* Tab colors - softer grays, subtle borders */
    g_themeColors.tabNormalBg = RGB(243, 243, 243);
    g_themeColors.tabNormalFg = RGB(96, 96, 96);
    g_themeColors.tabNormalBorder = RGB(229, 229, 229);
    g_themeColors.tabHoverBg = RGB(250, 250, 250);
    g_themeColors.tabHoverFg = RGB(28, 28, 28);
    g_themeColors.tabSelectedBg = RGB(255, 255, 255);
    g_themeColors.tabSelectedFg = RGB(28, 28, 28);
    g_themeColors.tabSelectedBorder = RGB(229, 229, 229);
    
    /* Toolbar colors - clean flat design */
    g_themeColors.toolbarBg = RGB(249, 249, 249);
    g_themeColors.toolbarBtnBg = RGB(249, 249, 249);
    g_themeColors.toolbarBtnFg = RGB(28, 28, 28);
    g_themeColors.toolbarBtnHoverBg = RGB(232, 232, 232);
    g_themeColors.toolbarBtnPressedBg = RGB(218, 218, 218);
    g_themeColors.toolbarSeparator = RGB(218, 218, 218);
    
    /* Status bar colors - subtle distinction */
    g_themeColors.statusbarBg = RGB(243, 243, 243);
    g_themeColors.statusbarFg = RGB(64, 64, 64);
    g_themeColors.statusbarBorder = RGB(229, 229, 229);
    
    /* Window colors */
    g_themeColors.windowBg = RGB(249, 249, 249);
    g_themeColors.windowBorder = RGB(218, 218, 218);
    
    /* Scrollbar colors */
    g_themeColors.scrollbarBg = RGB(243, 243, 243);
    g_themeColors.scrollbarThumb = RGB(192, 192, 192);
}

static void LoadDarkThemeColors(void)
{
    /* Modern Windows 11-style dark theme palette */
    
    /* Editor colors - VS Code-inspired dark */
    g_themeColors.editorBg = RGB(30, 30, 30);
    g_themeColors.editorFg = RGB(212, 212, 212);
    g_themeColors.editorSelBg = RGB(38, 79, 120);        /* Softer blue selection */
    g_themeColors.editorSelFg = RGB(255, 255, 255);
    g_themeColors.editorLineNumBg = RGB(30, 30, 30);
    g_themeColors.editorLineNumFg = RGB(110, 110, 110);
    g_themeColors.editorCaretLineBg = RGB(42, 42, 42);
    
    /* Tab colors - subtle elevation */
    g_themeColors.tabNormalBg = RGB(37, 37, 38);
    g_themeColors.tabNormalFg = RGB(150, 150, 150);
    g_themeColors.tabNormalBorder = RGB(60, 60, 60);
    g_themeColors.tabHoverBg = RGB(50, 50, 50);
    g_themeColors.tabHoverFg = RGB(212, 212, 212);
    g_themeColors.tabSelectedBg = RGB(30, 30, 30);
    g_themeColors.tabSelectedFg = RGB(255, 255, 255);
    g_themeColors.tabSelectedBorder = RGB(60, 60, 60);
    
    /* Toolbar colors - consistent with tabs */
    g_themeColors.toolbarBg = RGB(37, 37, 38);
    g_themeColors.toolbarBtnBg = RGB(37, 37, 38);
    g_themeColors.toolbarBtnFg = RGB(200, 200, 200);
    g_themeColors.toolbarBtnHoverBg = RGB(62, 62, 64);
    g_themeColors.toolbarBtnPressedBg = RGB(78, 78, 80);
    g_themeColors.toolbarSeparator = RGB(60, 60, 60);
    
    /* Status bar colors - Windows 11 dark style */
    g_themeColors.statusbarBg = RGB(37, 37, 38);
    g_themeColors.statusbarFg = RGB(170, 170, 170);
    g_themeColors.statusbarBorder = RGB(60, 60, 60);
    
    /* Window colors */
    g_themeColors.windowBg = RGB(37, 37, 38);
    g_themeColors.windowBorder = RGB(60, 60, 60);
    
    /* Scrollbar colors */
    g_themeColors.scrollbarBg = RGB(37, 37, 38);
    g_themeColors.scrollbarThumb = RGB(79, 79, 79);
}

BOOL InitializeTheme(void)
{
    /* Load theme from registry if available */
    extern BOOL RegReadDWORD(HKEY rootKey, const char* path, const char* name, DWORD* value);
    DWORD savedTheme = 0;
    if (RegReadDWORD(HKEY_CURRENT_USER, "Software\\Notepad+\\Theme", "CurrentTheme", &savedTheme)) {
        g_theme = (Theme)savedTheme;
        if (g_theme == THEME_DARK) {
            LoadDarkThemeColors();
        } else {
            g_theme = THEME_LIGHT;
            LoadLightThemeColors();
        }
    } else {
        /* Default to light theme if no saved preference */
        g_theme = THEME_LIGHT;
        LoadLightThemeColors();
    }
    return TRUE;
}

void CleanupTheme(void)
{
}

Theme GetCurrentTheme(void)
{
    return g_theme;
}

BOOL SetTheme(Theme theme)
{
    if (theme < 0 || theme >= THEME_COUNT) {
        return FALSE;
    }
    
    g_theme = theme;
    
    if (theme == THEME_DARK) {
        LoadDarkThemeColors();
    } else {
        LoadLightThemeColors();
    }
    
    HWND hwnd = GetMainWindow();
    if (hwnd) {
        /* Apply dark mode to window using Windows 10+ DWM API */
        typedef HRESULT (WINAPI *DwmSetWindowAttributeProc)(HWND, DWORD, LPCVOID, DWORD);
        HMODULE hDwmapi = LoadLibrary("dwmapi.dll");
        if (hDwmapi) {
            FARPROC procAddr = GetProcAddress(hDwmapi, "DwmSetWindowAttribute");
            if (procAddr) {
                DwmSetWindowAttributeProc setAttr = (DwmSetWindowAttributeProc)(void*)procAddr;
                BOOL value = (theme == THEME_DARK) ? TRUE : FALSE;
                /* DWMWA_USE_IMMERSIVE_DARK_MODE = 20 (Windows 11) or 19 (Windows 10) */
                /* Try Windows 11 attribute first */
                HRESULT hr = setAttr(hwnd, 20, &value, sizeof(value));
                if (FAILED(hr)) {
                    /* Fall back to Windows 10 attribute if Windows 11 fails */
                    setAttr(hwnd, 19, &value, sizeof(value));
                }
            }
            FreeLibrary(hDwmapi);
        }
        
        /* Force a non-client area redraw to update title bar immediately */
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                    SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
        RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_FRAME);
    }
    
    /* Save theme to registry for persistence */
    extern BOOL RegWriteDWORD(HKEY rootKey, const char* path, const char* name, DWORD value);
    RegWriteDWORD(HKEY_CURRENT_USER, "Software\\Notepad+\\Theme", "CurrentTheme", (DWORD)theme);
    
    return TRUE;
}

ThemeColors* GetThemeColors(void)
{
    return &g_themeColors;
}

void ToggleTheme(void)
{
    if (g_theme == THEME_LIGHT) {
        SetTheme(THEME_DARK);
    } else {
        SetTheme(THEME_LIGHT);
    }
}

void ApplyCurrentThemeToWindow(void)
{
    HWND hwnd = GetMainWindow();
    if (!hwnd) return;
    
    /* Apply dark mode to window using Windows 10+ DWM API */
    typedef HRESULT (WINAPI *DwmSetWindowAttributeProc)(HWND, DWORD, LPCVOID, DWORD);
    HMODULE hDwmapi = LoadLibrary("dwmapi.dll");
    if (hDwmapi) {
        FARPROC procAddr = GetProcAddress(hDwmapi, "DwmSetWindowAttribute");
        if (procAddr) {
            DwmSetWindowAttributeProc setAttr = (DwmSetWindowAttributeProc)(void*)procAddr;
            BOOL value = (g_theme == THEME_DARK) ? TRUE : FALSE;
            /* DWMWA_USE_IMMERSIVE_DARK_MODE = 20 (Windows 11) or 19 (Windows 10) */
            /* Try Windows 11 attribute first */
            HRESULT hr = setAttr(hwnd, 20, &value, sizeof(value));
            if (FAILED(hr)) {
                /* Fall back to Windows 10 attribute if Windows 11 fails */
                setAttr(hwnd, 19, &value, sizeof(value));
            }
        }
        FreeLibrary(hDwmapi);
    }
}

void ApplyThemeToEditor(HWND editor)
{
    if (!editor) return;
    
    /* Set default style colors */
    SendMessage(editor, SCI_STYLESETFORE, STYLE_DEFAULT, g_themeColors.editorFg);
    SendMessage(editor, SCI_STYLESETBACK, STYLE_DEFAULT, g_themeColors.editorBg);
    
    /* Propagate default style to all styles */
    SendMessage(editor, SCI_STYLECLEARALL, 0, 0);
    
    /* Set caret colors */
    SendMessage(editor, SCI_SETCARETFORE, g_themeColors.editorFg, 0);
    SendMessage(editor, SCI_SETCARETLINEVISIBLE, 1, 0);
    SendMessage(editor, SCI_SETCARETLINEBACK, g_themeColors.editorCaretLineBg, 0);
    
    /* Set selection colors */
    SendMessage(editor, SCI_SETSELFORE, 1, g_themeColors.editorSelFg);
    SendMessage(editor, SCI_SETSELBACK, 1, g_themeColors.editorSelBg);
    
    /* Set line number margin (margin 0) colors */
    SendMessage(editor, SCI_SETMARGINBACKN, 0, g_themeColors.editorLineNumBg);
    SendMessage(editor, SCI_STYLESETFORE, STYLE_LINENUMBER, g_themeColors.editorLineNumFg);
    SendMessage(editor, SCI_STYLESETBACK, STYLE_LINENUMBER, g_themeColors.editorLineNumBg);
    
    /* Set code folding margin (margin 2) background - required but markers will cover it */
    SendMessage(editor, SCI_SETMARGINBACKN, 2, g_themeColors.editorBg);
    
    /* Set folding marker colors based on theme
     * CRITICAL: On Windows, markers paint over the margin background,
     * so we must set ALL marker backgrounds to match the editor background
     * to make the folding column appear dark. This is the Notepad++ method. */
    COLORREF markerFg, markerBg;
    if (g_theme == THEME_DARK) {
        markerFg = RGB(200, 200, 200);    /* Light gray for fold icons */
        markerBg = g_themeColors.editorBg;  /* MUST match editor background */
    } else {
        markerFg = RGB(80, 80, 80);       /* Dark gray for fold icons */
        markerBg = g_themeColors.editorBg;  /* MUST match editor background */
    }
    
    /* Set background for ALL fold markers - this is what makes the column dark/light
     * Each marker type must have its background set to cover the margin properly */
    int foldMarkers[] = {
        SC_MARKNUM_FOLDER,
        SC_MARKNUM_FOLDEROPEN,
        SC_MARKNUM_FOLDERSUB,
        SC_MARKNUM_FOLDEREND,
        SC_MARKNUM_FOLDEROPENMID,
        SC_MARKNUM_FOLDERMIDTAIL,
        SC_MARKNUM_FOLDERTAIL
    };
    
    for (int i = 0; i < 7; i++) {
        SendMessage(editor, SCI_MARKERSETBACK, foldMarkers[i], markerBg);
        SendMessage(editor, SCI_MARKERSETFORE, foldMarkers[i], markerFg);
    }
    
    /* CRITICAL FIX: Add a background marker to ensure complete coverage of folding margin
     * This addresses the issue where box-style markers don't cover the entire margin area
     * in dark mode, leaving white/light patches. The background marker fills the entire margin. */
    SendMessage(editor, SCI_MARKERDEFINE, SC_MARKNUM_FOLDER, SC_MARK_BOXPLUS);
    SendMessage(editor, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPEN, SC_MARK_BOXMINUS);
    SendMessage(editor, SCI_MARKERDEFINE, SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE);
    SendMessage(editor, SCI_MARKERDEFINE, SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNER);
    SendMessage(editor, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEREND, SC_MARK_BOXPLUSCONNECTED);
    SendMessage(editor, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPENMID, SC_MARK_BOXMINUSCONNECTED);
    SendMessage(editor, SCI_MARKERDEFINE, SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNER);
    
    /* Ensure the folding margin is completely covered by setting a full-width background */
    /* This is the key fix - we use a margin-wide background marker */
    SendMessage(editor, SCI_SETFOLDMARGINCOLOUR, TRUE, markerBg);
    SendMessage(editor, SCI_SETFOLDMARGINHICOLOUR, TRUE, markerBg);
    
    /* Apply dark mode to scrollbars using SetWindowTheme API */
    typedef HRESULT (WINAPI *SetWindowThemeProc)(HWND, LPCWSTR, LPCWSTR);
    HMODULE hUxtheme = LoadLibrary("uxtheme.dll");
    if (hUxtheme) {
        SetWindowThemeProc setWindowTheme = (SetWindowThemeProc)GetProcAddress(hUxtheme, "SetWindowTheme");
        if (setWindowTheme) {
            if (g_theme == THEME_DARK) {
                /* Apply dark mode scrollbar theme */
                setWindowTheme(editor, L"DarkMode_Explorer", NULL);
            } else {
                /* Reset to default light theme */
                setWindowTheme(editor, L"Explorer", NULL);
            }
        }
        FreeLibrary(hUxtheme);
    }
    
    SendMessage(editor, SCI_COLOURISE, 0, -1);
}

void ApplyThemeToAllEditors(void)
{
    int tabCount = GetTabCount();
    for (int i = 0; i < tabCount; i++) {
        TabInfo* tab = GetTab(i);
        if (tab && tab->editorHandle) {
            /* Apply theme first (this clears all styles) */
            ApplyThemeToEditor(tab->editorHandle);
            
            /* Re-apply syntax highlighting AFTER theme (so colors are preserved) */
            if (strncmp(tab->filePath, "New ", 4) != 0) {
                ApplySyntaxHighlightingForFile(tab->editorHandle, tab->filePath);
            }
        }
    }
}

const char* GetThemeName(Theme theme)
{
    switch (theme) {
        case THEME_LIGHT: return "Light";
        case THEME_DARK: return "Dark";
        default: return "Unknown";
    }
}

BOOL SaveThemeToConfig(const char* configPath)
{
    FILE* file = fopen(configPath, "w");
    if (!file) return FALSE;
    fprintf(file, "theme=%s\n", GetThemeName(g_theme));
    fclose(file);
    return TRUE;
}

BOOL LoadThemeFromConfig(const char* configPath)
{
    FILE* file = fopen(configPath, "r");
    if (!file) return FALSE;
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "theme=", 6) == 0) {
            const char* themeName = line + 6;
            if (strcmp(themeName, "Dark") == 0) {
                SetTheme(THEME_DARK);
            } else {
                SetTheme(THEME_LIGHT);
            }
            break;
        }
    }
    fclose(file);
    return TRUE;
}
