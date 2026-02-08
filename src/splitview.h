/*
 * Split view / Clone tab header for Notepad+
 * Enables cloning current tab content to a new tab for comparison
 */

#ifndef SPLITVIEW_H
#define SPLITVIEW_H

#include <windows.h>

/* Split view management (legacy API - now redirects to clone functionality) */
BOOL InitializeSplitView(HWND parentWindow);
void CleanupSplitView(void);
BOOL IsSplitViewEnabled(void);
void EnableSplitView(BOOL enable);
void ResizeSplitView(int width, int height);

/* Split view editor access (legacy - returns NULL) */
HWND GetLeftEditorWindow(void);
HWND GetRightEditorWindow(void);
void SyncScrollPositions(HWND sourceEditor);

/* Load content into split view panes (legacy - now calls CloneCurrentTabToNewTab) */
BOOL LoadCurrentTabIntoLeftPane(void);
BOOL LoadCurrentTabIntoRightPane(void);
void ClearLeftPane(void);
void ClearRightPane(void);

/* Clone current tab to a new tab */
BOOL CloneCurrentTabToNewTab(void);

#endif /* SPLITVIEW_H */
