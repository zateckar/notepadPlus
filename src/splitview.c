/*
 * Split view / Clone tab implementation for Notepad+
 * Enables cloning current tab content to a new tab for comparison
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "splitview.h"
#include "tabs.h"
#include "Scintilla.h"
#include "editor.h"

/* Forward declarations for tabs.c functions */
extern int GetSelectedTab(void);
extern TabInfo* GetTab(int index);
extern int GetTabCount(void);
extern int AddTabWithFile(const char* filePath, BOOL isNewFile);
extern void SelectTab(int index);

/* Clone counter for naming */
static int g_cloneCounter = 1;

/* Parent window handle */
static HWND g_parentWindow = NULL;

/* Initialize split view (minimal initialization for clone functionality) */
BOOL InitializeSplitView(HWND parentWindow)
{
    g_parentWindow = parentWindow;
    g_cloneCounter = 1;
    return TRUE;
}

/* Cleanup split view */
void CleanupSplitView(void)
{
    g_parentWindow = NULL;
}

/* Check if split view is enabled (always returns FALSE - we use clone instead) */
BOOL IsSplitViewEnabled(void)
{
    return FALSE;
}

/* Enable/disable split view - now shows a message about using Clone */
void EnableSplitView(BOOL enable)
{
    (void)enable; /* Unused */
    CloneCurrentTabToNewTab();
}

/* Resize split view - no-op since we don't use split view anymore */
void ResizeSplitView(int width, int height)
{
    (void)width;
    (void)height;
    /* No-op - we use tabs instead of split view */
}

/* Get left editor window - returns NULL since we don't use hidden editors */
HWND GetLeftEditorWindow(void)
{
    return NULL;
}

/* Get right editor window - returns NULL since we don't use hidden editors */
HWND GetRightEditorWindow(void)
{
    return NULL;
}

/* Synchronize scroll positions - no-op */
void SyncScrollPositions(HWND sourceEditor)
{
    (void)sourceEditor;
    /* No-op - tabs are independent */
}

/* Clone current tab content to a new tab (replaces LoadCurrentTabIntoLeftPane) */
BOOL LoadCurrentTabIntoLeftPane(void)
{
    return CloneCurrentTabToNewTab();
}

/* Clone current tab content to a new tab (replaces LoadCurrentTabIntoRightPane) */
BOOL LoadCurrentTabIntoRightPane(void)
{
    return CloneCurrentTabToNewTab();
}

/* Clone current tab content to a new tab */
BOOL CloneCurrentTabToNewTab(void)
{
    int currentTab = GetSelectedTab();
    if (currentTab < 0) {
        return FALSE;
    }
    
    TabInfo* sourceTab = GetTab(currentTab);
    if (!sourceTab || !sourceTab->editorHandle) {
        return FALSE;
    }
    
    /* Share the document handle if possible for true split view behavior in a new tab */
    /* This ensures changes in one are reflected in the other */
    void* pDoc = (void*)SendMessage(sourceTab->editorHandle, SCI_GETDOCPOINTER, 0, 0);
    
    /* Get text from current tab (as fallback) */
    int textLen = (int)SendMessage(sourceTab->editorHandle, SCI_GETLENGTH, 0, 0);
    char* text = NULL;
    if (!pDoc) {
        text = (char*)malloc(textLen + 1);
        if (!text) return FALSE;
        SendMessage(sourceTab->editorHandle, SCI_GETTEXT, textLen + 1, (LPARAM)text);
    }
    
    /* Create a clone name */
    char cloneName[MAX_PATH];
    if (strncmp(sourceTab->filePath, "New ", 4) == 0) {
        /* New file - create a simple clone name */
        sprintf(cloneName, "Clone %d of %s", g_cloneCounter++, sourceTab->displayName);
    } else {
        /* Existing file - show it's a clone */
        char* baseName = strrchr(sourceTab->filePath, '\\');
        if (baseName) {
            baseName++;
        } else {
            baseName = sourceTab->filePath;
        }
        sprintf(cloneName, "Clone %d of %s", g_cloneCounter++, baseName);
    }
    
    /* Create a new tab with the cloned content */
    int newTabIndex = AddTabWithFile(NULL, TRUE);  /* Create as new file */
    if (newTabIndex < 0) {
        free(text);
        return FALSE;
    }
    
    /* Get the new tab and set its content */
    TabInfo* newTab = GetTab(newTabIndex);
    if (!newTab || !newTab->editorHandle) {
        if (text) free(text);
        return FALSE;
    }
    
    /* Set the cloned text or share the doc */
    if (pDoc) {
        SendMessage(newTab->editorHandle, SCI_SETDOCPOINTER, 0, (LPARAM)pDoc);
    } else {
        SendMessage(newTab->editorHandle, SCI_SETTEXT, 0, (LPARAM)text);
    }
    
    /* Update the display name to show it's a clone */
    strncpy(newTab->displayName, cloneName, MAX_PATH - 1);
    newTab->displayName[MAX_PATH - 1] = '\0';  /* Ensure null termination */
    
    /* Also update filePath to match display (for new files) */
    strncpy(newTab->filePath, cloneName, MAX_PATH - 1);
    newTab->filePath[MAX_PATH - 1] = '\0';  /* Ensure null termination */
    
    /* If it's a shared document, modification status should be synced */
    newTab->isModified = sourceTab->isModified;
    
    /* Copy the actual file path so Save works correctly */
    if (sourceTab->filePath[0] != '\0' && strncmp(sourceTab->filePath, "New ", 4) != 0) {
        /* Use explicit size to avoid truncation warning */
        size_t len = strlen(sourceTab->filePath);
        if (len >= MAX_PATH) len = MAX_PATH - 1;
        memcpy(newTab->filePath, sourceTab->filePath, len);
        newTab->filePath[len] = '\0';  /* Ensure null termination */
    }
    
    /* Select the new cloned tab */
    SelectTab(newTabIndex);
    
    free(text);
    return TRUE;
}

/* Clear left pane - no-op */
void ClearLeftPane(void)
{
    /* No-op - we use tabs instead of split panes */
}

/* Clear right pane - no-op */
void ClearRightPane(void)
{
    /* No-op - we use tabs instead of split panes */
}
