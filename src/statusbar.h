/*
 * Status bar system header for Notepad+
 * Status bar implementation showing cursor position, file encoding, etc.
 */

#ifndef STATUSBAR_H
#define STATUSBAR_H

#include <windows.h>
#include <commctrl.h>

/* Status bar configuration constants */
#define STATUSBAR_HEIGHT          20
#define STATUSBAR_MARGIN          2
#define STATUSBAR_PANE_SPACING    8
#define STATUSBAR_MIN_PANE_WIDTH  80

/* Status bar pane types */
typedef enum {
    PANE_CURSOR,        /* Cursor position (Line, Column) */
    PANE_ENCODING,      /* File encoding (UTF-8, UTF-16, etc.) */
    PANE_FILETYPE,      /* File type (Text, C++, Python, etc.) */
    PANE_POSITION,      /* Character position in file */
    PANE_LINEEND,       /* Line ending type (CRLF, LF, CR) */
    PANE_ZOOM,          /* Zoom level */
    PANE_COUNT          /* Number of panes */
} StatusBarPane;

/* Status bar pane information */
typedef struct {
    StatusBarPane type;         /* Pane type */
    char text[128];             /* Display text */
    int width;                  /* Pane width */
    int x, y;                   /* Pane position */
    BOOL visible;               /* Whether pane is visible */
} StatusBarPaneInfo;

/* Status bar state structure */
typedef struct {
    HWND hwnd;                  /* Status bar window handle */
    HWND parentWindow;          /* Parent window handle */
    StatusBarPaneInfo panes[PANE_COUNT];  /* Array of panes */
    HFONT normalFont;           /* Normal font */
    HBRUSH backgroundBrush;     /* Background brush */
    HPEN borderPen;             /* Border pen */
    BOOL isVisible;             /* Whether status bar is visible */
} StatusBar;

/* Status bar initialization and cleanup */
BOOL InitializeStatusBar(HWND parentWindow);
void CleanupStatusBar(void);
StatusBar* GetStatusBar(void);

/* Status bar visibility */
void ShowStatusBar(BOOL show);
BOOL IsStatusBarVisible(void);

/* Pane management */
void SetStatusBarText(StatusBarPane pane, const char* text);
const char* GetStatusBarText(StatusBarPane pane);
void SetStatusBarPaneVisible(StatusBarPane pane, BOOL visible);
BOOL GetStatusBarPaneVisible(StatusBarPane pane);
void SetStatusBarPaneWidth(StatusBarPane pane, int width);
int GetStatusBarPaneWidth(StatusBarPane pane);

/* Layout and sizing */
void ResizeStatusBar(int width, int yPosition);
void UpdateStatusBarLayout(void);
int GetStatusBarHeight(void);

/* Drawing functions */
void DrawStatusBar(HDC hdc, RECT* rect);
void DrawStatusBarPane(HDC hdc, StatusBarPaneInfo* pane);

/* Information update functions */
void UpdateCursorPosition(int line, int column);
void UpdateFilePosition(long position);
void UpdateFileEncoding(const char* encoding);
void UpdateFileType(const char* fileType);
void UpdateLineEndType(const char* lineEnd);
void UpdateZoomLevel(int zoomLevel);

/* Helper functions */
const char* GetEncodingFromScintilla(int encoding);
const char* GetLineEndTypeFromScintilla(int lineEndMode);
const char* GetFileTypeFromExtension(const char* extension);
const char* GetFileTypeFromPath(const char* filePath);
void CalculatePaneLayout(void);

/* Status bar window procedure (for custom control) */
LRESULT CALLBACK StatusBarWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif /* STATUSBAR_H */