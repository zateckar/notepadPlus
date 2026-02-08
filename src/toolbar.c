/*
 * Toolbar system implementation for Notepad+
 * Toolbar with embedded icons and tooltips
 */

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "toolbar.h"
#include "resource.h"
#include "themes.h"
#include "config.h"
#include "editor.h"
#include "statusbar.h"
#include "tabs.h"
#include <windowsx.h>

/* Global toolbar instance */
static Toolbar g_toolbar = {0};

/* Cached icons for toolbar buttons */
typedef struct {
    int buttonId;
    HICON hIcon;
} CachedIcon;

#define MAX_CACHED_ICONS 10
static CachedIcon g_cachedIcons[MAX_CACHED_ICONS] = {0};
static int g_cachedIconCount = 0;
static BOOL g_iconsInitialized = FALSE;

/* Window class name for toolbar */
#define TOOLBAR_CONTROL_CLASS_NAME "NotepadPlusToolbar"

/* Internal helper functions */
static void InvalidateToolbar(void);
static void InvalidateToolbarButton(int index);
static BOOL AddSeparator(void);
static void InitializeToolbarIcons(void);
static void CleanupToolbarIcons(void);
static int GetButtonXPosition(int buttonIndex, int toolbarWidth);

/* Calculate the X position of a button, accounting for right-aligned buttons
 * Returns the X coordinate where the button should be drawn
 */
static int GetButtonXPosition(int buttonIndex, int toolbarWidth)
{
    if (buttonIndex < 0 || buttonIndex >= g_toolbar.buttonCount) {
        return -1;
    }
    
    ToolbarButton* targetButton = &g_toolbar.buttons[buttonIndex];
    
    /* If button is not right-aligned, calculate from left as usual */
    if (!targetButton->isRightAligned) {
        int currentX = TOOLBAR_MARGIN;
        for (int i = 0; i < buttonIndex; i++) {
            ToolbarButton* button = &g_toolbar.buttons[i];
            if (button->id == -1) {
                /* Separator */
                currentX += TOOLBAR_BUTTON_SPACING * 2;
            } else if (button->type == BUTTON_DROPDOWN_MENU) {
                currentX += TOOLBAR_DROPDOWN_BUTTON_WIDTH + TOOLBAR_BUTTON_SPACING;
            } else {
                currentX += TOOLBAR_BUTTON_WIDTH + TOOLBAR_BUTTON_SPACING;
            }
        }
        return currentX;
    }
    
    /* Button is right-aligned - calculate from right edge */
    int rightX = toolbarWidth - TOOLBAR_MARGIN;
    
    /* Count total width of all right-aligned buttons */
    int rightAlignedWidth = 0;
    for (int i = buttonIndex; i < g_toolbar.buttonCount; i++) {
        ToolbarButton* button = &g_toolbar.buttons[i];
        if (button->isRightAligned) {
            if (button->type == BUTTON_DROPDOWN_MENU) {
                rightAlignedWidth += TOOLBAR_DROPDOWN_BUTTON_WIDTH + TOOLBAR_BUTTON_SPACING;
            } else if (button->id != -1) {
                rightAlignedWidth += TOOLBAR_BUTTON_WIDTH + TOOLBAR_BUTTON_SPACING;
            } else {
                rightAlignedWidth += TOOLBAR_BUTTON_SPACING * 2;
            }
        }
    }
    
    /* Position right-aligned buttons starting from the right edge */
    int currentX = rightX - rightAlignedWidth;
    
    /* Adjust for the target button specifically */
    for (int i = buttonIndex + 1; i < g_toolbar.buttonCount; i++) {
        ToolbarButton* button = &g_toolbar.buttons[i];
        if (button->isRightAligned) {
            if (button->type == BUTTON_DROPDOWN_MENU) {
                currentX += TOOLBAR_DROPDOWN_BUTTON_WIDTH + TOOLBAR_BUTTON_SPACING;
            } else if (button->id != -1) {
                currentX += TOOLBAR_BUTTON_WIDTH + TOOLBAR_BUTTON_SPACING;
            } else {
                currentX += TOOLBAR_BUTTON_SPACING * 2;
            }
        }
    }
    
    return currentX;
}

/* Initialize toolbar system */
BOOL InitializeToolbar(HWND parentWindow)
{
    WNDCLASSEX wc;
    HFONT systemFont;
    
    /* Initialize toolbar structure */
    memset(&g_toolbar, 0, sizeof(Toolbar));
    g_toolbar.parentWindow = parentWindow;
    g_toolbar.hoveredButton = -1;
    g_toolbar.pressedButton = -1;
    g_toolbar.isVisible = TRUE;
    
    /* Allocate initial buttons */
    g_toolbar.maxButtons = 20;
    g_toolbar.buttons = (ToolbarButton*)malloc(sizeof(ToolbarButton) * g_toolbar.maxButtons);
    if (!g_toolbar.buttons) {
        return FALSE;
    }
    
    /* Register toolbar window class */
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = ToolbarWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = sizeof(Toolbar*);
    wc.hInstance = GetModuleHandle(NULL);
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;  /* We'll paint our own background */
    wc.lpszMenuName = NULL;
    wc.lpszClassName = TOOLBAR_CONTROL_CLASS_NAME;
    wc.hIconSm = NULL;
    
    if (!RegisterClassEx(&wc)) {
        free(g_toolbar.buttons);
        return FALSE;
    }
    
    /* Create toolbar window */
    g_toolbar.hwnd = CreateWindowEx(
        0,
        TOOLBAR_CONTROL_CLASS_NAME,
        "",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 0, 0,
        parentWindow,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!g_toolbar.hwnd) {
        free(g_toolbar.buttons);
        return FALSE;
    }
    
    /* Store toolbar pointer in window data */
    SetWindowLongPtr(g_toolbar.hwnd, 0, (LONG_PTR)&g_toolbar);
    
    /* Create tooltip window */
    g_toolbar.tooltipWindow = CreateWindowEx(
        WS_EX_TOPMOST,
        TOOLTIPS_CLASS,
        NULL,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        g_toolbar.hwnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (g_toolbar.tooltipWindow) {
        SetWindowPos(g_toolbar.tooltipWindow, HWND_TOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    
    /* Create fonts and brushes */
    systemFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    g_toolbar.normalFont = systemFont;
    g_toolbar.backgroundBrush = GetSysColorBrush(COLOR_BTNFACE);
    g_toolbar.hoverBrush = CreateSolidBrush(RGB(220, 220, 220));
    g_toolbar.pressedBrush = CreateSolidBrush(RGB(200, 200, 200));
    g_toolbar.borderPen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));
    
    /* Initialize default toolbar buttons */
    InitializeDefaultButtons();
    
    return TRUE;
}

/* Cleanup toolbar system */
void CleanupToolbar(void)
{
    /* Cleanup cached toolbar icons */
    CleanupToolbarIcons();
    
    /* Cleanup buttons and icons */
    if (g_toolbar.buttons) {
        for (int i = 0; i < g_toolbar.buttonCount; i++) {
            ToolbarButton* button = &g_toolbar.buttons[i];
            if (button->icon) {
                DeleteObject(button->icon);
            }
            if (button->tooltip) {
                free(button->tooltip);
            }
        }
        free(g_toolbar.buttons);
        g_toolbar.buttons = NULL;
    }
    
    /* Cleanup brushes and pens */
    if (g_toolbar.hoverBrush) {
        DeleteObject(g_toolbar.hoverBrush);
    }
    if (g_toolbar.pressedBrush) {
        DeleteObject(g_toolbar.pressedBrush);
    }
    if (g_toolbar.borderPen) {
        DeleteObject(g_toolbar.borderPen);
    }
    
    /* Destroy tooltip */
    if (g_toolbar.tooltipWindow) {
        DestroyWindow(g_toolbar.tooltipWindow);
    }
    
    /* Destroy toolbar window */
    if (g_toolbar.hwnd) {
        DestroyWindow(g_toolbar.hwnd);
    }
    
    /* Unregister window class */
    UnregisterClass(TOOLBAR_CONTROL_CLASS_NAME, GetModuleHandle(NULL));
    
    memset(&g_toolbar, 0, sizeof(Toolbar));
}

/* Get toolbar instance */
Toolbar* GetToolbar(void)
{
    return &g_toolbar;
}

/* Show/hide toolbar */
void ShowToolbar(BOOL show)
{
    if (g_toolbar.isVisible != show) {
        g_toolbar.isVisible = show;
        ShowWindow(g_toolbar.hwnd, show ? SW_SHOW : SW_HIDE);
    }
}

/* Check if toolbar is visible */
BOOL IsToolbarVisible(void)
{
    return g_toolbar.isVisible;
}

/* Add toolbar button */
int AddToolbarButton(int id, HBITMAP icon, const char* tooltip)
{
    ToolbarButton* button;
    
    /* Expand buttons array if needed */
    if (g_toolbar.buttonCount >= g_toolbar.maxButtons) {
        int newMax = g_toolbar.maxButtons * 2;
        ToolbarButton* newButtons = (ToolbarButton*)realloc(g_toolbar.buttons, sizeof(ToolbarButton) * newMax);
        if (!newButtons) {
            return -1;
        }
        g_toolbar.buttons = newButtons;
        g_toolbar.maxButtons = newMax;
    }
    
    /* Initialize new button */
    button = &g_toolbar.buttons[g_toolbar.buttonCount];
    memset(button, 0, sizeof(ToolbarButton));
    
    button->id = id;
    button->index = g_toolbar.buttonCount;
    button->icon = icon;
    button->enabled = TRUE;
    button->pressed = FALSE;
    button->hovered = FALSE;
    
    if (tooltip) {
        button->tooltip = _strdup(tooltip);
    } else {
        button->tooltip = NULL;
    }
    
    g_toolbar.buttonCount++;
    UpdateToolbarLayout();
    
    return g_toolbar.buttonCount - 1;
}

/* Add toggle toolbar button */
int AddToolbarToggleButton(int id, HBITMAP icon, const char* tooltip, BOOL initialState)
{
    ToolbarButton* button;
    
    /* Expand buttons array if needed */
    if (g_toolbar.buttonCount >= g_toolbar.maxButtons) {
        int newMax = g_toolbar.maxButtons * 2;
        ToolbarButton* newButtons = (ToolbarButton*)realloc(g_toolbar.buttons, sizeof(ToolbarButton) * newMax);
        if (!newButtons) {
            return -1;
        }
        g_toolbar.buttons = newButtons;
        g_toolbar.maxButtons = newMax;
    }
    
    /* Initialize new button */
    button = &g_toolbar.buttons[g_toolbar.buttonCount];
    memset(button, 0, sizeof(ToolbarButton));
    
    button->id = id;
    button->index = g_toolbar.buttonCount;
    button->icon = icon;
    button->enabled = TRUE;
    button->pressed = FALSE;
    button->hovered = FALSE;
    button->type = BUTTON_TOGGLE;
    button->isToggled = initialState;
    
    if (tooltip) {
        button->tooltip = _strdup(tooltip);
    } else {
        button->tooltip = NULL;
    }
    
    g_toolbar.buttonCount++;
    UpdateToolbarLayout();
    
    return g_toolbar.buttonCount - 1;
}

/* Add separator */
static BOOL AddSeparator(void)
{
    ToolbarButton* button;
    
    /* Expand buttons array if needed */
    if (g_toolbar.buttonCount >= g_toolbar.maxButtons) {
        int newMax = g_toolbar.maxButtons * 2;
        ToolbarButton* newButtons = (ToolbarButton*)realloc(g_toolbar.buttons, sizeof(ToolbarButton) * newMax);
        if (!newButtons) {
            return FALSE;
        }
        g_toolbar.buttons = newButtons;
        g_toolbar.maxButtons = newMax;
    }
    
    /* Initialize separator button */
    button = &g_toolbar.buttons[g_toolbar.buttonCount];
    memset(button, 0, sizeof(ToolbarButton));
    
    button->id = -1; /* Separator indicator */
    button->index = g_toolbar.buttonCount;
    button->enabled = TRUE;
    
    g_toolbar.buttonCount++;
    
    return TRUE;
}

/* Remove toolbar button */
BOOL RemoveToolbarButton(int id)
{
    int index = GetButtonIndexFromId(id);
    if (index < 0) {
        return FALSE;
    }
    
    ToolbarButton* button = &g_toolbar.buttons[index];
    
    /* Cleanup button resources */
    if (button->icon) {
        DeleteObject(button->icon);
    }
    if (button->tooltip) {
        free(button->tooltip);
    }
    
    /* Shift remaining buttons */
    for (int i = index; i < g_toolbar.buttonCount - 1; i++) {
        g_toolbar.buttons[i] = g_toolbar.buttons[i + 1];
        g_toolbar.buttons[i].index = i;
    }
    
    g_toolbar.buttonCount--;
    UpdateToolbarLayout();
    
    return TRUE;
}

/* Enable/disable toolbar button */
BOOL EnableToolbarButton(int id, BOOL enabled)
{
    int index = GetButtonIndexFromId(id);
    if (index < 0) {
        return FALSE;
    }
    
    ToolbarButton* button = &g_toolbar.buttons[index];
    button->enabled = enabled;
    InvalidateToolbarButton(index);
    
    return TRUE;
}

/* Set toggle button state */
BOOL SetToolbarButtonToggled(int id, BOOL toggled)
{
    int index = GetButtonIndexFromId(id);
    if (index < 0) {
        return FALSE;
    }
    
    ToolbarButton* button = &g_toolbar.buttons[index];
    if (button->type != BUTTON_TOGGLE) {
        return FALSE;
    }
    
    button->isToggled = toggled;
    InvalidateToolbarButton(index);
    
    return TRUE;
}

/* Update toolbar button icon */
BOOL UpdateToolbarButtonIcon(int id, HBITMAP icon)
{
    int index = GetButtonIndexFromId(id);
    if (index < 0) {
        return FALSE;
    }
    
    ToolbarButton* button = &g_toolbar.buttons[index];
    
    if (button->icon) {
        DeleteObject(button->icon);
    }
    
    button->icon = icon;
    InvalidateToolbarButton(index);
    
    return TRUE;
}

/* Update toolbar button tooltip */
BOOL UpdateToolbarButtonTooltip(int id, const char* tooltip)
{
    int index = GetButtonIndexFromId(id);
    if (index < 0) {
        return FALSE;
    }
    
    ToolbarButton* button = &g_toolbar.buttons[index];
    
    if (button->tooltip) {
        free(button->tooltip);
    }
    
    button->tooltip = tooltip ? _strdup(tooltip) : NULL;
    
    return TRUE;
}

/* Resize toolbar */
void ResizeToolbar(int width, int yPosition)
{
    SetWindowPos(g_toolbar.hwnd, NULL, 0, yPosition, width, TOOLBAR_HEIGHT,
                SWP_NOZORDER | SWP_NOACTIVATE);
    
    UpdateToolbarLayout();
}

/* Update toolbar layout */
void UpdateToolbarLayout(void)
{
    InvalidateToolbar();
}

/* Get toolbar height */
int GetToolbarHeight(void)
{
    return g_toolbar.isVisible ? TOOLBAR_HEIGHT : 0;
}

/* Hit test for toolbar button */
int HitTestToolbarButton(int x, int y)
{
    if (!g_toolbar.isVisible || g_toolbar.buttonCount == 0) {
        return -1;
    }
    
    /* Get toolbar width for right-aligned button calculations */
    RECT toolbarRect;
    GetClientRect(g_toolbar.hwnd, &toolbarRect);
    int toolbarWidth = toolbarRect.right - toolbarRect.left;
    
    int buttonY = (TOOLBAR_HEIGHT - TOOLBAR_BUTTON_HEIGHT) / 2;
    
    for (int i = 0; i < g_toolbar.buttonCount; i++) {
        ToolbarButton* button = &g_toolbar.buttons[i];
        
        if (button->id == -1) {
            /* Separator - skip for hit testing */
            continue;
        }
        
        /* Get button position using helper function */
        int currentX = GetButtonXPosition(i, toolbarWidth);
        int buttonWidth = (button->type == BUTTON_DROPDOWN_MENU) ? TOOLBAR_DROPDOWN_BUTTON_WIDTH : TOOLBAR_BUTTON_WIDTH;
        
        /* Check if point is within button bounds */
        if (x >= currentX && x < currentX + buttonWidth &&
            y >= buttonY && y < buttonY + TOOLBAR_BUTTON_HEIGHT) {
            return i;
        }
    }
    
    return -1;
}

/* Handle toolbar click */
void HandleToolbarClick(int x, int y)
{
    int buttonIndex = HitTestToolbarButton(x, y);
    if (buttonIndex >= 0) {
        ToolbarButton* button = &g_toolbar.buttons[buttonIndex];
        if (button->enabled && button->id != -1) {
            /* Send command to parent window */
            SendMessage(g_toolbar.parentWindow, WM_COMMAND, button->id, 0);
        }
    }
}

/* Handle toolbar mouse down */
void HandleToolbarMouseDown(int x, int y)
{
    int buttonIndex = HitTestToolbarButton(x, y);
    if (buttonIndex >= 0) {
        ToolbarButton* button = &g_toolbar.buttons[buttonIndex];
        if (button->enabled) {
            g_toolbar.pressedButton = buttonIndex;
            button->pressed = TRUE;
            InvalidateToolbarButton(buttonIndex);
        }
    }
}

/* Handle toolbar mouse up */
void HandleToolbarMouseUp(int x, int y)
{
    if (g_toolbar.pressedButton >= 0) {
        ToolbarButton* button = &g_toolbar.buttons[g_toolbar.pressedButton];
        button->pressed = FALSE;
        
        int buttonIndex = HitTestToolbarButton(x, y);
        if (buttonIndex == g_toolbar.pressedButton && button->enabled) {
            HandleToolbarClick(x, y);
        }
        
        InvalidateToolbarButton(g_toolbar.pressedButton);
        g_toolbar.pressedButton = -1;
    }
}

/* Handle toolbar mouse move */
void HandleToolbarMouseMove(int x, int y)
{
    int newHoveredButton = HitTestToolbarButton(x, y);
    
    /* Update cursor */
    if (newHoveredButton >= 0) {
        SetCursor(LoadCursor(NULL, IDC_ARROW));
    }
    
    /* Update hovered states */
    if (newHoveredButton != g_toolbar.hoveredButton) {
        /* Clear previous hover state */
        if (g_toolbar.hoveredButton >= 0 && g_toolbar.hoveredButton < g_toolbar.buttonCount) {
            ToolbarButton* prevButton = &g_toolbar.buttons[g_toolbar.hoveredButton];
            prevButton->hovered = FALSE;
            InvalidateToolbarButton(g_toolbar.hoveredButton);
        }
        
        g_toolbar.hoveredButton = newHoveredButton;
        
        /* Set new hover state */
        if (newHoveredButton >= 0 && newHoveredButton < g_toolbar.buttonCount) {
            ToolbarButton* newButton = &g_toolbar.buttons[newHoveredButton];
            newButton->hovered = TRUE;
            InvalidateToolbarButton(newHoveredButton);
            
            /* Update tooltip */
            UpdateToolbarTooltip(newHoveredButton);
        } else {
            HideToolbarTooltip();
        }
    }
}

/* Handle toolbar mouse leave */
void HandleToolbarMouseLeave(void)
{
    if (g_toolbar.hoveredButton >= 0) {
        ToolbarButton* button = &g_toolbar.buttons[g_toolbar.hoveredButton];
        button->hovered = FALSE;
        InvalidateToolbarButton(g_toolbar.hoveredButton);
        g_toolbar.hoveredButton = -1;
    }
    
    HideToolbarTooltip();
}

/* Update toolbar tooltip */
void UpdateToolbarTooltip(int buttonIndex)
{
    if (!g_toolbar.tooltipWindow || buttonIndex < 0 || buttonIndex >= g_toolbar.buttonCount) {
        HideToolbarTooltip();
        return;
    }
    
    ToolbarButton* button = &g_toolbar.buttons[buttonIndex];
    
    if (button->tooltip && button->enabled) {
        /* Get toolbar width for right-aligned button calculations */
        RECT toolbarRect;
        GetClientRect(g_toolbar.hwnd, &toolbarRect);
        int toolbarWidth = toolbarRect.right - toolbarRect.left;
        
        /* Calculate button position using helper function */
        int currentX = GetButtonXPosition(buttonIndex, toolbarWidth);
        int buttonWidth = (button->type == BUTTON_DROPDOWN_MENU) ? TOOLBAR_DROPDOWN_BUTTON_WIDTH : TOOLBAR_BUTTON_WIDTH;
        
        int buttonY = (TOOLBAR_HEIGHT - TOOLBAR_BUTTON_HEIGHT) / 2;
        ShowToolbarTooltip(currentX + buttonWidth / 2, buttonY + TOOLBAR_BUTTON_HEIGHT / 2, button->tooltip);
    } else {
        HideToolbarTooltip();
    }
}

/* Track if tooltip tool has been added */
static BOOL g_tooltipToolAdded = FALSE;
static char g_currentTooltipText[256] = "";

/* Show toolbar tooltip */
void ShowToolbarTooltip(int x, int y, const char* text)
{
    UNREFERENCED_PARAMETER(x);
    UNREFERENCED_PARAMETER(y);
    
    if (!g_toolbar.tooltipWindow || !text) {
        return;
    }
    
    /* Store the tooltip text */
    strncpy(g_currentTooltipText, text, sizeof(g_currentTooltipText) - 1);
    g_currentTooltipText[sizeof(g_currentTooltipText) - 1] = '\0';
    
    TOOLINFO ti = {0};
    ti.cbSize = sizeof(TOOLINFO);
    ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    ti.hwnd = g_toolbar.hwnd;
    ti.uId = (UINT_PTR)g_toolbar.hwnd;
    ti.lpszText = g_currentTooltipText;
    
    if (!g_tooltipToolAdded) {
        /* First time - add the tool */
        RECT rc;
        GetClientRect(g_toolbar.hwnd, &rc);
        ti.rect = rc;
        SendMessage(g_toolbar.tooltipWindow, TTM_ADDTOOL, 0, (LPARAM)&ti);
        g_tooltipToolAdded = TRUE;
    } else {
        /* Update existing tool text */
        SendMessage(g_toolbar.tooltipWindow, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);
    }
    
    /* Activate the tooltip */
    SendMessage(g_toolbar.tooltipWindow, TTM_ACTIVATE, TRUE, 0);
}

/* Hide toolbar tooltip */
void HideToolbarTooltip(void)
{
    if (!g_toolbar.tooltipWindow) {
        return;
    }
    
    /* Just deactivate - don't remove the tool */
    SendMessage(g_toolbar.tooltipWindow, TTM_ACTIVATE, FALSE, 0);
}

/* Handle toolbar messages from main window */
BOOL HandleToolbarMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (!g_toolbar.isVisible || !g_toolbar.hwnd) {
        return FALSE;
    }
    
    /* Get the toolbar window rect to check if coordinates are within it */
    RECT toolbarRect;
    GetWindowRect(g_toolbar.hwnd, &toolbarRect);
    MapWindowPoints(NULL, hwnd, (LPPOINT)&toolbarRect, 2);
    
    switch (uMsg) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MOUSEMOVE:
        case WM_LBUTTONDBLCLK:
            {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                
                /* Check if the mouse coordinates are within the toolbar area */
                if (x >= toolbarRect.left && x <= toolbarRect.right &&
                    y >= toolbarRect.top && y <= toolbarRect.bottom) {
                    
                    /* Adjust coordinates to toolbar-relative */
                    x -= toolbarRect.left;
                    y -= toolbarRect.top;
                    lParam = MAKELPARAM(x, y);
                    
                    /* Forward to toolbar window procedure */
                    return SendMessage(g_toolbar.hwnd, uMsg, wParam, lParam);
                }
            }
            break;
            
        case WM_MOUSELEAVE:
            HandleToolbarMouseLeave();
            return TRUE;
    }
    
    return FALSE;
}

/* Get button index from ID */
int GetButtonIndexFromId(int id)
{
    for (int i = 0; i < g_toolbar.buttonCount; i++) {
        if (g_toolbar.buttons[i].id == id) {
            return i;
        }
    }
    return -1;
}

/* Get toolbar button */
ToolbarButton* GetToolbarButton(int index)
{
    if (index < 0 || index >= g_toolbar.buttonCount) {
        return NULL;
    }
    return &g_toolbar.buttons[index];
}

/* Invalidate toolbar */
static void InvalidateToolbar(void)
{
    InvalidateRect(g_toolbar.hwnd, NULL, TRUE);
}

/* Invalidate toolbar button */
static void InvalidateToolbarButton(int index)
{
    if (index < 0 || index >= g_toolbar.buttonCount) {
        return;
    }
    
    /* Get toolbar width for right-aligned button calculations */
    RECT toolbarRect;
    GetClientRect(g_toolbar.hwnd, &toolbarRect);
    int toolbarWidth = toolbarRect.right - toolbarRect.left;
    
    /* Calculate button position using helper function */
    int currentX = GetButtonXPosition(index, toolbarWidth);
    ToolbarButton* button = &g_toolbar.buttons[index];
    int buttonWidth = (button->type == BUTTON_DROPDOWN_MENU) ? TOOLBAR_DROPDOWN_BUTTON_WIDTH : TOOLBAR_BUTTON_WIDTH;
    
    int buttonY = (TOOLBAR_HEIGHT - TOOLBAR_BUTTON_HEIGHT) / 2;
    RECT rc = {currentX, buttonY, currentX + buttonWidth, buttonY + TOOLBAR_BUTTON_HEIGHT};
    InvalidateRect(g_toolbar.hwnd, &rc, TRUE);
}

/* Initialize default buttons */
void InitializeDefaultButtons(void)
{
    /* FAST STARTUP: Skip icon loading here - defer it via DeferToolbarIconLoading() */
    /* Icons will be loaded after window is visible, showing text labels initially */
    
    /* Add icon buttons - removed ID_FILE_NEW as it's already in tab bar */
    AddToolbarButton(ID_FILE_OPEN, NULL, "Open (Ctrl+O)");
    /* Recent menu dropdown next to Open */
    AddToolbarDropdownButton(ID_TOOLBAR_MENU_FILE, "Recent", "Recent menu");
    AddSeparator();
    AddToolbarButton(ID_FILE_SAVE, NULL, "Save (Ctrl+S)");
    /* Save As and Save All buttons together */
    AddToolbarButton(ID_FILE_SAVEAS, NULL, "Save As (Ctrl+Shift+S)");
    AddToolbarButton(ID_FILE_SAVEALL, NULL, "Save All");
    AddSeparator();
    AddToolbarButton(ID_FILE_OPENFOLDER, NULL, "Open Folder");
    AddSeparator();
    AddToolbarButton(ID_EDIT_UNDO, NULL, "Undo (Ctrl+Z)");
    AddToolbarButton(ID_EDIT_REDO, NULL, "Redo (Ctrl+Y)");
    AddSeparator();
    AddToolbarButton(ID_EDIT_CUT, NULL, "Cut (Ctrl+X)");
    AddToolbarButton(ID_EDIT_COPY, NULL, "Copy (Ctrl+C)");
    AddToolbarButton(ID_EDIT_PASTE, NULL, "Paste (Ctrl+V)");
    AddSeparator();
    AddToolbarButton(ID_EDIT_FIND, NULL, "Find (Ctrl+F)");
    /* Edit menu dropdown next to Find */
    AddToolbarDropdownButton(ID_TOOLBAR_MENU_EDIT, "Edit", "Edit menu");
    AddSeparator();
    
    /* Add standalone toggle buttons from former View menu */
    AppConfig* cfg = GetConfig();
    int activeTab = GetSelectedTab();
    TabInfo* activeTabInfo = (activeTab >= 0) ? GetTab(activeTab) : NULL;
    AddToolbarToggleButton(ID_VIEW_WORD_WRAP, NULL, "Word Wrap",
        activeTabInfo ? activeTabInfo->wordWrap : (cfg ? cfg->wordWrap : FALSE));
    AddToolbarToggleButton(ID_VIEW_CODEFOLDING, NULL, "Code Folding",
        activeTabInfo ? activeTabInfo->codeFoldingEnabled : IsCodeFoldingEnabled());
    AddToolbarToggleButton(ID_VIEW_CHANGEHISTORY, NULL, "Change History",
        activeTabInfo ? activeTabInfo->changeHistoryEnabled : IsChangeHistoryEnabled());
    AddToolbarToggleButton(ID_VIEW_LINE_NUMBERS, NULL, "Line Numbers",
        activeTabInfo ? activeTabInfo->showLineNumbers : FALSE);
    AddToolbarToggleButton(ID_VIEW_WHITESPACE, NULL, "Show Whitespace",
        activeTabInfo ? activeTabInfo->showWhitespace : FALSE);
    AddToolbarButton(ID_VIEW_SPLITVIEW_LOADRIGHT, NULL, "Clone to New Tab");
    AddSeparator();
    
    /* Add Preferences button and make it right-aligned */
    int prefIndex = AddToolbarButton(ID_OPTIONS_PREFERENCES, NULL, "Preferences");
    if (prefIndex >= 0) {
        g_toolbar.buttons[prefIndex].isRightAligned = TRUE;
    }
}

/* Load toolbar icon */
HBITMAP LoadToolbarIcon(int resourceId)
{
    HBITMAP icon = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(resourceId));
    if (!icon) {
        /* Create a simple placeholder icon if resource loading fails */
        icon = CreateMonochromeBitmap(TOOLBAR_ICON_SIZE, TOOLBAR_ICON_SIZE);
    }
    return icon;
}

/* Create monochrome bitmap */
HBITMAP CreateMonochromeBitmap(int width, int height)
{
    HDC hdc = GetDC(NULL);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP bitmap = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBitmap = SelectObject(memDC, bitmap);
    
    /* Create a simple placeholder */
    RECT rect = {0, 0, width, height};
    FillRect(memDC, &rect, GetStockObject(WHITE_BRUSH));
    
    SelectObject(memDC, oldBitmap);
    DeleteDC(memDC);
    ReleaseDC(NULL, hdc);
    
    return bitmap;
}

/* Toolbar window procedure */
LRESULT CALLBACK ToolbarWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Toolbar* toolbar = (Toolbar*)GetWindowLongPtr(hwnd, 0);
    
    switch (uMsg) {
        case WM_CREATE:
            return 0;
            
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                RECT rc;
                GetClientRect(hwnd, &rc);
                int toolbarWidth = rc.right - rc.left;
                
                /* Draw background */
                DrawToolbarBackground(hdc, &rc);
                
                /* Draw buttons using helper function for positioning */
                int buttonY = (TOOLBAR_HEIGHT - TOOLBAR_BUTTON_HEIGHT) / 2;
                
                for (int i = 0; i < toolbar->buttonCount; i++) {
                    ToolbarButton* button = &toolbar->buttons[i];
                    
                    if (button->id == -1) {
                        /* Separator - position depends on next button's alignment */
                        int sepX = GetButtonXPosition(i, toolbarWidth);
                        if (sepX >= 0) {
                            DrawToolbarSeparator(hdc, sepX + TOOLBAR_BUTTON_SPACING, 0, TOOLBAR_HEIGHT);
                        }
                        continue;
                    }
                    
                    /* Get button position using helper function */
                    int currentX = GetButtonXPosition(i, toolbarWidth);
                    int buttonWidth = (button->type == BUTTON_DROPDOWN_MENU) ? TOOLBAR_DROPDOWN_BUTTON_WIDTH : TOOLBAR_BUTTON_WIDTH;
                    
                    /* Draw all buttons, including disabled ones (just grayed out) */
                    RECT buttonRect = {currentX, buttonY, currentX + buttonWidth, buttonY + TOOLBAR_BUTTON_HEIGHT};
                    DrawToolbarButton(hdc, button, &buttonRect);
                }
                
                EndPaint(hwnd, &ps);
                return 0;
            }
            
        case WM_LBUTTONDOWN:
            {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                HandleToolbarMouseDown(x, y);
                SetCapture(hwnd);
                return 0;
            }
            
        case WM_LBUTTONUP:
            {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                HandleToolbarMouseUp(x, y);
                ReleaseCapture();
                return 0;
            }
            
        case WM_MOUSEMOVE:
            {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                HandleToolbarMouseMove(x, y);
                return 0;
            }
            
        case WM_MOUSELEAVE:
            HandleToolbarMouseLeave();
            return 0;
            
        case WM_SIZE:
            UpdateToolbarLayout();
            return 0;
            
        case WM_ERASEBKGND:
            return 1;  /* Handle in WM_PAINT */
            
        case WM_USER + 200:  /* WM_DEFERRED_ICON_LOAD */
            /* Load icons after window is visible for faster perceived startup */
            InitializeToolbarIcons();
            return 0;
            
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

/* Draw toolbar background */
void DrawToolbarBackground(HDC hdc, RECT* rect)
{
    /* Use theme colors for consistent look with menu */
    ThemeColors* colors = GetThemeColors();
    HBRUSH bgBrush = CreateSolidBrush(colors->toolbarBg);
    FillRect(hdc, rect, bgBrush);
    DeleteObject(bgBrush);
    
    /* No border line - toolbar blends seamlessly with menu bar */
}

/* Draw toolbar separator */
void DrawToolbarSeparator(HDC hdc, int x, int y, int height)
{
    /* Use theme color for separator */
    ThemeColors* colors = GetThemeColors();
    HPEN pen = CreatePen(PS_SOLID, 1, colors->toolbarSeparator);
    HPEN oldPen = SelectObject(hdc, pen);
    
    MoveToEx(hdc, x, y + 4, NULL);
    LineTo(hdc, x, y + height - 6);
    
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

/* Helper function to extract icon with full system path */
static HICON ExtractIconFromSystem(const char* dllName, int iconIndex)
{
    char fullPath[MAX_PATH];
    HICON hIcon = NULL;
    
    /* Build full system path */
    GetSystemDirectory(fullPath, MAX_PATH);
    strcat(fullPath, "\\");
    strcat(fullPath, dllName);
    
    /* Try to extract small icon */
    ExtractIconEx(fullPath, iconIndex, NULL, &hIcon, 1);
    
    return hIcon;
}

/* Cache a single icon for a button ID */
static void CacheIconForButton(int buttonId)
{
    if (g_cachedIconCount >= MAX_CACHED_ICONS) return;
    
    HICON hIcon = NULL;
    
    switch (buttonId) {
        case ID_FILE_NEW:
            hIcon = ExtractIconFromSystem("shell32.dll", 0);
            break;
        case ID_FILE_OPEN:
            hIcon = ExtractIconFromSystem("shell32.dll", 4);
            break;
        case ID_FILE_SAVE:
            hIcon = ExtractIconFromSystem("shell32.dll", 7);
            break;
        case ID_EDIT_FIND:
            hIcon = ExtractIconFromSystem("shell32.dll", 22);
            if (!hIcon) hIcon = ExtractIconFromSystem("shell32.dll", 23);
            break;
        case ID_EDIT_CUT:
            hIcon = ExtractIconFromSystem("imageres.dll", 161);
            if (!hIcon) hIcon = ExtractIconFromSystem("shell32.dll", 16762);
            if (!hIcon) hIcon = ExtractIconFromSystem("shell32.dll", 131);
            break;
        case ID_EDIT_COPY:
            hIcon = ExtractIconFromSystem("shell32.dll", 134);
            if (!hIcon) hIcon = ExtractIconFromSystem("imageres.dll", 162);
            break;
        case ID_EDIT_PASTE:
            hIcon = ExtractIconFromSystem("shell32.dll", 260);
            if (!hIcon) hIcon = ExtractIconFromSystem("shell32.dll", 259);
            if (!hIcon) hIcon = ExtractIconFromSystem("imageres.dll", 163);
            break;
    }
    
    if (hIcon) {
        g_cachedIcons[g_cachedIconCount].buttonId = buttonId;
        g_cachedIcons[g_cachedIconCount].hIcon = hIcon;
        g_cachedIconCount++;
    }
}

/* Deferred icon loading message */
#define WM_DEFERRED_ICON_LOAD (WM_USER + 200)

/* Initialize all toolbar icons - can be called immediately or deferred */
static void InitializeToolbarIcons(void)
{
    if (g_iconsInitialized) {
        return;
    }
    
    CacheIconForButton(ID_FILE_NEW);
    CacheIconForButton(ID_FILE_OPEN);
    CacheIconForButton(ID_FILE_SAVE);
    CacheIconForButton(ID_EDIT_CUT);
    CacheIconForButton(ID_EDIT_COPY);
    CacheIconForButton(ID_EDIT_PASTE);
    CacheIconForButton(ID_EDIT_FIND);
    
    g_iconsInitialized = TRUE;
    
    /* Trigger repaint to show loaded icons */
    if (g_toolbar.hwnd) {
        InvalidateRect(g_toolbar.hwnd, NULL, TRUE);
    }
}

/* Request deferred icon loading - icons load after window is visible */
void DeferToolbarIconLoading(void)
{
    if (g_toolbar.hwnd && !g_iconsInitialized) {
        PostMessage(g_toolbar.hwnd, WM_DEFERRED_ICON_LOAD, 0, 0);
    }
}

/* Cleanup cached icons */
static void CleanupToolbarIcons(void)
{
    for (int i = 0; i < g_cachedIconCount; i++) {
        if (g_cachedIcons[i].hIcon) {
            DestroyIcon(g_cachedIcons[i].hIcon);
            g_cachedIcons[i].hIcon = NULL;
        }
    }
    g_cachedIconCount = 0;
    g_iconsInitialized = FALSE;
}

/* Get cached icon for button */
static HICON GetCachedIcon(int buttonId)
{
    for (int i = 0; i < g_cachedIconCount; i++) {
        if (g_cachedIcons[i].buttonId == buttonId) {
            return g_cachedIcons[i].hIcon;
        }
    }
    return NULL;
}

/* Fluent icon font - cached for performance */
static HFONT g_fluentIconFont = NULL;
static BOOL g_fluentFontAvailable = FALSE;
static BOOL g_fluentFontChecked = FALSE;

/* Get or create the Fluent icon font */
static HFONT GetFluentIconFont(void)
{
    if (!g_fluentFontChecked) {
        g_fluentFontChecked = TRUE;
        /* Try Segoe Fluent Icons first (Windows 11), then Segoe MDL2 Assets (Windows 10) */
        g_fluentIconFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
                                       "Segoe Fluent Icons");
        if (g_fluentIconFont) {
            g_fluentFontAvailable = TRUE;
        } else {
            g_fluentIconFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
                                           "Segoe MDL2 Assets");
            if (g_fluentIconFont) {
                g_fluentFontAvailable = TRUE;
            }
        }
    }
    return g_fluentIconFont;
}

/* Get Fluent icon glyph for button ID */
static const wchar_t* GetFluentIconGlyph(int buttonId)
{
    /* Segoe MDL2 Assets / Segoe Fluent Icons glyphs */
    switch (buttonId) {
        case ID_FILE_NEW:   return L"\uE710";  /* Add/New */
        case ID_FILE_OPEN:  return L"\uE8E5";  /* OpenFile */
        case ID_FILE_SAVE:  return L"\uE74E";  /* Save */
        case ID_FILE_SAVEAS: return L"\uE792";  /* SaveAs - save with pencil */
        case ID_FILE_SAVEALL: return L"\uE74E";  /* SaveAll - same as save, positioned next to it */
        case ID_FILE_OPENFOLDER: return L"\uE8B7";  /* Open folder - Folder icon */
        case ID_EDIT_UNDO:  return L"\uE7A7";  /* Undo - curved arrow left */
        case ID_EDIT_REDO:  return L"\uE7A6";  /* Redo - curved arrow right */
        case ID_EDIT_CUT:   return L"\uE8C6";  /* Cut */
        case ID_EDIT_COPY:  return L"\uE8C8";  /* Copy */
        case ID_EDIT_PASTE: return L"\uE77F";  /* Paste */
        case ID_EDIT_FIND:  return L"\uE721";  /* Search/Find */
        case ID_VIEW_WORD_WRAP:     return L"\uE8A9";  /* Word wrap - document with wrap */
        case ID_VIEW_CODEFOLDING:   return L"\uE8C4";  /* Code folding - ShowBcc icon */
        case ID_VIEW_CHANGEHISTORY: return L"\uE81C";  /* Change history - history */
        case ID_VIEW_LINE_NUMBERS:  return L"\uE8BC";  /* Line numbers - grouped list with lines */
        case ID_VIEW_WHITESPACE:    return L"\uED1E";  /* Show whitespace - Subtitles */
        case ID_VIEW_SPLITVIEW_LOADRIGHT: return L"\uEA5B";  /* Clone/Copy - duplicate */
        /* Dropdown menu button icons */
        case ID_TOOLBAR_MENU_FILE: return L"\uE8A5";  /* Folder/Document */
        case ID_TOOLBAR_MENU_EDIT: return L"\uE70F";  /* Edit/Pencil */
        case ID_OPTIONS_PREFERENCES: return L"\uE713";  /* Settings/Gear */
        default:            return L"\uE8FD";  /* Unknown - question mark */
    }
}

/* Draw icons using Fluent font icons for modern look */
static void DrawToolbarIcon(HDC hdc, int buttonId, RECT* rect, BOOL enabled)
{
    HFONT hFluentFont = GetFluentIconFont();
    
    if (g_fluentFontAvailable && hFluentFont) {
        /* Use Fluent Icons font - modern, fast, vector-based */
        const wchar_t* glyph = GetFluentIconGlyph(buttonId);
        
        SetBkMode(hdc, TRANSPARENT);
        
        /* Get theme colors for icons */
        ThemeColors* colors = GetThemeColors();
        COLORREF iconColor;
        if (!enabled) {
            iconColor = RGB(180, 180, 180);
        } else if (colors) {
            iconColor = colors->toolbarBtnFg;
        } else {
            iconColor = RGB(28, 28, 28);
        }
        SetTextColor(hdc, iconColor);
        
        HFONT oldFont = (HFONT)SelectObject(hdc, hFluentFont);
        
        /* Draw the glyph centered in the button */
        DrawTextW(hdc, glyph, 1, (LPRECT)rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        
        SelectObject(hdc, oldFont);
    } else {
        /* Fallback: Use simple text labels with Segoe UI */
        const char* label = "";
        switch (buttonId) {
            case ID_FILE_NEW:   label = "+"; break;
            case ID_FILE_OPEN:  label = "..."; break;
            case ID_FILE_SAVE:  label = "S"; break;
            case ID_EDIT_CUT:   label = "X"; break;
            case ID_EDIT_COPY:  label = "C"; break;
            case ID_EDIT_PASTE: label = "V"; break;
            case ID_EDIT_FIND:  label = "?"; break;
            default:            label = "?"; break;
        }
        
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, enabled ? RGB(50, 50, 50) : RGB(150, 150, 150));
        
        HFONT hFont = CreateFont(12, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
        
        DrawText(hdc, label, -1, (LPRECT)rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        
        SelectObject(hdc, oldFont);
        DeleteObject(hFont);
    }
}

/* Draw toolbar button */
void DrawToolbarButton(HDC hdc, ToolbarButton* button, RECT* rect)
{
    /* Get theme colors */
    ThemeColors* colors = GetThemeColors();
    
    BOOL shouldDrawBackground = FALSE;
    COLORREF bgColor;
    BOOL shouldDrawBorder = FALSE;
    
    /* Determine if we should draw background and which color */
    if (button->pressed) {
        /* Pressed state - highest priority */
        shouldDrawBackground = TRUE;
        bgColor = colors->toolbarBtnPressedBg;
    } else if (button->type == BUTTON_TOGGLE && button->isToggled && !button->hovered) {
        /* Active toggle button (not hovered) - distinct appearance */
        shouldDrawBackground = TRUE;
        /* Use theme-aware toggle color: darker in light mode, lighter in dark mode */
        Theme theme = GetCurrentTheme();
        if (theme == THEME_DARK) {
            bgColor = RGB(60, 90, 120);  /* Darker blue for dark mode */
        } else {
            bgColor = RGB(200, 220, 240);  /* Light blue for light mode */
        }
        shouldDrawBorder = TRUE;  /* Add 2px accent border */
    } else if (button->hovered) {
        /* Hovered state */
        shouldDrawBackground = TRUE;
        if (button->type == BUTTON_TOGGLE && button->isToggled) {
            /* Active toggle button being hovered - keep toggle appearance */
            Theme theme = GetCurrentTheme();
            if (theme == THEME_DARK) {
                bgColor = RGB(60, 90, 120);  /* Darker blue for dark mode */
            } else {
                bgColor = RGB(200, 220, 240);  /* Light blue for light mode */
            }
            shouldDrawBorder = TRUE;
        } else {
            /* Regular hover */
            bgColor = colors->toolbarBtnHoverBg;
        }
    }
    /* Inactive toggle buttons when not hovered/pressed: no background, no border */
    
    /* Draw background if needed */
    if (shouldDrawBackground) {
        HBRUSH bgBrush = CreateSolidBrush(bgColor);
        HBRUSH oldBrush = SelectObject(hdc, bgBrush);
        HPEN oldPen = SelectObject(hdc, GetStockObject(NULL_PEN));
        /* Don't add +1 to avoid drawing outside bounds */
        RoundRect(hdc, rect->left, rect->top, rect->right, rect->bottom, 4, 4);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(bgBrush);
    }
    
    /* Draw border for active toggle buttons - INSET by 1px so 2px pen stays inside rect */
    if (shouldDrawBorder) {
        /* Inset the border rect by 1px so the 2px-wide pen (1px on each side) stays within bounds */
        RECT borderRect = {rect->left + 1, rect->top + 1, rect->right - 1, rect->bottom - 1};
        HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(0, 103, 192));  /* 2px accent blue border */
        HPEN oldPen = SelectObject(hdc, borderPen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, borderRect.left, borderRect.top, borderRect.right, borderRect.bottom, 3, 3);
        SelectObject(hdc, oldPen);
        DeleteObject(borderPen);
    }
    
    /* Draw icon for all buttons including dropdown menu buttons */
    if (button->type == BUTTON_DROPDOWN_MENU) {
        /* Draw icon for dropdown menu buttons */
        DrawToolbarIcon(hdc, button->id, rect, button->enabled);
    } else {
        /* Draw icon for regular buttons */
        DrawToolbarIcon(hdc, button->id, rect, button->enabled);
    }
}

/* Add dropdown menu button */
int AddToolbarDropdownButton(int id, const char* label, const char* tooltip)
{
    ToolbarButton* button;
    
    /* Expand buttons array if needed */
    if (g_toolbar.buttonCount >= g_toolbar.maxButtons) {
        int newMax = g_toolbar.maxButtons * 2;
        ToolbarButton* newButtons = (ToolbarButton*)realloc(g_toolbar.buttons, sizeof(ToolbarButton) * newMax);
        if (!newButtons) {
            return -1;
        }
        g_toolbar.buttons = newButtons;
        g_toolbar.maxButtons = newMax;
    }
    
    /* Initialize new button */
    button = &g_toolbar.buttons[g_toolbar.buttonCount];
    memset(button, 0, sizeof(ToolbarButton));
    
    button->id = id;
    button->index = g_toolbar.buttonCount;
    button->type = BUTTON_DROPDOWN_MENU;
    button->enabled = TRUE;
    button->pressed = FALSE;
    button->hovered = FALSE;
    button->label = label ? _strdup(label) : NULL;
    button->tooltip = tooltip ? _strdup(tooltip) : NULL;
    button->dropdownMenu = NULL;  /* Created on-demand */
    
    g_toolbar.buttonCount++;
    UpdateToolbarLayout();
    
    return g_toolbar.buttonCount - 1;
}

/* Create File dropdown menu - now used as "Recent" menu */
HMENU CreateFileDropdownMenu(void)
{
    HMENU menu = CreatePopupMenu();
    if (!menu) return NULL;
    
    /* Recent Files - shown directly in the menu */
    AppendMenu(menu, MF_STRING | MF_GRAYED, 0, "(No recent files)");
    AppendMenu(menu, MF_SEPARATOR, 0, NULL);
    AppendMenu(menu, MF_STRING, ID_FILE_CLEARRECENT, "&Clear Recent Files");
    
    return menu;
}

/* Create Edit dropdown menu */
HMENU CreateEditDropdownMenu(void)
{
    HMENU menu = CreatePopupMenu();
    if (!menu) return NULL;
    
    /* Basic edit operations */
    AppendMenu(menu, MF_STRING, ID_EDIT_UNDO, "&Undo\tCtrl+Z");
    AppendMenu(menu, MF_STRING, ID_EDIT_REDO, "&Redo\tCtrl+Y");
    AppendMenu(menu, MF_SEPARATOR, 0, NULL);
    AppendMenu(menu, MF_STRING, ID_EDIT_CUT, "Cu&t\tCtrl+X");
    AppendMenu(menu, MF_STRING, ID_EDIT_COPY, "&Copy\tCtrl+C");
    AppendMenu(menu, MF_STRING, ID_EDIT_PASTE, "&Paste\tCtrl+V");
    AppendMenu(menu, MF_STRING, ID_EDIT_SELECTALL, "Select &All\tCtrl+A");
    AppendMenu(menu, MF_SEPARATOR, 0, NULL);
    
    /* Search operations */
    AppendMenu(menu, MF_STRING, ID_EDIT_FIND, "&Find...\tCtrl+F");
    AppendMenu(menu, MF_STRING, ID_EDIT_FINDNEXT, "Find &Next\tF3");
    AppendMenu(menu, MF_STRING, ID_EDIT_FINDPREV, "Find &Previous\tShift+F3");
    AppendMenu(menu, MF_STRING, ID_EDIT_REPLACE, "&Replace...\tCtrl+H");
    AppendMenu(menu, MF_STRING, ID_EDIT_GOTOLINE, "&Go To Line...\tCtrl+G");
    AppendMenu(menu, MF_SEPARATOR, 0, NULL);
    
    /* Common line operations submenu */
    HMENU lineOpsMenu = CreatePopupMenu();
    AppendMenu(lineOpsMenu, MF_STRING, ID_EDIT_DUPLICATE_LINE, "&Duplicate Current Line\tCtrl+D");
    AppendMenu(lineOpsMenu, MF_STRING, ID_EDIT_DELETE_LINE, "&Delete Current Line\tCtrl+L");
    AppendMenu(lineOpsMenu, MF_STRING, ID_EDIT_MOVE_LINE_UP, "Move Line &Up\tCtrl+Shift+Up");
    AppendMenu(lineOpsMenu, MF_STRING, ID_EDIT_MOVE_LINE_DOWN, "Move Line &Down\tCtrl+Shift+Down");
    AppendMenu(lineOpsMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(lineOpsMenu, MF_STRING, ID_EDIT_JOIN_LINES, "&Join Lines\tCtrl+J");
    AppendMenu(lineOpsMenu, MF_STRING, ID_EDIT_SPLIT_LINES, "S&plit Lines");
    AppendMenu(lineOpsMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(lineOpsMenu, MF_STRING, ID_EDIT_TRIM_TRAILING, "Trim Trailing &Whitespace");
    AppendMenu(lineOpsMenu, MF_STRING, ID_EDIT_TRIM_LEADING, "Trim &Leading Whitespace");
    AppendMenu(lineOpsMenu, MF_STRING, ID_EDIT_TRIM_BOTH, "Trim &Both");
    AppendMenu(menu, MF_POPUP, (UINT_PTR)lineOpsMenu, "Line &Operations");
    
    /* EOL conversion submenu */
    HMENU eolMenu = CreatePopupMenu();
    AppendMenu(eolMenu, MF_STRING, ID_LINEEND_CRLF, "Windows (&CRLF)");
    AppendMenu(eolMenu, MF_STRING, ID_LINEEND_LF, "Unix (&LF)");
    AppendMenu(eolMenu, MF_STRING, ID_LINEEND_CR, "Mac (&CR)");
    AppendMenu(menu, MF_POPUP, (UINT_PTR)eolMenu, "EOL &Conversion");
    
    /* Case conversion submenu */
    HMENU caseMenu = CreatePopupMenu();
    AppendMenu(caseMenu, MF_STRING, ID_EDIT_UPPERCASE, "&UPPERCASE\tCtrl+Shift+U");
    AppendMenu(caseMenu, MF_STRING, ID_EDIT_LOWERCASE, "&lowercase\tCtrl+U");
    AppendMenu(caseMenu, MF_STRING, ID_EDIT_TITLECASE, "&Title Case");
    AppendMenu(caseMenu, MF_STRING, ID_EDIT_SENTENCECASE, "&Sentence case");
    AppendMenu(caseMenu, MF_STRING, ID_EDIT_INVERTCASE, "&iNVERT cASE");
    AppendMenu(menu, MF_POPUP, (UINT_PTR)caseMenu, "&Case Conversion");
    
    /* Encoding/Decoding submenu */
    HMENU encodingMenu = CreatePopupMenu();
    AppendMenu(encodingMenu, MF_STRING, ID_EDIT_BASE64_ENCODE, "Base64 &Encode");
    AppendMenu(encodingMenu, MF_STRING, ID_EDIT_BASE64_DECODE, "Base64 &Decode");
    AppendMenu(encodingMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(encodingMenu, MF_STRING, ID_EDIT_URL_ENCODE, "&URL Encode");
    AppendMenu(encodingMenu, MF_STRING, ID_EDIT_URL_DECODE, "URL &Decode");
    AppendMenu(menu, MF_POPUP, (UINT_PTR)encodingMenu, "&Encode/Decode");
    
    return menu;
}


/* Create Settings dropdown menu */
HMENU CreateSettingsDropdownMenu(void)
{
    HMENU menu = CreatePopupMenu();
    if (!menu) return NULL;
    
    AppendMenu(menu, MF_STRING, ID_OPTIONS_PREFERENCES, "&Preferences...");
    AppendMenu(menu, MF_SEPARATOR, 0, NULL);
    
    /* Theme submenu */
    HMENU themeMenu = CreatePopupMenu();
    AppendMenu(themeMenu, MF_STRING, ID_OPTIONS_THEME_DARK, "&Dark Theme");
    AppendMenu(themeMenu, MF_STRING, ID_OPTIONS_THEME_LIGHT, "&Light Theme");
    AppendMenu(menu, MF_POPUP, (UINT_PTR)themeMenu, "&Theme");
    
    AppendMenu(menu, MF_SEPARATOR, 0, NULL);
    
    /* Status bar option moved from View menu */
    AppendMenu(menu, MF_STRING, ID_VIEW_STATUSBAR, "&Status Bar");
    AppendMenu(menu, MF_SEPARATOR, 0, NULL);
    
    AppendMenu(menu, MF_STRING, ID_OPTIONS_AUTOINDENT, "Auto-&Indent");
    AppendMenu(menu, MF_STRING, ID_OPTIONS_BRACKETMATCH, "&Bracket Matching");
    AppendMenu(menu, MF_SEPARATOR, 0, NULL);
    AppendMenu(menu, MF_STRING, ID_HELP_ABOUT, "&About");
    
    return menu;
}

/* Update Settings menu checkboxes */
void UpdateSettingsMenuChecks(HMENU menu)
{
    if (!menu) return;
    
    AppConfig* cfg = GetConfig();
    Theme theme = GetCurrentTheme();
    
    /* Find theme submenu */
    int itemCount = GetMenuItemCount(menu);
    for (int i = 0; i < itemCount; i++) {
        MENUITEMINFO mii = {0};
        mii.cbSize = sizeof(MENUITEMINFO);
        mii.fMask = MIIM_SUBMENU;
        if (GetMenuItemInfo(menu, i, TRUE, &mii) && mii.hSubMenu) {
            HMENU themeMenu = mii.hSubMenu;
            CheckMenuItem(themeMenu, ID_OPTIONS_THEME_DARK, MF_BYCOMMAND | (theme == THEME_DARK ? MF_CHECKED : MF_UNCHECKED));
            CheckMenuItem(themeMenu, ID_OPTIONS_THEME_LIGHT, MF_BYCOMMAND | (theme == THEME_LIGHT ? MF_CHECKED : MF_UNCHECKED));
            break;
        }
    }
    
    /* Status bar check (moved from View menu) */
    CheckMenuItem(menu, ID_VIEW_STATUSBAR, MF_BYCOMMAND | (IsStatusBarVisible() ? MF_CHECKED : MF_UNCHECKED));
    
    if (cfg) {
        CheckMenuItem(menu, ID_OPTIONS_AUTOINDENT, MF_BYCOMMAND | (cfg->autoIndent ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(menu, ID_OPTIONS_BRACKETMATCH, MF_BYCOMMAND | (cfg->bracketMatching ? MF_CHECKED : MF_UNCHECKED));
    }
}


/* Show dropdown menu for a button */
void ShowDropdownMenu(int buttonIndex, HWND hwnd)
{
    (void)hwnd; /* Unused parameter */
    if (buttonIndex < 0 || buttonIndex >= g_toolbar.buttonCount) return;
    
    ToolbarButton* button = &g_toolbar.buttons[buttonIndex];
    if (button->type != BUTTON_DROPDOWN_MENU) return;
    
    HMENU menu = NULL;
    
    /* Create menu based on button ID */
    switch (button->id) {
        case ID_TOOLBAR_MENU_FILE:
            menu = CreateFileDropdownMenu();
            /* Update recent files in the menu */
            if (menu) {
                extern void UpdateRecentFilesMenu(void);
                extern int GetRecentFileCount(void);
                extern const char* GetRecentFile(int index);
                
                /* Find Recent Files submenu */
                int itemCount = GetMenuItemCount(menu);
                for (int i = 0; i < itemCount; i++) {
                    MENUITEMINFO mii = {0};
                    mii.cbSize = sizeof(MENUITEMINFO);
                    mii.fMask = MIIM_SUBMENU;
                    if (GetMenuItemInfo(menu, i, TRUE, &mii) && mii.hSubMenu) {
                        HMENU recentMenu = mii.hSubMenu;
                        /* Clear existing items */
                        while (GetMenuItemCount(recentMenu) > 0) {
                            DeleteMenu(recentMenu, 0, MF_BYPOSITION);
                        }
                        
                        /* Add recent files */
                        int recentCount = GetRecentFileCount();
                        if (recentCount == 0) {
                            AppendMenu(recentMenu, MF_STRING | MF_GRAYED, 0, "(No recent files)");
                        } else {
                            for (int j = 0; j < recentCount && j < 10; j++) {
                                const char* filePath = GetRecentFile(j);
                                if (filePath && filePath[0] != '\0') {
                                    const char* fileName = strrchr(filePath, '\\');
                                    if (!fileName) fileName = filePath;
                                    else fileName++;
                                    
                                    char menuText[MAX_PATH + 10];
                                    sprintf(menuText, "&%d %s", (j + 1) % 10, fileName);
                                    AppendMenu(recentMenu, MF_STRING, ID_FILE_RECENT_BASE + j, menuText);
                                }
                            }
                        }
                        break;
                    }
                }
            }
            break;
            
        case ID_TOOLBAR_MENU_EDIT:
            menu = CreateEditDropdownMenu();
            break;
            
        case ID_OPTIONS_PREFERENCES:
            menu = CreateSettingsDropdownMenu();
            UpdateSettingsMenuChecks(menu);
            break;
            
        default:
            return;
    }
    
    if (!menu) return;
    
    /* Get toolbar width for right-aligned button calculations */
    RECT toolbarRect;
    GetClientRect(g_toolbar.hwnd, &toolbarRect);
    int toolbarWidth = toolbarRect.right - toolbarRect.left;
    
    /* Calculate button position using helper function */
    int currentX = GetButtonXPosition(buttonIndex, toolbarWidth);
    
    int buttonY = (TOOLBAR_HEIGHT - TOOLBAR_BUTTON_HEIGHT) / 2;
    
    /* Convert to screen coordinates */
    POINT pt = {currentX, buttonY + TOOLBAR_BUTTON_HEIGHT};
    ClientToScreen(g_toolbar.hwnd, &pt);
    
    /* Show the menu */
    TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, g_toolbar.parentWindow, NULL);
    
    /* Cleanup */
    DestroyMenu(menu);
}
            