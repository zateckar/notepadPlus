/*
 * Registry-based configuration system header for Notepad+
 * Handles user-configurable settings via Windows Registry
 */

#ifndef REGISTRY_CONFIG_H
#define REGISTRY_CONFIG_H

#include <windows.h>
#include "config.h"

/* Registry paths */
#define REGISTRY_ROOT_KEY       HKEY_CURRENT_USER
#define REGISTRY_BASE_PATH      "Software\\Notepad+"
#define REGISTRY_VIEW_PATH      "Software\\Notepad+\\View"
#define REGISTRY_EDITOR_PATH    "Software\\Notepad+\\Editor"
#define REGISTRY_THEME_PATH     "Software\\Notepad+\\Theme"
#define REGISTRY_FIND_PATH      "Software\\Notepad+\\Find"
#define REGISTRY_SESSION_PATH   "Software\\Notepad+\\Session"
#define REGISTRY_BEHAVIOR_PATH  "Software\\Notepad+\\Behavior"
#define REGISTRY_WINDOW_PATH    "Software\\Notepad+\\Window"
#define REGISTRY_RECENT_PATH    "Software\\Notepad+\\RecentFiles"

/* Registry value names - View */
#define REG_SHOW_STATUSBAR      "ShowStatusBar"
#define REG_SHOW_LINE_NUMBERS   "ShowLineNumbers"
#define REG_WORD_WRAP           "WordWrap"

/* Registry value names - Editor */
#define REG_TAB_WIDTH           "TabWidth"
#define REG_USE_SPACES          "UseSpaces"
#define REG_SHOW_WHITESPACE     "ShowWhitespace"
#define REG_AUTO_INDENT         "AutoIndent"
#define REG_CODE_FOLDING        "CodeFoldingEnabled"
#define REG_BRACKET_MATCHING    "BracketMatchingEnabled"
#define REG_CHANGE_HISTORY      "ChangeHistoryEnabled"
#define REG_CARET_WIDTH         "CaretWidth"
#define REG_CARET_LINE_VISIBLE  "CaretLineVisible"
#define REG_ZOOM_LEVEL          "ZoomLevel"
#define REG_FONT_NAME           "FontName"
#define REG_FONT_SIZE           "FontSize"

/* Registry value names - Theme */
#define REG_CURRENT_THEME       "Theme"

/* Registry value names - Find */
#define REG_MATCH_CASE          "MatchCase"
#define REG_WHOLE_WORD          "WholeWord"
#define REG_USE_REGEX           "UseRegex"

/* Registry value names - Session */
#define REG_RESTORE_SESSION     "RestoreSession"
#define REG_SAVE_ON_EXIT        "SaveOnExit"

/* Registry value names - Behavior */
#define REG_SINGLE_INSTANCE     "SingleInstance"
#define REG_CONFIRM_EXIT        "ConfirmExit"
#define REG_AUTO_SAVE_INTERVAL  "AutoSaveInterval"
#define REG_BACKUP_ON_SAVE      "BackupOnSave"
#define REG_HIGHLIGHT_MATCHING  "HighlightMatchingWords"
#define REG_HIGHLIGHT_LINE      "HighlightCurrentLine"

/* Registry value names - Window */
#define REG_WINDOW_X            "X"
#define REG_WINDOW_Y            "Y"
#define REG_WINDOW_WIDTH        "Width"
#define REG_WINDOW_HEIGHT       "Height"
#define REG_WINDOW_MAXIMIZED    "Maximized"

/* Registry value names - Recent Files */
#define REG_RECENT_COUNT        "Count"
#define REG_RECENT_FILE_PREFIX  "File"

/* Registry value names - Session Tabs */
#define REG_TAB_COUNT           "TabCount"
#define REG_ACTIVE_TAB          "ActiveTab"
#define REG_TAB_PATH_PREFIX     "Tab"
#define REG_TAB_FILEPATH        "FilePath"
#define REG_TAB_CURSOR          "CursorPosition"
#define REG_TAB_FIRSTLINE       "FirstVisibleLine"
#define REG_TAB_ZOOM            "ZoomLevel"
#define REG_TAB_MODIFIED        "IsModified"
#define REG_TAB_UNSAVED         "IsUnsaved"
#define REG_TAB_PINNED          "IsPinned"
#define REG_TAB_TEMPPATH        "TempFilePath"
#define REG_TAB_DISPLAYNAME     "DisplayName"
#define REG_TAB_WORDWRAP        "WordWrap"
#define REG_TAB_LINENUMBERS     "LineNumbers"
#define REG_TAB_SHOWWHITESPACE  "ShowWhitespace"
#define REG_TAB_AUTOINDENT      "AutoIndent"
#define REG_TAB_CODEFOLDING     "CodeFoldingEnabled"
#define REG_TAB_CHANGEHISTORY   "ChangeHistoryEnabled"
#define REG_TAB_SPLITVIEW       "IsSplitView"

/*
 * Registry initialization
 * Creates all registry keys with default values if they don't exist
 */
BOOL InitializeRegistry(void);

/*
 * Save user settings to registry
 * Returns TRUE on success, FALSE on failure
 */
BOOL SaveToRegistry(const AppConfig* config);

/*
 * Load user settings from registry
 * Returns TRUE if settings were loaded, FALSE if registry doesn't exist (use defaults)
 */
BOOL LoadFromRegistry(AppConfig* config);

/*
 * Check if registry configuration exists
 * Returns TRUE if registry has been configured, FALSE otherwise
 */
BOOL IsRegistryConfigured(void);

/*
 * Delete all registry configuration (for uninstall)
 * Returns TRUE on success, FALSE on failure
 */
BOOL DeleteRegistryConfig(void);

/*
 * Helper: Read DWORD value from registry
 */
BOOL RegReadDWORD(HKEY rootKey, const char* path, const char* name, DWORD* value);

/*
 * Helper: Write DWORD value to registry
 */
BOOL RegWriteDWORD(HKEY rootKey, const char* path, const char* name, DWORD value);

/*
 * Helper: Read string value from registry
 */
BOOL RegReadString(HKEY rootKey, const char* path, const char* name, char* buffer, DWORD bufferSize);

/*
 * Helper: Write string value to registry
 */
BOOL RegWriteString(HKEY rootKey, const char* path, const char* name, const char* value);

/*
 * Helper: Read BOOL value from registry (stored as DWORD)
 */
BOOL RegReadBOOL(HKEY rootKey, const char* path, const char* name, BOOL* value);

/*
 * Helper: Write BOOL value to registry (stored as DWORD)
 */
BOOL RegWriteBOOL(HKEY rootKey, const char* path, const char* name, BOOL value);

/*
 * Save window state to registry
 */
BOOL SaveWindowStateToRegistry(int x, int y, int width, int height, BOOL maximized);

/*
 * Load window state from registry
 */
BOOL LoadWindowStateFromRegistry(int* x, int* y, int* width, int* height, BOOL* maximized);

/*
 * Save recent files to registry
 */
BOOL SaveRecentFilesToRegistry(const char recentFiles[][MAX_PATH], int count);

/*
 * Load recent files from registry
 */
BOOL LoadRecentFilesFromRegistry(char recentFiles[][MAX_PATH], int* count);

/*
 * Save session data to registry (tabs, active tab, etc.)
 */
BOOL SaveSessionToRegistry(const void* sessionData);

/*
 * Load session data from registry
 */
BOOL LoadSessionFromRegistry(void* sessionData);

/*
 * Check if session exists in registry
 */
BOOL HasSessionInRegistry(void);

/*
 * Clear session data from registry
 */
void ClearSessionRegistry(void);

#endif /* REGISTRY_CONFIG_H */
