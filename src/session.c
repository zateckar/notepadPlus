/*
 * Session management implementation for Notepad+
 * Saves and restores application session state
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "session.h"
#include "config.h"
#include "registry_config.h"
#include "tabs.h"
#include "window.h"
#include "Scintilla.h"

/* Import profiling macros from main.c when enabled */
#ifdef PROFILE_SESSION
extern LARGE_INTEGER g_startupFreq;
extern LARGE_INTEGER g_startupStart;
extern FILE* g_profileFile;

#define SESSION_PROFILE_MARK(label) do { \
    LARGE_INTEGER now; \
    QueryPerformanceCounter(&now); \
    double ms = (double)(now.QuadPart - g_startupStart.QuadPart) * 1000.0 / g_startupFreq.QuadPart; \
    char buf[256]; \
    sprintf(buf, "[SESSION] %s: %.2f ms\n", label, ms); \
    OutputDebugString(buf); \
    if (g_profileFile) { fprintf(g_profileFile, "%s", buf); fflush(g_profileFile); } \
} while(0)
#else
#define SESSION_PROFILE_MARK(label)
#endif

/* Global session data */
static SessionData g_session = {0};
static BOOL g_initialized = FALSE;
static BOOL g_sessionSaved = FALSE;  /* Track if session was already saved during shutdown */

/* Initialize session system */
BOOL InitializeSession(void)
{
    if (g_initialized) {
        return TRUE;
    }
    
    memset(&g_session, 0, sizeof(SessionData));
    
    /* Set default values */
    g_session.windowX = CW_USEDEFAULT;
    g_session.windowY = CW_USEDEFAULT;
    g_session.windowWidth = 800;
    g_session.windowHeight = 600;
    g_session.windowMaximized = FALSE;
    g_session.tabCount = 0;
    g_session.activeTabIndex = 0;
    
    g_initialized = TRUE;
    return TRUE;
}

/* Cleanup session system */
void CleanupSession(void)
{
    if (g_initialized) {
        /* Note: Session is saved in WM_CLOSE handler BEFORE window destruction.
         * Do NOT call SaveSession here - by this point, the editor windows are
         * already destroyed (since they are WS_CHILD of the main window), so
         * SendMessage to get content/zoom would fail and overwrite good data
         * with empty data. */
        g_initialized = FALSE;
    }
}

/* Save current session to registry */
BOOL SaveSession(void)
{
    /* Allow multiple saves - the flag is only used to prevent infinite recursion
     * We reset it at the start so we can save again if called multiple times */
    if (g_sessionSaved) {
        /* Already in the process of saving - prevent recursion */
        return TRUE;
    }
    
    /* Set flag to prevent recursion during this save operation */
    g_sessionSaved = TRUE;
    
    /* Load all placeholder tabs before saving to ensure their content is preserved */
    int tabCount = GetTabCount();
    for (int i = 0; i < tabCount; i++) {
        if (!IsTabLoaded(i)) {
            LoadTabContent(i);
        }
    }
    
    HWND mainWindow = GetMainWindow();
    
    /* Get window state */
    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(mainWindow, &wp)) {
        g_session.windowMaximized = (wp.showCmd == SW_SHOWMAXIMIZED);
        
        if (!g_session.windowMaximized) {
            RECT rect;
            GetWindowRect(mainWindow, &rect);
            g_session.windowX = rect.left;
            g_session.windowY = rect.top;
            g_session.windowWidth = rect.right - rect.left;
            g_session.windowHeight = rect.bottom - rect.top;
        } else {
            /* Use normal position when maximized */
            g_session.windowX = wp.rcNormalPosition.left;
            g_session.windowY = wp.rcNormalPosition.top;
            g_session.windowWidth = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
            g_session.windowHeight = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
        }
    }
    
    /* Get tab information (reuse tabCount from earlier) */
    int savedTabs = 0;
    
    /* Get temp directory for unsaved files */
    char tempDir[MAX_PATH];
    GetTempPath(MAX_PATH, tempDir);
    strcat(tempDir, "NotepadPlus\\");
    CreateDirectory(tempDir, NULL);
    
    for (int i = 0; i < tabCount && savedTabs < MAX_SESSION_TABS; i++) {
        TabInfo* tab = GetTab(i);
        if (tab) {
            /* Check if this is a new unsaved file */
            if (strncmp(tab->filePath, "New ", 4) == 0) {
                /* Save content to temporary file */
                if (tab->editorHandle) {
                    char tempFile[MAX_PATH];
                    sprintf(tempFile, "%s%s.txt", tempDir, tab->filePath);
                    
                    /* Get text from editor */
                    long textLength = (long)SendMessage(tab->editorHandle, SCI_GETLENGTH, 0, 0);
                    if (textLength > 0) {
                        char* buffer = (char*)malloc(textLength + 1);
                        if (buffer) {
                            SendMessage(tab->editorHandle, SCI_GETTEXT, textLength + 1, (LPARAM)buffer);
                            
                            /* Write to temp file */
                            FILE* file = fopen(tempFile, "wb");
                            if (file) {
                                fwrite(buffer, 1, textLength, file);
                                fclose(file);
                                
                                /* Store in session data */
                                strncpy(g_session.tabs[savedTabs].tempFilePath, tempFile, MAX_PATH - 1);
                                g_session.tabs[savedTabs].tempFilePath[MAX_PATH - 1] = '\0';
                                strncpy(g_session.tabs[savedTabs].filePath, tab->filePath, MAX_PATH - 1);
                                g_session.tabs[savedTabs].filePath[MAX_PATH - 1] = '\0';
                                g_session.tabs[savedTabs].isUnsaved = TRUE;
                                
                                /* Get cursor position and zoom level */
                                int cursorPos = (int)SendMessage(tab->editorHandle, SCI_GETCURRENTPOS, 0, 0);
                                int firstLine = (int)SendMessage(tab->editorHandle, SCI_GETFIRSTVISIBLELINE, 0, 0);
                                int zoomLevel = (int)SendMessage(tab->editorHandle, SCI_GETZOOM, 0, 0);
                                
                                g_session.tabs[savedTabs].cursorPosition = cursorPos;
                                g_session.tabs[savedTabs].firstVisibleLine = firstLine;
                                g_session.tabs[savedTabs].zoomLevel = zoomLevel;
                                g_session.tabs[savedTabs].isModified = tab->isModified;
                                g_session.tabs[savedTabs].isPinned = tab->isPinned;
                                
                                /* Capture per-tab view settings */
                                g_session.tabs[savedTabs].wordWrap = (SendMessage(tab->editorHandle, SCI_GETWRAPMODE, 0, 0) != SC_WRAP_NONE);
                                g_session.tabs[savedTabs].showLineNumbers = SendMessage(tab->editorHandle, SCI_GETMARGINWIDTHN, 0, 0) > 0;
                                g_session.tabs[savedTabs].showWhitespace = (SendMessage(tab->editorHandle, SCI_GETVIEWWS, 0, 0) != SCWS_INVISIBLE);
                                g_session.tabs[savedTabs].autoIndent = 0; /* SCI_GETAUTOMATICINDENT not available */
                                g_session.tabs[savedTabs].codeFoldingEnabled = tab->codeFoldingEnabled;
                                g_session.tabs[savedTabs].changeHistoryEnabled = tab->changeHistoryEnabled;
                                g_session.tabs[savedTabs].isSplitView = tab->isSplitView;
                                
                                /* Update global config zoom level */
                                AppConfig* cfg = GetConfig();
                                if (cfg) {
                                    cfg->zoomLevel = zoomLevel;
                                }
                                
                                savedTabs++;
                            }
                            free(buffer);
                        }
                    } else {
                        /* Empty unsaved file - just save metadata */
                        sprintf(tempFile, "%s%s.txt", tempDir, tab->filePath);
                        FILE* file = fopen(tempFile, "wb");
                        if (file) {
                            fclose(file);
                            strncpy(g_session.tabs[savedTabs].tempFilePath, tempFile, MAX_PATH - 1);
                            g_session.tabs[savedTabs].tempFilePath[MAX_PATH - 1] = '\0';
                            strncpy(g_session.tabs[savedTabs].filePath, tab->filePath, MAX_PATH - 1);
                            g_session.tabs[savedTabs].filePath[MAX_PATH - 1] = '\0';
                            g_session.tabs[savedTabs].isUnsaved = TRUE;
                            
                            /* Get cursor position and zoom level even for empty files */
                            int cursorPos = (int)SendMessage(tab->editorHandle, SCI_GETCURRENTPOS, 0, 0);
                            int firstLine = (int)SendMessage(tab->editorHandle, SCI_GETFIRSTVISIBLELINE, 0, 0);
                            int zoomLevel = (int)SendMessage(tab->editorHandle, SCI_GETZOOM, 0, 0);
                            
                            g_session.tabs[savedTabs].cursorPosition = cursorPos;
                            g_session.tabs[savedTabs].firstVisibleLine = firstLine;
                            g_session.tabs[savedTabs].zoomLevel = zoomLevel;
                            g_session.tabs[savedTabs].isModified = FALSE;
                            g_session.tabs[savedTabs].isPinned = tab->isPinned;
                            savedTabs++;
                        }
                    }
                }
            } else {
                /* Regular saved file */
                strncpy(g_session.tabs[savedTabs].filePath, tab->filePath, MAX_PATH - 1);
                g_session.tabs[savedTabs].filePath[MAX_PATH - 1] = '\0';
                
                /* If file has unsaved modifications, save content to temp file */
                if (tab->isModified && tab->editorHandle) {
                    char tempFile[MAX_PATH];
                    
                    /* Create unique temp filename using hash of file path */
                    unsigned int hash = 0;
                    const char* p = tab->filePath;
                    while (*p) {
                        hash = hash * 31 + (unsigned char)*p++;
                    }
                    sprintf(tempFile, "%sunsaved_%u.tmp", tempDir, hash);
                    
                    /* Get text from editor */
                    long textLength = (long)SendMessage(tab->editorHandle, SCI_GETLENGTH, 0, 0);
                    char* buffer = (char*)malloc(textLength + 1);
                    if (buffer) {
                        SendMessage(tab->editorHandle, SCI_GETTEXT, textLength + 1, (LPARAM)buffer);
                        
                        /* Write to temp file */
                        FILE* file = fopen(tempFile, "wb");
                        if (file) {
                            fwrite(buffer, 1, textLength, file);
                            fclose(file);
                            
                            /* Store temp file path for later restoration */
                            strncpy(g_session.tabs[savedTabs].tempFilePath, tempFile, MAX_PATH - 1);
                            g_session.tabs[savedTabs].tempFilePath[MAX_PATH - 1] = '\0';
                        } else {
                            g_session.tabs[savedTabs].tempFilePath[0] = '\0';
                        }
                        free(buffer);
                    } else {
                        g_session.tabs[savedTabs].tempFilePath[0] = '\0';
                    }
                } else {
                    g_session.tabs[savedTabs].tempFilePath[0] = '\0';
                }
                
                g_session.tabs[savedTabs].isUnsaved = FALSE;
                
                /* Get cursor position from editor */
                if (tab->editorHandle) {
                    int cursorPos = (int)SendMessage(tab->editorHandle, SCI_GETCURRENTPOS, 0, 0);
                    int firstLine = (int)SendMessage(tab->editorHandle, SCI_GETFIRSTVISIBLELINE, 0, 0);
                    int zoomLevel = (int)SendMessage(tab->editorHandle, SCI_GETZOOM, 0, 0);
                    
                    g_session.tabs[savedTabs].cursorPosition = cursorPos;
                    g_session.tabs[savedTabs].firstVisibleLine = firstLine;
                    g_session.tabs[savedTabs].zoomLevel = zoomLevel;
                    
                    /* Save zoom level to config too */
                    AppConfig* cfg = GetConfig();
                    if (cfg) {
                        cfg->zoomLevel = zoomLevel;
                    }
                }
                
                g_session.tabs[savedTabs].isModified = tab->isModified;
                g_session.tabs[savedTabs].isPinned = tab->isPinned;
                
                /* Capture per-tab view settings */
                g_session.tabs[savedTabs].wordWrap = (SendMessage(tab->editorHandle, SCI_GETWRAPMODE, 0, 0) != SC_WRAP_NONE);
                g_session.tabs[savedTabs].showLineNumbers = SendMessage(tab->editorHandle, SCI_GETMARGINWIDTHN, 0, 0) > 0;
                g_session.tabs[savedTabs].showWhitespace = (SendMessage(tab->editorHandle, SCI_GETVIEWWS, 0, 0) != SCWS_INVISIBLE);
                g_session.tabs[savedTabs].autoIndent = 0; /* SCI_GETAUTOMATICINDENT not available */
                g_session.tabs[savedTabs].codeFoldingEnabled = tab->codeFoldingEnabled;
                g_session.tabs[savedTabs].changeHistoryEnabled = tab->changeHistoryEnabled;
                g_session.tabs[savedTabs].isSplitView = tab->isSplitView;
                
                savedTabs++;
            }
        }
    }
    
    /* Update session counts */
    g_session.tabCount = savedTabs;
    g_session.activeTabIndex = GetSelectedTab();
    
    /* Save session to registry */
    BOOL result = SaveSessionToRegistry(&g_session);
    
    /* Clear the flag after successful save - allows future saves */
    g_sessionSaved = FALSE;
    
    return result;
}

/* Load session from registry */
BOOL LoadSession(void)
{
    /* Load from registry */
    return LoadSessionFromRegistry(&g_session);
}

/* Restore session - actually open the tabs */
BOOL RestoreSession(void)
{
    SESSION_PROFILE_MARK("RestoreSession start");
    
    AppConfig* config = GetConfig();
    if (!config || !config->restoreSession) {
        return FALSE;
    }
    
    /* Load session data first */
    if (!LoadSession()) {
        return FALSE;
    }
    
    SESSION_PROFILE_MARK("LoadSession done");
    
    /* Note: Window position/size is restored in InitializeWindow()
     * before the window is shown, so we don't need to set it here */
    
    /* Restore tabs in their original order - use lazy loading for fast startup:
     * Only load the active tab immediately, other tabs are loaded on demand */
    BOOL openedAnyTab = FALSE;
    int activeTabIndex = g_session.activeTabIndex;
    
    /* Create ALL tabs in their original order, but only load content for the active tab */
    for (int i = 0; i < g_session.tabCount; i++) {
        int tabIndex = -1;
        BOOL isActiveTab = (i == activeTabIndex);
        
        /* Check if this was an unsaved file */
        if (g_session.tabs[i].isUnsaved && g_session.tabs[i].tempFilePath[0] != '\0') {
            /* Unsaved file from temp file */
            if (GetFileAttributes(g_session.tabs[i].tempFilePath) != INVALID_FILE_ATTRIBUTES) {
                if (isActiveTab) {
                    /* Load active tab fully using FAST loading */
                    tabIndex = AddTabFast(g_session.tabs[i].tempFilePath, FALSE);
                } else {
                    /* Create placeholder for non-active tabs */
                    tabIndex = AddPlaceholderTab(g_session.tabs[i].tempFilePath, FALSE, g_session.tabs[i].isPinned);
                }
                
                if (tabIndex >= 0) {
                    TabInfo* tab = GetTab(tabIndex);
                    if (tab) {
                        /* Store temp file path before changing filePath */
                        strncpy(tab->tempFilePath, g_session.tabs[i].tempFilePath, MAX_PATH - 1);
                        tab->tempFilePath[MAX_PATH - 1] = '\0';
                        
                        /* Restore original display name (New X) */
                        strncpy(tab->filePath, g_session.tabs[i].filePath, MAX_PATH - 1);
                        tab->filePath[MAX_PATH - 1] = '\0';
                        tab->isModified = TRUE;
                        tab->isPinned = g_session.tabs[i].isPinned;
                        
                        /* Store session data for lazy loading */
                        tab->sessionCursorPos = g_session.tabs[i].cursorPosition;
                        tab->sessionFirstLine = g_session.tabs[i].firstVisibleLine;
                        tab->sessionZoomLevel = g_session.tabs[i].zoomLevel;
                        
                        /* Apply per-tab view settings safely after tab initialization */
                        ApplySessionViewSettings(tabIndex, &g_session.tabs[i]);
                        
                        UpdateTabDisplayName(tabIndex);
                    }
                }
            }
        } else if (g_session.tabs[i].filePath[0] != '\0') {
            /* Regular saved file */
            if (GetFileAttributes(g_session.tabs[i].filePath) != INVALID_FILE_ATTRIBUTES) {
                /* Check if we have a temp file with unsaved modifications */
                BOOL hasUnsavedChanges = g_session.tabs[i].isModified && 
                                         g_session.tabs[i].tempFilePath[0] != '\0' &&
                                         GetFileAttributes(g_session.tabs[i].tempFilePath) != INVALID_FILE_ATTRIBUTES;
                
                if (isActiveTab) {
                    /* Load active tab fully using FAST loading */
                    /* If we have unsaved changes, load from temp file first */
                    if (hasUnsavedChanges) {
                        tabIndex = AddTabFastFromTempFile(g_session.tabs[i].filePath, 
                                                          g_session.tabs[i].tempFilePath);
                    } else {
                        tabIndex = AddTabFast(g_session.tabs[i].filePath, FALSE);
                    }
                } else {
                    /* Create placeholder for non-active tabs */
                    tabIndex = AddPlaceholderTab(g_session.tabs[i].filePath, FALSE, g_session.tabs[i].isPinned);
                }
                
                if (tabIndex >= 0) {
                    TabInfo* tab = GetTab(tabIndex);
                    if (tab) {
                        tab->isPinned = g_session.tabs[i].isPinned;
                        
                        /* Store session data for lazy loading */
                        tab->sessionCursorPos = g_session.tabs[i].cursorPosition;
                        tab->sessionFirstLine = g_session.tabs[i].firstVisibleLine;
                        tab->sessionZoomLevel = g_session.tabs[i].zoomLevel;
                        
                        /* Store temp file path if there are unsaved changes */
                        if (hasUnsavedChanges) {
                            strncpy(tab->tempFilePath, g_session.tabs[i].tempFilePath, MAX_PATH - 1);
                            tab->tempFilePath[MAX_PATH - 1] = '\0';
                            tab->isModified = TRUE;
                            UpdateTabDisplayName(tabIndex);
                        }
                        
                        /* Apply per-tab view settings safely after tab initialization */
                        ApplySessionViewSettings(tabIndex, &g_session.tabs[i]);
                    }
                }
            }
        }
        
        /* Apply session state to active tab immediately */
        if (isActiveTab && tabIndex >= 0) {
            TabInfo* tab = GetTab(tabIndex);
            if (tab && tab->editorHandle) {
                SendMessage(tab->editorHandle, SCI_GOTOPOS,
                           g_session.tabs[i].cursorPosition, 0);
                SendMessage(tab->editorHandle, SCI_SETFIRSTVISIBLELINE,
                           g_session.tabs[i].firstVisibleLine, 0);
                /* Force scrollbar refresh by invalidating the editor window */
                InvalidateRect(tab->editorHandle, NULL, FALSE);
                /* Also update the scrollbar directly via Windows API */
                int lineCount = (int)SendMessage(tab->editorHandle, SCI_GETLINECOUNT, 0, 0);
                int firstVisible = g_session.tabs[i].firstVisibleLine;
                int linesOnScreen = (int)SendMessage(tab->editorHandle, SCI_LINESONSCREEN, 0, 0);
                SCROLLINFO si = {0};
                si.cbSize = sizeof(si);
                si.fMask = SIF_RANGE | SIF_POS | SIF_PAGE;
                si.nMin = 0;
                si.nMax = lineCount - 1;
                si.nPos = firstVisible;
                si.nPage = linesOnScreen;
                SetScrollInfo(tab->editorHandle, SB_VERT, &si, TRUE);
                SendMessage(tab->editorHandle, SCI_SETZOOM,
                           g_session.tabs[i].zoomLevel, 0);
                
                /* Apply per-tab view settings for active tab */
                SendMessage(tab->editorHandle, SCI_SETWRAPMODE, g_session.tabs[i].wordWrap ? SC_WRAP_WORD : SC_WRAP_NONE, 0);
                SendMessage(tab->editorHandle, SCI_SETMARGINWIDTHN, 0, g_session.tabs[i].showLineNumbers ? 40 : 0);
                SendMessage(tab->editorHandle, SCI_SETVIEWWS, g_session.tabs[i].showWhitespace ? SCWS_VISIBLEALWAYS : SCWS_INVISIBLE, 0);
                /* Note: autoIndent and codeFolding will be applied later when the tab is fully loaded */
            }
        }
        
        if (tabIndex >= 0) {
            openedAnyTab = TRUE;
        }
    }
    
    SESSION_PROFILE_MARK("All tabs created in original order");
    
    /* Select the active tab */
    if (openedAnyTab && activeTabIndex >= 0 && activeTabIndex < GetTabCount()) {
        SelectTab(activeTabIndex);
    }
    
    /* Update next tab ID to avoid duplicates */
    UpdateNextTabId();
    
    return openedAnyTab;
}

/* Get session data */
SessionData* GetSessionData(void)
{
    return &g_session;
}

/* Check if there's a saved session */
BOOL HasSavedSession(void)
{
    return HasSessionInRegistry();
}

/* Clear saved session */
void ClearSession(void)
{
    ClearSessionRegistry();
    
    memset(&g_session, 0, sizeof(SessionData));
    g_session.windowX = CW_USEDEFAULT;
    g_session.windowY = CW_USEDEFAULT;
    g_session.windowWidth = 800;
    g_session.windowHeight = 600;
}
