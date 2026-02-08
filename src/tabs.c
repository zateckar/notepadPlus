/*
 * Tab system implementation for Notepad+
 * Custom tab control with close buttons and tooltips
 */

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shlobj.h>
#include "tabs.h"
#include "editor.h"
#include "resource.h"
#include "window.h"
#include "syntax.h"
#include "themes.h"
#include "fileops.h"
#include "statusbar.h"
#include "toolbar.h"
#include "syntax.h"
#include "Scintilla.h"
#include "config.h"
#include <windowsx.h>

/* Import profiling macros from main.c when enabled */
#ifdef PROFILE_SESSION
extern LARGE_INTEGER g_startupFreq;
extern LARGE_INTEGER g_startupStart;
extern FILE* g_profileFile;

#define TABS_PROFILE_MARK(label) do { \
    LARGE_INTEGER now; \
    QueryPerformanceCounter(&now); \
    double ms = (double)(now.QuadPart - g_startupStart.QuadPart) * 1000.0 / g_startupFreq.QuadPart; \
    char buf[256]; \
    sprintf(buf, "[TABS] %s: %.2f ms\n", label, ms); \
    OutputDebugString(buf); \
    if (g_profileFile) { fprintf(g_profileFile, "%s", buf); fflush(g_profileFile); } \
} while(0)
#else
#define TABS_PROFILE_MARK(label)
#endif

/* Global tab control instance - declared early for use by all functions */
static TabControl g_tabControl = {0};
static int g_nextTabId = 1;

/* Scintilla function pointer for tab system */
static SciFnDirect g_TabSciFnDirect = NULL;

/* Deferred initialization flag - skip expensive operations during startup */
static BOOL g_deferExpensiveOperations = TRUE;

/* Pre-warmed Scintilla editor for fast session restore */
static HWND g_prewarmedEditor = NULL;

/* Set deferred operations mode */
void SetDeferredLoadingMode(BOOL defer)
{
    g_deferExpensiveOperations = defer;
}

/* Get deferred operations mode */
BOOL IsDeferredLoadingMode(void)
{
    return g_deferExpensiveOperations;
}

/* Apply expensive polish to an editor (DirectWrite, theme, syntax) - called after startup */
void PolishEditor(HWND editor, const char* filePath)
{
    if (!editor) return;
    
    SciFnDirect scintillaFunc = (SciFnDirect)SendMessage(editor, SCI_GETDIRECTFUNCTION, 0, 0);
    sptr_t scintillaPtr = SendMessage(editor, SCI_GETDIRECTPOINTER, 0, 0);
    
    if (!scintillaFunc || !scintillaPtr) return;
    
    /* Enable DirectWrite for better text rendering on high-DPI displays */
    scintillaFunc(scintillaPtr, SCI_SETTECHNOLOGY, SC_TECHNOLOGY_DIRECTWRITE, 0);
    
    /* Set font quality to LCD optimized for sharp subpixel rendering */
    /* SC_EFF_QUALITY_LCD_OPTIMIZED = 3 */
    scintillaFunc(scintillaPtr, SCI_SETFONTQUALITY, 3, 0);
    
    /* Re-apply font settings after technology change - this is required for DirectWrite */
    scintillaFunc(scintillaPtr, SCI_STYLESETFONT, STYLE_DEFAULT, (sptr_t)"Consolas");
    scintillaFunc(scintillaPtr, SCI_STYLESETSIZE, STYLE_DEFAULT, 10);
    scintillaFunc(scintillaPtr, SCI_STYLECLEARALL, 0, 0);
    
    /* Apply theme FIRST (this sets default colors and clears styles) */
    ApplyThemeToEditor(editor);
    
    /* Apply syntax highlighting AFTER theme (so colors don't get wiped) */
    if (filePath && strncmp(filePath, "New ", 4) != 0) {
        ApplySyntaxHighlightingForFile(editor, filePath);
    }
    
    /* Force a complete redraw with new DirectWrite rendering */
    InvalidateRect(editor, NULL, TRUE);
}

/* Polish all loaded tabs - call after startup is complete */
void PolishAllTabs(void)
{
    for (int i = 0; i < g_tabControl.tabCount; i++) {
        TabInfo* tab = &g_tabControl.tabs[i];
        if (tab->isLoaded && tab->editorHandle) {
            PolishEditor(tab->editorHandle, tab->filePath);
        }
    }
    g_deferExpensiveOperations = FALSE;
    
    /* Trigger final window resize now that startup is complete */
    /* This positions all editors correctly after skipping resizes during startup */
    HandleWindowResize(0, 0);
}

/* Add tab with fast loading - skips DirectWrite, syntax highlighting, and theme during startup */
int AddTabFast(const char* filePath, BOOL isNewFile)
{
    int index;
    TabInfo* tab;
    char displayName[MAX_PATH];
    
    TABS_PROFILE_MARK("AddTabFast: enter");
    
    /* Expand tabs array if needed */
    if (g_tabControl.tabCount >= g_tabControl.maxTabs) {
        int newMax = g_tabControl.maxTabs * 2;
        TabInfo* newTabs = (TabInfo*)realloc(g_tabControl.tabs, sizeof(TabInfo) * newMax);
        if (!newTabs) {
            return -1;
        }
        g_tabControl.tabs = newTabs;
        g_tabControl.maxTabs = newMax;
    }
    
    /* Initialize new tab */
    index = g_tabControl.tabCount;
    tab = &g_tabControl.tabs[index];
    memset(tab, 0, sizeof(TabInfo));
    
    /* Set file path */
    if (filePath && !isNewFile) {
        strncpy(tab->filePath, filePath, MAX_PATH - 1);
        tab->filePath[MAX_PATH - 1] = '\0';
        strcpy(displayName, GetShortDisplayName(filePath));
    } else {
        sprintf(tab->filePath, "New %d", g_nextTabId++);
        sprintf(displayName, "New %d", g_nextTabId - 1);
    }
    
    strcpy(tab->displayName, displayName);
    tab->isModified = FALSE;
    tab->state = TAB_STATE_NORMAL;
    
    /* Initialize per-tab settings with defaults */
    AppConfig* config = GetConfig();
    tab->wordWrap = config ? config->wordWrap : FALSE;
    tab->showLineNumbers = config ? config->showLineNumbers : TRUE;
    tab->showWhitespace = config ? config->showWhitespace : FALSE;
    tab->autoIndent = config ? config->autoIndent : FALSE;
    tab->codeFoldingEnabled = IsCodeFoldingEnabled();
    tab->changeHistoryEnabled = IsChangeHistoryEnabled();
    tab->isSplitView = FALSE; // Currently disabled
    
    TABS_PROFILE_MARK("AddTabFast: before CreateWindowEx");
    
    /* Use pre-warmed editor if available (saves ~6ms on first tab) */
    if (g_prewarmedEditor) {
        tab->editorHandle = g_prewarmedEditor;
        g_prewarmedEditor = NULL;
        TABS_PROFILE_MARK("AddTabFast: used pre-warmed editor");
    } else {
        /* Create Scintilla editor - FAST PATH: no DirectWrite yet */
        tab->editorHandle = CreateWindowEx(
            0,
            "Scintilla",
            "",
            WS_CHILD | WS_CLIPSIBLINGS,
            0, 0, 0, 0,
            g_tabControl.parentWindow,
            NULL,
            GetModuleHandle(NULL),
            NULL
        );
        TABS_PROFILE_MARK("AddTabFast: after CreateWindowEx");
    }
    
    if (!tab->editorHandle) {
        return -1;
    }
    
    /* Get Scintilla function pointer - retry if needed for pre-warmed editor */
    SciFnDirect scintillaFunc = NULL;
    sptr_t scintillaPtr = 0;
    int retries = 0;
    
    while (retries < 3) {
        scintillaFunc = (SciFnDirect)SendMessage(tab->editorHandle, SCI_GETDIRECTFUNCTION, 0, 0);
        scintillaPtr = SendMessage(tab->editorHandle, SCI_GETDIRECTPOINTER, 0, 0);
        
        if (scintillaFunc && scintillaPtr) {
            break;
        }
        
        /* Pre-warmed editor might need a moment to initialize - small delay */
        Sleep(1);
        retries++;
    }
    
    /* Validate function pointers before use */
    if (!scintillaFunc || !scintillaPtr) {
        /* Scintilla editor not fully initialized - cleanup and fail */
        DestroyWindow(tab->editorHandle);
        tab->editorHandle = NULL;
        return -1;
    }
    
    /* FAST PATH: Skip DirectWrite - use default technology (faster init) */
    /* DirectWrite will be enabled later via PolishEditor() */
    
    /* Set minimal editor properties */
    scintillaFunc(scintillaPtr, SCI_SETCODEPAGE, CP_UTF8, 0);
    scintillaFunc(scintillaPtr, SCI_STYLESETFONT, STYLE_DEFAULT, (sptr_t)"Consolas");
    scintillaFunc(scintillaPtr, SCI_STYLESETSIZE, STYLE_DEFAULT, 9);
    scintillaFunc(scintillaPtr, SCI_STYLECLEARALL, 0, 0);
    scintillaFunc(scintillaPtr, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
    scintillaFunc(scintillaPtr, SCI_SETMARGINWIDTHN, 0, 30);
    scintillaFunc(scintillaPtr, SCI_SETTABWIDTH, 4, 0);
    
    /* Apply default zoom level from config only if no session zoom */
    if (config && tab->sessionZoomLevel == 0) {
        scintillaFunc(scintillaPtr, SCI_SETZOOM, config->zoomLevel, 0);
    }
    
    /* Apply code folding state */
    if (tab->codeFoldingEnabled) {
        scintillaFunc(scintillaPtr, SCI_SETMARGINTYPEN, 2, SC_MARGIN_SYMBOL);
        scintillaFunc(scintillaPtr, SCI_SETMARGINMASKN, 2, SC_MASK_FOLDERS);
        scintillaFunc(scintillaPtr, SCI_SETMARGINWIDTHN, 2, 16);
        scintillaFunc(scintillaPtr, SCI_SETMARGINSENSITIVEN, 2, 1);
    } else {
        scintillaFunc(scintillaPtr, SCI_SETMARGINWIDTHN, 2, 0);
    }
    
    /* NOTE: Change history is enabled AFTER content is loaded to prevent marking initial content as modified */
    
    /* Apply per-tab view settings with validation */
    if (tab->wordWrap == TRUE || tab->wordWrap == FALSE) {  /* Validate it's a proper BOOL */
        scintillaFunc(scintillaPtr, SCI_SETWRAPMODE, tab->wordWrap ? SC_WRAP_WORD : SC_WRAP_NONE, 0);
    }
    if (tab->showLineNumbers == TRUE || tab->showLineNumbers == FALSE) {  /* Validate it's a proper BOOL */
        scintillaFunc(scintillaPtr, SCI_SETMARGINWIDTHN, 0, tab->showLineNumbers ? 40 : 0);
    }
    if (tab->showWhitespace == TRUE || tab->showWhitespace == FALSE) {  /* Validate it's a proper BOOL */
        scintillaFunc(scintillaPtr, SCI_SETVIEWWS, tab->showWhitespace ? SCWS_VISIBLEALWAYS : SCWS_INVISIBLE, 0);
    }
    
    TABS_PROFILE_MARK("AddTabFast: after editor setup");
    
    /* Load file content if not a new file */
    if (!isNewFile && filePath) {
        TABS_PROFILE_MARK("AddTabFast: before file open");
        FILE* file = fopen(filePath, "rb");
        if (file) {
            /* Get file size safely */
            if (fseek(file, 0, SEEK_END) == 0) {
                long fileSize = ftell(file);
                if (fileSize >= 0 && fseek(file, 0, SEEK_SET) == 0) {
                    /* Allocate buffer - handle empty files (size 0) */
                    char* buffer = (char*)malloc(fileSize + 1);
                    if (buffer) {
                    if (fileSize > 0) {
                        size_t bytesRead = fread(buffer, 1, fileSize, file);
                        buffer[bytesRead] = '\0';
                        
                        /* Detect and convert encoding (like AddTabWithFile does) */
                        BOOL hasBOM;
                        int encoding = DetectFileEncodingFromData(buffer, bytesRead, &hasBOM);
                        
                        /* Explicit BOM checks for accurate detection */
                        if (bytesRead >= 2 && (unsigned char)buffer[0] == 0xFF && (unsigned char)buffer[1] == 0xFE) {
                            encoding = ENCODING_UTF16_LE;
                        } else if (bytesRead >= 2 && (unsigned char)buffer[0] == 0xFE && (unsigned char)buffer[1] == 0xFF) {
                            encoding = ENCODING_UTF16_BE;
                        } else if (bytesRead >= 3 && (unsigned char)buffer[0] == 0xEF && 
                                  (unsigned char)buffer[1] == 0xBB && (unsigned char)buffer[2] == 0xBF) {
                            encoding = ENCODING_UTF8_BOM;
                        }
                        
                        /* Convert to UTF-8 if needed */
                        char* utf8Data = buffer;
                        if (encoding != ENCODING_UTF8) {
                            size_t utf8Size;
                            utf8Data = ConvertToUTF8(buffer, bytesRead, encoding, &utf8Size);
                            if (!utf8Data) {
                                utf8Data = buffer; /* Fallback to raw buffer if conversion fails */
                            }
                        }
                        
                        TABS_PROFILE_MARK("AddTabFast: after file read");
                        scintillaFunc(scintillaPtr, SCI_SETTEXT, 0, (sptr_t)utf8Data);
                        
                        /* Free converted data if it was different from buffer */
                        if (utf8Data != buffer) {
                            free(utf8Data);
                        }
                    } else {
                        /* Empty file - just set null terminator */
                        buffer[0] = '\0';
                        TABS_PROFILE_MARK("AddTabFast: after file read");
                        scintillaFunc(scintillaPtr, SCI_SETTEXT, 0, (sptr_t)buffer);
                    }
                        TABS_PROFILE_MARK("AddTabFast: after SCI_SETTEXT");
                        free(buffer);
                        TABS_PROFILE_MARK("AddTabFast: after free buffer");
                    }
                }
            }
            fclose(file);
            TABS_PROFILE_MARK("AddTabFast: after fclose");
        }
        /* For unsaved files (temp files), don't set save point - keep as modified */
        /* The tab's isModified flag will be set later in RestoreSession */
        TABS_PROFILE_MARK("AddTabFast: after SETSAVEPOINT");
        
        /* Update file type in status bar using new language detection */
        TABS_PROFILE_MARK("AddTabFast: before GetFileTypeFromPath");
        const char* fileType = GetFileTypeFromPath(filePath);
        TABS_PROFILE_MARK("AddTabFast: after GetFileTypeFromPath");
        UpdateFileType(fileType);
        TABS_PROFILE_MARK("AddTabFast: after UpdateFileType");
        
    /* FAST PATH: Skip syntax highlighting - will be applied later via PolishEditor() */
        /* FAST PATH: Skip adding to recent files - not needed during session restore */
    } else {
        /* Update file type in status bar for new files */
        UpdateFileType("Text");
        /* NOTE: SETSAVEPOINT is called AFTER EnableChangeHistory below for all cases */
    }
    
    /* FAST PATH: Skip theme application - will be applied later via PolishEditor() */
    
    /* Enable change history FIRST - this clears undo buffer and enables tracking */
    if (tab->changeHistoryEnabled) {
        EnableChangeHistory(tab->editorHandle, TRUE);
    }
    
    /* Set save point AFTER enabling change history
     * This must be AFTER EnableChangeHistory because that function calls
     * SCI_EMPTYUNDOBUFFER which would reset any prior save point.
     * The save point tells change history what the "clean" baseline is. */
    scintillaFunc(scintillaPtr, SCI_SETSAVEPOINT, 0, 0);
    
    /* Mark as loaded */
    tab->isLoaded = TRUE;
    
    /* Initially hide the editor */
    ShowWindow(tab->editorHandle, SW_HIDE);
    
    /* Increment tab count */
    g_tabControl.tabCount++;
    
    TABS_PROFILE_MARK("AddTabFast: before SelectTab");
    
    /* Select the new tab */
    SelectTab(index);
    
    TABS_PROFILE_MARK("AddTabFast: after SelectTab");
    
    /* Update layout */
    UpdateTabLayout();
    
    TABS_PROFILE_MARK("AddTabFast: after UpdateTabLayout");
    
    /* Trigger a window resize to position the new editor correctly */
    /* SKIP during startup - will be done once at the end */
    if (!IsDeferredLoadingMode()) {
        HandleWindowResize(0, 0);
    }
    
    TABS_PROFILE_MARK("AddTabFast: after HandleWindowResize");
    
    return index;
}

/* Add tab with fast loading from temp file - for restoring existing files with unsaved changes */
int AddTabFastFromTempFile(const char* filePath, const char* tempFilePath)
{
    int index;
    TabInfo* tab;
    char displayName[MAX_PATH];
    
    TABS_PROFILE_MARK("AddTabFastFromTempFile: enter");
    
    /* Expand tabs array if needed */
    if (g_tabControl.tabCount >= g_tabControl.maxTabs) {
        int newMax = g_tabControl.maxTabs * 2;
        TabInfo* newTabs = (TabInfo*)realloc(g_tabControl.tabs, sizeof(TabInfo) * newMax);
        if (!newTabs) {
            return -1;
        }
        g_tabControl.tabs = newTabs;
        g_tabControl.maxTabs = newMax;
    }
    
    /* Initialize new tab */
    index = g_tabControl.tabCount;
    tab = &g_tabControl.tabs[index];
    memset(tab, 0, sizeof(TabInfo));
    
    /* Set file path to the original file (not the temp file) */
    strncpy(tab->filePath, filePath, MAX_PATH - 1);
    tab->filePath[MAX_PATH - 1] = '\0';
    strcpy(displayName, GetShortDisplayName(filePath));
    
    /* Store temp file path for reference */
    strncpy(tab->tempFilePath, tempFilePath, MAX_PATH - 1);
    tab->tempFilePath[MAX_PATH - 1] = '\0';
    
    strcpy(tab->displayName, displayName);
    tab->isModified = TRUE;  /* Mark as modified since it has unsaved changes */
    tab->state = TAB_STATE_NORMAL;
    
    /* Initialize per-tab settings with defaults */
    AppConfig* config = GetConfig();
    tab->wordWrap = config ? config->wordWrap : FALSE;
    tab->showLineNumbers = config ? config->showLineNumbers : TRUE;
    tab->showWhitespace = config ? config->showWhitespace : FALSE;
    tab->autoIndent = config ? config->autoIndent : FALSE;
    tab->codeFoldingEnabled = IsCodeFoldingEnabled();
    tab->changeHistoryEnabled = IsChangeHistoryEnabled();
    tab->isSplitView = FALSE;
    
    TABS_PROFILE_MARK("AddTabFastFromTempFile: before CreateWindowEx");
    
    /* Use pre-warmed editor if available */
    if (g_prewarmedEditor) {
        tab->editorHandle = g_prewarmedEditor;
        g_prewarmedEditor = NULL;
        TABS_PROFILE_MARK("AddTabFastFromTempFile: used pre-warmed editor");
    } else {
        /* Create Scintilla editor - FAST PATH: no DirectWrite yet */
        tab->editorHandle = CreateWindowEx(
            0,
            "Scintilla",
            "",
            WS_CHILD | WS_CLIPSIBLINGS,
            0, 0, 0, 0,
            g_tabControl.parentWindow,
            NULL,
            GetModuleHandle(NULL),
            NULL
        );
        TABS_PROFILE_MARK("AddTabFastFromTempFile: after CreateWindowEx");
    }
    
    if (!tab->editorHandle) {
        return -1;
    }
    
    /* Get Scintilla function pointer */
    SciFnDirect scintillaFunc = NULL;
    sptr_t scintillaPtr = 0;
    int retries = 0;
    
    while (retries < 3) {
        scintillaFunc = (SciFnDirect)SendMessage(tab->editorHandle, SCI_GETDIRECTFUNCTION, 0, 0);
        scintillaPtr = SendMessage(tab->editorHandle, SCI_GETDIRECTPOINTER, 0, 0);
        
        if (scintillaFunc && scintillaPtr) {
            break;
        }
        
        Sleep(1);
        retries++;
    }
    
    if (!scintillaFunc || !scintillaPtr) {
        DestroyWindow(tab->editorHandle);
        tab->editorHandle = NULL;
        return -1;
    }
    
    /* Set minimal editor properties */
    scintillaFunc(scintillaPtr, SCI_SETCODEPAGE, CP_UTF8, 0);
    scintillaFunc(scintillaPtr, SCI_STYLESETFONT, STYLE_DEFAULT, (sptr_t)"Consolas");
    scintillaFunc(scintillaPtr, SCI_STYLESETSIZE, STYLE_DEFAULT, 9);
    scintillaFunc(scintillaPtr, SCI_STYLECLEARALL, 0, 0);
    scintillaFunc(scintillaPtr, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
    scintillaFunc(scintillaPtr, SCI_SETMARGINWIDTHN, 0, 30);
    scintillaFunc(scintillaPtr, SCI_SETTABWIDTH, 4, 0);
    
    /* Apply default zoom level from config only if no session zoom */
    if (config && tab->sessionZoomLevel == 0) {
        scintillaFunc(scintillaPtr, SCI_SETZOOM, config->zoomLevel, 0);
    }
    
    /* Apply code folding state */
    if (tab->codeFoldingEnabled) {
        scintillaFunc(scintillaPtr, SCI_SETMARGINTYPEN, 2, SC_MARGIN_SYMBOL);
        scintillaFunc(scintillaPtr, SCI_SETMARGINMASKN, 2, SC_MASK_FOLDERS);
        scintillaFunc(scintillaPtr, SCI_SETMARGINWIDTHN, 2, 16);
        scintillaFunc(scintillaPtr, SCI_SETMARGINSENSITIVEN, 2, 1);
    } else {
        scintillaFunc(scintillaPtr, SCI_SETMARGINWIDTHN, 2, 0);
    }
    
    /* NOTE: Change history is enabled AFTER content is loaded to prevent marking initial content as modified */
    
    /* Apply per-tab view settings with validation */
    if (tab->wordWrap == TRUE || tab->wordWrap == FALSE) {
        scintillaFunc(scintillaPtr, SCI_SETWRAPMODE, tab->wordWrap ? SC_WRAP_WORD : SC_WRAP_NONE, 0);
    }
    if (tab->showLineNumbers == TRUE || tab->showLineNumbers == FALSE) {
        scintillaFunc(scintillaPtr, SCI_SETMARGINWIDTHN, 0, tab->showLineNumbers ? 40 : 0);
    }
    if (tab->showWhitespace == TRUE || tab->showWhitespace == FALSE) {
        scintillaFunc(scintillaPtr, SCI_SETVIEWWS, tab->showWhitespace ? SCWS_VISIBLEALWAYS : SCWS_INVISIBLE, 0);
    }
    
    TABS_PROFILE_MARK("AddTabFastFromTempFile: after editor setup");
    
    /* Load content from temp file (contains unsaved modifications) */
    TABS_PROFILE_MARK("AddTabFastFromTempFile: before file open");
    FILE* file = fopen(tempFilePath, "rb");
    if (file) {
        if (fseek(file, 0, SEEK_END) == 0) {
            long fileSize = ftell(file);
            if (fileSize >= 0 && fseek(file, 0, SEEK_SET) == 0) {
                char* buffer = (char*)malloc(fileSize + 1);
                if (buffer) {
                    if (fileSize > 0) {
                        size_t bytesRead = fread(buffer, 1, fileSize, file);
                        buffer[bytesRead] = '\0';
                        
                        /* Temp files are always saved as UTF-8, so no encoding conversion needed */
                        TABS_PROFILE_MARK("AddTabFastFromTempFile: after file read");
                        scintillaFunc(scintillaPtr, SCI_SETTEXT, 0, (sptr_t)buffer);
                    } else {
                        buffer[0] = '\0';
                        scintillaFunc(scintillaPtr, SCI_SETTEXT, 0, (sptr_t)buffer);
                    }
                    TABS_PROFILE_MARK("AddTabFastFromTempFile: after SCI_SETTEXT");
                    free(buffer);
                }
            }
        }
        fclose(file);
        TABS_PROFILE_MARK("AddTabFastFromTempFile: after fclose");
    }
    
    /* Do NOT set save point - we want this to remain modified */
    /* The original file on disk is the "saved" state, and we have unsaved changes */
    
    /* Update file type in status bar using original file path for language detection */
    TABS_PROFILE_MARK("AddTabFastFromTempFile: before GetFileTypeFromPath");
    const char* fileType = GetFileTypeFromPath(filePath);
    TABS_PROFILE_MARK("AddTabFastFromTempFile: after GetFileTypeFromPath");
    UpdateFileType(fileType);
    TABS_PROFILE_MARK("AddTabFastFromTempFile: after UpdateFileType");
    
    /* Enable change history AFTER content is loaded
     * Do NOT set save point first since we want all content to show as modified */
    if (tab->changeHistoryEnabled) {
        EnableChangeHistory(tab->editorHandle, TRUE);
    }
    
    /* Update display name to show modification indicator */
    UpdateTabDisplayName(index);
    
    /* Mark as loaded */
    tab->isLoaded = TRUE;
    
    /* Initially hide the editor */
    ShowWindow(tab->editorHandle, SW_HIDE);
    
    /* Increment tab count */
    g_tabControl.tabCount++;
    
    TABS_PROFILE_MARK("AddTabFastFromTempFile: before SelectTab");
    
    /* Select the new tab */
    SelectTab(index);
    
    TABS_PROFILE_MARK("AddTabFastFromTempFile: after SelectTab");
    
    /* Update layout */
    UpdateTabLayout();
    
    TABS_PROFILE_MARK("AddTabFastFromTempFile: after UpdateTabLayout");
    
    /* Trigger a window resize to position the new editor correctly */
    /* SKIP during startup - will be done once at the end */
    if (!IsDeferredLoadingMode()) {
        HandleWindowResize(0, 0);
    }
    
    TABS_PROFILE_MARK("AddTabFastFromTempFile: after HandleWindowResize");
    
    return index;
}

/* Load Scintilla for tab system - with static linking, Scintilla is already registered */
static BOOL LoadScintillaForTabs(void)
{
    /* With static linking, Scintilla window class is registered by Scintilla_RegisterClasses()
     * called in main.c. No DLL loading needed. */
    return TRUE;
}

/* Function to update next tab ID based on existing tab names */
void UpdateNextTabId(void)
{
    int maxId = 0;
    for (int i = 0; i < g_tabControl.tabCount; i++) {
        TabInfo* tab = &g_tabControl.tabs[i];
        if (strncmp(tab->filePath, "New ", 4) == 0) {
            int id = atoi(tab->filePath + 4);
            if (id > maxId) {
                maxId = id;
            }
        }
    }
    if (maxId >= g_nextTabId) {
        g_nextTabId = maxId + 1;
    }
}

/* Window class name for tab control */
#define TAB_CONTROL_CLASS_NAME "NotepadPlusTabControl"

/* Internal helper functions */
static void CalculateTabPositions(void);
static void InvalidateTab(int index);
static void InvalidateAllTabs(void);
static void EnsureTabVisible(int index);
static void AdjustScrollButtons(void);
static void DrawPinIndicator(HDC hdc, int x, int y, BOOL isActiveTab);

/* Initialize tab system */
BOOL InitializeTabs(HWND parentWindow)
{
    WNDCLASSEX wc;
    HFONT systemFont;
    
    TABS_PROFILE_MARK("InitializeTabs: enter");
    
    /* Load Scintilla for tab system */
    if (!LoadScintillaForTabs()) {
        MessageBox(NULL, "Failed to load Scintilla for tabs", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    /* Initialize tab control structure */
    memset(&g_tabControl, 0, sizeof(TabControl));
    g_tabControl.parentWindow = parentWindow;
    g_tabControl.selectedIndex = -1;
    g_tabControl.hoveredTab = -1;
    g_tabControl.closeHoveredTab = -1;
    g_tabControl.showAddButton = TRUE;
    
    /* Allocate initial tabs */
    g_tabControl.maxTabs = 10;
    g_tabControl.tabs = (TabInfo*)malloc(sizeof(TabInfo) * g_tabControl.maxTabs);
    if (!g_tabControl.tabs) {
        return FALSE;
    }
    
    /* Register tab control window class */
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = TabWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = sizeof(TabControl*);
    wc.hInstance = GetModuleHandle(NULL);
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;  /* We'll paint our own background */
    wc.lpszMenuName = NULL;
    wc.lpszClassName = TAB_CONTROL_CLASS_NAME;
    wc.hIconSm = NULL;
    
    if (!RegisterClassEx(&wc)) {
        free(g_tabControl.tabs);
        return FALSE;
    }
    
    /* Create tab control window */
    g_tabControl.hwnd = CreateWindowEx(
        0,
        TAB_CONTROL_CLASS_NAME,
        "",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 0, 0,
        parentWindow,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!g_tabControl.hwnd) {
        free(g_tabControl.tabs);
        return FALSE;
    }
    
    /* Store tab control pointer in window data */
    SetWindowLongPtr(g_tabControl.hwnd, 0, (LONG_PTR)&g_tabControl);
    
    /* Create tooltip window */
    g_tabControl.tooltipWindow = CreateWindowEx(
        WS_EX_TOPMOST,
        TOOLTIPS_CLASS,
        NULL,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        g_tabControl.hwnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (g_tabControl.tooltipWindow) {
        SetWindowPos(g_tabControl.tooltipWindow, HWND_TOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        
        /* Set tooltip delay */
        SendMessage(g_tabControl.tooltipWindow, TTM_SETDELAYTIME, TTDT_INITIAL, TIP_DELAY);
    }
    
    /* Create fonts - larger text for better readability */
    systemFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    LOGFONT lf;
    GetObject(systemFont, sizeof(LOGFONT), &lf);
    
    /* Increase font size for larger tabs (negative height means point size) */
    if (lf.lfHeight < 0) {
        lf.lfHeight = lf.lfHeight - 2;  /* Make 2 units larger */
    } else {
        lf.lfHeight = lf.lfHeight + 2;
    }
    strcpy(lf.lfFaceName, "Segoe UI");  /* Modern Windows font */
    
    /* Normal font */
    g_tabControl.normalFont = CreateFontIndirect(&lf);
    
    /* Semi-bold font for selected tab (less heavy than bold) */
    lf.lfWeight = FW_SEMIBOLD;
    g_tabControl.boldFont = CreateFontIndirect(&lf);
    
    /* Load cursors */
    g_tabControl.handCursor = LoadCursor(NULL, IDC_HAND);
    g_tabControl.arrowCursor = LoadCursor(NULL, IDC_ARROW);
    
    TABS_PROFILE_MARK("InitializeTabs: before pre-warm editor");
    
    /* Pre-create a Scintilla editor for fast session restore */
    /* This moves the ~6ms CreateWindowEx cost to before the deferred session load */
    g_prewarmedEditor = CreateWindowEx(
        0,
        "Scintilla",
        "",
        WS_CHILD | WS_CLIPSIBLINGS,
        0, 0, 0, 0,
        parentWindow,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    /* Note: We don't configure it yet - that will happen when it's assigned to a tab */
    
    return TRUE;
}

/* Cleanup tab system */
void CleanupTabs(void)
{
    /* Destroy pre-warmed editor if it wasn't used */
    if (g_prewarmedEditor) {
        DestroyWindow(g_prewarmedEditor);
        g_prewarmedEditor = NULL;
    }
    
    /* Close all tabs and cleanup */
    if (g_tabControl.tabs) {
        CloseAllTabs();
        free(g_tabControl.tabs);
        g_tabControl.tabs = NULL;
    }
    
    /* Cleanup fonts */
    if (g_tabControl.normalFont) {
        DeleteObject(g_tabControl.normalFont);
    }
    
    if (g_tabControl.boldFont) {
        DeleteObject(g_tabControl.boldFont);
    }
    
    /* Destroy tooltip */
    if (g_tabControl.tooltipWindow) {
        DestroyWindow(g_tabControl.tooltipWindow);
    }
    
    /* Destroy tab control window */
    if (g_tabControl.hwnd) {
        DestroyWindow(g_tabControl.hwnd);
    }
    
    /* Unregister window class */
    UnregisterClass(TAB_CONTROL_CLASS_NAME, GetModuleHandle(NULL));
    
    memset(&g_tabControl, 0, sizeof(TabControl));
}

/* Get tab control instance */
TabControl* GetTabControl(void)
{
    return &g_tabControl;
}

/* Add new tab with default file */
int AddNewTab(const char* filePath)
{
    /* If filePath is provided, it's an existing file to open */
    /* If filePath is NULL or empty, it's a new untitled file */
    BOOL isNewFile = (filePath == NULL || filePath[0] == '\0');
    return AddTabWithFile(filePath, isNewFile);
}

/* Add tab with file */
int AddTabWithFile(const char* filePath, BOOL isNewFile)
{
    int index;
    TabInfo* tab;
    char displayName[MAX_PATH];
    
    /* Expand tabs array if needed */
    if (g_tabControl.tabCount >= g_tabControl.maxTabs) {
        int newMax = g_tabControl.maxTabs * 2;
        TabInfo* newTabs = (TabInfo*)realloc(g_tabControl.tabs, sizeof(TabInfo) * newMax);
        if (!newTabs) {
            return -1;
        }
        g_tabControl.tabs = newTabs;
        g_tabControl.maxTabs = newMax;
    }
    
    /* Initialize new tab */
    index = g_tabControl.tabCount;
    tab = &g_tabControl.tabs[index];
    memset(tab, 0, sizeof(TabInfo));
    
    /* Set file path */
    if (filePath && !isNewFile) {
        strncpy(tab->filePath, filePath, MAX_PATH - 1);
        tab->filePath[MAX_PATH - 1] = '\0';
        
        /* Get display name */
        strcpy(displayName, GetShortDisplayName(filePath));
    } else {
        /* New unsaved file */
        sprintf(tab->filePath, "New %d", g_nextTabId++);
        sprintf(displayName, "New %d", g_nextTabId - 1);
    }
    
    strcpy(tab->displayName, displayName);
    tab->isModified = FALSE;
    tab->state = TAB_STATE_NORMAL;
    
    /* Apply default view settings from config */
    AppConfig* config = GetConfig();
    if (config) {
        tab->wordWrap = config->wordWrap;
        tab->showLineNumbers = config->showLineNumbers;
        tab->showWhitespace = config->showWhitespace;
        tab->autoIndent = config->autoIndent;
        tab->isSplitView = FALSE; // Split view is currently disabled
    }
    tab->codeFoldingEnabled = IsCodeFoldingEnabled();
    tab->changeHistoryEnabled = IsChangeHistoryEnabled();
    
    /* Create hidden Scintilla editor for this tab */
    tab->editorHandle = CreateWindowEx(
        0,
        "Scintilla",
        "",
        WS_CHILD | WS_CLIPSIBLINGS,
        0, 0, 0, 0,
        g_tabControl.parentWindow,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!tab->editorHandle) {
        return -1;
    }
    
    /* Configure the editor */
    /* Get Scintilla function pointer */
    SciFnDirect scintillaFunc = (SciFnDirect)SendMessage(tab->editorHandle, SCI_GETDIRECTFUNCTION, 0, 0);
    sptr_t scintillaPtr = SendMessage(tab->editorHandle, SCI_GETDIRECTPOINTER, 0, 0);
    
    /* Enable DirectWrite for better text rendering on high-DPI displays */
    scintillaFunc(scintillaPtr, SCI_SETTECHNOLOGY, SC_TECHNOLOGY_DIRECTWRITE, 0);
    
    /* Set font quality to LCD optimized for sharp subpixel rendering */
    /* SC_EFF_QUALITY_LCD_OPTIMIZED = 3 */
    scintillaFunc(scintillaPtr, SCI_SETFONTQUALITY, 3, 0);
    
    /* Set basic editor properties */
    scintillaFunc(scintillaPtr, SCI_SETCODEPAGE, CP_UTF8, 0);
    scintillaFunc(scintillaPtr, SCI_STYLESETFONT, STYLE_DEFAULT, (sptr_t)"Consolas");
    scintillaFunc(scintillaPtr, SCI_STYLESETSIZE, STYLE_DEFAULT, 10);
    scintillaFunc(scintillaPtr, SCI_STYLECLEARALL, 0, 0);
    scintillaFunc(scintillaPtr, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
    scintillaFunc(scintillaPtr, SCI_SETMARGINWIDTHN, 0, 0); /* Will be set based on tab->showLineNumbers */
    scintillaFunc(scintillaPtr, SCI_SETTABWIDTH, 4, 0);
    
    /* Apply default zoom level from config only for truly new tabs (not session restore) */
    if (config) {
        scintillaFunc(scintillaPtr, SCI_SETZOOM, config->zoomLevel, 0);
    }
    
    /* Apply code folding state */
    if (tab->codeFoldingEnabled) {
        scintillaFunc(scintillaPtr, SCI_SETMARGINTYPEN, 2, SC_MARGIN_SYMBOL);
        scintillaFunc(scintillaPtr, SCI_SETMARGINMASKN, 2, SC_MASK_FOLDERS);
        scintillaFunc(scintillaPtr, SCI_SETMARGINWIDTHN, 2, 16);
        scintillaFunc(scintillaPtr, SCI_SETMARGINSENSITIVEN, 2, 1);
    } else {
        scintillaFunc(scintillaPtr, SCI_SETMARGINWIDTHN, 2, 0);
    }
    
    /* NOTE: Change history is enabled AFTER content is loaded to prevent marking initial content as modified */
    
    /* Apply auto-indent settings from config */
    if (config && config->autoIndent) {
        scintillaFunc(scintillaPtr, SCI_SETINDENTATIONGUIDES, SC_IV_LOOKBOTH, 0);
        scintillaFunc(scintillaPtr, SCI_SETTABINDENTS, 1, 0);
        scintillaFunc(scintillaPtr, SCI_SETBACKSPACEUNINDENTS, 1, 0);
    }
    
    /* Apply per-tab view settings */
    if (tab->wordWrap) {
        scintillaFunc(scintillaPtr, SCI_SETWRAPMODE, SC_WRAP_WORD, 0);
    }
    
    if (tab->showLineNumbers) {
        scintillaFunc(scintillaPtr, SCI_SETMARGINWIDTHN, 0, 40);
    } else {
       scintillaFunc(scintillaPtr, SCI_SETMARGINWIDTHN, 0, 0);
    }
    
    if (tab->showWhitespace) {
        scintillaFunc(scintillaPtr, SCI_SETVIEWWS, SCWS_VISIBLEALWAYS, 0);
    }
    
    /* If not a new file, load the file content with encoding conversion */
    if (!isNewFile && filePath) {
        FILE* file = fopen(filePath, "rb");
        if (file) {
            fseek(file, 0, SEEK_END);
            long fileSize = ftell(file);
            fseek(file, 0, SEEK_SET);
            
            char* buffer = (char*)malloc(fileSize + 1);
            if (buffer) {
                size_t bytesRead = fread(buffer, 1, fileSize, file);
                buffer[bytesRead] = '\0';
                
                /* Detect and convert encoding */
                BOOL hasBOM;
                int encoding = DetectFileEncodingFromData(buffer, bytesRead, &hasBOM);
                
                /* Explicit BOM checks */
                if (bytesRead >= 2 && (unsigned char)buffer[0] == 0xFF && (unsigned char)buffer[1] == 0xFE) {
                    encoding = ENCODING_UTF16_LE;
                } else if (bytesRead >= 2 && (unsigned char)buffer[0] == 0xFE && (unsigned char)buffer[1] == 0xFF) {
                    encoding = ENCODING_UTF16_BE;
                } else if (bytesRead >= 3 && (unsigned char)buffer[0] == 0xEF && 
                          (unsigned char)buffer[1] == 0xBB && (unsigned char)buffer[2] == 0xBF) {
                    encoding = ENCODING_UTF8_BOM;
                }
                
                if (encoding != ENCODING_UTF8) {
                    size_t utf8Size;
                    char* utf8Data = ConvertToUTF8(buffer, bytesRead, encoding, &utf8Size);
                    if (utf8Data) {
                        scintillaFunc(scintillaPtr, SCI_SETTEXT, 0, (sptr_t)utf8Data);
                        free(utf8Data);
                    } else {
                        scintillaFunc(scintillaPtr, SCI_SETTEXT, 0, (sptr_t)buffer);
                    }
                } else {
                    scintillaFunc(scintillaPtr, SCI_SETTEXT, 0, (sptr_t)buffer);
                }
                free(buffer);
            }
            fclose(file);
        }
        
        /* Update file type in status bar using new language detection */
        UpdateFileType(GetFileTypeFromPath(filePath));
        
        /* Add to recent files list */
        AddRecentFile(filePath);
    } else {
        /* Update file type in status bar for new files */
        UpdateFileType("Text");
    }
    
    /* Enable change history FIRST - this clears undo buffer and enables tracking */
    if (tab->changeHistoryEnabled) {
        EnableChangeHistory(tab->editorHandle, TRUE);
    }
    
    /* Set save point AFTER enabling change history
     * This must be AFTER EnableChangeHistory because that function calls
     * SCI_EMPTYUNDOBUFFER which would reset any prior save point.
     * The save point tells change history what the "clean" baseline is. */
    scintillaFunc(scintillaPtr, SCI_SETSAVEPOINT, 0, 0);
    
    /* Apply current theme to the editor FIRST (this clears styles) */
    ApplyThemeToEditor(tab->editorHandle);
    
    /* Apply syntax highlighting AFTER theme (so it doesn't get overwritten) */
    if (!isNewFile && filePath) {
        ApplySyntaxHighlightingForFile(tab->editorHandle, filePath);
    }
    
    /* Mark as loaded since we created the editor and loaded content */
    tab->isLoaded = TRUE;
    
    /* Initially hide the editor */
    ShowWindow(tab->editorHandle, SW_HIDE);
    
    /* Increment tab count */
    g_tabControl.tabCount++;
    
    /* Select the new tab */
    SelectTab(index);
    
    /* Update layout */
    UpdateTabLayout();
    
    /* Trigger a window resize to position the new editor correctly */
    HandleWindowResize(0, 0);
    
    return index;
}

/* Add placeholder tab for lazy loading - creates tab entry without loading content */
int AddPlaceholderTab(const char* filePath, BOOL isNewFile, BOOL isPinned)
{
    int index;
    TabInfo* tab;
    char displayName[MAX_PATH];
    
    /* Expand tabs array if needed */
    if (g_tabControl.tabCount >= g_tabControl.maxTabs) {
        int newMax = g_tabControl.maxTabs * 2;
        TabInfo* newTabs = (TabInfo*)realloc(g_tabControl.tabs, sizeof(TabInfo) * newMax);
        if (!newTabs) {
            return -1;
        }
        g_tabControl.tabs = newTabs;
        g_tabControl.maxTabs = newMax;
    }
    
    /* Initialize new tab */
    index = g_tabControl.tabCount;
    tab = &g_tabControl.tabs[index];
    memset(tab, 0, sizeof(TabInfo));
    
    /* Set file path */
    if (filePath && !isNewFile) {
        strncpy(tab->filePath, filePath, MAX_PATH - 1);
        tab->filePath[MAX_PATH - 1] = '\0';
        
        /* Get display name */
        strcpy(displayName, GetShortDisplayName(filePath));
    } else {
        /* New unsaved file */
        sprintf(tab->filePath, "New %d", g_nextTabId++);
        sprintf(displayName, "New %d", g_nextTabId - 1);
    }
    
    strcpy(tab->displayName, displayName);
    tab->isModified = FALSE;
    tab->state = TAB_STATE_NORMAL;
    tab->isPinned = isPinned;
    
    /* Apply default code folding from config */
    tab->codeFoldingEnabled = IsCodeFoldingEnabled();
    tab->changeHistoryEnabled = IsChangeHistoryEnabled();
    
    /* Apply default view settings from config */
    AppConfig* config = GetConfig();
    if (config) {
        tab->wordWrap = config->wordWrap;
        tab->showLineNumbers = config->showLineNumbers;
        tab->showWhitespace = config->showWhitespace;
        tab->autoIndent = config->autoIndent;
        tab->isSplitView = FALSE; // Split view is currently disabled
    }
    
    /* Mark as NOT loaded - content will be loaded on demand */
    tab->isLoaded = FALSE;
    tab->editorHandle = NULL;
    
    /* Increment tab count */
    g_tabControl.tabCount++;
    
    /* Update layout but don't select this tab */
    UpdateTabLayout();
    
    return index;
}

/* Load content for a placeholder tab - creates editor and loads file content */
BOOL LoadTabContent(int index)
{
    if (index < 0 || index >= g_tabControl.tabCount) {
        return FALSE;
    }
    
    TabInfo* tab = &g_tabControl.tabs[index];
    
    /* Already loaded */
    if (tab->isLoaded) {
        return TRUE;
    }
    
    /* Create Scintilla editor for this tab */
    tab->editorHandle = CreateWindowEx(
        0,
        "Scintilla",
        "",
        WS_CHILD | WS_CLIPSIBLINGS,
        0, 0, 0, 0,
        g_tabControl.parentWindow,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!tab->editorHandle) {
        return FALSE;
    }
    
    /* Configure the editor */
    SciFnDirect scintillaFunc = (SciFnDirect)SendMessage(tab->editorHandle, SCI_GETDIRECTFUNCTION, 0, 0);
    sptr_t scintillaPtr = SendMessage(tab->editorHandle, SCI_GETDIRECTPOINTER, 0, 0);
    
    /* Enable DirectWrite for better text rendering */
    scintillaFunc(scintillaPtr, SCI_SETTECHNOLOGY, SC_TECHNOLOGY_DIRECTWRITE, 0);
    
    /* Set font quality to LCD optimized for sharp subpixel rendering */
    /* SC_EFF_QUALITY_LCD_OPTIMIZED = 3 */
    scintillaFunc(scintillaPtr, SCI_SETFONTQUALITY, 3, 0);
    
    /* Set basic editor properties */
    scintillaFunc(scintillaPtr, SCI_SETCODEPAGE, CP_UTF8, 0);
    scintillaFunc(scintillaPtr, SCI_STYLESETFONT, STYLE_DEFAULT, (sptr_t)"Consolas");
    scintillaFunc(scintillaPtr, SCI_STYLESETSIZE, STYLE_DEFAULT, 10);
    scintillaFunc(scintillaPtr, SCI_STYLECLEARALL, 0, 0);
    scintillaFunc(scintillaPtr, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
    scintillaFunc(scintillaPtr, SCI_SETMARGINWIDTHN, 0, 35);
    scintillaFunc(scintillaPtr, SCI_SETTABWIDTH, 4, 0);
    
    /* Apply code folding state */
    if (tab->codeFoldingEnabled) {
        scintillaFunc(scintillaPtr, SCI_SETMARGINTYPEN, 2, SC_MARGIN_SYMBOL);
        scintillaFunc(scintillaPtr, SCI_SETMARGINMASKN, 2, SC_MASK_FOLDERS);
        scintillaFunc(scintillaPtr, SCI_SETMARGINWIDTHN, 2, 16);
        scintillaFunc(scintillaPtr, SCI_SETMARGINSENSITIVEN, 2, 1);
    } else {
        scintillaFunc(scintillaPtr, SCI_SETMARGINWIDTHN, 2, 0);
    }
    
    /* NOTE: Change history is enabled AFTER content is loaded to prevent marking initial content as modified */
    
    /* Apply auto-indent settings from config */
    AppConfig* config = GetConfig();
    if (config && config->autoIndent) {
        scintillaFunc(scintillaPtr, SCI_SETINDENTATIONGUIDES, SC_IV_LOOKBOTH, 0);
        scintillaFunc(scintillaPtr, SCI_SETTABINDENTS, 1, 0);
        scintillaFunc(scintillaPtr, SCI_SETBACKSPACEUNINDENTS, 1, 0);
    }
    
    /* Apply per-tab view settings stored from session or defaults */
    scintillaFunc(scintillaPtr, SCI_SETWRAPMODE, tab->wordWrap ? SC_WRAP_WORD : SC_WRAP_NONE, 0);
    scintillaFunc(scintillaPtr, SCI_SETMARGINWIDTHN, 0, tab->showLineNumbers ? 40 : 0);
    scintillaFunc(scintillaPtr, SCI_SETVIEWWS, tab->showWhitespace ? SCWS_VISIBLEALWAYS : SCWS_INVISIBLE, 0);
    
    /* Check if this is a new file or existing file */
    BOOL isNewFile = (strncmp(tab->filePath, "New ", 4) == 0);
    
    /* Determine actual file path to load
     * For new unsaved files: use temp file if available
     * For existing files with unsaved modifications: also use temp file */
    const char* fileToLoad = tab->filePath;
    BOOL loadFromTempFile = FALSE;
    
    if (tab->tempFilePath[0] != '\0') {
        /* Use temp file path for both new unsaved files and existing files with modifications */
        if (GetFileAttributes(tab->tempFilePath) != INVALID_FILE_ATTRIBUTES) {
            fileToLoad = tab->tempFilePath;
            loadFromTempFile = TRUE;
        }
    }
    
    if (!isNewFile || loadFromTempFile) {
        /* Load file content with encoding conversion */
        FILE* file = fopen(fileToLoad, "rb");
        if (file) {
            fseek(file, 0, SEEK_END);
            long fileSize = ftell(file);
            fseek(file, 0, SEEK_SET);
            
            char* buffer = (char*)malloc(fileSize + 1);
            if (buffer) {
                size_t bytesRead = fread(buffer, 1, fileSize, file);
                buffer[bytesRead] = '\0';
                
                /* Detect and convert encoding */
                BOOL hasBOM;
                int encoding = DetectFileEncodingFromData(buffer, bytesRead, &hasBOM);
                
                /* Explicit BOM checks */
                if (bytesRead >= 2 && (unsigned char)buffer[0] == 0xFF && (unsigned char)buffer[1] == 0xFE) {
                    encoding = ENCODING_UTF16_LE;
                } else if (bytesRead >= 2 && (unsigned char)buffer[0] == 0xFE && (unsigned char)buffer[1] == 0xFF) {
                    encoding = ENCODING_UTF16_BE;
                } else if (bytesRead >= 3 && (unsigned char)buffer[0] == 0xEF && 
                          (unsigned char)buffer[1] == 0xBB && (unsigned char)buffer[2] == 0xBF) {
                    encoding = ENCODING_UTF8_BOM;
                }
                
                if (encoding != ENCODING_UTF8) {
                    size_t utf8Size;
                    char* utf8Data = ConvertToUTF8(buffer, bytesRead, encoding, &utf8Size);
                    if (utf8Data) {
                        scintillaFunc(scintillaPtr, SCI_SETTEXT, 0, (sptr_t)utf8Data);
                        free(utf8Data);
                    } else {
                        scintillaFunc(scintillaPtr, SCI_SETTEXT, 0, (sptr_t)buffer);
                    }
                } else {
                    scintillaFunc(scintillaPtr, SCI_SETTEXT, 0, (sptr_t)buffer);
                }
                free(buffer);
            }
            fclose(file);
        }
        
        /* Note: Save point is set AFTER enabling change history below */
    } else {
        /* Note: Save point is set AFTER enabling change history below */
    }
    
    /* Enable change history FIRST - this clears undo buffer and enables tracking */
    if (tab->changeHistoryEnabled) {
        EnableChangeHistory(tab->editorHandle, TRUE);
    }
    
    /* Set save point AFTER enabling change history (for non-modified files)
     * This must be AFTER EnableChangeHistory because that function calls
     * SCI_EMPTYUNDOBUFFER which would reset any prior save point.
     * The save point tells change history what the "clean" baseline is. */
    if (!isNewFile && !loadFromTempFile) {
        scintillaFunc(scintillaPtr, SCI_SETSAVEPOINT, 0, 0);
    } else if (isNewFile && !loadFromTempFile) {
        /* New file without temp file - mark as clean */
        scintillaFunc(scintillaPtr, SCI_SETSAVEPOINT, 0, 0);
    }
    /* Note: Files loaded from temp file intentionally don't get SETSAVEPOINT
     * so they appear as modified with change history markers */
    
    /* Apply current theme to the editor FIRST (this clears styles) */
    ApplyThemeToEditor(tab->editorHandle);
    
    /* Apply syntax highlighting AFTER theme (so it doesn't get overwritten) */
    BOOL isExistingFile = (strncmp(tab->filePath, "New ", 4) != 0);
    if (isExistingFile) {
        ApplySyntaxHighlightingForFile(tab->editorHandle, tab->filePath);
        
        /* Update file type in status bar using new language detection */
        UpdateFileType(GetFileTypeFromPath(tab->filePath));
    } else {
        /* Update file type in status bar for new files */
        UpdateFileType("Text");
    }
    
    /* Always apply session state - zoom level 0 is valid (100% zoom) */
    /* If no session data was set, these will just be 0 which are valid defaults */
    scintillaFunc(scintillaPtr, SCI_SETZOOM, tab->sessionZoomLevel, 0);
    scintillaFunc(scintillaPtr, SCI_GOTOPOS, tab->sessionCursorPos, 0);
    scintillaFunc(scintillaPtr, SCI_SETFIRSTVISIBLELINE, tab->sessionFirstLine, 0);
    /* Force scrollbar refresh by invalidating the editor window */
    InvalidateRect(tab->editorHandle, NULL, FALSE);
    /* Also update the scrollbar directly via Windows API */
    int lineCount = (int)scintillaFunc(scintillaPtr, SCI_GETLINECOUNT, 0, 0);
    int firstVisible = tab->sessionFirstLine;
    int linesOnScreen = (int)scintillaFunc(scintillaPtr, SCI_LINESONSCREEN, 0, 0);
    SCROLLINFO si = {0};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_POS | SIF_PAGE;
    si.nMin = 0;
    si.nMax = lineCount - 1;
    si.nPos = firstVisible;
    si.nPage = linesOnScreen;
    SetScrollInfo(tab->editorHandle, SB_VERT, &si, TRUE);
    
    /* Mark as loaded */
    tab->isLoaded = TRUE;
    
    /* Initially hide the editor */
    ShowWindow(tab->editorHandle, SW_HIDE);
    
    /* Trigger a window resize to position the editor correctly */
    HandleWindowResize(0, 0);
    
    return TRUE;
}

/* Check if tab content is loaded */
BOOL IsTabLoaded(int index)
{
    if (index < 0 || index >= g_tabControl.tabCount) {
        return FALSE;
    }
    
    return g_tabControl.tabs[index].isLoaded;
}

/* Close tab with confirmation if needed */
BOOL CloseTabWithConfirmation(int index)
{
    if (index < 0 || index >= g_tabControl.tabCount) {
        return FALSE;
    }
    
    TabInfo* tab = &g_tabControl.tabs[index];
    
    /* Check if file is modified */
    if (tab->isModified) {
        /* Don't show dialog for new/unsaved files - they will be auto-saved in session */
        if (strncmp(tab->filePath, "New ", 4) != 0) {
            /* Only show save dialog for existing files */
            char message[MAX_PATH + 100];
            sprintf(message, "Do you want to save changes to %s?", tab->displayName);
            
            int result = MessageBox(g_tabControl.parentWindow, message, "Notepad+",
                                   MB_YESNOCANCEL | MB_ICONQUESTION);
            
            if (result == IDCANCEL) {
                return FALSE;  /* User cancelled */
            } else if (result == IDYES) {
                /* Save the file */
                if (!SaveTabToFile(index)) {
                    return FALSE;  /* Save failed */
                }
            }
            /* IDNO: Don't save, just close */
        }
        /* For new files (New 1, New 2, etc.), just close silently */
    }
    
    /* Proceed with closing */
    return CloseTab(index);
}

/* Close tab without confirmation */
BOOL CloseTab(int index)
{
    if (index < 0 || index >= g_tabControl.tabCount) {
        return FALSE;
    }
    
    TabInfo* tab = &g_tabControl.tabs[index];
    BOOL wasSelected = (g_tabControl.selectedIndex == index);
    
    /* Destroy editor window */
    if (tab->editorHandle) {
        DestroyWindow(tab->editorHandle);
    }
    
    /* Adjust selected index if needed */
    int newSelectedIndex = -1;
    if (wasSelected) {
        if (g_tabControl.tabCount > 1) {
            if (index > 0) {
                newSelectedIndex = index - 1;
            } else {
                newSelectedIndex = 0;
            }
        }
    } else if (g_tabControl.selectedIndex > index) {
        newSelectedIndex = g_tabControl.selectedIndex - 1;
    } else {
        newSelectedIndex = g_tabControl.selectedIndex;
    }
    
    /* Shift remaining tabs */
    for (int i = index; i < g_tabControl.tabCount - 1; i++) {
        g_tabControl.tabs[i] = g_tabControl.tabs[i + 1];
    }
    
    /* Decrease tab count */
    g_tabControl.tabCount--;
    
    /* Update selected index and show the new selected tab's editor */
    g_tabControl.selectedIndex = -1; /* Reset first to avoid issues in SelectTab */
    if (newSelectedIndex >= 0 && newSelectedIndex < g_tabControl.tabCount) {
        SelectTab(newSelectedIndex);
    }
    
    /* Update layout */
    UpdateTabLayout();
    
    return TRUE;
}

/* Close all tabs */
BOOL CloseAllTabs(void)
{
    while (g_tabControl.tabCount > 0) {
        if (!CloseTabWithConfirmation(g_tabControl.tabCount - 1)) {
            return FALSE;  /* User cancelled closing a tab */
        }
    }
    return TRUE;
}

/* Close all tabs except one */
BOOL CloseAllTabsExcept(int exceptIndex)
{
    if (exceptIndex < 0 || exceptIndex >= g_tabControl.tabCount) {
        return FALSE;
    }
    
    /* Close tabs from the end to avoid index shifting issues */
    for (int i = g_tabControl.tabCount - 1; i >= 0; i--) {
        if (i != exceptIndex) {
            if (!CloseTabWithConfirmation(i)) {
                return FALSE;  /* User cancelled */
            }
        }
    }
    
    return TRUE;
}

/* Select tab */
void SelectTab(int index)
{
    if (index < 0 || index >= g_tabControl.tabCount) {
        return;
    }
    
    TABS_PROFILE_MARK("SelectTab: enter");
    
    /* Hide current editor */
    if (g_tabControl.selectedIndex >= 0 && g_tabControl.selectedIndex < g_tabControl.tabCount) {
        TabInfo* currentTab = &g_tabControl.tabs[g_tabControl.selectedIndex];
        if (currentTab->editorHandle) {
            ShowWindow(currentTab->editorHandle, SW_HIDE);
        }
        currentTab->state = TAB_STATE_NORMAL;
    }
    
    TABS_PROFILE_MARK("SelectTab: after hide current");
    
    /* Load tab content if not loaded (lazy loading) */
    if (!g_tabControl.tabs[index].isLoaded) {
        if (!LoadTabContent(index)) {
            /* Failed to load content - should not happen normally */
            return;
        }
    }
    
    TABS_PROFILE_MARK("SelectTab: after lazy load check");
    
    /* Show new editor */
    g_tabControl.selectedIndex = index;
    g_tabControl.tabs[index].state = TAB_STATE_SELECTED;
    ShowWindow(g_tabControl.tabs[index].editorHandle, SW_SHOW);
    
    TABS_PROFILE_MARK("SelectTab: after show new");
    
    /* FIX: Force scrollbar recalculation by triggering a resize
     * When an editor was hidden (not active tab), scrollbars may not be calculated
     * properly until content changes or window resizes. We force a resize here
     * to ensure scrollbars appear correctly for content that needs them.
     * SKIP during startup - will be done once at the end in PolishAllTabs() */
    if (!IsDeferredLoadingMode()) {
        HandleWindowResize(0, 0);
        
        /* CRITICAL FIX: Force the editor to recalculate scrollbars by invalidating it.
         * This is needed because HandleWindowResize may use SWP_NOREDRAW during startup,
         * which prevents Scintilla from calculating scrollbars properly. */
        InvalidateRect(g_tabControl.tabs[index].editorHandle, NULL, FALSE);
        
        /* Additional fix: Force Scintilla to recalculate scroll width for horizontal scrollbar */
        SciFnDirect scintillaFunc = (SciFnDirect)SendMessage(g_tabControl.tabs[index].editorHandle, SCI_GETDIRECTFUNCTION, 0, 0);
        sptr_t scintillaPtr = SendMessage(g_tabControl.tabs[index].editorHandle, SCI_GETDIRECTPOINTER, 0, 0);
        if (scintillaFunc && scintillaPtr) {
            /* Toggle scroll width tracking to force recalculation */
            scintillaFunc(scintillaPtr, SCI_SETSCROLLWIDTHTRACKING, 0, 0);
            scintillaFunc(scintillaPtr, SCI_SETSCROLLWIDTHTRACKING, 1, 0);
            
            /* Also force a scrollbar refresh by sending WM_SIZE to the editor */
            RECT rc;
            GetClientRect(g_tabControl.tabs[index].editorHandle, &rc);
            SendMessage(g_tabControl.tabs[index].editorHandle, WM_SIZE, 0, 
                       MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));
        }
    }
    
    /* Make sure tab is visible */
    EnsureTabVisible(index);
    
    /* Bring editor to front and set focus */
    SetWindowPos(g_tabControl.tabs[index].editorHandle, HWND_TOP, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE);
    SetFocus(g_tabControl.tabs[index].editorHandle);
    
    /* Update status bar with current tab's zoom level and cursor position */
    TabInfo* tab = &g_tabControl.tabs[index];
    if (tab->editorHandle) {
        int zoomLevel = (int)SendMessage(tab->editorHandle, SCI_GETZOOM, 0, 0);
        int pos = (int)SendMessage(tab->editorHandle, SCI_GETCURRENTPOS, 0, 0);
        int line = (int)SendMessage(tab->editorHandle, SCI_LINEFROMPOSITION, pos, 0);
        int lineStart = (int)SendMessage(tab->editorHandle, SCI_POSITIONFROMLINE, line, 0);
        int col = pos - lineStart;
        
        UpdateZoomLevel(zoomLevel);
        UpdateCursorPosition(line, col);
        UpdateFilePosition(pos);
    }
    
    /* Update Save button state based on tab's modified state */
    EnableToolbarButton(ID_FILE_SAVE, tab->isModified);
    
    /* Update code folding toolbar button state based on current tab */
    SetToolbarButtonToggled(ID_VIEW_CODEFOLDING, tab->codeFoldingEnabled);
    SetToolbarButtonToggled(ID_VIEW_CHANGEHISTORY, tab->changeHistoryEnabled);
    
    /* Update Word Wrap, Line Numbers, and Whitespace toolbar button states */
    SetToolbarButtonToggled(ID_VIEW_WORD_WRAP, tab->wordWrap);
    SetToolbarButtonToggled(ID_VIEW_LINE_NUMBERS, tab->showLineNumbers);
    SetToolbarButtonToggled(ID_VIEW_WHITESPACE, tab->showWhitespace);
    
    /* Update file type in status bar based on current tab */
    LanguageType lang = DetectLanguage(tab->filePath);
    UpdateFileType(GetLanguageName(lang));
    
    /* Update window title with current file path */
    extern void UpdateWindowTitle(const char* filePath);
    UpdateWindowTitle(tab->filePath);
    
    /* Enable/disable "Open Folder" button based on whether file is saved */
    /* Button is enabled only for existing files (not new unsaved files) */
    BOOL isExistingFile = (strncmp(tab->filePath, "New ", 4) != 0);
    EnableToolbarButton(ID_FILE_OPENFOLDER, isExistingFile);
    
    /* Update layout */
    InvalidateAllTabs();
}

/* Get selected tab index */
int GetSelectedTab(void)
{
    return g_tabControl.selectedIndex;
}

/* Get tab count */
int GetTabCount(void)
{
    return g_tabControl.tabCount;
}

/* Get tab information */
TabInfo* GetTab(int index)
{
    if (index < 0 || index >= g_tabControl.tabCount) {
        return NULL;
    }
    return &g_tabControl.tabs[index];
}

/* Get tab info */
BOOL GetTabInfo(int index, char** filePath, BOOL* isModified)
{
    if (index < 0 || index >= g_tabControl.tabCount) {
        return FALSE;
    }
    
    TabInfo* tab = &g_tabControl.tabs[index];
    if (filePath) *filePath = tab->filePath;
    if (isModified) *isModified = tab->isModified;
    return TRUE;
}

/* Set tab modified state */
BOOL SetTabModified(int index, BOOL modified)
{
    if (index < 0 || index >= g_tabControl.tabCount) {
        return FALSE;
    }
    
    TabInfo* tab = &g_tabControl.tabs[index];
    if (tab->isModified != modified) {
        tab->isModified = modified;
        UpdateTabDisplayName(index);
        InvalidateTab(index);
    }
    
    return TRUE;
}

/* Update tab display name */
BOOL UpdateTabDisplayName(int index)
{
    if (index < 0 || index >= g_tabControl.tabCount) {
        return FALSE;
    }
    
    TabInfo* tab = &g_tabControl.tabs[index];
    
    if (strncmp(tab->filePath, "New ", 4) == 0) {
        /* New file */
        strcpy(tab->displayName, tab->filePath);
    } else {
        /* Existing file */
        strcpy(tab->displayName, GetShortDisplayName(tab->filePath));
    }
    
    /* Add asterisk if modified */
    if (tab->isModified) {
        strcat(tab->displayName, "*");
    }
    
    InvalidateTab(index);
    return TRUE;
}

/* Resize tabs */
void ResizeTabs(int width, int height)
{
    UNREFERENCED_PARAMETER(height);
    
    SetWindowPos(g_tabControl.hwnd, NULL, 0, 0, width, TAB_HEIGHT,
                SWP_NOZORDER | SWP_NOACTIVATE);
    
    UpdateTabLayout();
}

/* Update tab layout */
void UpdateTabLayout(void)
{
    CalculateTabPositions();
    InvalidateAllTabs();
}

/* Calculate tab positions */
static void CalculateTabPositions(void)
{
    if (g_tabControl.tabCount == 0) {
        return;
    }
    
    RECT rc;
    GetClientRect(g_tabControl.hwnd, &rc);
    
    int x = g_tabControl.scrollOffset;
    int y = 2;
    
    /* Calculate positions for regular tabs */
    for (int i = 0; i < g_tabControl.tabCount; i++) {
        TabInfo* tab = &g_tabControl.tabs[i];
        
        int textWidth, textHeight;
        GetTabTextDimensions(tab->displayName, &textWidth, &textHeight, i == g_tabControl.selectedIndex);
        
        tab->width = textWidth + TAB_PADDING * 2 + CLOSE_BUTTON_SIZE + 4;
        if (tab->width < MIN_TAB_WIDTH) {
            tab->width = MIN_TAB_WIDTH;
        }
        if (tab->width > MAX_TAB_WIDTH) {
            tab->width = MAX_TAB_WIDTH;
        }
        
        tab->height = TAB_HEIGHT - 4;
        tab->x = x;
        tab->y = y;
        
        /* Calculate close button position */
        tab->closeButtonX = x + tab->width - CLOSE_BUTTON_SIZE - 6;
        tab->closeButtonY = y + (tab->height - CLOSE_BUTTON_SIZE) / 2;
        
        x += tab->width;
    }
    
    /* Calculate add button position */
    if (g_tabControl.showAddButton) {
        g_tabControl.addTabX = x + 4;
        g_tabControl.addTabY = y + (TAB_HEIGHT - 4 - 16) / 2;
    }
    
    /* Adjust scroll if needed */
    AdjustScrollButtons();
}

/* Ensure tab is visible */
static void EnsureTabVisible(int index)
{
    if (index < 0 || index >= g_tabControl.tabCount) {
        return;
    }
    
    RECT rc;
    GetClientRect(g_tabControl.hwnd, &rc);
    int clientWidth = rc.right - rc.left;
    
    TabInfo* tab = &g_tabControl.tabs[index];
    int tabRight = tab->x + tab->width - g_tabControl.scrollOffset;
    
    if (tab->x - g_tabControl.scrollOffset < 0) {
        /* Tab is to the left of visible area */
        g_tabControl.scrollOffset = tab->x;
    } else if (tabRight > clientWidth) {
        /* Tab is to the right of visible area */
        g_tabControl.scrollOffset = tab->x + tab->width - clientWidth;
        if (g_tabControl.scrollOffset < 0) {
            g_tabControl.scrollOffset = 0;
        }
    }
    
    UpdateTabLayout();
}

/* Adjust scroll buttons visibility */
static void AdjustScrollButtons(void)
{
    /* TODO: Implement scroll left/right buttons if needed */
}

/* Invalidate specific tab */
static void InvalidateTab(int index)
{
    if (index < 0 || index >= g_tabControl.tabCount) {
        return;
    }
    
    TabInfo* tab = &g_tabControl.tabs[index];
    RECT rc = {tab->x, tab->y, tab->x + tab->width, tab->y + tab->height};
    InvalidateRect(g_tabControl.hwnd, &rc, TRUE);
}

/* Invalidate all tabs */
static void InvalidateAllTabs(void)
{
    InvalidateRect(g_tabControl.hwnd, NULL, TRUE);
}

/* Hit test for tab */
int HitTestTab(int x, int y)
{
    if (g_tabControl.tabCount == 0) {
        return -1;
    }
    
    for (int i = 0; i < g_tabControl.tabCount; i++) {
        TabInfo* tab = &g_tabControl.tabs[i];
        if (x >= tab->x && x < tab->x + tab->width &&
            y >= tab->y && y < tab->y + tab->height) {
            return i;
        }
    }
    
    return -1;
}

/* Hit test for close button */
BOOL HitTestCloseButton(int tabIndex, int x, int y)
{
    if (tabIndex < 0 || tabIndex >= g_tabControl.tabCount) {
        return FALSE;
    }
    
    TabInfo* tab = &g_tabControl.tabs[tabIndex];
    
    /* Pinned tabs don't have a close button */
    if (tab->isPinned) {
        return FALSE;
    }
    
    return (x >= tab->closeButtonX && x < tab->closeButtonX + CLOSE_BUTTON_SIZE &&
            y >= tab->closeButtonY && y < tab->closeButtonY + CLOSE_BUTTON_SIZE);
}

/* Handle tab click */
void HandleTabClick(int x, int y)
{
    int tabIndex = HitTestTab(x, y);
    if (tabIndex >= 0) {
        if (HitTestCloseButton(tabIndex, x, y)) {
            CloseTabWithConfirmation(tabIndex);
        } else {
            SelectTab(tabIndex);
        }
    } else if (g_tabControl.showAddButton) {
        /* Check if add button was clicked */
        if (x >= g_tabControl.addTabX && x < g_tabControl.addTabX + 16 &&
            y >= g_tabControl.addTabY && y < g_tabControl.addTabY + 16) {
            AddNewTab(NULL);
        }
    }
}

/* Handle tab mouse down */
void HandleTabMouseDown(int x, int y)
{
    int tabIndex = HitTestTab(x, y);
    if (tabIndex >= 0) {
        if (HitTestCloseButton(tabIndex, x, y)) {
            g_tabControl.closeHoveredTab = tabIndex;
        } else {
            SelectTab(tabIndex);
        }
        InvalidateTab(tabIndex);
    }
}

/* Handle tab mouse up */
void HandleTabMouseUp(int x, int y)
{
    if (g_tabControl.closeHoveredTab >= 0) {
        int tabIndex = g_tabControl.closeHoveredTab;
        g_tabControl.closeHoveredTab = -1;
        
        if (HitTestCloseButton(tabIndex, x, y)) {
            CloseTabWithConfirmation(tabIndex);
        } else {
            InvalidateTab(tabIndex);
        }
    } else {
        /* Check if add button was clicked */
        if (g_tabControl.showAddButton) {
            int addX = g_tabControl.addTabX - g_tabControl.scrollOffset;
            int addY = g_tabControl.addTabY;
            if (x >= addX && x < addX + 16 &&
                y >= addY && y < addY + 16) {
                AddNewTab(NULL);
                /* Trigger a window resize to position the new editor */
                HandleWindowResize(0, 0);
            }
        }
    }
}

/* Handle tab mouse move */
void HandleTabMouseMove(int x, int y)
{
    int newHoveredTab = HitTestTab(x, y);
    BOOL newCloseHovered = (newHoveredTab >= 0) ? HitTestCloseButton(newHoveredTab, x, y) : FALSE;
    
    /* Update cursor */
    if (newCloseHovered) {
        SetCursor(g_tabControl.handCursor);
    } else {
        SetCursor(g_tabControl.arrowCursor);
    }
    
    /* Update hovered states */
    if (newHoveredTab != g_tabControl.hoveredTab || 
        (newHoveredTab >= 0 && newCloseHovered != g_tabControl.tabs[newHoveredTab].isCloseHovered)) {
        
        /* Clear previous hover states */
        if (g_tabControl.hoveredTab >= 0 && g_tabControl.hoveredTab < g_tabControl.tabCount) {
            g_tabControl.tabs[g_tabControl.hoveredTab].state = 
                (g_tabControl.hoveredTab == g_tabControl.selectedIndex) ? TAB_STATE_SELECTED : TAB_STATE_NORMAL;
            g_tabControl.tabs[g_tabControl.hoveredTab].isCloseHovered = FALSE;
            InvalidateTab(g_tabControl.hoveredTab);
        }
        
        g_tabControl.hoveredTab = newHoveredTab;
        
        /* Set new hover states */
        if (newHoveredTab >= 0 && newHoveredTab < g_tabControl.tabCount) {
            /* Preserve SELECTED state for active tab to avoid text position shift */
            if (newHoveredTab != g_tabControl.selectedIndex) {
                g_tabControl.tabs[newHoveredTab].state = TAB_STATE_HOVER;
            }
            g_tabControl.tabs[newHoveredTab].isCloseHovered = newCloseHovered;
            InvalidateTab(newHoveredTab);
            
            /* Update tooltip */
            UpdateTooltip(newHoveredTab);
        } else {
            HideTooltip();
        }
    }
}

/* Handle tab mouse leave */
void HandleTabMouseLeave(void)
{
    if (g_tabControl.hoveredTab >= 0) {
        int prevHovered = g_tabControl.hoveredTab;
        g_tabControl.hoveredTab = -1;
        
        if (prevHovered < g_tabControl.tabCount) {
            g_tabControl.tabs[prevHovered].state =
                (prevHovered == g_tabControl.selectedIndex) ? TAB_STATE_SELECTED : TAB_STATE_NORMAL;
            g_tabControl.tabs[prevHovered].isCloseHovered = FALSE;
            InvalidateTab(prevHovered);
        }
    }
    
    HideTooltip();
}

/* Update tooltip */
void UpdateTooltip(int tabIndex)
{
    if (!g_tabControl.tooltipWindow || tabIndex < 0 || tabIndex >= g_tabControl.tabCount) {
        HideTooltip();
        return;
    }
    
    TabInfo* tab = &g_tabControl.tabs[tabIndex];
    char tooltipText[MAX_PATH * 2];
    
    /* Create tooltip text */
    sprintf(tooltipText, "File: %s\n%s", 
            tab->filePath, 
            tab->isModified ? "Modified" : "Saved");
    
    ShowTooltip(tab->x + tab->width / 2, tab->y + tab->height / 2, tooltipText);
}

/* Show tooltip */
void ShowTooltip(int x, int y, const char* text)
{
    UNREFERENCED_PARAMETER(x);
    UNREFERENCED_PARAMETER(y);
    
    if (!g_tabControl.tooltipWindow) {
        return;
    }
    
    TOOLINFO ti = {0};
    ti.cbSize = sizeof(TOOLINFO);
    ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    ti.hwnd = g_tabControl.hwnd;
    ti.uId = (UINT_PTR)g_tabControl.hwnd;
    ti.lpszText = (char*)text;
    
    SendMessage(g_tabControl.tooltipWindow, TTM_ADDTOOL, 0, (LPARAM)&ti);
    SendMessage(g_tabControl.tooltipWindow, TTM_UPDATE, 0, 0);
}

/* Hide tooltip */
void HideTooltip(void)
{
    if (!g_tabControl.tooltipWindow) {
        return;
    }
    
    SendMessage(g_tabControl.tooltipWindow, TTM_DELTOOL, 0, 0);
}

/* Get tab text dimensions */
void GetTabTextDimensions(const char* text, int* width, int* height, BOOL isBold)
{
    HDC hdc = GetDC(g_tabControl.hwnd);
    HFONT oldFont = SelectObject(hdc, isBold ? g_tabControl.boldFont : g_tabControl.normalFont);
    
    SIZE size;
    GetTextExtentPoint32(hdc, text, strlen(text), &size);
    
    SelectObject(hdc, oldFont);
    ReleaseDC(g_tabControl.hwnd, hdc);
    
    if (width) *width = size.cx;
    if (height) *height = size.cy;
}

/* Get short display name from file path */
char* GetShortDisplayName(const char* filePath)
{
    static char displayName[MAX_PATH];
    char* fileName;
    
    /* Extract filename from path */
    fileName = strrchr(filePath, '\\');
    if (fileName) {
        fileName++;
    } else {
        fileName = (char*)filePath;
    }
    
    strncpy(displayName, fileName, MAX_PATH - 1);
    displayName[MAX_PATH - 1] = '\0';
    
    return displayName;
}

/* Tab control window procedure */
LRESULT CALLBACK TabWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    TabControl* tc = (TabControl*)GetWindowLongPtr(hwnd, 0);
    
    switch (uMsg) {
        case WM_CREATE:
            return 0;
            
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                RECT rc;
                GetClientRect(hwnd, &rc);
                
                /* Get theme colors for consistent styling */
                ThemeColors* colors = GetThemeColors();
                
                /* Fill background with theme color */
                HBRUSH bgBrush = CreateSolidBrush(colors->toolbarBg);
                FillRect(hdc, &rc, bgBrush);
                DeleteObject(bgBrush);
                
                /* Draw subtle separator line at bottom using theme color */
                HPEN sepPen = CreatePen(PS_SOLID, 1, colors->tabNormalBorder);
                HPEN oldPen = SelectObject(hdc, sepPen);
                MoveToEx(hdc, 0, rc.bottom - 1, NULL);
                LineTo(hdc, rc.right, rc.bottom - 1);
                SelectObject(hdc, oldPen);
                DeleteObject(sepPen);
                
                /* Draw tabs */
                for (int i = 0; i < tc->tabCount; i++) {
                    TabInfo* tab = &tc->tabs[i];
                    
                    /* Skip if tab is scrolled out of view */
                    if (tab->x + tab->width - tc->scrollOffset < 0 ||
                        tab->x - tc->scrollOffset > rc.right) {
                        continue;
                    }
                    
                    int tabX = tab->x - tc->scrollOffset;
                    int tabY = tab->y;
                    
                    /* Determine tab colors from theme with Fluent-style distinction */
                    COLORREF bgColor, textColor, accentColor;
                    BOOL drawAccent = FALSE;
                    int extraTopMargin = 0;
                    int cornerRadius = 4;  /* Fluent-style rounded corners */
                    
                    if (tab->state == TAB_STATE_SELECTED) {
                        /* Active tab: elevated appearance with accent indicator */
                        bgColor = colors->tabSelectedBg;
                        textColor = colors->tabSelectedFg;
                        accentColor = RGB(0, 103, 192);  /* Windows 11 blue accent */
                        drawAccent = TRUE;
                        extraTopMargin = -2;  /* Make active tab slightly taller */
                    } else if (tab->state == TAB_STATE_HOVER) {
                        /* Hover state: subtle highlight */
                        bgColor = colors->tabHoverBg;
                        textColor = colors->tabHoverFg;
                    } else {
                        /* Inactive tabs: blend with background */
                        bgColor = colors->tabNormalBg;
                        textColor = colors->tabNormalFg;
                    }
                    
                    /* Adjust tab rectangle for active tab height */
                    int adjustedTabY = tabY + extraTopMargin;
                    int adjustedHeight = tab->height - extraTopMargin;
                    
                    /* Create rounded rectangle region for Fluent-style corners */
                    HRGN tabRegion = CreateRoundRectRgn(
                        tabX, adjustedTabY,
                        tabX + tab->width + 1, adjustedTabY + adjustedHeight + cornerRadius,
                        cornerRadius * 2, cornerRadius * 2
                    );
                    
                    /* Draw rounded tab background */
                    HBRUSH tabBrush = CreateSolidBrush(bgColor);
                    FillRgn(hdc, tabRegion, tabBrush);
                    DeleteObject(tabBrush);
                    
                    /* Draw border for all tabs - subtle for inactive, more visible for selected */
                    {
                        COLORREF borderColor;
                        if (tab->state == TAB_STATE_SELECTED) {
                            borderColor = colors->tabSelectedBorder;
                        } else {
                            /* Subtle border for inactive tabs to show separation */
                            borderColor = colors->tabNormalBorder;
                        }
                        HPEN borderPen = CreatePen(PS_SOLID, 1, borderColor);
                        SelectObject(hdc, borderPen);
                        SelectObject(hdc, GetStockObject(NULL_BRUSH));
                        RoundRect(hdc, tabX, adjustedTabY, 
                                  tabX + tab->width, adjustedTabY + adjustedHeight + cornerRadius,
                                  cornerRadius * 2, cornerRadius * 2);
                        DeleteObject(borderPen);
                    }
                    
                    DeleteObject(tabRegion);
                    
                    /* Draw accent indicator for active tab (TOP edge style - Fluent Design) */
                    if (drawAccent) {
                        HBRUSH accentBrush = CreateSolidBrush(accentColor);
                        RECT accentRect = {
                            tabX + 8,
                            adjustedTabY,
                            tabX + tab->width - 8,
                            adjustedTabY + 3
                        };
                        /* Rounded accent bar at TOP */
                        HRGN accentRgn = CreateRoundRectRgn(
                            accentRect.left, accentRect.top,
                            accentRect.right + 1, accentRect.bottom + 1,
                            3, 3
                        );
                        FillRgn(hdc, accentRgn, accentBrush);
                        DeleteObject(accentRgn);
                        DeleteObject(accentBrush);
                    }
                    
                    /* Draw tab text */
                    HFONT font = (tab->state == TAB_STATE_SELECTED) ? tc->boldFont : tc->normalFont;
                    HFONT oldFont = SelectObject(hdc, font);
                    SetTextColor(hdc, textColor);
                    SetBkMode(hdc, TRANSPARENT);
                    
                    /* Center text vertically with slight offset for accent */
                    int textTopMargin = 2;
                    RECT textRect = {
                        tabX + TAB_PADDING,
                        adjustedTabY + textTopMargin,
                        tabX + tab->width - TAB_PADDING - CLOSE_BUTTON_SIZE - 4,
                        adjustedTabY + adjustedHeight - (drawAccent ? 5 : 2)
                    };
                    DrawText(hdc, tab->displayName, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
                    
                    SelectObject(hdc, oldFont);
                    
                    /* Draw close button or pin indicator - adjust position for active tab */
                    int closeX = tab->closeButtonX - tc->scrollOffset;
                    int closeY = tab->closeButtonY + extraTopMargin;
                    if (tab->isPinned) {
                        /* Draw pin indicator instead of close button */
                        DrawPinIndicator(hdc, closeX, closeY, tab->state == TAB_STATE_SELECTED);
                    } else {
                        /* Draw close button with theme colors */
                        DrawCloseButton(hdc, closeX, closeY, tab->isCloseHovered, tab->state == TAB_STATE_SELECTED);
                    }
                }
                
                /* Draw add button with theme colors */
                if (tc->showAddButton) {
                    int addX = tc->addTabX - tc->scrollOffset;
                    int addY = tc->addTabY;
                    DrawAddButton(hdc, addX, addY, FALSE);
                }
                
                EndPaint(hwnd, &ps);
                return 0;
            }
            
        case WM_LBUTTONDOWN:
            {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                HandleTabMouseDown(x, y);
                SetCapture(hwnd);
                return 0;
            }
            
        case WM_LBUTTONUP:
            {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                HandleTabMouseUp(x, y);
                ReleaseCapture();
                return 0;
            }
            
        case WM_MOUSEMOVE:
            {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                HandleTabMouseMove(x, y);
                return 0;
            }
            
        case WM_MOUSELEAVE:
            HandleTabMouseLeave();
            return 0;
            
        case WM_RBUTTONDOWN:
            {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                int tabIndex = HitTestTab(x, y);
                if (tabIndex >= 0) {
                    /* Select the tab first */
                    SelectTab(tabIndex);
                    /* Show context menu */
                    ShowTabContextMenu(tabIndex, x, y);
                }
                return 0;
            }
            
        case WM_SIZE:
            UpdateTabLayout();
            return 0;
            
        case WM_ERASEBKGND:
            return 1;  /* Handle in WM_PAINT */
            
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

/* Draw close button with theme-aware colors */
void DrawCloseButton(HDC hdc, int x, int y, BOOL isHovered, BOOL isActiveTab)
{
    ThemeColors* colors = GetThemeColors();
    COLORREF normalColor = colors->tabNormalFg;
    COLORREF hoverColor = RGB(196, 43, 28);  /* Windows 11 close button red */
    
    COLORREF color;
    if (isHovered) {
        color = hoverColor;
    } else if (isActiveTab) {
        color = colors->tabSelectedFg;
    } else {
        color = normalColor;
    }
    
    /* Draw hover background circle for close button */
    if (isHovered) {
        HBRUSH hoverBrush = CreateSolidBrush(RGB(232, 17, 35));  /* Fluent red */
        HRGN hoverRgn = CreateEllipticRgn(x, y, x + CLOSE_BUTTON_SIZE, y + CLOSE_BUTTON_SIZE);
        FillRgn(hdc, hoverRgn, hoverBrush);
        DeleteObject(hoverRgn);
        DeleteObject(hoverBrush);
        color = RGB(255, 255, 255);  /* White X on red background */
    }
    
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HPEN oldPen = SelectObject(hdc, pen);
    
    /* Draw X with cleaner lines */
    int margin = 4;
    MoveToEx(hdc, x + margin, y + margin, NULL);
    LineTo(hdc, x + CLOSE_BUTTON_SIZE - margin, y + CLOSE_BUTTON_SIZE - margin);
    
    MoveToEx(hdc, x + CLOSE_BUTTON_SIZE - margin, y + margin, NULL);
    LineTo(hdc, x + margin, y + CLOSE_BUTTON_SIZE - margin);
    
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

/* Draw add button with theme-aware colors */
void DrawAddButton(HDC hdc, int x, int y, BOOL isHovered)
{
    ThemeColors* colors = GetThemeColors();
    COLORREF normalColor = colors->tabNormalFg;
    COLORREF hoverColor = RGB(0, 103, 192);  /* Windows 11 accent blue */
    
    COLORREF color = isHovered ? hoverColor : normalColor;
    
    /* Draw hover background circle for add button */
    if (isHovered) {
        HBRUSH hoverBrush = CreateSolidBrush(colors->tabHoverBg);
        HRGN hoverRgn = CreateEllipticRgn(x, y, x + 16, y + 16);
        FillRgn(hdc, hoverRgn, hoverBrush);
        DeleteObject(hoverRgn);
        DeleteObject(hoverBrush);
    }
    
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HPEN oldPen = SelectObject(hdc, pen);
    
    /* Draw + with cleaner lines */
    int margin = 4;
    /* Vertical line */
    MoveToEx(hdc, x + 8, y + margin, NULL);
    LineTo(hdc, x + 8, y + 16 - margin);
    
    /* Horizontal line */
    MoveToEx(hdc, x + margin, y + 8, NULL);
    LineTo(hdc, x + 16 - margin, y + 8);
    
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

/* Draw pin indicator for pinned tabs */
static void DrawPinIndicator(HDC hdc, int x, int y, BOOL isActiveTab)
{
    ThemeColors* colors = GetThemeColors();
    COLORREF color = isActiveTab ? RGB(0, 103, 192) : colors->tabNormalFg;
    
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HPEN oldPen = SelectObject(hdc, pen);
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = SelectObject(hdc, brush);
    
    /* Draw a small pin icon (circle with line) */
    int cx = x + CLOSE_BUTTON_SIZE / 2;
    int cy = y + CLOSE_BUTTON_SIZE / 2;
    Ellipse(hdc, cx - 3, cy - 3, cx + 3, cy + 3);
    
    SelectObject(hdc, oldBrush);
    DeleteObject(brush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

/* Pin a tab */
BOOL PinTab(int index)
{
    if (index < 0 || index >= g_tabControl.tabCount) {
        return FALSE;
    }
    
    TabInfo* tab = &g_tabControl.tabs[index];
    if (tab->isPinned) {
        return TRUE;  /* Already pinned */
    }
    
    tab->isPinned = TRUE;
    
    /* Sort to move pinned tab to the left */
    SortPinnedTabs();
    
    /* Update layout */
    UpdateTabLayout();
    
    return TRUE;
}

/* Unpin a tab */
BOOL UnpinTab(int index)
{
    if (index < 0 || index >= g_tabControl.tabCount) {
        return FALSE;
    }
    
    TabInfo* tab = &g_tabControl.tabs[index];
    if (!tab->isPinned) {
        return TRUE;  /* Already unpinned */
    }
    
    tab->isPinned = FALSE;
    
    /* Update layout (no need to sort, unpinned tabs stay in place) */
    UpdateTabLayout();
    
    return TRUE;
}

/* Check if a tab is pinned */
BOOL IsTabPinned(int index)
{
    if (index < 0 || index >= g_tabControl.tabCount) {
        return FALSE;
    }
    
    return g_tabControl.tabs[index].isPinned;
}

/* Sort tabs so pinned tabs are on the left */
void SortPinnedTabs(void)
{
    if (g_tabControl.tabCount <= 1) {
        return;
    }
    
    /* Simple bubble sort - move pinned tabs to the left */
    BOOL swapped;
    
    /* Remember which tab is selected by its editor handle */
    HWND selectedEditor = NULL;
    if (g_tabControl.selectedIndex >= 0 && g_tabControl.selectedIndex < g_tabControl.tabCount) {
        selectedEditor = g_tabControl.tabs[g_tabControl.selectedIndex].editorHandle;
    }
    
    do {
        swapped = FALSE;
        for (int i = 0; i < g_tabControl.tabCount - 1; i++) {
            /* If current tab is not pinned but next one is, swap them */
            if (!g_tabControl.tabs[i].isPinned && g_tabControl.tabs[i + 1].isPinned) {
                TabInfo temp = g_tabControl.tabs[i];
                g_tabControl.tabs[i] = g_tabControl.tabs[i + 1];
                g_tabControl.tabs[i + 1] = temp;
                swapped = TRUE;
            }
        }
    } while (swapped);
    
    /* Find the new index of the selected tab */
    if (selectedEditor) {
        for (int i = 0; i < g_tabControl.tabCount; i++) {
            if (g_tabControl.tabs[i].editorHandle == selectedEditor) {
                g_tabControl.selectedIndex = i;
                break;
            }
        }
    }
}

/* Apply session view settings to an already initialized tab */
void ApplySessionViewSettings(int tabIndex, const SessionTab* sessionTab)
{
    if (!sessionTab || tabIndex < 0 || tabIndex >= g_tabControl.tabCount) {
        return;
    }
    
    TabInfo* tab = GetTab(tabIndex);
    if (!tab) {
        return;
    }
    
    /* Store settings in tab fields first (even for placeholder tabs without editorHandle) */
    if (sessionTab->wordWrap == TRUE || sessionTab->wordWrap == FALSE) {
        tab->wordWrap = sessionTab->wordWrap;
    }
    
    if (sessionTab->showLineNumbers == TRUE || sessionTab->showLineNumbers == FALSE) {
        tab->showLineNumbers = sessionTab->showLineNumbers;
    }
    
    if (sessionTab->showWhitespace == TRUE || sessionTab->showWhitespace == FALSE) {
        tab->showWhitespace = sessionTab->showWhitespace;
    }
    
    if (sessionTab->autoIndent == TRUE || sessionTab->autoIndent == FALSE) {
        tab->autoIndent = sessionTab->autoIndent;
    }
    
    if (sessionTab->codeFoldingEnabled == TRUE || sessionTab->codeFoldingEnabled == FALSE) {
        tab->codeFoldingEnabled = sessionTab->codeFoldingEnabled;
    }
    
    if (sessionTab->changeHistoryEnabled == TRUE || sessionTab->changeHistoryEnabled == FALSE) {
        tab->changeHistoryEnabled = sessionTab->changeHistoryEnabled;
    }
    
    if (sessionTab->isSplitView == TRUE || sessionTab->isSplitView == FALSE) {
        tab->isSplitView = sessionTab->isSplitView;
    }
    
    /* Apply settings to editor only if editorHandle exists */
    if (!tab->editorHandle) {
        return;
    }
    
    /* Apply settings to the editor window */
    SendMessage(tab->editorHandle, SCI_SETWRAPMODE,
               tab->wordWrap ? SC_WRAP_WORD : SC_WRAP_NONE, 0);
    
    SendMessage(tab->editorHandle, SCI_SETMARGINWIDTHN, 0,
               tab->showLineNumbers ? 40 : 0);
    
    SendMessage(tab->editorHandle, SCI_SETVIEWWS,
               tab->showWhitespace ? SCWS_VISIBLEALWAYS : SCWS_INVISIBLE, 0);
    
    /* Apply code folding settings */
    SciFnDirect scintillaFunc = (SciFnDirect)SendMessage(tab->editorHandle, SCI_GETDIRECTFUNCTION, 0, 0);
    sptr_t scintillaPtr = SendMessage(tab->editorHandle, SCI_GETDIRECTPOINTER, 0, 0);
    if (scintillaFunc && scintillaPtr) {
        if (tab->codeFoldingEnabled) {
            scintillaFunc(scintillaPtr, SCI_SETMARGINTYPEN, 2, SC_MARGIN_SYMBOL);
            scintillaFunc(scintillaPtr, SCI_SETMARGINMASKN, 2, SC_MASK_FOLDERS);
            scintillaFunc(scintillaPtr, SCI_SETMARGINWIDTHN, 2, 16);
            scintillaFunc(scintillaPtr, SCI_SETMARGINSENSITIVEN, 2, 1);
        } else {
            scintillaFunc(scintillaPtr, SCI_SETMARGINWIDTHN, 2, 0);
        }
        
        /* Apply change history settings */
        EnableChangeHistory(tab->editorHandle, tab->changeHistoryEnabled);
    }
}

/* Show context menu for tab */
void ShowTabContextMenu(int tabIndex, int x, int y)
{
    if (tabIndex < 0 || tabIndex >= g_tabControl.tabCount) {
        return;
    }
    
    TabInfo* tab = &g_tabControl.tabs[tabIndex];
    HMENU hMenu = CreatePopupMenu();
    
    if (!hMenu) {
        return;
    }
    
    /* Add Pin/Unpin option */
    if (tab->isPinned) {
        AppendMenu(hMenu, MF_STRING, ID_TAB_UNPIN, "Unpin Tab");
    } else {
        AppendMenu(hMenu, MF_STRING, ID_TAB_PIN, "Pin Tab");
    }
    
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    
    /* Add Close option (disabled for pinned tabs) */
    if (tab->isPinned) {
        AppendMenu(hMenu, MF_STRING | MF_GRAYED, ID_TAB_CLOSE, "Close Tab");
    } else {
        AppendMenu(hMenu, MF_STRING, ID_TAB_CLOSE, "Close Tab");
    }
    
    AppendMenu(hMenu, MF_STRING, ID_TAB_CLOSEOTHERS, "Close Other Tabs");
    AppendMenu(hMenu, MF_STRING, ID_TAB_CLOSEALL, "Close All Tabs");
    
    /* Convert to screen coordinates */
    POINT pt = {x, y};
    ClientToScreen(g_tabControl.hwnd, &pt);
    
    /* Store the tab index for the command handler */
    /* We'll use a global or pass it through SetProp */
    SetProp(g_tabControl.hwnd, "ContextTabIndex", (HANDLE)(intptr_t)tabIndex);
    
    /* Show the menu */
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             pt.x, pt.y, 0, g_tabControl.hwnd, NULL);
    
    /* Handle the command */
    switch (cmd) {
        case ID_TAB_PIN:
            PinTab(tabIndex);
            break;
        case ID_TAB_UNPIN:
            UnpinTab(tabIndex);
            break;
        case ID_TAB_CLOSE:
            if (!tab->isPinned) {
                CloseTabWithConfirmation(tabIndex);
            }
            break;
        case ID_TAB_CLOSEOTHERS:
            CloseAllTabsExcept(tabIndex);
            break;
        case ID_TAB_CLOSEALL:
            CloseAllTabs();
            break;
    }
    
    RemoveProp(g_tabControl.hwnd, "ContextTabIndex");
    DestroyMenu(hMenu);
}
