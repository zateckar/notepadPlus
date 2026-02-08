/*
 * Registry-based configuration system implementation for Notepad+
 * Handles user-configurable settings via Windows Registry
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "registry_config.h"
#include "config.h"
#include "session.h"

/* Forward declarations for internal helpers */
static BOOL CreateRegistryKey(HKEY rootKey, const char* path);
static BOOL WriteDefaultValues(void);

/*
 * Helper: Read DWORD value from registry
 */
BOOL RegReadDWORD(HKEY rootKey, const char* path, const char* name, DWORD* value)
{
    HKEY hKey;
    LONG result;
    DWORD dataSize = sizeof(DWORD);
    DWORD type;
    
    result = RegOpenKeyEx(rootKey, path, 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        return FALSE;
    }
    
    result = RegQueryValueEx(hKey, name, NULL, &type, (LPBYTE)value, &dataSize);
    RegCloseKey(hKey);
    
    return (result == ERROR_SUCCESS && type == REG_DWORD);
}

/*
 * Helper: Write DWORD value to registry
 */
BOOL RegWriteDWORD(HKEY rootKey, const char* path, const char* name, DWORD value)
{
    HKEY hKey;
    LONG result;
    
    result = RegCreateKeyEx(rootKey, path, 0, NULL, REG_OPTION_NON_VOLATILE,
                           KEY_WRITE, NULL, &hKey, NULL);
    if (result != ERROR_SUCCESS) {
        return FALSE;
    }
    
    result = RegSetValueEx(hKey, name, 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));
    RegCloseKey(hKey);
    
    return (result == ERROR_SUCCESS);
}

/*
 * Helper: Read string value from registry
 */
BOOL RegReadString(HKEY rootKey, const char* path, const char* name, char* buffer, DWORD bufferSize)
{
    HKEY hKey;
    LONG result;
    DWORD type;
    
    result = RegOpenKeyEx(rootKey, path, 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        return FALSE;
    }
    
    result = RegQueryValueEx(hKey, name, NULL, &type, (LPBYTE)buffer, &bufferSize);
    RegCloseKey(hKey);
    
    return (result == ERROR_SUCCESS && type == REG_SZ);
}

/*
 * Helper: Write string value to registry
 */
BOOL RegWriteString(HKEY rootKey, const char* path, const char* name, const char* value)
{
    HKEY hKey;
    LONG result;
    
    result = RegCreateKeyEx(rootKey, path, 0, NULL, REG_OPTION_NON_VOLATILE,
                           KEY_WRITE, NULL, &hKey, NULL);
    if (result != ERROR_SUCCESS) {
        return FALSE;
    }
    
    result = RegSetValueEx(hKey, name, 0, REG_SZ, (const BYTE*)value, 
                          (DWORD)(strlen(value) + 1));
    RegCloseKey(hKey);
    
    return (result == ERROR_SUCCESS);
}

/*
 * Helper: Read BOOL value from registry (stored as DWORD)
 */
BOOL RegReadBOOL(HKEY rootKey, const char* path, const char* name, BOOL* value)
{
    DWORD dwValue;
    if (RegReadDWORD(rootKey, path, name, &dwValue)) {
        *value = (dwValue != 0);
        return TRUE;
    }
    return FALSE;
}

/*
 * Helper: Write BOOL value to registry (stored as DWORD)
 */
BOOL RegWriteBOOL(HKEY rootKey, const char* path, const char* name, BOOL value)
{
    return RegWriteDWORD(rootKey, path, name, value ? 1 : 0);
}

/*
 * Create a registry key if it doesn't exist
 */
static BOOL CreateRegistryKey(HKEY rootKey, const char* path)
{
    HKEY hKey;
    LONG result;
    
    result = RegCreateKeyEx(rootKey, path, 0, NULL, REG_OPTION_NON_VOLATILE,
                           KEY_WRITE, NULL, &hKey, NULL);
    if (result != ERROR_SUCCESS) {
        return FALSE;
    }
    
    RegCloseKey(hKey);
    return TRUE;
}

/*
 * Write default values to registry
 */
static BOOL WriteDefaultValues(void)
{
    BOOL success = TRUE;
    
    /* View defaults */
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_VIEW_PATH, REG_SHOW_STATUSBAR, TRUE);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_VIEW_PATH, REG_SHOW_LINE_NUMBERS, TRUE);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_VIEW_PATH, REG_WORD_WRAP, FALSE);
    
    /* Editor defaults */
    success &= RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_TAB_WIDTH, 4);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_USE_SPACES, FALSE);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_SHOW_WHITESPACE, FALSE);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_AUTO_INDENT, FALSE);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_CODE_FOLDING, TRUE);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_BRACKET_MATCHING, TRUE);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_CHANGE_HISTORY, TRUE);
    success &= RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_ZOOM_LEVEL, 0);
    success &= RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_CARET_WIDTH, 1);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_CARET_LINE_VISIBLE, TRUE);
    success &= RegWriteString(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_FONT_NAME, "Consolas");
    success &= RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_FONT_SIZE, 10);
    
    /* Theme defaults */
    success &= RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_THEME_PATH, REG_CURRENT_THEME, 0);
    
    /* Find defaults */
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_FIND_PATH, REG_MATCH_CASE, FALSE);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_FIND_PATH, REG_WHOLE_WORD, FALSE);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_FIND_PATH, REG_USE_REGEX, FALSE);
    
    /* Session defaults */
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_SESSION_PATH, REG_RESTORE_SESSION, TRUE);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_SESSION_PATH, REG_SAVE_ON_EXIT, FALSE);
    
    /* Behavior defaults */
    success &= RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH, REG_AUTO_SAVE_INTERVAL, 0);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH, REG_SINGLE_INSTANCE, FALSE);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH, REG_CONFIRM_EXIT, FALSE);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH, REG_BACKUP_ON_SAVE, FALSE);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH, REG_HIGHLIGHT_MATCHING, TRUE);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH, REG_HIGHLIGHT_LINE, TRUE);
    
    return success;
}

/*
 * Initialize registry configuration
 * Creates all registry keys with default values if they don't exist
 */
BOOL InitializeRegistry(void)
{
    /* Create all registry keys */
    if (!CreateRegistryKey(REGISTRY_ROOT_KEY, REGISTRY_BASE_PATH)) {
        return FALSE;
    }
    if (!CreateRegistryKey(REGISTRY_ROOT_KEY, REGISTRY_VIEW_PATH)) {
        return FALSE;
    }
    if (!CreateRegistryKey(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH)) {
        return FALSE;
    }
    if (!CreateRegistryKey(REGISTRY_ROOT_KEY, REGISTRY_THEME_PATH)) {
        return FALSE;
    }
    if (!CreateRegistryKey(REGISTRY_ROOT_KEY, REGISTRY_FIND_PATH)) {
        return FALSE;
    }
    if (!CreateRegistryKey(REGISTRY_ROOT_KEY, REGISTRY_SESSION_PATH)) {
        return FALSE;
    }
    if (!CreateRegistryKey(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH)) {
        return FALSE;
    }
    if (!CreateRegistryKey(REGISTRY_ROOT_KEY, REGISTRY_WINDOW_PATH)) {
        return FALSE;
    }
    if (!CreateRegistryKey(REGISTRY_ROOT_KEY, REGISTRY_RECENT_PATH)) {
        return FALSE;
    }
    
    /* Write default values */
    return WriteDefaultValues();
}

/*
 * Check if registry configuration exists
 * Returns TRUE if registry has been configured, FALSE otherwise
 */
BOOL IsRegistryConfigured(void)
{
    HKEY hKey;
    LONG result;
    
    /* Check if the base registry key exists */
    result = RegOpenKeyEx(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        return FALSE;
    }
    
    RegCloseKey(hKey);
    return TRUE;
}

/*
 * Save user settings to registry
 */
BOOL SaveToRegistry(const AppConfig* config)
{
    if (!config) {
        return FALSE;
    }
    
    BOOL success = TRUE;
    
    /* View settings */
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_VIEW_PATH, REG_SHOW_STATUSBAR, config->showStatusBar);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_VIEW_PATH, REG_SHOW_LINE_NUMBERS, config->showLineNumbers);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_VIEW_PATH, REG_WORD_WRAP, config->wordWrap);
    
    /* Editor settings */
    success &= RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_TAB_WIDTH, config->tabWidth);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_USE_SPACES, config->useSpaces);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_SHOW_WHITESPACE, config->showWhitespace);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_AUTO_INDENT, config->autoIndent);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_CODE_FOLDING, config->codeFoldingEnabled);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_BRACKET_MATCHING, config->bracketMatching);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_CHANGE_HISTORY, config->changeHistoryEnabled);
    success &= RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_ZOOM_LEVEL, config->zoomLevel);
    success &= RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_CARET_WIDTH, config->caretWidth);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_CARET_LINE_VISIBLE, config->highlightCurrentLine);
    success &= RegWriteString(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_FONT_NAME, config->fontName);
    success &= RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_FONT_SIZE, config->fontSize);
    
    /* Theme settings */
    success &= RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_THEME_PATH, REG_CURRENT_THEME, config->theme);
    
    /* Find settings */
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_FIND_PATH, REG_MATCH_CASE, config->matchCase);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_FIND_PATH, REG_WHOLE_WORD, config->wholeWord);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_FIND_PATH, REG_USE_REGEX, config->useRegex);
    
    /* Session settings */
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_SESSION_PATH, REG_RESTORE_SESSION, config->restoreSession);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_SESSION_PATH, REG_SAVE_ON_EXIT, config->saveOnExit);
    
    /* Behavior settings */
    success &= RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH, REG_AUTO_SAVE_INTERVAL, config->autoSaveInterval);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH, REG_SINGLE_INSTANCE, config->singleInstance);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH, REG_CONFIRM_EXIT, config->confirmExit);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH, REG_BACKUP_ON_SAVE, config->backupOnSave);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH, REG_HIGHLIGHT_MATCHING, config->highlightMatchingWords);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH, REG_HIGHLIGHT_LINE, config->highlightCurrentLine);
    
    return success;
}

/*
 * Load user settings from registry
 */
BOOL LoadFromRegistry(AppConfig* config)
{
    if (!config) {
        return FALSE;
    }
    
    /* Check if registry exists */
    if (!IsRegistryConfigured()) {
        return FALSE;
    }
    
    /* View settings */
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_VIEW_PATH, REG_SHOW_STATUSBAR, &config->showStatusBar);
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_VIEW_PATH, REG_SHOW_LINE_NUMBERS, &config->showLineNumbers);
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_VIEW_PATH, REG_WORD_WRAP, &config->wordWrap);
    
    /* Editor settings */
    DWORD dwValue;
    if (RegReadDWORD(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_TAB_WIDTH, &dwValue)) {
        config->tabWidth = (int)dwValue;
    }
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_USE_SPACES, &config->useSpaces);
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_SHOW_WHITESPACE, &config->showWhitespace);
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_AUTO_INDENT, &config->autoIndent);
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_CODE_FOLDING, &config->codeFoldingEnabled);
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_BRACKET_MATCHING, &config->bracketMatching);
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_CHANGE_HISTORY, &config->changeHistoryEnabled);
    if (RegReadDWORD(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_ZOOM_LEVEL, &dwValue)) {
        config->zoomLevel = (int)dwValue;
    }
    if (RegReadDWORD(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_CARET_WIDTH, &dwValue)) {
        config->caretWidth = (int)dwValue;
    }
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_CARET_LINE_VISIBLE, &config->highlightCurrentLine);
    RegReadString(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_FONT_NAME, config->fontName, sizeof(config->fontName));
    if (RegReadDWORD(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH, REG_FONT_SIZE, &dwValue)) {
        config->fontSize = (int)dwValue;
    }
    
    /* Theme settings */
    if (RegReadDWORD(REGISTRY_ROOT_KEY, REGISTRY_THEME_PATH, REG_CURRENT_THEME, &dwValue)) {
        config->theme = (int)dwValue;
    }
    
    /* Find settings */
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_FIND_PATH, REG_MATCH_CASE, &config->matchCase);
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_FIND_PATH, REG_WHOLE_WORD, &config->wholeWord);
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_FIND_PATH, REG_USE_REGEX, &config->useRegex);
    
    /* Session settings */
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_SESSION_PATH, REG_RESTORE_SESSION, &config->restoreSession);
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_SESSION_PATH, REG_SAVE_ON_EXIT, &config->saveOnExit);
    
    /* Behavior settings */
    if (RegReadDWORD(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH, REG_AUTO_SAVE_INTERVAL, &dwValue)) {
        config->autoSaveInterval = (int)dwValue;
    }
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH, REG_SINGLE_INSTANCE, &config->singleInstance);
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH, REG_CONFIRM_EXIT, &config->confirmExit);
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH, REG_BACKUP_ON_SAVE, &config->backupOnSave);
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH, REG_HIGHLIGHT_MATCHING, &config->highlightMatchingWords);
    RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH, REG_HIGHLIGHT_LINE, &config->highlightCurrentLine);
    
    return TRUE;
}

/*
 * Delete all registry configuration (for uninstall)
 */
BOOL DeleteRegistryConfig(void)
{
    LONG result;
    
    /* Delete all tab subkeys under Session first */
    for (int i = 0; i < 100; i++) {
        char tabPath[MAX_PATH];
        sprintf(tabPath, "%s\\%s%d", REGISTRY_SESSION_PATH, REG_TAB_PATH_PREFIX, i);
        RegDeleteKey(REGISTRY_ROOT_KEY, tabPath);
    }
    
    /* Delete all subkeys */
    RegDeleteKey(REGISTRY_ROOT_KEY, REGISTRY_RECENT_PATH);
    RegDeleteKey(REGISTRY_ROOT_KEY, REGISTRY_WINDOW_PATH);
    RegDeleteKey(REGISTRY_ROOT_KEY, REGISTRY_BEHAVIOR_PATH);
    RegDeleteKey(REGISTRY_ROOT_KEY, REGISTRY_SESSION_PATH);
    RegDeleteKey(REGISTRY_ROOT_KEY, REGISTRY_FIND_PATH);
    RegDeleteKey(REGISTRY_ROOT_KEY, REGISTRY_THEME_PATH);
    RegDeleteKey(REGISTRY_ROOT_KEY, REGISTRY_EDITOR_PATH);
    RegDeleteKey(REGISTRY_ROOT_KEY, REGISTRY_VIEW_PATH);
    
    /* Delete base key */
    result = RegDeleteKey(REGISTRY_ROOT_KEY, REGISTRY_BASE_PATH);
    
    return (result == ERROR_SUCCESS);
}

/*
 * Save window state to registry
 */
BOOL SaveWindowStateToRegistry(int x, int y, int width, int height, BOOL maximized)
{
    BOOL success = TRUE;
    
    success &= RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_WINDOW_PATH, REG_WINDOW_X, x);
    success &= RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_WINDOW_PATH, REG_WINDOW_Y, y);
    success &= RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_WINDOW_PATH, REG_WINDOW_WIDTH, width);
    success &= RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_WINDOW_PATH, REG_WINDOW_HEIGHT, height);
    success &= RegWriteBOOL(REGISTRY_ROOT_KEY, REGISTRY_WINDOW_PATH, REG_WINDOW_MAXIMIZED, maximized);
    
    return success;
}

/*
 * Load window state from registry
 */
BOOL LoadWindowStateFromRegistry(int* x, int* y, int* width, int* height, BOOL* maximized)
{
    if (!x || !y || !width || !height || !maximized) {
        return FALSE;
    }
    
    DWORD dwValue;
    
    if (RegReadDWORD(REGISTRY_ROOT_KEY, REGISTRY_WINDOW_PATH, REG_WINDOW_X, &dwValue)) {
        *x = (int)dwValue;
    } else {
        return FALSE;
    }
    
    if (RegReadDWORD(REGISTRY_ROOT_KEY, REGISTRY_WINDOW_PATH, REG_WINDOW_Y, &dwValue)) {
        *y = (int)dwValue;
    } else {
        return FALSE;
    }
    
    if (RegReadDWORD(REGISTRY_ROOT_KEY, REGISTRY_WINDOW_PATH, REG_WINDOW_WIDTH, &dwValue)) {
        *width = (int)dwValue;
    } else {
        return FALSE;
    }
    
    if (RegReadDWORD(REGISTRY_ROOT_KEY, REGISTRY_WINDOW_PATH, REG_WINDOW_HEIGHT, &dwValue)) {
        *height = (int)dwValue;
    } else {
        return FALSE;
    }
    
    if (!RegReadBOOL(REGISTRY_ROOT_KEY, REGISTRY_WINDOW_PATH, REG_WINDOW_MAXIMIZED, maximized)) {
        return FALSE;
    }
    
    return TRUE;
}

/*
 * Save recent files to registry
 */
BOOL SaveRecentFilesToRegistry(const char recentFiles[][MAX_PATH], int count)
{
    if (!recentFiles || count < 0 || count > 10) {
        return FALSE;
    }
    
    BOOL success = TRUE;
    
    /* Save count */
    success &= RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_RECENT_PATH, REG_RECENT_COUNT, count);
    
    /* Save each file path */
    for (int i = 0; i < count; i++) {
        char valueName[16];
        sprintf(valueName, "%s%d", REG_RECENT_FILE_PREFIX, i);
        success &= RegWriteString(REGISTRY_ROOT_KEY, REGISTRY_RECENT_PATH, valueName, recentFiles[i]);
    }
    
    return success;
}

/*
 * Load recent files from registry
 */
BOOL LoadRecentFilesFromRegistry(char recentFiles[][MAX_PATH], int* count)
{
    if (!recentFiles || !count) {
        return FALSE;
    }
    
    DWORD dwCount;
    if (!RegReadDWORD(REGISTRY_ROOT_KEY, REGISTRY_RECENT_PATH, REG_RECENT_COUNT, &dwCount)) {
        *count = 0;
        return FALSE;
    }
    
    *count = (int)dwCount;
    if (*count > 10) *count = 10;
    
    /* Load each file path */
    for (int i = 0; i < *count; i++) {
        char valueName[16];
        sprintf(valueName, "%s%d", REG_RECENT_FILE_PREFIX, i);
        if (!RegReadString(REGISTRY_ROOT_KEY, REGISTRY_RECENT_PATH, valueName,
                          recentFiles[i], MAX_PATH)) {
            recentFiles[i][0] = '\0';
        }
    }
    
    return TRUE;
}

/*
 * Save session data to registry (tabs, active tab, etc.)
 * The sessionData parameter should be a SessionData* pointer
 */
BOOL SaveSessionToRegistry(const void* sessionData)
{
    if (!sessionData) {
        return FALSE;
    }
    
    /* Use the actual SessionData type from session.h */
    const SessionData* session = (const SessionData*)sessionData;
    BOOL success = TRUE;
    
    /* Save window state */
    success &= SaveWindowStateToRegistry(session->windowX, session->windowY,
                                        session->windowWidth, session->windowHeight,
                                        session->windowMaximized);
    
    /* Save tab count and active tab */
    success &= RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_SESSION_PATH, REG_TAB_COUNT, session->tabCount);
    success &= RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_SESSION_PATH, REG_ACTIVE_TAB, session->activeTabIndex);
    
    /* Delete old tab entries that are no longer valid */
    /* This cleans up any leftover entries from previous sessions with more tabs */
    for (int i = session->tabCount; i < MAX_SESSION_TABS; i++) {
        char tabPath[MAX_PATH];
        sprintf(tabPath, "%s\\%s%d", REGISTRY_SESSION_PATH, REG_TAB_PATH_PREFIX, i);
        RegDeleteKey(REGISTRY_ROOT_KEY, tabPath);
    }
    
    /* Save each tab */
    for (int i = 0; i < session->tabCount && i < MAX_SESSION_TABS; i++) {
        char tabPath[MAX_PATH];
        sprintf(tabPath, "%s\\%s%d", REGISTRY_SESSION_PATH, REG_TAB_PATH_PREFIX, i);
        
        /* Create tab subkey */
        CreateRegistryKey(REGISTRY_ROOT_KEY, tabPath);
        
        /* Save tab data */
        success &= RegWriteString(REGISTRY_ROOT_KEY, tabPath, REG_TAB_FILEPATH, session->tabs[i].filePath);
        success &= RegWriteDWORD(REGISTRY_ROOT_KEY, tabPath, REG_TAB_CURSOR, session->tabs[i].cursorPosition);
        success &= RegWriteDWORD(REGISTRY_ROOT_KEY, tabPath, REG_TAB_FIRSTLINE, session->tabs[i].firstVisibleLine);
        success &= RegWriteDWORD(REGISTRY_ROOT_KEY, tabPath, REG_TAB_ZOOM, session->tabs[i].zoomLevel);
        success &= RegWriteBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_MODIFIED, session->tabs[i].isModified);
        success &= RegWriteBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_UNSAVED, session->tabs[i].isUnsaved);
        success &= RegWriteBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_PINNED, session->tabs[i].isPinned);
        
        /* Save per-tab view settings */
        success &= RegWriteBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_WORDWRAP, session->tabs[i].wordWrap);
        success &= RegWriteBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_LINENUMBERS, session->tabs[i].showLineNumbers);
        success &= RegWriteBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_SHOWWHITESPACE, session->tabs[i].showWhitespace);
        success &= RegWriteBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_AUTOINDENT, session->tabs[i].autoIndent);
        success &= RegWriteBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_CODEFOLDING, session->tabs[i].codeFoldingEnabled);
        success &= RegWriteBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_CHANGEHISTORY, session->tabs[i].changeHistoryEnabled);
        success &= RegWriteBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_SPLITVIEW, session->tabs[i].isSplitView);
        
        /* Save tempFilePath for BOTH new unsaved files AND existing files with unsaved modifications */
        if (session->tabs[i].tempFilePath[0] != '\0') {
            success &= RegWriteString(REGISTRY_ROOT_KEY, tabPath, REG_TAB_TEMPPATH, session->tabs[i].tempFilePath);
        }
    }
    
    return success;
}

/*
 * Load session data from registry
 * The sessionData parameter should be a SessionData* pointer
 */
BOOL LoadSessionFromRegistry(void* sessionData)
{
    if (!sessionData) {
        return FALSE;
    }
    
    /* Use the actual SessionData type from session.h */
    SessionData* session = (SessionData*)sessionData;
    
    /* Load window state - but don't fail if it doesn't exist */
    if (!LoadWindowStateFromRegistry(&session->windowX, &session->windowY,
                                     &session->windowWidth, &session->windowHeight,
                                     &session->windowMaximized)) {
        /* Set defaults if window state not found */
        session->windowX = CW_USEDEFAULT;
        session->windowY = CW_USEDEFAULT;
        session->windowWidth = 800;
        session->windowHeight = 600;
        session->windowMaximized = FALSE;
    }
    
    /* Load tab count and active tab */
    DWORD dwValue;
    if (!RegReadDWORD(REGISTRY_ROOT_KEY, REGISTRY_SESSION_PATH, REG_TAB_COUNT, &dwValue)) {
        /* No session data - clean up any orphaned tab entries and return FALSE */
        for (int i = 0; i < MAX_SESSION_TABS; i++) {
            char tabPath[MAX_PATH];
            sprintf(tabPath, "%s\\%s%d", REGISTRY_SESSION_PATH, REG_TAB_PATH_PREFIX, i);
            RegDeleteKey(REGISTRY_ROOT_KEY, tabPath);
        }
        session->tabCount = 0;
        session->activeTabIndex = 0;
        return FALSE;
    }
    session->tabCount = (int)dwValue;
    
    /* Sanity check: ensure tabCount is reasonable */
    if (session->tabCount < 0 || session->tabCount > MAX_SESSION_TABS) {
        /* Clean up any orphaned tab entries */
        for (int i = 0; i < MAX_SESSION_TABS; i++) {
            char tabPath[MAX_PATH];
            sprintf(tabPath, "%s\\%s%d", REGISTRY_SESSION_PATH, REG_TAB_PATH_PREFIX, i);
            RegDeleteKey(REGISTRY_ROOT_KEY, tabPath);
        }
        /* Also reset the tab count in registry */
        RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_SESSION_PATH, REG_TAB_COUNT, 0);
        session->tabCount = 0;
        session->activeTabIndex = 0;
        return FALSE;
    }
    
    if (RegReadDWORD(REGISTRY_ROOT_KEY, REGISTRY_SESSION_PATH, REG_ACTIVE_TAB, &dwValue)) {
        session->activeTabIndex = (int)dwValue;
    } else {
        session->activeTabIndex = 0;
    }
    
    /* Load each tab */
    for (int i = 0; i < session->tabCount && i < 100; i++) {
        char tabPath[MAX_PATH];
        sprintf(tabPath, "%s\\%s%d", REGISTRY_SESSION_PATH, REG_TAB_PATH_PREFIX, i);
        
        /* Load tab data */
        RegReadString(REGISTRY_ROOT_KEY, tabPath, REG_TAB_FILEPATH,
                     session->tabs[i].filePath, MAX_PATH);
        
        if (RegReadDWORD(REGISTRY_ROOT_KEY, tabPath, REG_TAB_CURSOR, &dwValue)) {
            session->tabs[i].cursorPosition = (int)dwValue;
        }
        
        if (RegReadDWORD(REGISTRY_ROOT_KEY, tabPath, REG_TAB_FIRSTLINE, &dwValue)) {
            session->tabs[i].firstVisibleLine = (int)dwValue;
        }
        
        if (RegReadDWORD(REGISTRY_ROOT_KEY, tabPath, REG_TAB_ZOOM, &dwValue)) {
            session->tabs[i].zoomLevel = (int)dwValue;
        }
        
        RegReadBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_MODIFIED, &session->tabs[i].isModified);
        RegReadBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_UNSAVED, &session->tabs[i].isUnsaved);
        RegReadBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_PINNED, &session->tabs[i].isPinned);
        
        /* Load per-tab view settings with defaults for legacy registry data */
        if (!RegReadBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_WORDWRAP, &session->tabs[i].wordWrap)) {
            session->tabs[i].wordWrap = FALSE;  /* Default: word wrap disabled */
        }
        if (!RegReadBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_LINENUMBERS, &session->tabs[i].showLineNumbers)) {
            session->tabs[i].showLineNumbers = TRUE;  /* Default: show line numbers */
        }
        if (!RegReadBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_SHOWWHITESPACE, &session->tabs[i].showWhitespace)) {
            session->tabs[i].showWhitespace = FALSE;  /* Default: hide whitespace */
        }
        if (!RegReadBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_AUTOINDENT, &session->tabs[i].autoIndent)) {
            session->tabs[i].autoIndent = TRUE;  /* Default: auto indent enabled */
        }
        if (!RegReadBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_CODEFOLDING, &session->tabs[i].codeFoldingEnabled)) {
            session->tabs[i].codeFoldingEnabled = FALSE;  /* Default: code folding disabled */
        }
        if (!RegReadBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_CHANGEHISTORY, &session->tabs[i].changeHistoryEnabled)) {
            session->tabs[i].changeHistoryEnabled = TRUE;  /* Default: change history enabled */
        }
        if (!RegReadBOOL(REGISTRY_ROOT_KEY, tabPath, REG_TAB_SPLITVIEW, &session->tabs[i].isSplitView)) {
            session->tabs[i].isSplitView = FALSE;  /* Default: split view disabled */
        }
        
        /* Always try to load tempFilePath - it can exist for both new unsaved files
         * AND existing files with unsaved modifications */
        if (!RegReadString(REGISTRY_ROOT_KEY, tabPath, REG_TAB_TEMPPATH,
                          session->tabs[i].tempFilePath, MAX_PATH)) {
            session->tabs[i].tempFilePath[0] = '\0';
        }
    }
    
    return TRUE;
}

/*
 * Check if session exists in registry
 */
BOOL HasSessionInRegistry(void)
{
    DWORD dwValue;
    return RegReadDWORD(REGISTRY_ROOT_KEY, REGISTRY_SESSION_PATH, REG_TAB_COUNT, &dwValue);
}

/*
 * Clear session data from registry
 */
void ClearSessionRegistry(void)
{
    /* Delete all tab subkeys */
    for (int i = 0; i < MAX_SESSION_TABS; i++) {
        char tabPath[MAX_PATH];
        sprintf(tabPath, "%s\\%s%d", REGISTRY_SESSION_PATH, REG_TAB_PATH_PREFIX, i);
        RegDeleteKey(REGISTRY_ROOT_KEY, tabPath);
    }
    
    /* Reset tab count and active tab */
    RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_SESSION_PATH, REG_TAB_COUNT, 0);
    RegWriteDWORD(REGISTRY_ROOT_KEY, REGISTRY_SESSION_PATH, REG_ACTIVE_TAB, 0);
}
