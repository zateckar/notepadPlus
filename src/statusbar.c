/*
 * Status bar system implementation for Notepad+
 * Status bar showing cursor position, file encoding, etc.
 */

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "statusbar.h"
#include "resource.h"
#include "Scintilla.h"
#include "themes.h"
#include "syntax.h"

/* Debug output for profiling */
#define DEBUG_STATUSBAR_INIT 1

#if DEBUG_STATUSBAR_INIT
#include <stdio.h>
static LARGE_INTEGER g_sbFreq;
static LARGE_INTEGER g_sbStart;
static FILE* g_sbProfileFile = NULL;
static int g_sbProfileInitialized = 0;

static void SB_PROFILE_INIT(void) {
    if (g_sbProfileInitialized) return;
    QueryPerformanceFrequency(&g_sbFreq);
    QueryPerformanceCounter(&g_sbStart);
    char profilePath[MAX_PATH];
    GetModuleFileName(NULL, profilePath, MAX_PATH);
    char* sl = strrchr(profilePath, '\\');
    if (sl) { sl[1] = '\0'; strcat(profilePath, "statusbar_profile.txt"); }
    g_sbProfileFile = fopen(profilePath, "w");
    g_sbProfileInitialized = 1;
}

static void SB_PROFILE_MARK(const char* label) {
    if (!g_sbProfileInitialized) SB_PROFILE_INIT();
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double ms = (double)(now.QuadPart - g_sbStart.QuadPart) * 1000.0 / g_sbFreq.QuadPart;
    char buf[256];
    sprintf(buf, "[SB] %s: %.2f ms\n", label, ms);
    OutputDebugString(buf);
    if (g_sbProfileFile) { fprintf(g_sbProfileFile, "%s", buf); fflush(g_sbProfileFile); }
}

static void SB_PROFILE_CLOSE(void) {
    /* Don't close the file - keep it open for the entire session */
    /* if (g_sbProfileFile) { fclose(g_sbProfileFile); g_sbProfileFile = NULL; } */
}
#else
#define SB_PROFILE_INIT()
#define SB_PROFILE_MARK(label)
#define SB_PROFILE_CLOSE()
#endif

/* Global status bar instance */
static StatusBar g_statusBar = {0};

/* Window class name for status bar */
#define STATUSBAR_CONTROL_CLASS_NAME "NotepadPlusStatusBar"

/* Internal helper functions */
static void InvalidateStatusBar(void);
static void InvalidateStatusBarPane(StatusBarPane pane);

/* Initialize status bar system */
BOOL InitializeStatusBar(HWND parentWindow)
{
    WNDCLASSEX wc;
    
#if DEBUG_STATUSBAR_INIT
    SB_PROFILE_INIT();
    SB_PROFILE_MARK("InitializeStatusBar START");
#endif
    
    /* Initialize status bar structure */
    memset(&g_statusBar, 0, sizeof(StatusBar));
    g_statusBar.parentWindow = parentWindow;
    g_statusBar.isVisible = TRUE;
    
    /* Initialize panes */
    for (int i = 0; i < PANE_COUNT; i++) {
        g_statusBar.panes[i].type = (StatusBarPane)i;
        g_statusBar.panes[i].text[0] = '\0';
        g_statusBar.panes[i].width = STATUSBAR_MIN_PANE_WIDTH;
        g_statusBar.panes[i].visible = TRUE;
    }
    
#if DEBUG_STATUSBAR_INIT
    SB_PROFILE_MARK("After pane init");
#endif
    
    /* Set default text for each pane */
    SetStatusBarText(PANE_CURSOR, "Ln 1, Col 1");
    SetStatusBarText(PANE_ENCODING, "UTF-8");
    SetStatusBarText(PANE_FILETYPE, "Text");
    SetStatusBarText(PANE_POSITION, "Pos 1");
    SetStatusBarText(PANE_LINEEND, "CRLF");
    SetStatusBarText(PANE_ZOOM, "100%");
    
    /* Set default widths */
    SetStatusBarPaneWidth(PANE_CURSOR, 80);
    SetStatusBarPaneWidth(PANE_ENCODING, 60);
    SetStatusBarPaneWidth(PANE_FILETYPE, 80);
    SetStatusBarPaneWidth(PANE_POSITION, 80);
    SetStatusBarPaneWidth(PANE_LINEEND, 60);
    SetStatusBarPaneWidth(PANE_ZOOM, 60);
    
#if DEBUG_STATUSBAR_INIT
    SB_PROFILE_MARK("After SetStatusBarText/Width");
#endif
    
    /* Register status bar window class */
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = StatusBarWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = sizeof(StatusBar*);
    wc.hInstance = GetModuleHandle(NULL);
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = STATUSBAR_CONTROL_CLASS_NAME;
    wc.hIconSm = NULL;
    
    if (!RegisterClassEx(&wc)) {
        return FALSE;
    }
    
#if DEBUG_STATUSBAR_INIT
    SB_PROFILE_MARK("After RegisterClassEx");
#endif
    
    /* Create status bar window */
    g_statusBar.hwnd = CreateWindowEx(
        0,
        STATUSBAR_CONTROL_CLASS_NAME,
        "",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 0, 0,
        parentWindow,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
#if DEBUG_STATUSBAR_INIT
    SB_PROFILE_MARK("After CreateWindowEx");
#endif
    
    if (!g_statusBar.hwnd) {
        return FALSE;
    }
    
    /* Store status bar pointer in window data */
    SetWindowLongPtr(g_statusBar.hwnd, 0, (LONG_PTR)&g_statusBar);
    
    /* Create fonts and brushes */
    g_statusBar.normalFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    g_statusBar.backgroundBrush = GetSysColorBrush(COLOR_BTNFACE);
    g_statusBar.borderPen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));
    
#if DEBUG_STATUSBAR_INIT
    SB_PROFILE_MARK("InitializeStatusBar END");
    SB_PROFILE_CLOSE();
#endif
    
    return TRUE;
}

/* Cleanup status bar system */
void CleanupStatusBar(void)
{
    /* Cleanup brushes and pens */
    if (g_statusBar.borderPen) {
        DeleteObject(g_statusBar.borderPen);
    }
    
    /* Destroy status bar window */
    if (g_statusBar.hwnd) {
        DestroyWindow(g_statusBar.hwnd);
    }
    
    /* Unregister window class */
    UnregisterClass(STATUSBAR_CONTROL_CLASS_NAME, GetModuleHandle(NULL));
    
    memset(&g_statusBar, 0, sizeof(StatusBar));
}

/* Get status bar instance */
StatusBar* GetStatusBar(void)
{
    return &g_statusBar;
}

/* Show/hide status bar */
void ShowStatusBar(BOOL show)
{
    if (g_statusBar.isVisible != show) {
        g_statusBar.isVisible = show;
        ShowWindow(g_statusBar.hwnd, show ? SW_SHOW : SW_HIDE);
    }
}

/* Check if status bar is visible */
BOOL IsStatusBarVisible(void)
{
    return g_statusBar.isVisible;
}

/* Set status bar text */
void SetStatusBarText(StatusBarPane pane, const char* text)
{
    if (pane < 0 || pane >= PANE_COUNT) {
        return;
    }
    
    if (text) {
        strncpy(g_statusBar.panes[pane].text, text, sizeof(g_statusBar.panes[pane].text) - 1);
        g_statusBar.panes[pane].text[sizeof(g_statusBar.panes[pane].text) - 1] = '\0';
    } else {
        g_statusBar.panes[pane].text[0] = '\0';
    }
    
    InvalidateStatusBarPane(pane);
}

/* Get status bar text */
const char* GetStatusBarText(StatusBarPane pane)
{
    if (pane < 0 || pane >= PANE_COUNT) {
        return "";
    }
    
    return g_statusBar.panes[pane].text;
}

/* Set status bar pane visibility */
void SetStatusBarPaneVisible(StatusBarPane pane, BOOL visible)
{
    if (pane < 0 || pane >= PANE_COUNT) {
        return;
    }
    
    if (g_statusBar.panes[pane].visible != visible) {
        g_statusBar.panes[pane].visible = visible;
        CalculatePaneLayout();
        InvalidateStatusBar();
    }
}

/* Get status bar pane visibility */
BOOL GetStatusBarPaneVisible(StatusBarPane pane)
{
    if (pane < 0 || pane >= PANE_COUNT) {
        return FALSE;
    }
    
    return g_statusBar.panes[pane].visible;
}

/* Set status bar pane width */
void SetStatusBarPaneWidth(StatusBarPane pane, int width)
{
    if (pane < 0 || pane >= PANE_COUNT || width < STATUSBAR_MIN_PANE_WIDTH) {
        return;
    }
    
    if (g_statusBar.panes[pane].width != width) {
        g_statusBar.panes[pane].width = width;
        /* Only invalidate if window already exists (skip during init) */
        if (g_statusBar.hwnd) {
            CalculatePaneLayout();
            InvalidateStatusBar();
        }
    }
}

/* Get status bar pane width */
int GetStatusBarPaneWidth(StatusBarPane pane)
{
    if (pane < 0 || pane >= PANE_COUNT) {
        return 0;
    }
    
    return g_statusBar.panes[pane].width;
}

/* Resize status bar */
void ResizeStatusBar(int width, int yPosition)
{
    SetWindowPos(g_statusBar.hwnd, NULL, 0, yPosition, width, STATUSBAR_HEIGHT,
                SWP_NOZORDER | SWP_NOACTIVATE);
    
    CalculatePaneLayout();
}

/* Update status bar layout */
void UpdateStatusBarLayout(void)
{
    CalculatePaneLayout();
    InvalidateStatusBar();
}

/* Get status bar height */
int GetStatusBarHeight(void)
{
    return g_statusBar.isVisible ? STATUSBAR_HEIGHT : 0;
}

/* Calculate pane layout */
void CalculatePaneLayout(void)
{
    RECT rc;
    GetClientRect(g_statusBar.hwnd, &rc);
    
    int currentX = STATUSBAR_MARGIN;
    
    /* Calculate positions for visible panes */
    for (int i = 0; i < PANE_COUNT; i++) {
        if (!g_statusBar.panes[i].visible) {
            continue;
        }
        
        g_statusBar.panes[i].x = currentX;
        g_statusBar.panes[i].y = 2;
        
        currentX += g_statusBar.panes[i].width + STATUSBAR_PANE_SPACING;
    }
}

/* Update cursor position */
void UpdateCursorPosition(int line, int column)
{
    char text[32];
    sprintf(text, "Ln %d, Col %d", line + 1, column + 1);
    SetStatusBarText(PANE_CURSOR, text);
}

/* Update file position */
void UpdateFilePosition(long position)
{
    char text[32];
    sprintf(text, "Pos %ld", position + 1);
    SetStatusBarText(PANE_POSITION, text);
}

/* Update file encoding */
void UpdateFileEncoding(const char* encoding)
{
    SetStatusBarText(PANE_ENCODING, encoding ? encoding : "Unknown");
}

/* Update file type */
void UpdateFileType(const char* fileType)
{
    SetStatusBarText(PANE_FILETYPE, fileType ? fileType : "Text");
}

/* Update line ending type */
void UpdateLineEndType(const char* lineEnd)
{
    SetStatusBarText(PANE_LINEEND, lineEnd ? lineEnd : "Unknown");
}

/* Update zoom level */
void UpdateZoomLevel(int zoomLevel)
{
    char text[32];
    /* Scintilla zoom: 0 = 100%, each point is ~10% change */
    /* Calculate percentage: 100% + (zoomLevel * 10%) */
    int percentage = 100 + (zoomLevel * 10);
    sprintf(text, "%d%%", percentage);
    SetStatusBarText(PANE_ZOOM, text);
}

/* Get encoding from Scintilla */
const char* GetEncodingFromScintilla(int encoding)
{
    switch (encoding) {
        case SC_CP_UTF8:
            return "UTF-8";
        case 932:
            return "Shift-JIS";
        case 936:
            return "GBK";
        case 949:
            return "EUC-KR";
        case 950:
            return "Big5";
        case 1252:
            return "ANSI";
        default:
            return "Unknown";
    }
}

/* Get line end type from Scintilla */
const char* GetLineEndTypeFromScintilla(int lineEndMode)
{
    switch (lineEndMode) {
        case 0:  /* SC_EOL_CRLF */
            return "CRLF";
        case 1:  /* SC_EOL_CR */
            return "CR";
        case 2:  /* SC_EOL_LF */
            return "LF";
        default:
            return "Unknown";
    }
}

/* Get file type from file path using generated language detection */
const char* GetFileTypeFromPath(const char* filePath)
{
    SB_PROFILE_MARK("GetFileTypeFromPath: enter");
    
    if (!filePath || filePath[0] == '\0') {
        SB_PROFILE_MARK("GetFileTypeFromPath: null/empty path");
        return "Text";
    }

    SB_PROFILE_MARK("GetFileTypeFromPath: before DetectLanguage");
    /* Use the generated language detection system */
    LanguageType lang = DetectLanguage(filePath);
    SB_PROFILE_MARK("GetFileTypeFromPath: after DetectLanguage");
    
    SB_PROFILE_MARK("GetFileTypeFromPath: before GetLanguageName");
    const char* langName = GetLanguageName(lang);
    SB_PROFILE_MARK("GetFileTypeFromPath: after GetLanguageName");
    
    /* Validate return value */
    if (!langName || langName[0] == '\0') {
        SB_PROFILE_MARK("GetFileTypeFromPath: null/empty langName");
        return "Text";
    }
    
    SB_PROFILE_MARK("GetFileTypeFromPath: exit");
    return langName;
}

/* Legacy function - kept for compatibility but uses new system */
const char* GetFileTypeFromExtension(const char* extension)
{
    if (!extension) {
        return "Text";
    }

    /* Create a dummy filename with the extension for detection */
    char dummyFilename[256];
    sprintf(dummyFilename, "dummy%s", extension);

    /* Use the generated language detection system */
    LanguageType lang = DetectLanguage(dummyFilename);
    return GetLanguageName(lang);
}

/* Invalidate status bar */
static void InvalidateStatusBar(void)
{
    InvalidateRect(g_statusBar.hwnd, NULL, TRUE);
}

/* Invalidate status bar pane */
static void InvalidateStatusBarPane(StatusBarPane pane)
{
    /* Skip if window not created yet (during init) */
    if (!g_statusBar.hwnd) {
        return;
    }
    
    if (pane < 0 || pane >= PANE_COUNT || !g_statusBar.panes[pane].visible) {
        return;
    }
    
    StatusBarPaneInfo* paneInfo = &g_statusBar.panes[pane];
    RECT rc = {paneInfo->x, paneInfo->y, paneInfo->x + paneInfo->width, paneInfo->y + STATUSBAR_HEIGHT - 2};
    InvalidateRect(g_statusBar.hwnd, &rc, TRUE);
}

/* Hit test which pane was clicked */
static StatusBarPane HitTestPane(int x, int y)
{
    UNREFERENCED_PARAMETER(y);
    
    StatusBar* sb = GetStatusBar();
    for (int i = 0; i < PANE_COUNT; i++) {
        if (!sb->panes[i].visible) continue;
        
        if (x >= sb->panes[i].x && x < sb->panes[i].x + sb->panes[i].width) {
            return (StatusBarPane)i;
        }
    }
    return -1;  /* No pane hit */
}

/* Show encoding context menu */
static void ShowEncodingContextMenu(HWND hwnd, int x, int y)
{
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_ENCODING_UTF8, "UTF-8");
    AppendMenu(hMenu, MF_STRING, ID_ENCODING_UTF8BOM, "UTF-8 with BOM");
    AppendMenu(hMenu, MF_STRING, ID_ENCODING_UTF16LE, "UTF-16 LE");
    AppendMenu(hMenu, MF_STRING, ID_ENCODING_UTF16BE, "UTF-16 BE");
    AppendMenu(hMenu, MF_STRING, ID_ENCODING_ANSI, "ANSI");
    
    POINT pt = {x, y};
    ClientToScreen(hwnd, &pt);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, GetParent(hwnd), NULL);
    DestroyMenu(hMenu);
}

/* Show line ending context menu */
static void ShowLineEndingContextMenu(HWND hwnd, int x, int y)
{
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_LINEEND_CRLF, "Windows (CRLF)");
    AppendMenu(hMenu, MF_STRING, ID_LINEEND_LF, "Unix (LF)");
    AppendMenu(hMenu, MF_STRING, ID_LINEEND_CR, "Mac (CR)");
    
    POINT pt = {x, y};
    ClientToScreen(hwnd, &pt);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, GetParent(hwnd), NULL);
    DestroyMenu(hMenu);
}

/* Status bar window procedure */
LRESULT CALLBACK StatusBarWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    StatusBar* statusBar = (StatusBar*)GetWindowLongPtr(hwnd, 0);
    UNREFERENCED_PARAMETER(statusBar);
    
    switch (uMsg) {
        case WM_CREATE:
            return 0;
            
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                RECT rc;
                GetClientRect(hwnd, &rc);
                
                /* Draw background and all panes */
                DrawStatusBar(hdc, &rc);
                
                EndPaint(hwnd, &ps);
                return 0;
            }
            
        case WM_LBUTTONDOWN:
            {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                StatusBarPane pane = HitTestPane(x, y);
                
                if (pane == PANE_ENCODING) {
                    ShowEncodingContextMenu(hwnd, x, y);
                } else if (pane == PANE_LINEEND) {
                    ShowLineEndingContextMenu(hwnd, x, y);
                }
                return 0;
            }
            
        case WM_SIZE:
            CalculatePaneLayout();
            return 0;
            
        case WM_ERASEBKGND:
            return 1;  /* Handle in WM_PAINT */
            
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

/* Draw status bar with theme-aware colors */
void DrawStatusBar(HDC hdc, RECT* rect)
{
    /* Get theme colors for consistent styling */
    ThemeColors* colors = GetThemeColors();
    StatusBar* sb = GetStatusBar();
    
    /* Draw background with theme color */
    HBRUSH bgBrush = CreateSolidBrush(colors->statusbarBg);
    FillRect(hdc, rect, bgBrush);
    DeleteObject(bgBrush);
    
    /* Draw subtle top border using theme color */
    HPEN borderPen = CreatePen(PS_SOLID, 1, colors->statusbarBorder);
    HPEN oldPen = SelectObject(hdc, borderPen);
    MoveToEx(hdc, 0, 0, NULL);
    LineTo(hdc, rect->right, 0);
    
    /* Draw visible panes */
    for (int i = 0; i < PANE_COUNT; i++) {
        if (!sb->panes[i].visible) {
            continue;
        }
        
        DrawStatusBarPane(hdc, &sb->panes[i]);
        
        /* Draw subtle separator between panes (except after last pane) */
        if (i < PANE_COUNT - 1) {
            int nextPane = i + 1;
            while (nextPane < PANE_COUNT && !sb->panes[nextPane].visible) {
                nextPane++;
            }
            
            if (nextPane < PANE_COUNT) {
                int sepX = sb->panes[i].x + sb->panes[i].width + STATUSBAR_PANE_SPACING / 2;
                /* Draw shorter, more subtle separator */
                int sepTop = 4;
                int sepBottom = STATUSBAR_HEIGHT - 4;
                MoveToEx(hdc, sepX, sepTop, NULL);
                LineTo(hdc, sepX, sepBottom);
            }
        }
    }
    
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
}

/* Draw status bar pane with theme-aware colors */
void DrawStatusBarPane(HDC hdc, StatusBarPaneInfo* pane)
{
    if (!pane->visible) {
        return;
    }
    
    /* Get theme colors */
    ThemeColors* colors = GetThemeColors();
    
    /* Set text color from theme and font */
    SetTextColor(hdc, colors->statusbarFg);
    SetBkMode(hdc, TRANSPARENT);
    HFONT oldFont = SelectObject(hdc, GetStatusBar()->normalFont);
    
    /* Draw pane text with proper vertical centering */
    RECT textRect = {pane->x + 6, pane->y, pane->x + pane->width - 6, pane->y + STATUSBAR_HEIGHT - 4};
    DrawText(hdc, pane->text, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    
    SelectObject(hdc, oldFont);
}
