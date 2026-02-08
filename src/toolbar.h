/*
 * Toolbar system header for Notepad+
 * Custom toolbar with embedded icons and dropdown menus
 */

#ifndef TOOLBAR_H
#define TOOLBAR_H

#include <windows.h>

/* Toolbar dimensions */
#define TOOLBAR_HEIGHT          32
#define TOOLBAR_BUTTON_WIDTH    28
#define TOOLBAR_BUTTON_HEIGHT   24
#define TOOLBAR_BUTTON_SPACING  2
#define TOOLBAR_MARGIN          4
#define TOOLBAR_ICON_SIZE       16

/* Dropdown menu button dimensions - same as regular buttons */
#define TOOLBAR_DROPDOWN_BUTTON_WIDTH   28
#define TOOLBAR_DROPDOWN_BUTTON_HEIGHT  24

/* Button types */
typedef enum {
    BUTTON_NORMAL,
    BUTTON_SEPARATOR,
    BUTTON_DROPDOWN_MENU,  /* Dropdown menu button */
    BUTTON_TOGGLE          /* Toggle button with active/inactive visual states */
} ToolbarButtonType;

/* Toolbar button structure */
typedef struct {
    int id;                  /* Button command ID */
    int index;               /* Button index */
    HBITMAP icon;            /* Button icon (optional) */
    char* tooltip;           /* Tooltip text */
    char* label;             /* Text label for dropdown menus */
    BOOL enabled;            /* Is button enabled? */
    BOOL pressed;            /* Is button currently pressed? */
    BOOL hovered;            /* Is button currently hovered? */
    BOOL isToggled;          /* For toggle buttons: is currently in active state? */
    BOOL isRightAligned;     /* Is button aligned to the right side? */
    ToolbarButtonType type;  /* Button type */
    HMENU dropdownMenu;      /* Popup menu for dropdown buttons */
} ToolbarButton;

/* Toolbar structure */
typedef struct {
    HWND hwnd;               /* Toolbar window handle */
    HWND parentWindow;       /* Parent window handle */
    HWND tooltipWindow;      /* Tooltip window handle */
    ToolbarButton* buttons;  /* Array of buttons */
    int buttonCount;         /* Number of buttons */
    int maxButtons;          /* Maximum buttons capacity */
    int hoveredButton;       /* Currently hovered button index */
    int pressedButton;       /* Currently pressed button index */
    BOOL isVisible;          /* Toolbar visibility */
    HFONT normalFont;        /* Normal text font */
    HBRUSH backgroundBrush;  /* Background brush */
    HBRUSH hoverBrush;       /* Hover state brush */
    HBRUSH pressedBrush;     /* Pressed state brush */
    HPEN borderPen;          /* Border pen */
} Toolbar;

/* Initialize and cleanup */
BOOL InitializeToolbar(HWND parentWindow);
void CleanupToolbar(void);

/* Get toolbar instance */
Toolbar* GetToolbar(void);

/* Show/hide toolbar */
void ShowToolbar(BOOL show);
BOOL IsToolbarVisible(void);

/* Button management */
int AddToolbarButton(int id, HBITMAP icon, const char* tooltip);
int AddToolbarToggleButton(int id, HBITMAP icon, const char* tooltip, BOOL initialState);
int AddToolbarDropdownButton(int id, const char* label, const char* tooltip);
BOOL RemoveToolbarButton(int id);
BOOL EnableToolbarButton(int id, BOOL enabled);
BOOL SetToolbarButtonToggled(int id, BOOL toggled);
BOOL UpdateToolbarButtonIcon(int id, HBITMAP icon);
BOOL UpdateToolbarButtonTooltip(int id, const char* tooltip);

/* Layout and rendering */
void ResizeToolbar(int width, int yPosition);
void UpdateToolbarLayout(void);
int GetToolbarHeight(void);

/* Hit testing */
int HitTestToolbarButton(int x, int y);

/* Event handling */
void HandleToolbarClick(int x, int y);
void HandleToolbarMouseDown(int x, int y);
void HandleToolbarMouseUp(int x, int y);
void HandleToolbarMouseMove(int x, int y);
void HandleToolbarMouseLeave(void);

/* Tooltip handling */
void UpdateToolbarTooltip(int buttonIndex);
void ShowToolbarTooltip(int x, int y, const char* text);
void HideToolbarTooltip(void);

/* Message handling */
BOOL HandleToolbarMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/* Button utilities */
int GetButtonIndexFromId(int id);
ToolbarButton* GetToolbarButton(int index);

/* Default buttons initialization */
void InitializeDefaultButtons(void);

/* Icon loading */
HBITMAP LoadToolbarIcon(int resourceId);
HBITMAP CreateMonochromeBitmap(int width, int height);

/* Drawing functions */
void DrawToolbarBackground(HDC hdc, RECT* rect);
void DrawToolbarButton(HDC hdc, ToolbarButton* button, RECT* rect);
void DrawToolbarSeparator(HDC hdc, int x, int y, int height);

/* Window procedure */
LRESULT CALLBACK ToolbarWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/* Deferred icon loading for fast startup */
void DeferToolbarIconLoading(void);

/* Dropdown menu management */
HMENU CreateFileDropdownMenu(void);
HMENU CreateEditDropdownMenu(void);
HMENU CreateSettingsDropdownMenu(void);
void UpdateSettingsMenuChecks(HMENU menu);
void ShowDropdownMenu(int buttonIndex, HWND hwnd);

#endif /* TOOLBAR_H */
