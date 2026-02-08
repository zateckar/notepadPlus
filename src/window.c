/*
 * Window management implementation for Notepad+
 */

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "resource.h"
#include "window.h"
#include "editor.h"
#include "tabs.h"
#include "toolbar.h"
#include "statusbar.h"
#include "splitview.h"
#include "config.h"
#include "session.h"
#include "themes.h"
#include "registry_config.h"

/* Check if we're in deferred loading mode (during startup) */
extern BOOL IsDeferredLoadingMode(void);

/* Import profiling macros from main.c when enabled */
#ifdef PROFILE_SESSION
extern LARGE_INTEGER g_startupFreq;
extern LARGE_INTEGER g_startupStart;
extern FILE* g_profileFile;

#define WINDOW_PROFILE_MARK(label) do { \
    LARGE_INTEGER now; \
    QueryPerformanceCounter(&now); \
    double ms = (double)(now.QuadPart - g_startupStart.QuadPart) * 1000.0 / g_startupFreq.QuadPart; \
    char buf[256]; \
    sprintf(buf, "[WINDOW] %s: %.2f ms\n", label, ms); \
    OutputDebugString(buf); \
    if (g_profileFile) { fprintf(g_profileFile, "%s", buf); fflush(g_profileFile); } \
} while(0)
#else
#define WINDOW_PROFILE_MARK(label)
#endif

/* External globals */
extern HINSTANCE g_hInstance;
extern HWND g_hMainWindow;

/* Forward declaration of WndProc */
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/* Window class name registration */
BOOL RegisterWindowClass(void)
{
    WNDCLASSEX wc;
    
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = g_hInstance;
    wc.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_APPICON));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    /* CRITICAL FIX: Set background brush based on current theme to prevent white flash
     * This ensures the window has the correct background color from creation */
    extern Theme GetCurrentTheme(void);
    Theme currentTheme = GetCurrentTheme();
    if (currentTheme == THEME_DARK) {
        wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));  /* Dark theme background */
    } else {
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);  /* Default system background */
    }
    
    wc.lpszMenuName = NULL;  /* No menu in resources - we create it programmatically */
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hIconSm = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_APPICONSMALL));
    
    return RegisterClassEx(&wc);
}

/* Initialize the main window */
BOOL InitializeWindow(HINSTANCE hInstance, int nCmdShow)
{
    int windowX = CW_USEDEFAULT;
    int windowY = CW_USEDEFAULT;
    int windowWidth = 800;
    int windowHeight = 600;
    BOOL windowMaximized = FALSE;
    
    /* Register window class */
    if (!RegisterWindowClass()) {
        return FALSE;
    }
    
    /* Load window position from registry */
    if (!LoadWindowStateFromRegistry(&windowX, &windowY, &windowWidth, &windowHeight, &windowMaximized)) {
        /* Fall back to config values if registry load fails */
        AppConfig* config = GetConfig();
        if (config) {
            windowX = config->windowX;
            windowY = config->windowY;
            windowWidth = config->windowWidth;
            windowHeight = config->windowHeight;
            windowMaximized = config->windowMaximized;
        }
    }
    
    /* Validate window position is on screen */
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    if (windowX != CW_USEDEFAULT) {
        if (windowX < -100 || windowX >= screenWidth) {
            windowX = CW_USEDEFAULT;
        }
        if (windowY < -100 || windowY >= screenHeight) {
            windowY = CW_USEDEFAULT;
        }
    }
    
    /* Ensure reasonable window size */
    if (windowWidth < 200) windowWidth = 800;
    if (windowHeight < 150) windowHeight = 600;
    if (windowWidth > screenWidth + 100) windowWidth = 800;
    if (windowHeight > screenHeight + 100) windowHeight = 600;
    
    /* Create main window with saved position/size */
    g_hMainWindow = CreateWindowEx(
        WS_EX_OVERLAPPEDWINDOW,
        WINDOW_CLASS_NAME,
        "Notepad+",
        WS_OVERLAPPEDWINDOW,
        windowX, windowY, windowWidth, windowHeight,
        NULL, NULL, hInstance, NULL
    );
    
    if (!g_hMainWindow) {
        return FALSE;
    }
    
    /* CRITICAL FIX: Apply theme IMMEDIATELY after creation to eliminate white flash
     * Apply before showing the window to ensure proper dark theme from start */
    extern void ApplyCurrentThemeToWindow(void);
    ApplyCurrentThemeToWindow();
    
    /* Show window - use SW_MAXIMIZE if it was maximized */
    if (windowMaximized) {
        ShowWindow(g_hMainWindow, SW_MAXIMIZE);
    } else {
        ShowWindow(g_hMainWindow, nCmdShow);
    }
    
    /* Force immediate redraw to ensure theme is fully applied */
    UpdateWindow(g_hMainWindow);
    UpdateWindow(g_hMainWindow);
    
    return TRUE;
}

/* Cleanup window resources */
void CleanupWindow(void)
{
    /* Nothing to cleanup for now - window resources are freed automatically */
}

/* Get main window handle */
HWND GetMainWindow(void)
{
    return g_hMainWindow;
}

/* Update window title with file path */
void UpdateWindowTitle(const char* filePath)
{
    if (!g_hMainWindow) {
        return;
    }
    
    char title[MAX_PATH + 64];
    
    if (filePath && filePath[0] != '\0') {
        /* Check if it's a new file (starts with "New ") */
        if (strncmp(filePath, "New ", 4) == 0) {
            /* New unsaved file - show just the app name and file name */
            sprintf(title, "Notepad+ - %s", filePath);
        } else {
            /* Existing file - show app name with full path */
            sprintf(title, "Notepad+ - %s", filePath);
        }
    } else {
        /* No file open - just app name */
        strcpy(title, "Notepad+");
    }
    
    SetWindowText(g_hMainWindow, title);
}

/* Handle editor resize when window size changes */
void HandleEditorResize(int width, int height)
{
    /* This function is deprecated - use HandleWindowResize instead */
    HandleWindowResize(width, height);
}

/* Handle window resizing with proper UI layout */
void HandleWindowResize(int width, int height)
{
    RECT clientRect;
    int currentY = 0;
    int toolbarHeight = 0;
    int tabHeight = TAB_HEIGHT;
    int statusbarHeight = 0;
    int editorHeight;
    
    WINDOW_PROFILE_MARK("HandleWindowResize: enter");
    
    /* If width and height are 0, get the current client rect */
    if (width == 0 && height == 0) {
        GetClientRect(g_hMainWindow, &clientRect);
        width = clientRect.right - clientRect.left;
        height = clientRect.bottom - clientRect.top;
    }
    
    /* Calculate status bar height first */
    if (IsStatusBarVisible()) {
        statusbarHeight = GetStatusBarHeight();
    }
    
    /* Calculate toolbar height */
    if (IsToolbarVisible()) {
        toolbarHeight = GetToolbarHeight();
    }
    
    WINDOW_PROFILE_MARK("HandleWindowResize: after height calcs");
    
    /* Position toolbar (below menu, at top of client area) */
    if (IsToolbarVisible()) {
        ResizeToolbar(width, currentY);
        currentY += toolbarHeight;
    }
    
    WINDOW_PROFILE_MARK("HandleWindowResize: after ResizeToolbar");
    
    /* Position tabs (below toolbar) */
    TabControl* tabControl = GetTabControl();
    if (tabControl && tabControl->hwnd) {
        SetWindowPos(tabControl->hwnd, NULL, 0, currentY, width, tabHeight,
                    SWP_NOZORDER | SWP_NOACTIVATE);
        UpdateTabLayout();
    }
    currentY += tabHeight;
    
    WINDOW_PROFILE_MARK("HandleWindowResize: after tab layout");
    
    /* Calculate remaining height for editor */
    editorHeight = height - currentY - statusbarHeight;
    if (editorHeight < 0) {
        editorHeight = 0;
    }
    
    /* Handle split view or regular tab editors */
    if (IsSplitViewEnabled()) {
        /* Hide all tab editors when split view is active */
        int tabCount = GetTabCount();
        for (int i = 0; i < tabCount; i++) {
            TabInfo* tab = GetTab(i);
            if (tab && tab->editorHandle) {
                ShowWindow(tab->editorHandle, SW_HIDE);
            }
        }
        
        /* Resize split view to fill editor area */
        ResizeSplitView(width, editorHeight);
    } else {
        /* Resize ALL editor windows (not just selected one) */
        int tabCount = GetTabCount();
        WINDOW_PROFILE_MARK("HandleWindowResize: before editor resize loop");
        
        /* During startup, use SWP_NOREDRAW to defer expensive Scintilla painting */
        UINT swpFlags = SWP_NOZORDER | SWP_NOACTIVATE;
        if (IsDeferredLoadingMode()) {
            swpFlags |= SWP_NOREDRAW;
        }
        
        for (int i = 0; i < tabCount; i++) {
            TabInfo* tab = GetTab(i);
            if (tab && tab->editorHandle) {
                SetWindowPos(tab->editorHandle, NULL, 0, currentY, width, editorHeight, swpFlags);
            }
        }
        WINDOW_PROFILE_MARK("HandleWindowResize: after editor resize loop");
    }
    
    /* Resize status bar if visible */
    if (IsStatusBarVisible()) {
        ResizeStatusBar(width, height - statusbarHeight);
    }
    
    WINDOW_PROFILE_MARK("HandleWindowResize: exit");
}
