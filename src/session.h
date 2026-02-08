/*
 * Session management header for Notepad+
 * Handles saving and restoring application session
 */

#ifndef SESSION_H
#define SESSION_H

#include <windows.h>

/* Maximum number of tabs to save in session */
#define MAX_SESSION_TABS 100

/* Session tab information */
typedef struct {
    char filePath[MAX_PATH];
    char tempFilePath[MAX_PATH];
    int cursorPosition;
    int scrollPosition;
    int firstVisibleLine;
    int zoomLevel;
    BOOL isModified;
    BOOL isUnsaved;
    BOOL isPinned;
    /* Per-tab view settings */
    BOOL wordWrap;
    BOOL showLineNumbers;
    BOOL showWhitespace;
    BOOL autoIndent;
    BOOL codeFoldingEnabled;
    BOOL changeHistoryEnabled;
    BOOL isSplitView;
} SessionTab;

/* Session data structure */
typedef struct {
    /* Window state */
    int windowX;
    int windowY;
    int windowWidth;
    int windowHeight;
    BOOL windowMaximized;
    
    /* Tab information */
    SessionTab tabs[MAX_SESSION_TABS];
    int tabCount;
    int activeTabIndex;
} SessionData;

/* Session system initialization */
BOOL InitializeSession(void);
void CleanupSession(void);

/* Session management */
BOOL SaveSession(void);
BOOL LoadSession(void);
BOOL RestoreSession(void);

/* Get session data */
SessionData* GetSessionData(void);

/* Session state */
BOOL HasSavedSession(void);
void ClearSession(void);

#endif /* SESSION_H */
