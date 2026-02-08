/*
 * Editor component implementation for Notepad+
 * Interfaces with Scintilla editing component
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commdlg.h>
#include "editor.h"
#include "resource.h"
#include "config.h"

/* Editor state */
static HWND g_hEditor = NULL;
static char g_currentFile[MAX_PATH] = "";
static BOOL g_modified = FALSE;

/* Advanced features state */
static BOOL g_bracketMatchingEnabled = TRUE;
static BOOL g_codeFoldingEnabled = TRUE;
static BOOL g_autoIndentEnabled = TRUE;
static BOOL g_wordAutocompleteEnabled = TRUE;

/* Scintilla message function pointer - use the one from Scintilla.h */
static SciFnDirect g_SciFnDirect = NULL;
static sptr_t g_SciPtr = 0;

/* Send message to Scintilla editor */
static sptr_t SendEditor(unsigned int msg, uptr_t wParam, sptr_t lParam)
{
    return g_SciFnDirect(g_SciPtr, msg, wParam, lParam);
}

/* Load Scintilla DLL and initialize editor control */
static BOOL LoadScintilla(void)
{
    HMODULE hScintilla;
    char dllPath[MAX_PATH];
    
    /* Get the path to the executable directory */
    GetModuleFileName(NULL, dllPath, MAX_PATH);
    char* lastSlash = strrchr(dllPath, '\\');
    if (lastSlash) {
        lastSlash[1] = '\0';  /* Keep the backslash */
        strcat(dllPath, "Scintilla.dll");
    } else {
        strcpy(dllPath, "Scintilla.dll");
    }
    
    /* Load Scintilla DLL from executable directory */
    hScintilla = LoadLibrary(dllPath);
    if (!hScintilla) {
        char errorMsg[MAX_PATH + 100];
        sprintf(errorMsg, "Failed to load Scintilla.dll from: %s", dllPath);
        MessageBox(NULL, errorMsg, "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    /* Get the Scintilla function pointer */
    g_SciFnDirect = (SciFnDirect)GetProcAddress(hScintilla, "Scintilla_DirectFunction");
    if (!g_SciFnDirect) {
        MessageBox(NULL, "Failed to get Scintilla function pointer", "Error", MB_ICONERROR | MB_OK);
        FreeLibrary(hScintilla);
        return FALSE;
    }
    
    return TRUE;
}

/* Initialize the editor component */
BOOL InitializeEditor(HWND parentWindow)
{
    /* Load Scintilla DLL */
    if (!LoadScintilla()) {
        return FALSE;
    }
    
    /* Create Scintilla editor window */
    g_hEditor = CreateWindowEx(
        0,
        "Scintilla",
        "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 0, 0,
        parentWindow,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!g_hEditor) {
        MessageBox(NULL, "Failed to create Scintilla editor", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    /* Get Scintilla pointer */
    g_SciPtr = (sptr_t)GetWindowLongPtr(g_hEditor, 0);
    
    /* Configure editor */
    ConfigureEditor();
    
    return TRUE;
}

/* Cleanup editor resources */
void CleanupEditor(void)
{
    /* Scintilla control will be destroyed automatically when parent is destroyed */
    g_hEditor = NULL;
}

/* Get editor window handle */
HWND GetEditorWindow(void)
{
    return g_hEditor;
}

/* Resize editor to fit parent window */
void ResizeEditor(HWND parent, int width, int height)
{
    UNREFERENCED_PARAMETER(parent);
    
    if (g_hEditor) {
        /* Fill the entire client area for now */
        SetWindowPos(g_hEditor, NULL, 0, 0, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

/* Configure editor settings */
void ConfigureEditor(void)
{
    if (!g_hEditor) return;
    
    /* Enable DirectWrite for better text rendering on high-DPI displays */
    SendEditor(SCI_SETTECHNOLOGY, SC_TECHNOLOGY_DIRECTWRITE, 0);
    
    /* Set basic editor properties */
    SendEditor(SCI_SETCODEPAGE, CP_UTF8, 0);
    SendEditor(SCI_SETCARETLINEVISIBLE, 1, 0);
    SendEditor(SCI_SETCARETLINEBACK, 0xE8E8E8, 0);
    SendEditor(SCI_SETHSCROLLBAR, 1, 0);
    SendEditor(SCI_SETVSCROLLBAR, 1, 0);
    
    /* Enable automatic scroll width tracking for horizontal scrollbar */
    SendEditor(SCI_SETSCROLLWIDTH, 1, 0);
    SendEditor(SCI_SETSCROLLWIDTHTRACKING, 1, 0);
    
    /* Set default font */
    SendEditor(SCI_STYLESETFONT, STYLE_DEFAULT, (sptr_t)"Consolas");
    SendEditor(SCI_STYLESETSIZE, STYLE_DEFAULT, 9);
    SendEditor(SCI_STYLECLEARALL, 0, 0);
    
    /* Set line numbers */
    SendEditor(SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
    SendEditor(SCI_SETMARGINWIDTHN, 0, 30);
    
    /* Set selection colors */
    SendEditor(SCI_SETSELFORE, 1, 0xFFFFFF);
    SendEditor(SCI_SETSELBACK, 1, 0x3366CC);
    
    /* Enable brace matching */
    SendEditor(SCI_BRACEHIGHLIGHTINDICATOR, 1, 0);
    
    /* Set tab width */
    SendEditor(SCI_SETTABWIDTH, 4, 0);
    
    /* Enable advanced features by default */
    SetBracketMatching(TRUE);
    SetCodeFolding(TRUE);
    SetAutoIndent(TRUE);
}

/* Create a new file */
BOOL EditorNewFile(void)
{
    if (g_modified) {
        /* TODO: Ask to save current file */
    }
    
    /* Clear editor */
    SendEditor(SCI_CLEARALL, 0, 0);
    SendEditor(SCI_EMPTYUNDOBUFFER, 0, 0);
    
    /* Clear file name and reset modified state */
    g_currentFile[0] = '\0';
    g_modified = FALSE;
    
    return TRUE;
}

/* Open a file */
BOOL EditorOpenFile(void)
{
    OPENFILENAME ofn;
    char filename[MAX_PATH] = "";
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetParent(g_hEditor);
    ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = "txt";
    
    if (!GetOpenFileName(&ofn)) {
        return FALSE; /* User cancelled */
    }
    
    /* Load file into editor */
    FILE* file = fopen(filename, "rb");
    if (!file) {
        MessageBox(NULL, "Failed to open file", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    /* Get file size */
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    /* Allocate buffer and read file */
    char* buffer = (char*)malloc(fileSize + 1);
    if (!buffer) {
        fclose(file);
        MessageBox(NULL, "Out of memory", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    size_t bytesRead = fread(buffer, 1, fileSize, file);
    buffer[bytesRead] = '\0';
    fclose(file);
    
    /* Set text in editor */
    SendEditor(SCI_SETTEXT, 0, (sptr_t)buffer);
    free(buffer);
    
    /* Update file name and reset state */
    strcpy(g_currentFile, filename);
    SendEditor(SCI_EMPTYUNDOBUFFER, 0, 0);
    g_modified = FALSE;
    
    return TRUE;
}

/* Save current file */
BOOL EditorSaveFile(void)
{
    /* If no file name, use Save As */
    if (g_currentFile[0] == '\0') {
        return EditorSaveFileAs();
    }
    
    /* Get text from editor */
    long textLength = (long)SendEditor(SCI_GETLENGTH, 0, 0);
    char* buffer = (char*)malloc(textLength + 1);
    if (!buffer) {
        MessageBox(NULL, "Out of memory", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    SendEditor(SCI_GETTEXT, textLength + 1, (sptr_t)buffer);
    
    /* Write to file */
    FILE* file = fopen(g_currentFile, "wb");
    if (!file) {
        free(buffer);
        MessageBox(NULL, "Failed to save file", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    size_t bytesWritten = fwrite(buffer, 1, textLength, file);
    fclose(file);
    free(buffer);
    
    if (bytesWritten != (size_t)textLength) {
        MessageBox(NULL, "Failed to write complete file", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    /* Reset modified state */
    SendEditor(SCI_SETSAVEPOINT, 0, 0);
    g_modified = FALSE;
    
    return TRUE;
}

/* Save file with new name */
BOOL EditorSaveFileAs(void)
{
    OPENFILENAME ofn;
    char filename[MAX_PATH] = "";
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetParent(g_hEditor);
    ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = "txt";
    
    if (g_currentFile[0] != '\0') {
        strcpy(filename, g_currentFile);
    }
    
    if (!GetSaveFileName(&ofn)) {
        return FALSE; /* User cancelled */
    }
    
    /* Set new file name and save */
    strcpy(g_currentFile, filename);
    return EditorSaveFile();
}

/* Edit operations */
void EditorUndo(void)
{
    SendEditor(SCI_UNDO, 0, 0);
}

void EditorRedo(void)
{
    SendEditor(SCI_REDO, 0, 0);
}

void EditorCut(void)
{
    SendEditor(SCI_CUT, 0, 0);
}

void EditorCopy(void)
{
    SendEditor(SCI_COPY, 0, 0);
}

void EditorPaste(void)
{
    SendEditor(SCI_PASTE, 0, 0);
}

void EditorSelectAll(void)
{
    SendEditor(SCI_SELECTALL, 0, 0);
}

/* Editor state functions */
BOOL IsModified(void)
{
    return (SendEditor(SCI_GETMODIFY, 0, 0) != 0);
}

int GetLineCount(void)
{
    return (int)SendEditor(SCI_GETLINECOUNT, 0, 0);
}

int GetCurrentLine(void)
{
    long pos = (long)SendEditor(SCI_GETCURRENTPOS, 0, 0);
    return (int)SendEditor(SCI_LINEFROMPOSITION, pos, 0);
}

int GetCurrentColumn(void)
{
    long pos = (long)SendEditor(SCI_GETCURRENTPOS, 0, 0);
    long line = SendEditor(SCI_LINEFROMPOSITION, pos, 0);
    long lineStart = SendEditor(SCI_POSITIONFROMLINE, line, 0);
    return (int)(pos - lineStart);
}

long GetCurrentPosition(void)
{
    return (long)SendEditor(SCI_GETCURRENTPOS, 0, 0);
}

/* Advanced features implementation */

/* Enable/disable bracket matching */
void SetBracketMatching(BOOL enable)
{
    g_bracketMatchingEnabled = enable;
    
    if (!g_hEditor) return;
    
    if (enable) {
        /* Configure bracket highlighting colors */
        SendEditor(SCI_STYLESETFORE, STYLE_BRACELIGHT, 0x0000FF);  /* Blue foreground */
        SendEditor(SCI_STYLESETBACK, STYLE_BRACELIGHT, 0xFFFFE0);  /* Light yellow background */
        SendEditor(SCI_STYLESETBOLD, STYLE_BRACELIGHT, 1);         /* Bold */
        
        /* Configure bad brace highlighting */
        SendEditor(SCI_STYLESETFORE, STYLE_BRACEBAD, 0x0000FF);    /* Blue foreground */
        SendEditor(SCI_STYLESETBACK, STYLE_BRACEBAD, 0x0000FF);    /* Blue background for unmatched */
        
        /* Update current bracket highlight */
        UpdateBracketHighlight();
    } else {
        /* Clear bracket highlighting */
        SendEditor(SCI_BRACEBADLIGHT, INVALID_POSITION, 0);
    }
}

/* Enable/disable code folding */
void SetCodeFolding(BOOL enable)
{
    g_codeFoldingEnabled = enable;
    
    if (!g_hEditor) return;
    
    if (enable) {
        /* Set up folding margin (margin 2) */
        SendEditor(SCI_SETMARGINTYPEN, 2, SC_MARGIN_SYMBOL);
        SendEditor(SCI_SETMARGINMASKN, 2, SC_MASK_FOLDERS);
        SendEditor(SCI_SETMARGINWIDTHN, 2, 16);
        SendEditor(SCI_SETMARGINSENSITIVEN, 2, 1);
        
        /* Set folding markers - using box style for clean coverage */
        SendEditor(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPEN, SC_MARK_BOXMINUS);
        SendEditor(SCI_MARKERDEFINE, SC_MARKNUM_FOLDER, SC_MARK_BOXPLUS);
        SendEditor(SCI_MARKERDEFINE, SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE);
        SendEditor(SCI_MARKERDEFINE, SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNER);
        SendEditor(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEREND, SC_MARK_BOXPLUSCONNECTED);
        SendEditor(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPENMID, SC_MARK_BOXMINUSCONNECTED);
        SendEditor(SCI_MARKERDEFINE, SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNER);
        
        /* CRITICAL FIX: Apply theme colors immediately to prevent white/light patches in dark mode
         * This ensures the folding margin has correct colors from the moment folding is enabled */
        extern void ApplyThemeToEditor(HWND editor);
        ApplyThemeToEditor(g_hEditor);
        
        /* Enable automatic folding */
        SendEditor(SCI_SETAUTOMATICFOLD, SC_AUTOMATICFOLD_SHOW | SC_AUTOMATICFOLD_CLICK, 0);
        
        /* Set fold flags for visual indicators */
        SendEditor(SCI_SETFOLDFLAGS, SC_FOLDFLAG_LINEAFTER_CONTRACTED, 0);
    } else {
        /* Hide folding margin */
        SendEditor(SCI_SETMARGINWIDTHN, 2, 0);
    }
}

/* Enable/disable auto-indent */
void SetAutoIndent(BOOL enable)
{
    g_autoIndentEnabled = enable;
    
    if (!g_hEditor) return;
    
    if (enable) {
        /* Enable automatic indentation */
        SendEditor(SCI_SETINDENTATIONGUIDES, SC_IV_LOOKBOTH, 0);
        SendEditor(SCI_SETTABINDENTS, 1, 0);
        SendEditor(SCI_SETBACKSPACEUNINDENTS, 1, 0);
    } else {
        /* Disable indentation guides */
        SendEditor(SCI_SETINDENTATIONGUIDES, SC_IV_NONE, 0);
        SendEditor(SCI_SETTABINDENTS, 0, 0);
        SendEditor(SCI_SETBACKSPACEUNINDENTS, 0, 0);
    }
}


/* Update bracket highlighting at current position */
void UpdateBracketHighlight(void)
{
    if (!g_hEditor || !g_bracketMatchingEnabled) return;
    
    /* Get current position */
    long pos = (long)SendEditor(SCI_GETCURRENTPOS, 0, 0);
    
    /* Check if we're next to a bracket */
    char ch = 0;
    if (pos > 0) {
        ch = (char)SendEditor(SCI_GETCHARAT, pos - 1, 0);
    }
    
    /* Bracket characters to match */
    const char* brackets = "()[]{}";
    long bracketPos = -1;
    
    /* Check character before cursor */
    if (pos > 0 && strchr(brackets, ch)) {
        bracketPos = pos - 1;
    } else {
        /* Check character at cursor */
        ch = (char)SendEditor(SCI_GETCHARAT, pos, 0);
        if (strchr(brackets, ch)) {
            bracketPos = pos;
        }
    }
    
    if (bracketPos >= 0) {
        /* Find matching bracket */
        long matchPos = (long)SendEditor(SCI_BRACEMATCH, bracketPos, 0);
        
        if (matchPos >= 0) {
            /* Highlight matching brackets */
            SendEditor(SCI_BRACEHIGHLIGHT, bracketPos, matchPos);
        } else {
            /* Highlight bad bracket (no match) */
            SendEditor(SCI_BRACEBADLIGHT, bracketPos, 0);
        }
    } else {
        /* Clear bracket highlighting */
        SendEditor(SCI_BRACEBADLIGHT, INVALID_POSITION, 0);
    }
}

/* Check if bracket matching is enabled */
BOOL IsBracketMatchingEnabled(void)
{
    /* Read from config to allow default settings for new tabs */
    extern AppConfig* GetConfig(void);
    AppConfig* config = GetConfig();
    if (config) {
        return config->bracketMatching;
    }
    return g_bracketMatchingEnabled;
}

/* Check if code folding is enabled */
BOOL IsCodeFoldingEnabled(void)
{
    /* Read from config to allow default settings for new tabs */
    extern AppConfig* GetConfig(void);
    AppConfig* config = GetConfig();
    if (config) {
        return config->codeFoldingEnabled;
    }
    return g_codeFoldingEnabled;
}

/* Check if change history is enabled */
BOOL IsChangeHistoryEnabled(void)
{
    extern AppConfig* GetConfig(void);
    AppConfig* config = GetConfig();
    if (config) {
        return config->changeHistoryEnabled;
    }
    return TRUE;
}

/* Enable/disable change history markers for an editor */
void EnableChangeHistory(HWND editor, BOOL enable)
{
    if (!editor) return;
    
    if (enable) {
        /* CRITICAL: Change history can only be enabled when undo history is enabled and empty.
         * Per Scintilla docs: "It should be enabled once when a file is loaded after calling
         * SCI_SETUNDOCOLLECTION(true) and SCI_SETSAVEPOINT."
         * 
         * The caller must ensure content is loaded and SCI_SETSAVEPOINT is called BEFORE
         * calling this function. We just need to empty the undo buffer here. */
        
        /* Ensure undo collection is enabled */
        SendMessage(editor, SCI_SETUNDOCOLLECTION, 1, 0);
        
        /* Empty undo buffer - required before enabling change history */
        SendMessage(editor, SCI_EMPTYUNDOBUFFER, 0, 0);
        
        /* Enable change history with markers in margin */
        /* Scintilla automatically configures margin for history markers
         * when SC_CHANGE_HISTORY_MARKERS is enabled. No manual margin
         * setup needed - this prevents duplicate marker issues. */
        SendMessage(editor, SCI_SETCHANGEHISTORY, 3, 0);
    } else {
        /* Disable change history */
        SendMessage(editor, SCI_SETCHANGEHISTORY, 0, 0); /* SC_CHANGE_HISTORY_DISABLED */
        SendMessage(editor, SCI_SETMARGINWIDTHN, 3, 0);
    }
}

/* Check if auto-indent is enabled */
BOOL IsAutoIndentEnabled(void)
{
    return g_autoIndentEnabled;
}


/* Set line numbers visibility */
void SetLineNumbers(BOOL show)
{
    if (!g_hEditor) return;
    
    if (show) {
        /* Show line numbers in margin 0 */
        SendEditor(SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
        SendEditor(SCI_SETMARGINWIDTHN, 0, 40);
    } else {
        /* Hide line numbers */
        SendEditor(SCI_SETMARGINWIDTHN, 0, 0);
    }
}

/* Set word wrap mode */
void SetWordWrap(BOOL wrap)
{
    if (!g_hEditor) return;
    
    if (wrap) {
        /* Enable word wrap */
        SendEditor(SCI_SETWRAPMODE, SC_WRAP_WORD, 0);
    } else {
        /* Disable word wrap */
        SendEditor(SCI_SETWRAPMODE, SC_WRAP_NONE, 0);
    }
}

/* Word autocomplete implementation */

/* Enable/disable word autocomplete */
void SetWordAutocomplete(BOOL enable)
{
    g_wordAutocompleteEnabled = enable;
    
    if (!g_hEditor) return;
    
    if (enable) {
        /* Configure autocomplete behavior */
        SendEditor(SCI_AUTOCSETIGNORECASE, 1, 0);  /* Case-insensitive */
        SendEditor(SCI_AUTOCSETAUTOHIDE, 1, 0);    /* Auto-hide if no match */
        SendEditor(SCI_AUTOCSETCANCELATSTART, 0, 0); /* Don't cancel at start */
        SendEditor(SCI_AUTOCSETDROPRESTOFWORD, 1, 0); /* Drop rest of word on complete */
        SendEditor(SCI_AUTOCSETMAXHEIGHT, 10, 0);  /* Max 10 items visible */
    } else {
        /* Cancel any active autocomplete */
        if (SendEditor(SCI_AUTOCACTIVE, 0, 0)) {
            SendEditor(SCI_AUTOCCANCEL, 0, 0);
        }
    }
}

/* Check if word autocomplete is enabled */
BOOL IsWordAutocompleteEnabled(void)
{
    return g_wordAutocompleteEnabled;
}

/* Comparison function for qsort */
static int CompareStrings(const void* a, const void* b)
{
    return strcmp(*(const char**)a, *(const char**)b);
}

/* Trigger word autocomplete at current position */
void TriggerWordAutocompleteForEditor(HWND editorHandle)
{
    if (!editorHandle || !g_wordAutocompleteEnabled) return;
    
    /* Don't show if autocomplete is already active */
    if (SendMessage(editorHandle, SCI_AUTOCACTIVE, 0, 0)) {
        return;
    }
    
    /* Get current position */
    long pos = (long)SendMessage(editorHandle, SCI_GETCURRENTPOS, 0, 0);
    
    /* Get the current word start position */
    long wordStart = (long)SendMessage(editorHandle, SCI_WORDSTARTPOSITION, pos, 1);
    
    /* Calculate how many characters we've typed */
    long charsTyped = pos - wordStart;
    
    /* Only trigger if we've typed at least 2 characters */
    if (charsTyped < 2) {
        return;
    }
    
    /* Get the partial word */
    char partialWord[256];
    if (charsTyped >= (long)sizeof(partialWord)) {
        return;  /* Word too long */
    }
    
    struct Sci_TextRange tr;
    tr.chrg.cpMin = wordStart;
    tr.chrg.cpMax = pos;
    tr.lpstrText = partialWord;
    SendMessage(editorHandle, SCI_GETTEXTRANGE, 0, (LPARAM)&tr);
    partialWord[charsTyped] = '\0';
    
    /* Get all text from document */
    long textLength = (long)SendMessage(editorHandle, SCI_GETLENGTH, 0, 0);
    char* buffer = (char*)malloc(textLength + 1);
    if (!buffer) return;
    
    SendMessage(editorHandle, SCI_GETTEXT, textLength + 1, (LPARAM)buffer);
    
    /* Collect unique words that start with the partial word */
    char** words = (char**)malloc(sizeof(char*) * 1000);  /* Max 1000 words */
    int wordCount = 0;
    int maxWords = 1000;
    
    /* Tokenize the buffer to find words */
    char* token = strtok(buffer, " \t\n\r.,;:!?()[]{}\"'<>/\\|@#$%^&*+-=~`");
    while (token != NULL && wordCount < maxWords) {
        /* Check if word starts with partial word and is longer */
        if (strlen(token) > (size_t)charsTyped && 
            _strnicmp(token, partialWord, charsTyped) == 0 &&
            strcmp(token, partialWord) != 0) {
            
            /* Check if word is already in list */
            BOOL found = FALSE;
            for (int i = 0; i < wordCount; i++) {
                if (_stricmp(words[i], token) == 0) {
                    found = TRUE;
                    break;
                }
            }
            
            /* Add word if not duplicate */
            if (!found) {
                words[wordCount] = _strdup(token);
                if (words[wordCount]) {
                    wordCount++;
                }
            }
        }
        
        token = strtok(NULL, " \t\n\r.,;:!?()[]{}\"'<>/\\|@#$%^&*+-=~`");
    }
    
    free(buffer);
    
    /* If we found matching words, show autocomplete */
    if (wordCount > 0) {
        /* Sort words alphabetically */
        qsort(words, wordCount, sizeof(char*), CompareStrings);
        
        /* Build space-separated list for Scintilla */
        size_t totalLen = 0;
        for (int i = 0; i < wordCount; i++) {
            totalLen += strlen(words[i]) + 1;  /* +1 for space/null */
        }
        
        char* wordList = (char*)malloc(totalLen + 1);
        if (wordList) {
            wordList[0] = '\0';
            for (int i = 0; i < wordCount; i++) {
                if (i > 0) strcat(wordList, " ");
                strcat(wordList, words[i]);
            }
            
            /* Show autocomplete list */
            SendMessage(editorHandle, SCI_AUTOCSHOW, charsTyped, (LPARAM)wordList);
            
            free(wordList);
        }
    }
    
    /* Free word list */
    for (int i = 0; i < wordCount; i++) {
        free(words[i]);
    }
    free(words);
}
