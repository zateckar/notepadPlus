/*
 * Find and Replace header for Notepad+
 */

#ifndef FINDREPLACE_H
#define FINDREPLACE_H

#include <windows.h>
#include "Scintilla.h"

/* Scintilla direct function pointer type */
typedef sptr_t (*SciFnDirect)(sptr_t ptr, unsigned int iMessage, uptr_t wParam, sptr_t lParam);

/* Find/replace configuration */
#define MAX_FIND_TEXT_LENGTH   256
#define MAX_REPLACE_TEXT_LENGTH 256
#define MAX_SEARCH_HISTORY     20

/* Dialog control IDs if not in resource.h */
#ifndef IDC_FIND_COMBO
#define IDC_FIND_COMBO 1001
#endif
#ifndef IDC_REPLACE_COMBO
#define IDC_REPLACE_COMBO 1002
#endif
#ifndef IDC_MATCH_CASE
#define IDC_MATCH_CASE 1003
#endif
#ifndef IDC_WHOLE_WORD
#define IDC_WHOLE_WORD 1004
#endif
#ifndef IDC_FIND_NEXT
#define IDC_FIND_NEXT 1005
#endif
#ifndef IDC_CLOSE_BUTTON
#define IDC_CLOSE_BUTTON 1006
#endif
#ifndef IDC_MARK_ALL
#define IDC_MARK_ALL 1013
#endif
#ifndef IDC_DIR_LABEL
#define IDC_DIR_LABEL 1007
#endif
#ifndef IDC_DIRECTION_UP
#define IDC_DIRECTION_UP 1008
#endif
#ifndef IDC_DIRECTION_DOWN
#define IDC_DIRECTION_DOWN 1009
#endif
#ifndef IDC_REPLACE
#define IDC_REPLACE 1011
#endif
#ifndef IDC_REPLACE_ALL
#define IDC_REPLACE_ALL 1012
#endif
#ifndef IDC_FIND_COUNT
#define IDC_FIND_COUNT 1014
#endif
#ifndef IDC_MARK_ALL
#define IDC_MARK_ALL 1013
#endif

/* Search direction */
typedef enum {
    SEARCH_DIRECTION_DOWN,
    SEARCH_DIRECTION_UP
} SearchDirection;

/* Find/replace state structure */
typedef struct {
    HWND hwndFind;              /* Find dialog window */
    HWND hwndReplace;           /* Replace dialog window */
    HWND hwndFindCombo;         /* Find text combo box */
    HWND hwndReplaceCombo;      /* Replace text combo box */
    HWND hwndMatchCase;         /* Match case checkbox */
    HWND hwndWholeWord;         /* Whole word checkbox */
    HWND hwndRegex;             /* Regex checkbox */
    HWND hwndDirectionUp;       /* Direction up radio */
    HWND hwndDirectionDown;     /* Direction down radio */
    HWND hwndFindNext;          /* Find Next button */
    HWND hwndReplaceBtn;        /* Replace button */
    HWND hwndReplaceAll;        /* Replace All button */
    HWND hwndMarkAll;           /* Mark All button */
    char findText[MAX_FIND_TEXT_LENGTH];
    char replaceText[MAX_REPLACE_TEXT_LENGTH];
    int flags;
    SearchDirection searchDirection;
} FindReplaceState;

/* Find/replace system initialization and cleanup */
BOOL InitializeFindReplace(void);
void CleanupFindReplace(void);

/* Dialog creation */
BOOL ShowFindDialog(HWND parent);
BOOL ShowReplaceDialog(HWND parent);

/* Control creation for dialogs */
void CreateFindControls(HWND hwnd);
void CreateReplaceControls(HWND hwnd);

/* Search operations */
BOOL FindNext(void);
BOOL FindPrevious(void);
BOOL Replace(void);
int ReplaceAll(void);
int CountMatches(void);

/* Search history */
void AddToSearchHistory(const char* text);
void ClearSearchHistory(void);

/* Get current find text */
const char* GetFindText(void);

/* Set find text for quick search (F3/Shift+F3) */
void SetFindText(const char* text);

/* Get find/replace state */
FindReplaceState* GetFindReplaceState(void);

#endif /* FINDREPLACE_H */
