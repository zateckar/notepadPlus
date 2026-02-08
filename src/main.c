/*
 * Notepad+ - A lightweight, fast text editor
 * Main application entry point
 */

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include <windowsx.h>

/* Startup profiling - set to 1 to enable timing output (debug builds only) */
#define PROFILE_STARTUP 0
/* Session profiling - enable detailed timing for session restoration (debug builds only) */
#define PROFILE_SESSION 0

#if PROFILE_STARTUP
/* These variables are exported (non-static) so session.c and tabs.c can access them */
LARGE_INTEGER g_startupFreq;
LARGE_INTEGER g_startupStart;
FILE* g_profileFile = NULL;

#define PROFILE_INIT() do { \
    QueryPerformanceFrequency(&g_startupFreq); \
    QueryPerformanceCounter(&g_startupStart); \
    char profilePath[MAX_PATH]; \
    GetModuleFileName(NULL, profilePath, MAX_PATH); \
    char* sl = strrchr(profilePath, '\\'); \
    if (sl) { sl[1] = '\0'; strcat(profilePath, "startup_profile.txt"); } \
    g_profileFile = fopen(profilePath, "w"); \
} while(0)

#define PROFILE_MARK(label) do { \
    LARGE_INTEGER now; \
    QueryPerformanceCounter(&now); \
    double ms = (double)(now.QuadPart - g_startupStart.QuadPart) * 1000.0 / g_startupFreq.QuadPart; \
    char buf[256]; \
    sprintf(buf, "[STARTUP] %s: %.2f ms\n", label, ms); \
    OutputDebugString(buf); \
    if (g_profileFile) { fprintf(g_profileFile, "%s", buf); fflush(g_profileFile); } \
} while(0)

#define PROFILE_CLOSE() do { \
    if (g_profileFile) { fclose(g_profileFile); g_profileFile = NULL; } \
} while(0)
#else
#define PROFILE_INIT()
#define PROFILE_MARK(label)
#define PROFILE_CLOSE()
#endif
#include "resource.h"
#include "window.h"
#include "tabs.h"
#include "toolbar.h"
#include "statusbar.h"
#include "findreplace.h"
#include "themes.h"
#include "config.h"
#include "session.h"
#include "Scintilla.h"
#include "SciLexer.h"
#include "syntax.h"
#include "fileops.h"
#include "editor.h"
#include "splitview.h"
#include "gotoline.h"
#include "preferences.h"
#include "shellintegrate.h"
#include <dwmapi.h>
#include <uxtheme.h>

/* Indicator for word highlighting */
#define INDICATOR_WORD_HIGHLIGHT 8

/* Auto-save timer ID */
#define IDT_AUTOSAVE_TIMER 2001

/* Application instance handle */
HINSTANCE g_hInstance = NULL;
/* Main window handle */
HWND g_hMainWindow = NULL;
/* Accelerator table handle */
HACCEL g_hAccel = NULL;

/* Forward declarations */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void ProcessCommandLineFiles(void);

/*
 * Application entry point
 * 
 * OPTIMIZED STARTUP SEQUENCE:
 * 1. DPI awareness (fast, required before window creation)
 * 2. Common controls init (fast)
 * 3. Scintilla DLL load and keep handle (avoid double load)
 * 4. Config/Session load (INI file reads - fast)
 * 5. Window creation and show (user sees window FAST)
 * 6. Then initialize UI components
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG msg;
    
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    
    PROFILE_INIT();
    
    /* Store instance handle globally */
    g_hInstance = hInstance;
    
    PROFILE_MARK("Start");
    
    /* Enable Per-Monitor V2 DPI awareness for sharp text on HiDPI displays */
    /* Use GetModuleHandle first - it's faster than LoadLibrary when already loaded */
    HMODULE hUser32 = GetModuleHandle("user32.dll");
    if (hUser32) {
        typedef BOOL (WINAPI *SetProcessDpiAwarenessContextProc)(DPI_AWARENESS_CONTEXT);
        SetProcessDpiAwarenessContextProc setDpiFunc = 
            (SetProcessDpiAwarenessContextProc)(void*)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (setDpiFunc) {
            /* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = -4 */
            setDpiFunc((DPI_AWARENESS_CONTEXT)(-4));
        } else {
            typedef BOOL (WINAPI *SetProcessDPIAwareProc)(void);
            SetProcessDPIAwareProc setDpiAware = (SetProcessDPIAwareProc)(void*)GetProcAddress(hUser32, "SetProcessDPIAware");
            if (setDpiAware) setDpiAware();
        }
    }
    
    PROFILE_MARK("DPI awareness set");
    
    /* Initialize common controls - minimal set only */
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_WIN95_CLASSES;
    
    if (!InitCommonControlsEx(&icc)) {
        MessageBox(NULL, "Failed to initialize common controls", "Error", MB_ICONERROR | MB_OK);
        return 1;
    }
    
    PROFILE_MARK("Common controls init");
    
    /* Register Scintilla window class - for static linking */
    /* Scintilla_RegisterClasses is available directly from the static library */
    Scintilla_RegisterClasses((void*)hInstance);
    
    PROFILE_MARK("Scintilla DLL loaded");
    
    /* Initialize configuration system - INI file read is fast */
    InitializeConfig();
    
    PROFILE_MARK("Config loaded");
    
    /* Initialize session system */
    if (!InitializeSession()) {
        MessageBox(NULL, "Failed to initialize session system", "Error", MB_ICONERROR | MB_OK);
        return 1;
    }
    
    /* Initialize theme system - just sets up data structures */
    InitializeTheme();
    
    /* Apply saved theme from config */
    AppConfig* config = GetConfig();
    if (config) {
        SetTheme((Theme)config->theme);
    }
    
    PROFILE_MARK("Theme/Session init");
    
    /* Initialize syntax highlighting system */
    InitializeSyntax();
    
    /* Initialize find/replace system */
    InitializeFindReplace();
    
    PROFILE_MARK("Syntax/FindReplace init");
    
    /* Initialize window and SHOW IT IMMEDIATELY */
    if (!InitializeWindow(hInstance, nCmdShow)) {
        MessageBox(NULL, "Failed to initialize main window", "Error", MB_ICONERROR | MB_OK);
        return 1;
    }
    
    PROFILE_MARK("Window shown");
    
    /* Initialize tab system */
    if (!InitializeTabs(g_hMainWindow)) {
        MessageBox(NULL, "Failed to initialize tab system", "Error", MB_ICONERROR | MB_OK);
        CleanupWindow();
        return 1;
    }
    
    PROFILE_MARK("Tabs init");
    
    /* Initialize toolbar */
    if (!InitializeToolbar(g_hMainWindow)) {
        MessageBox(NULL, "Failed to initialize toolbar", "Error", MB_ICONERROR | MB_OK);
        CleanupTabs();
        CleanupWindow();
        return 1;
    }
    
    PROFILE_MARK("Toolbar init");
    
    /* Initialize status bar */
    if (!InitializeStatusBar(g_hMainWindow)) {
        MessageBox(NULL, "Failed to initialize status bar", "Error", MB_ICONERROR | MB_OK);
        CleanupToolbar();
        CleanupTabs();
        CleanupWindow();
        return 1;
    }
    
    PROFILE_MARK("StatusBar init");
    
    /* Initialize split view */
    if (!InitializeSplitView(g_hMainWindow)) {
        MessageBox(NULL, "Failed to initialize split view", "Error", MB_ICONERROR | MB_OK);
        CleanupStatusBar();
        CleanupToolbar();
        CleanupTabs();
        CleanupWindow();
        return 1;
    }
    
    PROFILE_MARK("SplitView init");
    
    /* Apply theme to editors BEFORE restoring session */
    ApplyThemeToAllEditors();
    
    /* Trigger initial layout */
    RECT rect;
    GetClientRect(g_hMainWindow, &rect);
    HandleWindowResize(rect.right - rect.left, rect.bottom - rect.top);
    
    /* Load accelerator table for keyboard shortcuts */
    g_hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATORS));
    
    PROFILE_MARK("Pre-session complete");
    
    /* Session restoration is deferred via WM_USER message for perceived fast startup */
    /* This allows the window to appear immediately, then load session content */
    PostMessage(g_hMainWindow, WM_USER + 100, 0, 0);  /* WM_DEFERRED_SESSION_LOAD */
    
    /* Main message loop */
    while (GetMessage(&msg, NULL, 0, 0)) {
        /* Handle find/replace modeless dialogs */
        FindReplaceState* frState = GetFindReplaceState();
        if (frState->hwndFind && IsWindow(frState->hwndFind) && IsDialogMessage(frState->hwndFind, &msg)) {
            continue;
        }
        if (frState->hwndReplace && IsWindow(frState->hwndReplace) && IsDialogMessage(frState->hwndReplace, &msg)) {
            continue;
        }

        /* Handle keyboard shortcuts */
        if (!TranslateAccelerator(g_hMainWindow, g_hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    /* Cleanup */
    CleanupSession();
    CleanupConfig();
    CleanupFindReplace();
    CleanupSyntax();
    CleanupTheme();
    CleanupSplitView();
    CleanupStatusBar();
    CleanupToolbar();
    CleanupTabs();
    CleanupWindow();
    
    return (int)msg.wParam;
}

/* Helper function to highlight all occurrences of a word */
static void HighlightWordOccurrences(HWND editor, const char* word, int wordLen)
{
    if (!editor || !word || wordLen <= 0) return;
    
    /* Clear previous highlights */
    SendMessage(editor, SCI_SETINDICATORCURRENT, INDICATOR_WORD_HIGHLIGHT, 0);
    int docLen = (int)SendMessage(editor, SCI_GETLENGTH, 0, 0);
    SendMessage(editor, SCI_INDICATORCLEARRANGE, 0, docLen);
    
    /* Set indicator style */
    SendMessage(editor, SCI_INDICSETSTYLE, INDICATOR_WORD_HIGHLIGHT, INDIC_ROUNDBOX);
    SendMessage(editor, SCI_INDICSETFORE, INDICATOR_WORD_HIGHLIGHT, RGB(255, 255, 0));
    SendMessage(editor, SCI_INDICSETALPHA, INDICATOR_WORD_HIGHLIGHT, 100);
    SendMessage(editor, SCI_INDICSETOUTLINEALPHA, INDICATOR_WORD_HIGHLIGHT, 200);
    SendMessage(editor, SCI_INDICSETUNDER, INDICATOR_WORD_HIGHLIGHT, TRUE);
    
    /* Search for all occurrences */
    SendMessage(editor, SCI_SETTARGETSTART, 0, 0);
    SendMessage(editor, SCI_SETTARGETEND, docLen, 0);
    SendMessage(editor, SCI_SETSEARCHFLAGS, SCFIND_WHOLEWORD, 0);
    
    int pos;
    while ((pos = (int)SendMessage(editor, SCI_SEARCHINTARGET, wordLen, (LPARAM)word)) >= 0) {
        /* Highlight this occurrence */
        SendMessage(editor, SCI_INDICATORFILLRANGE, pos, wordLen);
        
        /* Move target to search for next occurrence */
        SendMessage(editor, SCI_SETTARGETSTART, pos + wordLen, 0);
        SendMessage(editor, SCI_SETTARGETEND, docLen, 0);
    }
}

/*
 * Main window procedure
 */
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    /* Handle toolbar messages first */
    if (HandleToolbarMessage(hWnd, uMsg, wParam, lParam)) {
        return 0;
    }
    
    /* Handle tab messages */
    if (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP || uMsg == WM_MOUSEMOVE || uMsg == WM_MOUSELEAVE) {
        /* Get tab control rect to check if coordinates are within it */
        TabControl* tabControl = GetTabControl();
        if (tabControl && tabControl->hwnd) {
            RECT tabRect;
            GetWindowRect(tabControl->hwnd, &tabRect);
            MapWindowPoints(NULL, hWnd, (LPPOINT)&tabRect, 2);
            
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            
            /* Check if the mouse coordinates are within the tab area */
            if (x >= tabRect.left && x <= tabRect.right &&
                y >= tabRect.top && y <= tabRect.bottom) {
                
                /* Adjust coordinates to tab-relative */
                x -= tabRect.left;
                y -= tabRect.top;
                lParam = MAKELPARAM(x, y);
                
                /* Forward to tab control */
                SendMessage(tabControl->hwnd, uMsg, wParam, lParam);
                return 0;
            }
        }
    }
    
    switch (uMsg) {
        case WM_CREATE:
            /* Enable drag and drop */
            DragAcceptFiles(hWnd, TRUE);
            return 0;
        
        case WM_ERASEBKGND:
            /* Let default handler paint background */
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
            
        case WM_SIZE:
            /* Handle window resize */
            HandleWindowResize(LOWORD(lParam), HIWORD(lParam));
            /* CRITICAL FIX: Update window state in config on resize */
            UpdateCurrentWindowState();
            return 0;
            
        case WM_MOVE:
            /* CRITICAL FIX: Update window state in config on move */
            UpdateCurrentWindowState();
            return 0;
            
        case WM_SYSCOMMAND:
            /* Handle maximize/minimize commands */
            if (wParam == SC_MAXIMIZE || wParam == SC_RESTORE || wParam == SC_MINIMIZE) {
                /* Let the default handler process the command first */
                LRESULT result = DefWindowProc(hWnd, uMsg, wParam, lParam);
                /* Update window state after the system command completes */
                UpdateCurrentWindowState();
                return result;
            }
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
            
        case WM_DROPFILES:
            {
                HDROP hDrop = (HDROP)wParam;
                char filePath[MAX_PATH];
                
                /* Get first dropped file */
                if (DragQueryFile(hDrop, 0, filePath, MAX_PATH) > 0) {
                    int tabIndex = AddNewTab(filePath);
                    SelectTab(tabIndex);
                }
                
                DragFinish(hDrop);
                return 0;
            }
        case WM_DESTROY:
            /* Handle window destruction */
            DragAcceptFiles(hWnd, FALSE);
            PostQuitMessage(0);
            return 0;
            
        case WM_NOTIFY:
            {
                NMHDR* nmhdr = (NMHDR*)lParam;
                
                /* Handle Scintilla notifications */
                if (nmhdr->code == SCN_MODIFIED) {
                    SCNotification* scn = (SCNotification*)lParam;
                    
                    /* Check if content was modified */
                    if (scn->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) {
                        /* Find which tab owns this editor */
                        int tabCount = GetTabCount();
                        for (int i = 0; i < tabCount; i++) {
                            TabInfo* tab = GetTab(i);
                            if (tab && tab->editorHandle == nmhdr->hwndFrom) {
                                SetTabModified(i, TRUE);
                                /* Update Save button state */
                                EnableToolbarButton(ID_FILE_SAVE, TRUE);
                                break;
                            }
                        }
                    }
                } else if (nmhdr->code == SCN_SAVEPOINTREACHED) {
                    /* Document saved - mark as unmodified */
                    int tabCount = GetTabCount();
                    for (int i = 0; i < tabCount; i++) {
                        TabInfo* tab = GetTab(i);
                        if (tab && tab->editorHandle == nmhdr->hwndFrom) {
                            SetTabModified(i, FALSE);
                            /* Update Save button state */
                            EnableToolbarButton(ID_FILE_SAVE, FALSE);
                            break;
                        }
                    }
                } else if (nmhdr->code == SCN_SAVEPOINTLEFT) {
                    /* Document modified - mark as modified */
                    int tabCount = GetTabCount();
                    for (int i = 0; i < tabCount; i++) {
                        TabInfo* tab = GetTab(i);
                        if (tab && tab->editorHandle == nmhdr->hwndFrom) {
                            SetTabModified(i, TRUE);
                            break;
                        }
                    }
                } else if (nmhdr->code == SCN_DOUBLECLICK) {
                    /* Double-click - highlight all occurrences of word under cursor */
                    AppConfig* cfg = GetConfig();
                    if (cfg && cfg->highlightMatchingWords) {
                        HWND editor = nmhdr->hwndFrom;
                        
                        /* Get word at cursor position */
                        int pos = (int)SendMessage(editor, SCI_GETCURRENTPOS, 0, 0);
                        int wordStart = (int)SendMessage(editor, SCI_WORDSTARTPOSITION, pos, TRUE);
                        int wordEnd = (int)SendMessage(editor, SCI_WORDENDPOSITION, pos, TRUE);
                        int wordLen = wordEnd - wordStart;
                        
                        if (wordLen > 0 && wordLen < 256) {
                            char word[256];
                            struct Sci_TextRange tr;
                            tr.chrg.cpMin = wordStart;
                            tr.chrg.cpMax = wordEnd;
                            tr.lpstrText = word;
                            SendMessage(editor, SCI_GETTEXTRANGE, 0, (LPARAM)&tr);
                            
                            HighlightWordOccurrences(editor, word, wordLen);
                        }
                    }
                } else if (nmhdr->code == SCN_CHARADDED) {
                    /* Character added - trigger autocomplete if enabled */
                    if (IsWordAutocompleteEnabled()) {
                        /* Find which tab owns this editor */
                        int tabCount = GetTabCount();
                        for (int i = 0; i < tabCount; i++) {
                            TabInfo* tab = GetTab(i);
                            if (tab && tab->editorHandle == nmhdr->hwndFrom) {
                                /* Trigger autocomplete through direct Scintilla API */
                                SciFnDirect sciFn = (SciFnDirect)SendMessage(tab->editorHandle, SCI_GETDIRECTFUNCTION, 0, 0);
                                sptr_t sciPtr = SendMessage(tab->editorHandle, SCI_GETDIRECTPOINTER, 0, 0);
                                
                                if (sciFn && sciPtr) {
                                    /* Get current position */
                                    long pos = (long)sciFn(sciPtr, SCI_GETCURRENTPOS, 0, 0);
                                    long wordStart = (long)sciFn(sciPtr, SCI_WORDSTARTPOSITION, pos, 1);
                                    long charsTyped = pos - wordStart;
                                    
                                    /* Trigger if we've typed at least 2 characters */
                                    if (charsTyped >= 2) {
                                        TriggerWordAutocompleteForEditor(tab->editorHandle);
                                    }
                                }
                                break;
                            }
                        }
                    }
                } else if (nmhdr->code == SCN_UPDATEUI) {
                    /* Update status bar with cursor position and zoom level */
                    HWND editor = nmhdr->hwndFrom;
                    int pos = (int)SendMessage(editor, SCI_GETCURRENTPOS, 0, 0);
                    int line = (int)SendMessage(editor, SCI_LINEFROMPOSITION, pos, 0);
                    int lineStart = (int)SendMessage(editor, SCI_POSITIONFROMLINE, line, 0);
                    int col = pos - lineStart;
                    int zoomLevel = (int)SendMessage(editor, SCI_GETZOOM, 0, 0);
                    
                    UpdateCursorPosition(line, col);
                    UpdateFilePosition(pos);
                    UpdateZoomLevel(zoomLevel);
                }
            }
            return 0;
            
        case WM_COMMAND:
            /* Handle menu commands and toolbar buttons */
            switch (LOWORD(wParam)) {
                /* Handle dropdown menu button clicks */
                case ID_TOOLBAR_MENU_FILE:
                case ID_TOOLBAR_MENU_EDIT:
                case ID_TOOLBAR_MENU_VIEW:
                case ID_TOOLBAR_MENU_OPTIONS:
                    {
                        int buttonId = LOWORD(wParam);
                        int buttonIndex = GetButtonIndexFromId(buttonId);
                        if (buttonIndex >= 0) {
                            ShowDropdownMenu(buttonIndex, hWnd);
                        }
                    }
                    break;
                    
                case ID_FILE_NEW:
                    AddNewTab(NULL);
                    break;
                    
                case ID_FILE_OPEN:
                    {
                        OPENFILENAME ofn;
                        char filePath[MAX_PATH] = "";
                        char filter[MAX_PATH * 4] = "All Files\0*.*\0Text Files\0*.txt\0C/C++ Files\0*.c;*.cpp;*.h\0";
                        
                        memset(&ofn, 0, sizeof(OPENFILENAME));
                        ofn.lStructSize = sizeof(OPENFILENAME);
                        ofn.hwndOwner = hWnd;
                        ofn.lpstrFilter = filter;
                        ofn.nFilterIndex = 1;
                        ofn.lpstrFile = filePath;
                        ofn.nMaxFile = MAX_PATH;
                        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
                        
                        if (GetOpenFileName(&ofn)) {
                            int tabIndex = AddNewTab(filePath);
                            if (tabIndex >= 0) {
                                SelectTab(tabIndex);
                            }
                        }
                    }
                    break;
                    
                case ID_FILE_SAVE:
                    {
                        /* Get current tab and save its content */
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            SaveTabToFile(currentTab);
                        }
                    }
                    break;
                    
                case ID_FILE_SAVEAS:
                    {
                        /* Save As - save current tab with a new name */
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            SaveTabToFileAs(currentTab);
                        }
                    }
                    break;
                    
                case ID_FILE_SAVEALL:
                    {
                        /* Save All - save all modified tabs */
                        int tabCount = GetTabCount();
                        int savedCount = 0;
                        for (int i = 0; i < tabCount; i++) {
                            TabInfo* tab = GetTab(i);
                            if (tab && tab->isModified) {
                                if (SaveTabToFile(i)) {
                                    savedCount++;
                                }
                            }
                        }
                        
                        if (savedCount > 0) {
                            char msg[100];
                            sprintf(msg, "Saved %d file(s)", savedCount);
                            UpdateFileEncoding(msg);  /* Temporary display in status bar */
                        }
                    }
                    break;
                    
                case ID_FILE_OPENFOLDER:
                    {
                        /* Open containing folder of current file */
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->filePath[0] != '\0') {
                                /* Check if it's a real file (not a new unsaved file) */
                                if (strncmp(tab->filePath, "New ", 4) != 0) {
                                    /* Extract folder path */
                                    char folderPath[MAX_PATH];
                                    strcpy(folderPath, tab->filePath);
                                    char* lastSlash = strrchr(folderPath, '\\');
                                    if (lastSlash) {
                                        *lastSlash = '\0';  /* Terminate at last backslash to get folder */
                                    }
                                    
                                    /* Open folder in Windows Explorer */
                                    ShellExecute(NULL, "open", folderPath, NULL, NULL, SW_SHOWNORMAL);
                                }
                            }
                        }
                    }
                    break;
                    
                case ID_FILE_RECENTFILES:
                    /* Handled by recent file IDs */
                    break;
                    
                case ID_EDIT_GOTOLINE:
                    {
                        /* Go to line dialog */
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                ShowGoToLineDialog(hWnd, tab->editorHandle);
                                /* Don't call SetFocus here - the dialog handles focus itself */
                            }
                        }
                    }
                    break;

                case ID_OPTIONS_PREFERENCES:
                    ShowPreferencesDialog(hWnd);
                    break;

                case ID_OPTIONS_AUTOINDENT:
                    {
                        /* Toggle auto-indent for current tab only */
                        int activeTab = GetSelectedTab();
                        if (activeTab >= 0) {
                            TabInfo* tab = GetTab(activeTab);
                            if (tab && tab->editorHandle) {
                                /* Toggle the per-tab state */
                                tab->autoIndent = !tab->autoIndent;
                                
                                /* Apply auto-indent settings to editor */
                                SendMessage(tab->editorHandle, SCI_SETINDENTATIONGUIDES,
                                    tab->autoIndent ? SC_IV_LOOKBOTH : SC_IV_NONE, 0);
                                SendMessage(tab->editorHandle, SCI_SETTABINDENTS,
                                    tab->autoIndent ? 1 : 0, 0);
                                SendMessage(tab->editorHandle, SCI_SETBACKSPACEUNINDENTS,
                                    tab->autoIndent ? 1 : 0, 0);
                                
                                /* Update toggle button state based on current tab */
                                SetToolbarButtonToggled(ID_OPTIONS_AUTOINDENT, tab->autoIndent);
                            }
                        }
                    }
                    break;
                    
                case ID_OPTIONS_BRACKETMATCH:
                    {
                        AppConfig* cfg = GetConfig();
                        cfg->bracketMatching = !cfg->bracketMatching;
                        SetBracketMatching(cfg->bracketMatching);
                    }
                    break;
                    
                case ID_FILE_EXIT:
                    /* Use WM_CLOSE to trigger save confirmation */
                    SendMessage(hWnd, WM_CLOSE, 0, 0);
                    break;
                    
                case ID_OPTIONS_AUTOSAVE:
                    {
                        AppConfig* cfg = GetConfig();
                        cfg->autoSave = !cfg->autoSave;
                        if (cfg->autoSave) {
                            SetTimer(hWnd, IDT_AUTOSAVE_TIMER, cfg->autoSaveInterval * 1000, NULL);
                        } else {
                            KillTimer(hWnd, IDT_AUTOSAVE_TIMER);
                        }
                    }
                    break;
                    
                case ID_OPTIONS_RESTORESESSION:
                    {
                        AppConfig* cfg = GetConfig();
                        cfg->restoreSession = !cfg->restoreSession;
                    }
                    break;
                    
                case ID_EDIT_UNDO:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                SendMessage(tab->editorHandle, SCI_UNDO, 0, 0);
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_REDO:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                SendMessage(tab->editorHandle, SCI_REDO, 0, 0);
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_CUT:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                SendMessage(tab->editorHandle, SCI_CUT, 0, 0);
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_COPY:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                SendMessage(tab->editorHandle, SCI_COPY, 0, 0);
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_PASTE:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                SendMessage(tab->editorHandle, SCI_PASTE, 0, 0);
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_SELECTALL:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                SendMessage(tab->editorHandle, SCI_SELECTALL, 0, 0);
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_FIND:
                    {
                        /* Get selected text to pre-populate find dialog */
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                int selStart = (int)SendMessage(tab->editorHandle, SCI_GETSELECTIONSTART, 0, 0);
                                int selEnd = (int)SendMessage(tab->editorHandle, SCI_GETSELECTIONEND, 0, 0);
                                
                                if (selEnd > selStart && (selEnd - selStart) < 256) {
                                    char selectedText[256];
                                    struct Sci_TextRange tr;
                                    tr.chrg.cpMin = selStart;
                                    tr.chrg.cpMax = selEnd;
                                    tr.lpstrText = selectedText;
                                    SendMessage(tab->editorHandle, SCI_GETTEXTRANGE, 0, (LPARAM)&tr);
                                    
                                    /* Set the find text */
                                    SetFindText(selectedText);
                                }
                            }
                        }
                        ShowFindDialog(hWnd);
                    }
                    break;
                    
                case ID_EDIT_FINDNEXT:
                    /* F3 - Find Next */
                    if (!FindNext()) {
                        MessageBox(hWnd, "Text not found", "Find", MB_ICONINFORMATION | MB_OK);
                    }
                    break;
                    
                case ID_EDIT_FINDPREV:
                    /* Shift+F3 - Find Previous */
                    if (!FindPrevious()) {
                        MessageBox(hWnd, "Text not found", "Find", MB_ICONINFORMATION | MB_OK);
                    }
                    break;
                    
                case ID_EDIT_REPLACE:
                    ShowReplaceDialog(hWnd);
                    break;
                    
                /* Common line operations */
                case ID_EDIT_DUPLICATE_LINE:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                /* SCI_LINEDUPLICATE duplicates the current line */
                                SendMessage(tab->editorHandle, SCI_LINEDUPLICATE, 0, 0);
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_DELETE_LINE:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                /* SCI_LINEDELETE deletes the current line */
                                SendMessage(tab->editorHandle, SCI_LINEDELETE, 0, 0);
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_MOVE_LINE_UP:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                /* Move current line up by swapping with previous line */
                                int curLine = (int)SendMessage(tab->editorHandle, SCI_LINEFROMPOSITION, 
                                    SendMessage(tab->editorHandle, SCI_GETCURRENTPOS, 0, 0), 0);
                                if (curLine > 0) {
                                    SendMessage(tab->editorHandle, SCI_MOVESELECTEDLINESUP, 0, 0);
                                }
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_MOVE_LINE_DOWN:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                /* Move current line down by swapping with next line */
                                int curLine = (int)SendMessage(tab->editorHandle, SCI_LINEFROMPOSITION,
                                    SendMessage(tab->editorHandle, SCI_GETCURRENTPOS, 0, 0), 0);
                                int totalLines = (int)SendMessage(tab->editorHandle, SCI_GETLINECOUNT, 0, 0);
                                if (curLine < totalLines - 1) {
                                    SendMessage(tab->editorHandle, SCI_MOVESELECTEDLINESDOWN, 0, 0);
                                }
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_JOIN_LINES:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                /* Join current line with next line */
                                int curLine = (int)SendMessage(tab->editorHandle, SCI_LINEFROMPOSITION,
                                    SendMessage(tab->editorHandle, SCI_GETCURRENTPOS, 0, 0), 0);
                                int lineStart = (int)SendMessage(tab->editorHandle, SCI_POSITIONFROMLINE, curLine, 0);
                                int lineEnd = (int)SendMessage(tab->editorHandle, SCI_GETLINEENDPOSITION, curLine, 0);
                                int nextLineEnd = (int)SendMessage(tab->editorHandle, SCI_GETLINEENDPOSITION, curLine + 1, 0);
                                
                                /* Delete the line ending between current and next line */
                                if (nextLineEnd > lineEnd) {
                                    /* Target the EOL characters */
                                    SendMessage(tab->editorHandle, SCI_SETTARGETSTART, lineEnd, 0);
                                    SendMessage(tab->editorHandle, SCI_SETTARGETEND, 
                                        (int)SendMessage(tab->editorHandle, SCI_POSITIONFROMLINE, curLine + 1, 0), 0);
                                    SendMessage(tab->editorHandle, SCI_REPLACETARGET, 0, (LPARAM)"");
                                }
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_SPLIT_LINES:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                /* Split lines at word wrap boundaries */
                                SendMessage(tab->editorHandle, SCI_TARGETFROMSELECTION, 0, 0);
                                SendMessage(tab->editorHandle, SCI_LINESSPLIT, 0, 0);
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_TRIM_TRAILING:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                SendMessage(tab->editorHandle, SCI_BEGINUNDOACTION, 0, 0);
                                int lineCount = (int)SendMessage(tab->editorHandle, SCI_GETLINECOUNT, 0, 0);
                                for (int i = 0; i < lineCount; i++) {
                                    int lineStart = (int)SendMessage(tab->editorHandle, SCI_POSITIONFROMLINE, i, 0);
                                    int lineEnd = (int)SendMessage(tab->editorHandle, SCI_GETLINEENDPOSITION, i, 0);
                                    /* Find end of non-whitespace content */
                                    int pos = lineEnd - 1;
                                    while (pos >= lineStart) {
                                        char ch = (char)SendMessage(tab->editorHandle, SCI_GETCHARAT, pos, 0);
                                        if (ch != ' ' && ch != '\t') break;
                                        pos--;
                                    }
                                    if (pos < lineEnd - 1) {
                                        SendMessage(tab->editorHandle, SCI_SETTARGETSTART, pos + 1, 0);
                                        SendMessage(tab->editorHandle, SCI_SETTARGETEND, lineEnd, 0);
                                        SendMessage(tab->editorHandle, SCI_REPLACETARGET, 0, (LPARAM)"");
                                    }
                                }
                                SendMessage(tab->editorHandle, SCI_ENDUNDOACTION, 0, 0);
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_TRIM_LEADING:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                SendMessage(tab->editorHandle, SCI_BEGINUNDOACTION, 0, 0);
                                int lineCount = (int)SendMessage(tab->editorHandle, SCI_GETLINECOUNT, 0, 0);
                                for (int i = 0; i < lineCount; i++) {
                                    int lineStart = (int)SendMessage(tab->editorHandle, SCI_POSITIONFROMLINE, i, 0);
                                    int lineEnd = (int)SendMessage(tab->editorHandle, SCI_GETLINEENDPOSITION, i, 0);
                                    /* Find start of non-whitespace content */
                                    int pos = lineStart;
                                    while (pos < lineEnd) {
                                        char ch = (char)SendMessage(tab->editorHandle, SCI_GETCHARAT, pos, 0);
                                        if (ch != ' ' && ch != '\t') break;
                                        pos++;
                                    }
                                    if (pos > lineStart) {
                                        SendMessage(tab->editorHandle, SCI_SETTARGETSTART, lineStart, 0);
                                        SendMessage(tab->editorHandle, SCI_SETTARGETEND, pos, 0);
                                        SendMessage(tab->editorHandle, SCI_REPLACETARGET, 0, (LPARAM)"");
                                    }
                                }
                                SendMessage(tab->editorHandle, SCI_ENDUNDOACTION, 0, 0);
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_TRIM_BOTH:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                SendMessage(tab->editorHandle, SCI_BEGINUNDOACTION, 0, 0);
                                int lineCount = (int)SendMessage(tab->editorHandle, SCI_GETLINECOUNT, 0, 0);
                                for (int i = 0; i < lineCount; i++) {
                                    int lineStart = (int)SendMessage(tab->editorHandle, SCI_POSITIONFROMLINE, i, 0);
                                    int lineEnd = (int)SendMessage(tab->editorHandle, SCI_GETLINEENDPOSITION, i, 0);
                                    /* Find start of non-whitespace content */
                                    int startPos = lineStart;
                                    while (startPos < lineEnd) {
                                        char ch = (char)SendMessage(tab->editorHandle, SCI_GETCHARAT, startPos, 0);
                                        if (ch != ' ' && ch != '\t') break;
                                        startPos++;
                                    }
                                    /* Find end of non-whitespace content */
                                    int endPos = lineEnd - 1;
                                    while (endPos >= lineStart) {
                                        char ch = (char)SendMessage(tab->editorHandle, SCI_GETCHARAT, endPos, 0);
                                        if (ch != ' ' && ch != '\t') break;
                                        endPos--;
                                    }
                                    /* Trim trailing first */
                                    if (endPos < lineEnd - 1) {
                                        SendMessage(tab->editorHandle, SCI_SETTARGETSTART, endPos + 1, 0);
                                        SendMessage(tab->editorHandle, SCI_SETTARGETEND, lineEnd, 0);
                                        SendMessage(tab->editorHandle, SCI_REPLACETARGET, 0, (LPARAM)"");
                                    }
                                    /* Then trim leading */
                                    if (startPos > lineStart) {
                                        SendMessage(tab->editorHandle, SCI_SETTARGETSTART, lineStart, 0);
                                        SendMessage(tab->editorHandle, SCI_SETTARGETEND, startPos, 0);
                                        SendMessage(tab->editorHandle, SCI_REPLACETARGET, 0, (LPARAM)"");
                                    }
                                }
                                SendMessage(tab->editorHandle, SCI_ENDUNDOACTION, 0, 0);
                            }
                        }
                    }
                    break;
                    
                /* Case conversion */
                case ID_EDIT_UPPERCASE:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                SendMessage(tab->editorHandle, SCI_UPPERCASE, 0, 0);
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_LOWERCASE:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                SendMessage(tab->editorHandle, SCI_LOWERCASE, 0, 0);
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_TITLECASE:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                /* Get selected text and convert to title case */
                                int selStart = (int)SendMessage(tab->editorHandle, SCI_GETSELECTIONSTART, 0, 0);
                                int selEnd = (int)SendMessage(tab->editorHandle, SCI_GETSELECTIONEND, 0, 0);
                                if (selEnd > selStart && selEnd - selStart < 65536) {
                                    char* text = malloc(selEnd - selStart + 1);
                                    if (text) {
                                        struct Sci_TextRange tr;
                                        tr.chrg.cpMin = selStart;
                                        tr.chrg.cpMax = selEnd;
                                        tr.lpstrText = text;
                                        SendMessage(tab->editorHandle, SCI_GETTEXTRANGE, 0, (LPARAM)&tr);
                                        
                                        /* Convert to title case */
                                        BOOL newWord = TRUE;
                                        for (int i = 0; text[i]; i++) {
                                            if (text[i] == ' ' || text[i] == '\t' || text[i] == '\n' || text[i] == '\r') {
                                                newWord = TRUE;
                                            } else if (newWord) {
                                                if (text[i] >= 'a' && text[i] <= 'z') {
                                                    text[i] = text[i] - 'a' + 'A';
                                                }
                                                newWord = FALSE;
                                            } else {
                                                if (text[i] >= 'A' && text[i] <= 'Z') {
                                                    text[i] = text[i] - 'A' + 'a';
                                                }
                                            }
                                        }
                                        
                                        SendMessage(tab->editorHandle, SCI_REPLACESEL, 0, (LPARAM)text);
                                        free(text);
                                    }
                                }
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_SENTENCECASE:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                /* Get selected text and convert to sentence case */
                                int selStart = (int)SendMessage(tab->editorHandle, SCI_GETSELECTIONSTART, 0, 0);
                                int selEnd = (int)SendMessage(tab->editorHandle, SCI_GETSELECTIONEND, 0, 0);
                                if (selEnd > selStart && selEnd - selStart < 65536) {
                                    char* text = malloc(selEnd - selStart + 1);
                                    if (text) {
                                        struct Sci_TextRange tr;
                                        tr.chrg.cpMin = selStart;
                                        tr.chrg.cpMax = selEnd;
                                        tr.lpstrText = text;
                                        SendMessage(tab->editorHandle, SCI_GETTEXTRANGE, 0, (LPARAM)&tr);
                                        
                                        /* Convert to sentence case */
                                        BOOL newSentence = TRUE;
                                        for (int i = 0; text[i]; i++) {
                                            if (text[i] == '.' || text[i] == '!' || text[i] == '?') {
                                                newSentence = TRUE;
                                            } else if (newSentence && ((text[i] >= 'a' && text[i] <= 'z') || (text[i] >= 'A' && text[i] <= 'Z'))) {
                                                if (text[i] >= 'a' && text[i] <= 'z') {
                                                    text[i] = text[i] - 'a' + 'A';
                                                }
                                                newSentence = FALSE;
                                            } else if (!newSentence && text[i] >= 'A' && text[i] <= 'Z') {
                                                text[i] = text[i] - 'A' + 'a';
                                            }
                                        }
                                        
                                        SendMessage(tab->editorHandle, SCI_REPLACESEL, 0, (LPARAM)text);
                                        free(text);
                                    }
                                }
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_INVERTCASE:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                /* Get selected text and invert case */
                                int selStart = (int)SendMessage(tab->editorHandle, SCI_GETSELECTIONSTART, 0, 0);
                                int selEnd = (int)SendMessage(tab->editorHandle, SCI_GETSELECTIONEND, 0, 0);
                                if (selEnd > selStart && selEnd - selStart < 65536) {
                                    char* text = malloc(selEnd - selStart + 1);
                                    if (text) {
                                        struct Sci_TextRange tr;
                                        tr.chrg.cpMin = selStart;
                                        tr.chrg.cpMax = selEnd;
                                        tr.lpstrText = text;
                                        SendMessage(tab->editorHandle, SCI_GETTEXTRANGE, 0, (LPARAM)&tr);
                                        
                                        /* Invert case */
                                        for (int i = 0; text[i]; i++) {
                                            if (text[i] >= 'a' && text[i] <= 'z') {
                                                text[i] = text[i] - 'a' + 'A';
                                            } else if (text[i] >= 'A' && text[i] <= 'Z') {
                                                text[i] = text[i] - 'A' + 'a';
                                            }
                                        }
                                        
                                        SendMessage(tab->editorHandle, SCI_REPLACESEL, 0, (LPARAM)text);
                                        free(text);
                                    }
                                }
                            }
                        }
                    }
                    break;
                    
                /* Base64 encoding/decoding */
                case ID_EDIT_BASE64_ENCODE:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                /* Get selected text */
                                int selStart = (int)SendMessage(tab->editorHandle, SCI_GETSELECTIONSTART, 0, 0);
                                int selEnd = (int)SendMessage(tab->editorHandle, SCI_GETSELECTIONEND, 0, 0);
                                if (selEnd > selStart) {
                                    int len = selEnd - selStart;
                                    char* text = malloc(len + 1);
                                    if (text) {
                                        struct Sci_TextRange tr;
                                        tr.chrg.cpMin = selStart;
                                        tr.chrg.cpMax = selEnd;
                                        tr.lpstrText = text;
                                        SendMessage(tab->editorHandle, SCI_GETTEXTRANGE, 0, (LPARAM)&tr);
                                        
                                        /* Calculate base64 encoded length */
                                        int encodedLen = ((len + 2) / 3) * 4 + 1;
                                        char* encoded = malloc(encodedLen);
                                        if (encoded) {
                                            /* Simple base64 encoding */
                                            static const char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                                            int i, j;
                                            for (i = 0, j = 0; i < len; i += 3) {
                                                unsigned int val = (unsigned char)text[i] << 16;
                                                if (i + 1 < len) val |= (unsigned char)text[i + 1] << 8;
                                                if (i + 2 < len) val |= (unsigned char)text[i + 2];
                                                
                                                encoded[j++] = base64[(val >> 18) & 0x3F];
                                                encoded[j++] = base64[(val >> 12) & 0x3F];
                                                encoded[j++] = (i + 1 < len) ? base64[(val >> 6) & 0x3F] : '=';
                                                encoded[j++] = (i + 2 < len) ? base64[val & 0x3F] : '=';
                                            }
                                            encoded[j] = '\0';
                                            
                                            SendMessage(tab->editorHandle, SCI_REPLACESEL, 0, (LPARAM)encoded);
                                            free(encoded);
                                        }
                                        free(text);
                                    }
                                }
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_BASE64_DECODE:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                /* Get selected text */
                                int selStart = (int)SendMessage(tab->editorHandle, SCI_GETSELECTIONSTART, 0, 0);
                                int selEnd = (int)SendMessage(tab->editorHandle, SCI_GETSELECTIONEND, 0, 0);
                                if (selEnd > selStart) {
                                    int len = selEnd - selStart;
                                    char* text = malloc(len + 1);
                                    if (text) {
                                        struct Sci_TextRange tr;
                                        tr.chrg.cpMin = selStart;
                                        tr.chrg.cpMax = selEnd;
                                        tr.lpstrText = text;
                                        SendMessage(tab->editorHandle, SCI_GETTEXTRANGE, 0, (LPARAM)&tr);
                                        
                                        /* Calculate decoded length */
                                        int padding = 0;
                                        if (len >= 2 && text[len - 1] == '=') padding++;
                                        if (len >= 2 && text[len - 2] == '=') padding++;
                                        int decodedLen = (len / 4) * 3 - padding;
                                        char* decoded = malloc(decodedLen + 1);
                                        if (decoded) {
                                            /* Simple base64 decoding */
                                            static const unsigned char base64_decode[256] = {
                                                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                                                0,0,0,0,0,0,0,0,0,0,0,62,0,0,0,63,52,53,54,55,56,57,58,59,60,61,0,0,0,0,0,0,
                                                0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,0,0,0,0,0,
                                                0,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,0,0,0,0,0,
                                                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                                                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                                                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                                                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
                                            };
                                            int i, j;
                                            for (i = 0, j = 0; i < len; i += 4) {
                                                unsigned int val = (base64_decode[(unsigned char)text[i]] << 18) |
                                                                   (base64_decode[(unsigned char)text[i + 1]] << 12) |
                                                                   (base64_decode[(unsigned char)text[i + 2]] << 6) |
                                                                   base64_decode[(unsigned char)text[i + 3]];
                                                decoded[j++] = (val >> 16) & 0xFF;
                                                if (text[i + 2] != '=') decoded[j++] = (val >> 8) & 0xFF;
                                                if (text[i + 3] != '=') decoded[j++] = val & 0xFF;
                                            }
                                            decoded[decodedLen] = '\0';
                                            
                                            SendMessage(tab->editorHandle, SCI_REPLACESEL, 0, (LPARAM)decoded);
                                            free(decoded);
                                        }
                                        free(text);
                                    }
                                }
                            }
                        }
                    }
                    break;
                    
                /* URL encoding/decoding */
                case ID_EDIT_URL_ENCODE:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                /* Get selected text */
                                int selStart = (int)SendMessage(tab->editorHandle, SCI_GETSELECTIONSTART, 0, 0);
                                int selEnd = (int)SendMessage(tab->editorHandle, SCI_GETSELECTIONEND, 0, 0);
                                if (selEnd > selStart) {
                                    int len = selEnd - selStart;
                                    char* text = malloc(len + 1);
                                    if (text) {
                                        struct Sci_TextRange tr;
                                        tr.chrg.cpMin = selStart;
                                        tr.chrg.cpMax = selEnd;
                                        tr.lpstrText = text;
                                        SendMessage(tab->editorHandle, SCI_GETTEXTRANGE, 0, (LPARAM)&tr);
                                        
                                        /* Calculate encoded length (worst case: all chars need %XX) */
                                        char* encoded = malloc(len * 3 + 1);
                                        if (encoded) {
                                            int j = 0;
                                            static const char hex[] = "0123456789ABCDEF";
                                            for (int i = 0; i < len; i++) {
                                                unsigned char c = (unsigned char)text[i];
                                                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || 
                                                    (c >= '0' && c <= '9') || c == '-' || c == '_' || 
                                                    c == '.' || c == '~') {
                                                    encoded[j++] = c;
                                                } else {
                                                    encoded[j++] = '%';
                                                    encoded[j++] = hex[c >> 4];
                                                    encoded[j++] = hex[c & 0xF];
                                                }
                                            }
                                            encoded[j] = '\0';
                                            
                                            SendMessage(tab->editorHandle, SCI_REPLACESEL, 0, (LPARAM)encoded);
                                            free(encoded);
                                        }
                                        free(text);
                                    }
                                }
                            }
                        }
                    }
                    break;
                    
                case ID_EDIT_URL_DECODE:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                /* Get selected text */
                                int selStart = (int)SendMessage(tab->editorHandle, SCI_GETSELECTIONSTART, 0, 0);
                                int selEnd = (int)SendMessage(tab->editorHandle, SCI_GETSELECTIONEND, 0, 0);
                                if (selEnd > selStart) {
                                    int len = selEnd - selStart;
                                    char* text = malloc(len + 1);
                                    if (text) {
                                        struct Sci_TextRange tr;
                                        tr.chrg.cpMin = selStart;
                                        tr.chrg.cpMax = selEnd;
                                        tr.lpstrText = text;
                                        SendMessage(tab->editorHandle, SCI_GETTEXTRANGE, 0, (LPARAM)&tr);
                                        
                                        char* decoded = malloc(len + 1);
                                        if (decoded) {
                                            int j = 0;
                                            for (int i = 0; i < len; i++) {
                                                if (text[i] == '%' && i + 2 < len) {
                                                    unsigned char hi = text[i + 1];
                                                    unsigned char lo = text[i + 2];
                                                    unsigned char val = 0;
                                                    if (hi >= '0' && hi <= '9') val |= (hi - '0') << 4;
                                                    else if (hi >= 'A' && hi <= 'F') val |= (hi - 'A' + 10) << 4;
                                                    else if (hi >= 'a' && hi <= 'f') val |= (hi - 'a' + 10) << 4;
                                                    if (lo >= '0' && lo <= '9') val |= (lo - '0');
                                                    else if (lo >= 'A' && lo <= 'F') val |= (lo - 'A' + 10);
                                                    else if (lo >= 'a' && lo <= 'f') val |= (lo - 'a' + 10);
                                                    decoded[j++] = val;
                                                    i += 2;
                                                } else if (text[i] == '+') {
                                                    decoded[j++] = ' ';
                                                } else {
                                                    decoded[j++] = text[i];
                                                }
                                            }
                                            decoded[j] = '\0';
                                            
                                            SendMessage(tab->editorHandle, SCI_REPLACESEL, 0, (LPARAM)decoded);
                                            free(decoded);
                                        }
                                        free(text);
                                    }
                                }
                            }
                        }
                    }
                    break;
                    
                case ID_TAB_CLOSEALL:
                    CloseAllTabs();
                    if (GetTabCount() == 0) AddNewTab(NULL);
                    break;
                    
                case ID_TAB_CLOSEOTHERS:
                    CloseAllTabsExcept(GetSelectedTab());
                    break;
                    
                case ID_TAB_CLOSE:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            CloseTab(currentTab);
                            /* If no tabs left, create a new one */
                            if (GetTabCount() == 0) {
                                AddNewTab(NULL);
                            }
                        }
                    }
                    break;
                    
                case ID_TAB_NEXT:
                    {
                        int currentTab = GetSelectedTab();
                        int tabCount = GetTabCount();
                        if (tabCount > 1) {
                            int nextTab = (currentTab + 1) % tabCount;
                            SelectTab(nextTab);
                        }
                    }
                    break;
                    
                case ID_TAB_PREV:
                    {
                        int currentTab = GetSelectedTab();
                        int tabCount = GetTabCount();
                        if (tabCount > 1) {
                            int prevTab = (currentTab - 1 + tabCount) % tabCount;
                            SelectTab(prevTab);
                        }
                    }
                    break;
                    
                case ID_VIEW_TOOLBAR:
                    {
                        ShowToolbar(!IsToolbarVisible());
                        HandleWindowResize(0, 0);  /* Trigger layout update */
                    }
                    break;
                    
                case ID_VIEW_STATUSBAR:
                    {
                        ShowStatusBar(!IsStatusBarVisible());
                        HandleWindowResize(0, 0);  /* Trigger layout update */
                    }
                    break;
                    
                case ID_VIEW_WORD_WRAP:
                    {
                        /* Toggle word wrap for current tab only */
                        int activeTab = GetSelectedTab();
                        if (activeTab >= 0) {
                            TabInfo* tab = GetTab(activeTab);
                            if (tab && tab->editorHandle) {
                                tab->wordWrap = !tab->wordWrap;
                                SendMessage(tab->editorHandle, SCI_SETWRAPMODE,
                                    tab->wordWrap ? SC_WRAP_WORD : SC_WRAP_NONE, 0);
                                /* Update toggle button state */
                                SetToolbarButtonToggled(ID_VIEW_WORD_WRAP, tab->wordWrap);
                            }
                        }
                    }
                    break;
                    
                case ID_VIEW_LINE_NUMBERS:
                    {
                        /* Toggle line numbers for current tab only */
                        int activeTab = GetSelectedTab();
                        if (activeTab >= 0) {
                            TabInfo* tab = GetTab(activeTab);
                            if (tab && tab->editorHandle) {
                                /* Toggle the per-tab state */
                                tab->showLineNumbers = !tab->showLineNumbers;
                                
                                /* Apply to the editor */
                                SendMessage(tab->editorHandle, SCI_SETMARGINWIDTHN, 0,
                                    tab->showLineNumbers ? 40 : 0);
                                
                                /* Update toggle button state based on current tab */
                                SetToolbarButtonToggled(ID_VIEW_LINE_NUMBERS, tab->showLineNumbers);
                            }
                        }
                    }
                    break;
                    
                case ID_VIEW_WHITESPACE:
                    {
                        /* Toggle whitespace visibility for current tab only */
                        int activeTab = GetSelectedTab();
                        if (activeTab >= 0) {
                            TabInfo* tab = GetTab(activeTab);
                            if (tab && tab->editorHandle) {
                                /* Toggle the per-tab state */
                                tab->showWhitespace = !tab->showWhitespace;
                                
                                /* Apply to the editor */
                                SendMessage(tab->editorHandle, SCI_SETVIEWWS,
                                    tab->showWhitespace ? SCWS_VISIBLEALWAYS : SCWS_INVISIBLE, 0);
                                
                                /* Update toggle button state based on current tab */
                                SetToolbarButtonToggled(ID_VIEW_WHITESPACE, tab->showWhitespace);
                            }
                        }
                    }
                    break;
                    
                case ID_VIEW_SPLITVIEW:
                    /* Redirect to clone functionality as requested */
                    CloneCurrentTabToNewTab();
                    break;
                    
                case ID_VIEW_SPLITVIEW_LOADLEFT:
                case ID_VIEW_SPLITVIEW_LOADRIGHT:
                    /* Clone to New Tab - create a copy of current document in a new tab */
                    if (!CloneCurrentTabToNewTab()) {
                        MessageBox(hWnd, "Failed to clone current tab", "Clone to New Tab", MB_OK | MB_ICONWARNING);
                    }
                    break;
                    
                case ID_VIEW_CODEFOLDING:
                    {
                        /* Toggle code folding for current tab only */
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                /* Toggle the per-tab state */
                                tab->codeFoldingEnabled = !tab->codeFoldingEnabled;
                                
                                /* Apply to the editor */
                                if (tab->codeFoldingEnabled) {
                                    SendMessage(tab->editorHandle, SCI_SETMARGINTYPEN, 2, SC_MARGIN_SYMBOL);
                                    SendMessage(tab->editorHandle, SCI_SETMARGINMASKN, 2, SC_MASK_FOLDERS);
                                    SendMessage(tab->editorHandle, SCI_SETMARGINWIDTHN, 2, 16);
                                    SendMessage(tab->editorHandle, SCI_SETMARGINSENSITIVEN, 2, 1);
                                } else {
                                    SendMessage(tab->editorHandle, SCI_SETMARGINWIDTHN, 2, 0);
                                }
                                
                                /* Update toggle button state based on current tab */
                                SetToolbarButtonToggled(ID_VIEW_CODEFOLDING, tab->codeFoldingEnabled);
                            }
                        }
                    }
                    break;
                    
                case ID_VIEW_CHANGEHISTORY:
                    {
                        /* Toggle change history for current tab only */
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                tab->changeHistoryEnabled = !tab->changeHistoryEnabled;
                                EnableChangeHistory(tab->editorHandle, tab->changeHistoryEnabled);
                                SetToolbarButtonToggled(ID_VIEW_CHANGEHISTORY, tab->changeHistoryEnabled);
                            }
                        }
                    }
                    break;
                    
                case ID_OPTIONS_THEME_DARK:
                    SetTheme(THEME_DARK);
                    ApplyThemeToAllEditors();
                    break;
                    
                case ID_OPTIONS_THEME_LIGHT:
                    SetTheme(THEME_LIGHT);
                    ApplyThemeToAllEditors();
                    break;
                    
                case ID_FILE_CLEARRECENT:
                    /* Clear recent files */
                    ClearRecentFiles();
                    SaveConfig();
                    break;
                    
                /* Encoding changes */
                case ID_ENCODING_UTF8:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                SendMessage(tab->editorHandle, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
                                UpdateFileEncoding("UTF-8");
                            }
                        }
                    }
                    break;
                    
                case ID_ENCODING_UTF8BOM:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                SendMessage(tab->editorHandle, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
                                UpdateFileEncoding("UTF-8 BOM");
                            }
                        }
                    }
                    break;
                    
                case ID_ENCODING_UTF16LE:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            UpdateFileEncoding("UTF-16 LE");
                        }
                    }
                    break;
                    
                case ID_ENCODING_UTF16BE:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            UpdateFileEncoding("UTF-16 BE");
                        }
                    }
                    break;
                    
                case ID_ENCODING_ANSI:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                SendMessage(tab->editorHandle, SCI_SETCODEPAGE, 0, 0);
                                UpdateFileEncoding("ANSI");
                            }
                        }
                    }
                    break;
                    
                /* Line ending changes */
                case ID_LINEEND_CRLF:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                SendMessage(tab->editorHandle, SCI_SETEOLMODE, SC_EOL_CRLF, 0);
                                SendMessage(tab->editorHandle, SCI_CONVERTEOLS, SC_EOL_CRLF, 0);
                                UpdateLineEndType("CRLF");
                                SetTabModified(currentTab, TRUE);
                            }
                        }
                    }
                    break;
                    
                case ID_LINEEND_LF:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                SendMessage(tab->editorHandle, SCI_SETEOLMODE, SC_EOL_LF, 0);
                                SendMessage(tab->editorHandle, SCI_CONVERTEOLS, SC_EOL_LF, 0);
                                UpdateLineEndType("LF");
                                SetTabModified(currentTab, TRUE);
                            }
                        }
                    }
                    break;
                    
                case ID_LINEEND_CR:
                    {
                        int currentTab = GetSelectedTab();
                        if (currentTab >= 0) {
                            TabInfo* tab = GetTab(currentTab);
                            if (tab && tab->editorHandle) {
                                SendMessage(tab->editorHandle, SCI_SETEOLMODE, SC_EOL_CR, 0);
                                SendMessage(tab->editorHandle, SCI_CONVERTEOLS, SC_EOL_CR, 0);
                                UpdateLineEndType("CR");
                                SetTabModified(currentTab, TRUE);
                            }
                        }
                    }
                    break;

                case ID_HELP_ABOUT:
                    MessageBox(hWnd, "Notepad+ - Phase 2 Implementation\n\nA lightweight, fast text editor with dark/light theme support",
                              "About Notepad+", MB_ICONINFORMATION | MB_OK);
                    break;
                
                case ID_OPTIONS_INSTALL_SHELL:
                    {
                        if (!IsRunningAsAdministrator()) {
                            int result = MessageBox(hWnd,
                                "Installing shell integration requires administrator privileges.\n\n"
                                "Would you like to restart Notepad+ as administrator?",
                                "Administrator Required",
                                MB_YESNO | MB_ICONQUESTION);
                            
                            if (result == IDYES) {
                                RequestAdministratorPrivileges();
                            }
                            break;
                        }
                        
                        if (InstallShellIntegration(TRUE)) {
                            MessageBox(hWnd,
                                "Shell integration installed successfully!\n\n"
                                "You can now right-click files and folders to open them with Notepad+.",
                                "Success",
                                MB_OK | MB_ICONINFORMATION);
                        }
                    }
                    break;

                case ID_OPTIONS_UNINSTALL_SHELL:
                    {
                        if (!IsRunningAsAdministrator()) {
                            MessageBox(hWnd,
                                "Uninstalling shell integration requires administrator privileges.\n\n"
                                "Please restart Notepad+ as administrator.",
                                "Administrator Required",
                                MB_OK | MB_ICONWARNING);
                            break;
                        }
                        
                        if (UninstallShellIntegration(TRUE)) {
                            MessageBox(hWnd,
                                "Shell integration uninstalled successfully.",
                                "Success",
                                MB_OK | MB_ICONINFORMATION);
                        }
                    }
                    break;

                case ID_OPTIONS_REGISTER_FILES:
                    {
                        if (!IsRunningAsAdministrator()) {
                            int result = MessageBox(hWnd,
                                "Registering file associations requires administrator privileges.\n\n"
                                "Would you like to restart Notepad+ as administrator?",
                                "Administrator Required",
                                MB_YESNO | MB_ICONQUESTION);
                            
                            if (result == IDYES) {
                                RequestAdministratorPrivileges();
                            }
                            break;
                        }
                        
                        if (RegisterFileAssociations()) {
                            MessageBox(hWnd,
                                "File associations registered successfully!\n\n"
                                "Notepad+ is now available in the 'Open With' menu for supported file types.",
                                "Success",
                                MB_OK | MB_ICONINFORMATION);
                        }
                    }
                    break;

                case ID_OPTIONS_UNREGISTER_FILES:
                    {
                        if (!IsRunningAsAdministrator()) {
                            MessageBox(hWnd,
                                "Unregistering file associations requires administrator privileges.\n\n"
                                "Please restart Notepad+ as administrator.",
                                "Administrator Required",
                                MB_OK | MB_ICONWARNING);
                            break;
                        }
                        
                        if (UnregisterFileAssociations()) {
                            MessageBox(hWnd,
                                "File associations unregistered successfully.",
                                "Success",
                                MB_OK | MB_ICONINFORMATION);
                        }
                    }
                    break;
                    
                default:
                    /* Check if it's a recent file menu item */
                    if (LOWORD(wParam) >= ID_FILE_RECENT_BASE && 
                        LOWORD(wParam) < ID_FILE_RECENT_BASE + 10) {
                        int recentIndex = LOWORD(wParam) - ID_FILE_RECENT_BASE;
                        const char* filePath = GetRecentFile(recentIndex);
                        
                        if (filePath && filePath[0] != '\0') {
                            /* Check if file still exists */
                            if (GetFileAttributes(filePath) != INVALID_FILE_ATTRIBUTES) {
                                int tabIndex = AddNewTab(filePath);
                                if (tabIndex >= 0) {
                                    SelectTab(tabIndex);
                                }
                            } else {
                                char msg[MAX_PATH + 100];
                                sprintf(msg, "File not found:\n%s\n\nRemove from recent files list?", filePath);
                                if (MessageBox(hWnd, msg, "Notepad+",
                                             MB_YESNO | MB_ICONWARNING) == IDYES) {
                                    /* Remove from recent files */
                                    SaveConfig();
                                }
                            }
                        }
                    } else {
                        return DefWindowProc(hWnd, uMsg, wParam, lParam);
                    }
            }
            return 0;
            
        case WM_TIMER:
            /* Handle auto-save timer */
            if (wParam == IDT_AUTOSAVE_TIMER) {
                AppConfig* cfg = GetConfig();
                if (cfg && cfg->autoSave) {
                    /* Auto-save all modified tabs that have a file path */
                    int tabCount = GetTabCount();
                    for (int i = 0; i < tabCount; i++) {
                        TabInfo* tab = GetTab(i);
                        if (tab && tab->isModified && strncmp(tab->filePath, "New ", 4) != 0) {
                            SaveTabToFile(i);
                        }
                    }
                }
            }
            return 0;
            
        case WM_CLOSE:
            {
                /* Save session before closing (this includes unsaved documents as temp files) */
                SaveSession();
                
                /* Kill auto-save timer */
                KillTimer(hWnd, IDT_AUTOSAVE_TIMER);
                
                /* Proceed with window destruction - no save prompt */
                DestroyWindow(hWnd);
            }
            return 0;
        
        case WM_USER + 100:  /* WM_DEFERRED_SESSION_LOAD */
            {
                PROFILE_MARK("Deferred session load start");
                
                /* Defer toolbar icon loading - icons load after window is visible */
                DeferToolbarIconLoading();
                
                /* Try to restore previous session first */
                AppConfig* cfg = GetConfig();
                BOOL sessionRestored = FALSE;
                if (cfg && cfg->restoreSession) {
                    sessionRestored = RestoreSession();
                }
                
                /* If no session was restored, create a new empty tab */
                if (!sessionRestored || GetTabCount() == 0) {
                    AddNewTab(NULL);
                }
                
                /* Process command line files - open them as additional tabs */
                ProcessCommandLineFiles();
                
                PROFILE_MARK("Session restored");
                
                /* Defer polish (DirectWrite, syntax, theme) to after startup metrics */
                PostMessage(hWnd, WM_USER + 101, 0, 0);  /* WM_DEFERRED_POLISH */
                
                /* Update status bar with current zoom level after session restore */
                {
                    int currentTab = GetSelectedTab();
                    if (currentTab >= 0) {
                        TabInfo* tab = GetTab(currentTab);
                        if (tab && tab->editorHandle) {
                            int zoomLevel = (int)SendMessage(tab->editorHandle, SCI_GETZOOM, 0, 0);
                            UpdateZoomLevel(zoomLevel);
                        }
                    }
                }
                
                /* Start auto-save timer if enabled */
                if (cfg && cfg->autoSave && cfg->autoSaveInterval > 0) {
                    SetTimer(hWnd, IDT_AUTOSAVE_TIMER, cfg->autoSaveInterval * 1000, NULL);
                }
                
                PROFILE_MARK("Startup complete (total)");
                PROFILE_CLOSE();
            }
            return 0;
            
        case WM_USER + 101:  /* WM_DEFERRED_POLISH - Apply DirectWrite, syntax, theme after startup */
            {
                /* Apply expensive polish operations to all loaded tabs */
                /* This runs after the startup timing is complete, so doesn't affect perceived startup time */
                PolishAllTabs();
            }
            return 0;
            
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
}

/*
 * Process command line arguments and open any specified files
 * Uses __argc and __argv from the C runtime
 */
static void ProcessCommandLineFiles(void)
{
    /* Microsoft C runtime provides these globals for command line access */
    extern int __argc;
    extern char** __argv;
    
    /* Skip the first argument (executable path) and process remaining arguments as file paths */
    for (int i = 1; i < __argc; i++) {
        const char* filePath = __argv[i];
        
        /* Skip empty arguments */
        if (!filePath || filePath[0] == '\0') {
            continue;
        }
        
        /* Skip command-line options (arguments starting with - or /) */
        if (filePath[0] == '-' || filePath[0] == '/') {
            continue;
        }
        
        /* Check if the file exists before trying to open it */
        DWORD fileAttrs = GetFileAttributes(filePath);
        if (fileAttrs != INVALID_FILE_ATTRIBUTES && !(fileAttrs & FILE_ATTRIBUTE_DIRECTORY)) {
            /* File exists and is not a directory - open it */
            int tabIndex = AddNewTab(filePath);
            if (tabIndex >= 0) {
                SelectTab(tabIndex);
            }
        }
    }
}
