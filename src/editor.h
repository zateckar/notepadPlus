/*
 * Editor component header for Notepad+
 * Interfaces with Scintilla editing component
 */

#ifndef EDITOR_H
#define EDITOR_H

#include <windows.h>
#include "Scintilla.h"

/* Editor initialization and cleanup */
BOOL InitializeEditor(HWND parentWindow);
void CleanupEditor(void);

/* Editor window management */
HWND GetEditorWindow(void);
void ResizeEditor(HWND parent, int width, int height);

/* File operations */
BOOL EditorNewFile(void);
BOOL EditorOpenFile(void);
BOOL EditorSaveFile(void);
BOOL EditorSaveFileAs(void);

/* Edit operations */
void EditorUndo(void);
void EditorRedo(void);
void EditorCut(void);
void EditorCopy(void);
void EditorPaste(void);
void EditorSelectAll(void);

/* Editor setup and configuration */
void ConfigureEditor(void);
void SetEditorTheme(int theme);
void SetLineNumbers(BOOL show);
void SetWordWrap(BOOL wrap);

/* Advanced features */
void SetBracketMatching(BOOL enable);
void SetCodeFolding(BOOL enable);
void SetAutoIndent(BOOL enable);
void UpdateBracketHighlight(void);
BOOL IsBracketMatchingEnabled(void);
BOOL IsCodeFoldingEnabled(void);
void EnableChangeHistory(HWND editor, BOOL enable);
BOOL IsChangeHistoryEnabled(void);
BOOL IsAutoIndentEnabled(void);

/* Autocomplete features */
void SetWordAutocomplete(BOOL enable);
BOOL IsWordAutocompleteEnabled(void);
void TriggerWordAutocompleteForEditor(HWND editorHandle);

/* Editor state and information */
BOOL IsModified(void);
int GetLineCount(void);
int GetCurrentLine(void);
int GetCurrentColumn(void);
long GetCurrentPosition(void);

#endif /* EDITOR_H */