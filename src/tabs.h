/*
 * Tab system header for Notepad+
 * Custom tab control implementation with close buttons and tooltips
 */

#ifndef TABS_H
#define TABS_H

#include <windows.h>
#include <commctrl.h>
#include "session.h"  /* For SessionTab forward declaration */

/* Tab control configuration constants */
#define MIN_TAB_WIDTH           100
#define MAX_TAB_WIDTH           300
#define TAB_HEIGHT              34
#define CLOSE_BUTTON_SIZE       16
#define TAB_PADDING             10
#define TIP_DELAY               500     /* milliseconds */

/* File encoding types */
typedef enum {
    ENCODING_UTF8,
    ENCODING_UTF8_BOM,
    ENCODING_UTF16_LE,
    ENCODING_UTF16_BE,
    ENCODING_ANSI,
    ENCODING_COUNT
} FileEncoding;

/* Line ending types */
typedef enum {
    LINEEND_CRLF,   /* Windows */
    LINEEND_LF,     /* Unix */
    LINEEND_CR      /* Mac classic */
} LineEnding;

/* Tab states */
typedef enum {
    TAB_STATE_NORMAL,
    TAB_STATE_HOVER,
    TAB_STATE_PRESSED,
    TAB_STATE_SELECTED
} TabState;

/* Tab control data structure */
typedef struct {
    char filePath[MAX_PATH];          /* Full path to the file */
    char displayName[MAX_PATH];       /* Display name (filename + *) */
    HWND editorHandle;                /* Associated Scintilla editor */
    HWND secondaryEditorHandle;       /* Secondary editor for split view */
    BOOL isModified;                  /* Whether the file has been modified */
    BOOL isSplitView;                 /* Whether split view is active */
    BOOL isPinned;                    /* Whether the tab is pinned */
    BOOL isLoaded;                    /* Whether the tab content is loaded (for lazy loading) */
    char savedContent[1];             /* Placeholder - actual content stored separately for new files */
    FileEncoding encoding;            /* File encoding */
    LineEnding lineEnding;            /* Line ending type */
    int zoomLevel;                    /* Zoom percentage (100 = normal) */
    BOOL codeFoldingEnabled;          /* Per-tab code folding state */
    BOOL changeHistoryEnabled;        /* Per-tab change history state */
    TabState state;                   /* Current visual state of the tab */
    int x, y;                         /* Tab position */
    int width, height;                /* Tab dimensions */
    int closeButtonX, closeButtonY;   /* Close button position */
    BOOL isCloseHovered;              /* Whether close button is hovered */
    /* Per-tab view settings */
    BOOL wordWrap;                    /* Word wrap setting */
    BOOL showLineNumbers;             /* Show line numbers setting */
    BOOL showWhitespace;              /* Show whitespace setting */
    BOOL autoIndent;                  /* Auto-indent setting */
    /* Session restore data - used for lazy loading */
    int sessionCursorPos;             /* Cursor position from session */
    int sessionFirstLine;             /* First visible line from session */
    int sessionZoomLevel;             /* Zoom level from session */
    char tempFilePath[MAX_PATH];     /* Temp file path for unsaved files during session restore */
} TabInfo;

/* Tab control state structure */
typedef struct {
    HWND hwnd;                        /* Tab control window handle */
    HWND parentWindow;                /* Parent window handle */
    HWND tooltipWindow;               /* Tooltip window handle */
    HWND minimapWindow;               /* Minimap window handle */
    TabInfo* tabs;                    /* Array of tabs */
    int tabCount;                     /* Number of tabs */
    int maxTabs;                      /* Maximum tabs allocated */
    int selectedIndex;                /* Currently selected tab index */
    int hoveredTab;                   /* Currently hovered tab index (-1 if none) */
    int closeHoveredTab;              /* Tab whose close button is hovered (-1 if none) */
    int scrollOffset;                 /* Horizontal scroll offset for tabs */
    BOOL showAddButton;               /* Whether to show the '+' button */
    BOOL showMinimap;                 /* Whether to show the minimap */
    int addTabX, addTabY;             /* Position of the '+' button */
    HFONT normalFont;                 /* Normal tab font */
    HFONT boldFont;                   /* Selected tab font */
    HCURSOR handCursor;               /* Hand cursor for close button */
    HCURSOR arrowCursor;              /* Regular arrow cursor */
} TabControl;

/* Tab system initialization and cleanup */
BOOL InitializeTabs(HWND parentWindow);
void CleanupTabs(void);
TabControl* GetTabControl(void);

/* Tab creation and management */
int AddNewTab(const char* filePath);
int AddTabWithFile(const char* filePath, BOOL isNewFile);
int AddPlaceholderTab(const char* filePath, BOOL isNewFile, BOOL isPinned);  /* Lazy loading - creates tab without loading content */
BOOL LoadTabContent(int index);  /* Load content for a placeholder tab */
BOOL IsTabLoaded(int index);  /* Check if tab content is loaded */
BOOL CloseTab(int index);
BOOL CloseTabWithConfirmation(int index);
BOOL CloseAllTabs(void);
BOOL CloseAllTabsExcept(int exceptIndex);
void SelectTab(int index);
int GetSelectedTab(void);
int GetTabCount(void);

/* Tab information */
TabInfo* GetTab(int index);
BOOL GetTabInfo(int index, char** filePath, BOOL* isModified);
BOOL SetTabModified(int index, BOOL modified);
BOOL UpdateTabDisplayName(int index);
BOOL SetTabFilePath(int index, const char* filePath);

/* File operations */
BOOL SaveTabFile(int index);
BOOL SaveTabFileAs(int index);
BOOL SaveAllTabs(void);
BOOL ReloadTabFile(int index);

/* Encoding operations */
FileEncoding GetTabEncoding(int index);
BOOL SetTabEncoding(int index, FileEncoding encoding);
BOOL ConvertTabEncoding(int index, FileEncoding newEncoding);
const char* GetEncodingName(FileEncoding encoding);
FileEncoding DetectFileEncoding(const char* filePath);

/* Line ending operations */
LineEnding GetTabLineEnding(int index);
BOOL SetTabLineEnding(int index, LineEnding lineEnding);
const char* GetLineEndingName(LineEnding lineEnding);
LineEnding DetectLineEnding(const char* content, int length);

/* Zoom operations */
int GetTabZoom(int index);
BOOL SetTabZoom(int index, int zoomLevel);
BOOL ZoomIn(int index);
BOOL ZoomOut(int index);
BOOL ZoomReset(int index);

/* Split view operations */
BOOL ToggleSplitView(int index);
BOOL IsSplitViewActive(int index);
HWND GetSecondaryEditor(int index);

/* Minimap operations */
BOOL ShowMinimap(BOOL show);
BOOL IsMinimapVisible(void);
void UpdateMinimap(void);

/* Code folding operations */
void EnableCodeFolding(HWND editor, BOOL enable);
void FoldAll(HWND editor);
void UnfoldAll(HWND editor);
void ToggleFold(HWND editor, int line);

/* Bracket matching */
void EnableBracketMatching(HWND editor, BOOL enable);
void UpdateBracketMatch(HWND editor);

/* Auto-indent */
void EnableAutoIndent(HWND editor, BOOL enable);
void HandleAutoIndent(HWND editor, int ch);

/* Tab control drawing and layout */
void ResizeTabs(int width, int height);
void UpdateTabLayout(void);
void ScrollTabs(int direction);  /* -1 for left, 1 for right */

/* Tab hit testing and interaction */
int HitTestTab(int x, int y);
BOOL HitTestCloseButton(int tabIndex, int x, int y);
void HandleTabClick(int x, int y);
void HandleTabMouseDown(int x, int y);
void HandleTabMouseUp(int x, int y);
void HandleTabMouseMove(int x, int y);
void HandleTabMouseLeave(void);

/* Tooltip management */
void UpdateTooltip(int tabIndex);
void ShowTooltip(int x, int y, const char* text);
void HideTooltip(void);

/* Tab control window procedure (for custom control) */
LRESULT CALLBACK TabWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/* Helper functions */
void GetTabTextDimensions(const char* text, int* width, int* height, BOOL isBold);
void DrawCloseButton(HDC hdc, int x, int y, BOOL isHovered, BOOL isActiveTab);
void DrawAddButton(HDC hdc, int x, int y, BOOL isHovered);
char* GetShortDisplayName(const char* filePath);

/* Tab ID management */
void UpdateNextTabId(void);

/* Deferred loading for fast startup */
void SetDeferredLoadingMode(BOOL defer);
BOOL IsDeferredLoadingMode(void);
int AddTabFast(const char* filePath, BOOL isNewFile);  /* Fast loading without DirectWrite/syntax/theme */
int AddTabFastFromTempFile(const char* filePath, const char* tempFilePath);  /* Load from temp file with unsaved changes */
void PolishEditor(HWND editor, const char* filePath);  /* Apply DirectWrite, syntax, theme after startup */
void PolishAllTabs(void);  /* Polish all loaded tabs - call after startup */

/* Tab pinning operations */
BOOL PinTab(int index);
BOOL UnpinTab(int index);
BOOL IsTabPinned(int index);
void SortPinnedTabs(void);
void ShowTabContextMenu(int tabIndex, int x, int y);

/* Session view settings application */
void ApplySessionViewSettings(int tabIndex, const SessionTab* sessionTab);

#endif /* TABS_H */
