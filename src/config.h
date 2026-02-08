/*
 * Configuration system header for Notepad+
 * Handles persistent application settings using Windows Registry
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <windows.h>

/* Configuration structure */
typedef struct {
    /* Window position and size */
    int windowX;
    int windowY;
    int windowWidth;
    int windowHeight;
    BOOL windowMaximized;
    
    /* Recent files list */
    char recentFiles[10][MAX_PATH];
    int recentFileCount;
    
    /* View settings */
    BOOL showToolbar;
    BOOL showStatusBar;
    BOOL showLineNumbers;
    BOOL wordWrap;
    
    /* Editor settings */
    char fontName[64];
    int fontSize;
    int tabWidth;
    BOOL useSpaces;
    int zoomLevel;  /* Editor zoom level (-10 to +20) */
    BOOL showWhitespace;
    BOOL autoIndent;
    BOOL bracketMatching;
    BOOL codeFoldingEnabled;
    BOOL changeHistoryEnabled;
    
    /* Theme settings */
    int theme;  /* 0 = light, 1 = dark */
    BOOL darkModeEditorOnly;
    
    /* Find/Replace settings */
    BOOL matchCase;
    BOOL wholeWord;
    BOOL useRegex;
    BOOL searchDown;
    
    /* Session behavior settings */
    BOOL restoreSession;
    BOOL saveOnExit;
    BOOL autoSave;
    int autoSaveInterval;  /* in seconds, 0=disabled */
    
    /* General behavior settings */
    BOOL singleInstance;
    BOOL confirmExit;
    BOOL backupOnSave;
    
    /* Editor behavior */
    BOOL highlightMatchingWords;
    BOOL highlightCurrentLine;
    int caretWidth;  /* 1-3 pixels */
} AppConfig;

/* Configuration system initialization */
BOOL InitializeConfig(void);
void CleanupConfig(void);

/* Get configuration */
AppConfig* GetConfig(void);

/* Save and load configuration */
BOOL SaveConfig(void);
BOOL LoadConfig(void);

/* Recent files management */
void AddRecentFile(const char* filePath);
void ClearRecentFiles(void);
const char* GetRecentFile(int index);
int GetRecentFileCount(void);

/* Apply configuration to UI */
void ApplyConfig(void);

/* Update current window state in config (call on move/resize) */
void UpdateCurrentWindowState(void);

#endif /* CONFIG_H */
