/*
 * Theme system header for Notepad+
 */

#ifndef THEMES_H
#define THEMES_H

#include <windows.h>

/* Theme types */
typedef enum {
    THEME_LIGHT,
    THEME_DARK,
    THEME_COUNT
} Theme;

/* Theme colors structure */
typedef struct {
    /* Editor colors */
    COLORREF editorBg;
    COLORREF editorFg;
    COLORREF editorSelBg;
    COLORREF editorSelFg;
    COLORREF editorLineNumBg;
    COLORREF editorLineNumFg;
    COLORREF editorCaretLineBg;
    
    /* Tab colors */
    COLORREF tabNormalBg;
    COLORREF tabNormalFg;
    COLORREF tabNormalBorder;
    COLORREF tabHoverBg;
    COLORREF tabHoverFg;
    COLORREF tabSelectedBg;
    COLORREF tabSelectedFg;
    COLORREF tabSelectedBorder;
    
    /* Toolbar colors */
    COLORREF toolbarBg;
    COLORREF toolbarBtnBg;
    COLORREF toolbarBtnFg;
    COLORREF toolbarBtnHoverBg;
    COLORREF toolbarBtnPressedBg;
    COLORREF toolbarSeparator;
    
    /* Status bar colors */
    COLORREF statusbarBg;
    COLORREF statusbarFg;
    COLORREF statusbarBorder;
    
    /* Window colors */
    COLORREF windowBg;
    COLORREF windowBorder;
    
    /* Scrollbar colors */
    COLORREF scrollbarBg;
    COLORREF scrollbarThumb;
} ThemeColors;

/* Theme system initialization and cleanup */
BOOL InitializeTheme(void);
void CleanupTheme(void);

/* Theme management */
Theme GetCurrentTheme(void);
BOOL SetTheme(Theme theme);
void ToggleTheme(void);
const char* GetThemeName(Theme theme);

/* Get theme colors */
ThemeColors* GetThemeColors(void);

/* Apply theme to UI components */
void ApplyTheme(void);
void ApplyThemeToEditor(HWND editor);
void ApplyThemeToAllEditors(void);
void ApplyCurrentThemeToWindow(void);

/* Configuration persistence */
BOOL SaveThemeToConfig(const char* configPath);
BOOL LoadThemeFromConfig(const char* configPath);

#endif /* THEMES_H */
