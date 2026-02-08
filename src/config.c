/*
 * Configuration system implementation for Notepad+
 * Handles persistent application settings using Windows Registry
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "registry_config.h"
#include "themes.h"
#include "tabs.h"
#include "toolbar.h"
#include "statusbar.h"
#include "window.h"
#include "Scintilla.h"

/* Global configuration instance */
static AppConfig g_config = {0};
static BOOL g_initialized = FALSE;

/* Set default configuration values */
static void SetDefaultConfig(void)
{
    /* Window settings */
    g_config.windowX = CW_USEDEFAULT;
    g_config.windowY = CW_USEDEFAULT;
    g_config.windowWidth = 800;
    g_config.windowHeight = 600;
    g_config.windowMaximized = FALSE;
    
    /* View settings */
    g_config.showToolbar = TRUE;
    g_config.showStatusBar = TRUE;
    g_config.showLineNumbers = TRUE;
    g_config.wordWrap = FALSE;
    
    /* Editor settings */
    strcpy(g_config.fontName, "Consolas");
    g_config.fontSize = 10;
    g_config.tabWidth = 4;
    g_config.useSpaces = FALSE;
    g_config.zoomLevel = 0;
    
    /* Theme settings */
    g_config.theme = THEME_LIGHT;
    g_config.darkModeEditorOnly = TRUE;
    
    /* Find/Replace settings */
    g_config.matchCase = FALSE;
    g_config.wholeWord = FALSE;
    g_config.useRegex = FALSE;
    g_config.searchDown = TRUE;
    
    /* Recent files */
    g_config.recentFileCount = 0;
    for (int i = 0; i < 10; i++) {
        g_config.recentFiles[i][0] = '\0';
    }
    
    /* Session settings */
    g_config.restoreSession = TRUE;
    g_config.saveOnExit = FALSE;
    g_config.autoSave = FALSE;
    g_config.autoSaveInterval = 0;  /* 0 = disabled */
    
    /* General behavior */
    g_config.singleInstance = FALSE;
    g_config.confirmExit = FALSE;
    g_config.backupOnSave = FALSE;
    
    /* Behavior */
    g_config.highlightMatchingWords = TRUE;
    g_config.highlightCurrentLine = TRUE;
    g_config.showWhitespace = FALSE;
    g_config.autoIndent = FALSE;
    g_config.bracketMatching = FALSE;
    g_config.codeFoldingEnabled = TRUE;
    g_config.changeHistoryEnabled = TRUE;
    g_config.caretWidth = 1;
}

/* Initialize configuration system */
BOOL InitializeConfig(void)
{
    if (g_initialized) {
        return TRUE;
    }
    
    /* Set default values first */
    SetDefaultConfig();
    
    /* Try to load existing configuration from registry */
    LoadConfig();
    
    g_initialized = TRUE;
    return TRUE;
}

/* Cleanup configuration system */
void CleanupConfig(void)
{
    if (g_initialized) {
        SaveConfig();
        g_initialized = FALSE;
    }
}

/* Get configuration */
AppConfig* GetConfig(void)
{
    return &g_config;
}

/* Save configuration to registry */
BOOL SaveConfig(void)
{
    /* Update window position from actual window state before saving */
    HWND mainWindow = GetMainWindow();
    if (mainWindow) {
        WINDOWPLACEMENT wp;
        wp.length = sizeof(WINDOWPLACEMENT);
        if (GetWindowPlacement(mainWindow, &wp)) {
            g_config.windowMaximized = (wp.showCmd == SW_SHOWMAXIMIZED);
            
            if (!g_config.windowMaximized) {
                RECT rect;
                GetWindowRect(mainWindow, &rect);
                g_config.windowX = rect.left;
                g_config.windowY = rect.top;
                g_config.windowWidth = rect.right - rect.left;
                g_config.windowHeight = rect.bottom - rect.top;
            } else {
                /* Use normal position when maximized */
                g_config.windowX = wp.rcNormalPosition.left;
                g_config.windowY = wp.rcNormalPosition.top;
                g_config.windowWidth = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
                g_config.windowHeight = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
            }
        }
    }
    
    /* Save user settings to registry */
    SaveToRegistry(&g_config);
    
    /* Save window state to registry */
    SaveWindowStateToRegistry(g_config.windowX, g_config.windowY,
                             g_config.windowWidth, g_config.windowHeight,
                             g_config.windowMaximized);
    
    /* Save recent files to registry */
    SaveRecentFilesToRegistry(g_config.recentFiles, g_config.recentFileCount);
    
    return TRUE;
}

/* Load configuration from registry */
BOOL LoadConfig(void)
{
    /* Check if registry configuration exists */
    if (IsRegistryConfigured()) {
        /* Load user settings from registry */
        LoadFromRegistry(&g_config);
        
        /* Load window state from registry */
        LoadWindowStateFromRegistry(&g_config.windowX, &g_config.windowY,
                                   &g_config.windowWidth, &g_config.windowHeight,
                                   &g_config.windowMaximized);
        
        /* Load recent files from registry */
        LoadRecentFilesFromRegistry(g_config.recentFiles, &g_config.recentFileCount);
    } else {
        /* No registry config exists, use defaults and initialize registry */
        InitializeRegistry();
        SaveConfig();
    }
    
    return TRUE;
}

/* Add file to recent files list */
void AddRecentFile(const char* filePath)
{
    if (!filePath || filePath[0] == '\0') {
        return;
    }
    
    /* Check if file is already in the list */
    for (int i = 0; i < g_config.recentFileCount; i++) {
        if (_stricmp(g_config.recentFiles[i], filePath) == 0) {
            /* Move this file to the top */
            if (i > 0) {
                char temp[MAX_PATH];
                strcpy(temp, g_config.recentFiles[i]);
                for (int j = i; j > 0; j--) {
                    strcpy(g_config.recentFiles[j], g_config.recentFiles[j - 1]);
                }
                strcpy(g_config.recentFiles[0], temp);
            }
            return;
        }
    }
    
    /* Shift existing files down */
    for (int i = 9; i > 0; i--) {
        strcpy(g_config.recentFiles[i], g_config.recentFiles[i - 1]);
    }
    
    /* Add new file at the top */
    strncpy(g_config.recentFiles[0], filePath, MAX_PATH - 1);
    g_config.recentFiles[0][MAX_PATH - 1] = '\0';
    
    /* Increment count if not at max */
    if (g_config.recentFileCount < 10) {
        g_config.recentFileCount++;
    }
}

/* Clear recent files list */
void ClearRecentFiles(void)
{
    g_config.recentFileCount = 0;
    for (int i = 0; i < 10; i++) {
        g_config.recentFiles[i][0] = '\0';
    }
}

/* Get recent file by index */
const char* GetRecentFile(int index)
{
    if (index < 0 || index >= g_config.recentFileCount) {
        return NULL;
    }
    return g_config.recentFiles[index];
}

/* Get recent file count */
int GetRecentFileCount(void)
{
    return g_config.recentFileCount;
}

/* Apply configuration to UI */
void ApplyConfig(void)
{
    /* Apply view settings */
    ShowToolbar(g_config.showToolbar);
    ShowStatusBar(g_config.showStatusBar);
    
    /* Apply theme */
    SetTheme((Theme)g_config.theme);
    ApplyThemeToAllEditors();
    
    /* Apply editor settings to all tabs */
    int tabCount = GetTabCount();
    for (int i = 0; i < tabCount; i++) {
        TabInfo* tab = GetTab(i);
        if (tab && tab->editorHandle) {
            /* Apply font */
            SendMessage(tab->editorHandle, SCI_STYLESETFONT, STYLE_DEFAULT, (LPARAM)g_config.fontName);
            SendMessage(tab->editorHandle, SCI_STYLESETSIZE, STYLE_DEFAULT, g_config.fontSize);
            SendMessage(tab->editorHandle, SCI_STYLECLEARALL, 0, 0);
            
            /* Apply tab width */
            SendMessage(tab->editorHandle, SCI_SETTABWIDTH, g_config.tabWidth, 0);
            
            /* Apply line numbers - always show them */
            SendMessage(tab->editorHandle, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
            SendMessage(tab->editorHandle, SCI_SETMARGINWIDTHN, 0, 40);
            
            /* Apply word wrap */
            SendMessage(tab->editorHandle, SCI_SETWRAPMODE, g_config.wordWrap ? SC_WRAP_WORD : SC_WRAP_NONE, 0);
            
            /* Apply whitespace visibility */
            SendMessage(tab->editorHandle, SCI_SETVIEWWS, g_config.showWhitespace ? SCWS_VISIBLEALWAYS : SCWS_INVISIBLE, 0);
            
            /* Apply zoom level */
            SendMessage(tab->editorHandle, SCI_SETZOOM, g_config.zoomLevel, 0);
        }
    }
}

/* Update current window state in config (call on move/resize) */
void UpdateCurrentWindowState(void)
{
    HWND mainWindow = GetMainWindow();
    if (mainWindow) {
        WINDOWPLACEMENT wp;
        wp.length = sizeof(WINDOWPLACEMENT);
        if (GetWindowPlacement(mainWindow, &wp)) {
            g_config.windowMaximized = (wp.showCmd == SW_SHOWMAXIMIZED);
            
            if (!g_config.windowMaximized) {
                RECT rect;
                GetWindowRect(mainWindow, &rect);
                g_config.windowX = rect.left;
                g_config.windowY = rect.top;
                g_config.windowWidth = rect.right - rect.left;
                g_config.windowHeight = rect.bottom - rect.top;
            } else {
                /* Use normal position when maximized */
                g_config.windowX = wp.rcNormalPosition.left;
                g_config.windowY = wp.rcNormalPosition.top;
                g_config.windowWidth = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
                g_config.windowHeight = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
            }
        }
    }
}
