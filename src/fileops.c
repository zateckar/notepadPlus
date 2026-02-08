/*
 * File operations implementation for Notepad+
 * Handles file save, load, and encoding operations
 */

#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fileops.h"
#include "tabs.h"
#include "config.h"
#include "syntax.h"
#include "statusbar.h"
#include "Scintilla.h"
#include "lexer_mappings_generated.h"

/* BOM markers */
static const unsigned char UTF8_BOM[] = {0xEF, 0xBB, 0xBF};
static const unsigned char UTF16_LE_BOM[] = {0xFF, 0xFE};
static const unsigned char UTF16_BE_BOM[] = {0xFE, 0xFF};

/* Encoding names */
static const char* g_encodingNames[] = {
    "UTF-8",
    "UTF-8 with BOM",
    "UTF-16 LE",
    "UTF-16 BE",
    "ANSI"
};

/* Line ending names */
static const char* g_lineEndingNames[] = {
    "Windows (CRLF)",
    "Unix (LF)",
    "Mac (CR)"
};

/* Get encoding name */
const char* GetEncodingName(FileEncoding encoding)
{
    if (encoding >= 0 && encoding < sizeof(g_encodingNames) / sizeof(g_encodingNames[0])) {
        return g_encodingNames[encoding];
    }
    return "Unknown";
}

/* Get line ending name */
const char* GetLineEndingName(LineEnding lineEnding)
{
    if (lineEnding >= 0 && lineEnding < sizeof(g_lineEndingNames) / sizeof(g_lineEndingNames[0])) {
        return g_lineEndingNames[lineEnding];
    }
    return "Unknown";
}

/* Detect file encoding from data */
int DetectFileEncodingFromData(const char* data, size_t size, BOOL* hasBOM)
{
    if (hasBOM) *hasBOM = FALSE;
    
    if (size < 2) {
        return ENCODING_UTF8;  /* Assume UTF-8 for small files */
    }
    
    const unsigned char* udata = (const unsigned char*)data;
    
    /* Check for BOM markers */
    if (size >= 3 && memcmp(udata, UTF8_BOM, 3) == 0) {
        if (hasBOM) *hasBOM = TRUE;
        return ENCODING_UTF8_BOM;
    }
    
    if (size >= 2 && memcmp(udata, UTF16_LE_BOM, 2) == 0) {
        if (hasBOM) *hasBOM = TRUE;
        return ENCODING_UTF16_LE;
    }
    
    if (size >= 2 && memcmp(udata, UTF16_BE_BOM, 2) == 0) {
        if (hasBOM) *hasBOM = TRUE;
        return ENCODING_UTF16_BE;
    }
    
    /* Check for valid UTF-8 */
    BOOL isValidUTF8 = TRUE;
    BOOL hasHighBytes = FALSE;
    
    for (size_t i = 0; i < size && isValidUTF8; i++) {
        if (udata[i] >= 0x80) {
            hasHighBytes = TRUE;
            if ((udata[i] & 0xE0) == 0xC0) {
                /* 2-byte sequence */
                if (i + 1 >= size || (udata[i + 1] & 0xC0) != 0x80) {
                    isValidUTF8 = FALSE;
                } else {
                    i++;
                }
            } else if ((udata[i] & 0xF0) == 0xE0) {
                /* 3-byte sequence */
                if (i + 2 >= size || (udata[i + 1] & 0xC0) != 0x80 || (udata[i + 2] & 0xC0) != 0x80) {
                    isValidUTF8 = FALSE;
                } else {
                    i += 2;
                }
            } else if ((udata[i] & 0xF8) == 0xF0) {
                /* 4-byte sequence */
                if (i + 3 >= size || (udata[i + 1] & 0xC0) != 0x80 || 
                    (udata[i + 2] & 0xC0) != 0x80 || (udata[i + 3] & 0xC0) != 0x80) {
                    isValidUTF8 = FALSE;
                } else {
                    i += 3;
                }
            } else {
                isValidUTF8 = FALSE;
            }
        }
    }
    
    if (isValidUTF8) {
        return ENCODING_UTF8;
    }
    
    /* If not valid UTF-8 and has high bytes, assume ANSI */
    if (hasHighBytes) {
        return ENCODING_ANSI;
    }
    
    return ENCODING_UTF8;  /* Pure ASCII is valid UTF-8 */
}

/* Detect line ending type from data */
int DetectLineEndingFromData(const char* data, size_t size)
{
    int crlfCount = 0;
    int lfCount = 0;
    int crCount = 0;
    
    for (size_t i = 0; i < size; i++) {
        if (data[i] == '\r') {
            if (i + 1 < size && data[i + 1] == '\n') {
                crlfCount++;
                i++;  /* Skip the LF */
            } else {
                crCount++;
            }
        } else if (data[i] == '\n') {
            lfCount++;
        }
    }
    
    /* Return the most common line ending */
    if (crlfCount >= lfCount && crlfCount >= crCount) {
        return LINEEND_CRLF;
    } else if (lfCount >= crCount) {
        return LINEEND_LF;
    } else {
        return LINEEND_CR;
    }
}

/* Convert from other encoding to UTF-8 */
char* ConvertToUTF8(const char* data, size_t size, int encoding, size_t* outSize)
{
    if (encoding == ENCODING_UTF8 || encoding == ENCODING_UTF8_BOM) {
        /* Already UTF-8, just copy (skip BOM if present) */
        size_t offset = (encoding == ENCODING_UTF8_BOM && size >= 3) ? 3 : 0;
        size_t newSize = size - offset;
        char* result = (char*)malloc(newSize + 1);
        if (result) {
            memcpy(result, data + offset, newSize);
            result[newSize] = '\0';
            if (outSize) *outSize = newSize;
        }
        return result;
    }
    
    if (encoding == ENCODING_UTF16_LE || encoding == ENCODING_UTF16_BE) {
        /* Skip BOM */
        size_t offset = 2;
        if (size < offset) {
            if (outSize) *outSize = 0;
            return _strdup("");
        }
        
        const wchar_t* wdata = (const wchar_t*)(data + offset);
        size_t wlen = (size - offset) / 2;
        
        /* Swap bytes for big-endian */
        wchar_t* swapped = NULL;
        if (encoding == ENCODING_UTF16_BE) {
            swapped = (wchar_t*)malloc(wlen * 2);
            if (swapped) {
                for (size_t i = 0; i < wlen; i++) {
                    swapped[i] = ((wdata[i] & 0xFF) << 8) | ((wdata[i] >> 8) & 0xFF);
                }
                wdata = swapped;
            }
        }
        
        /* Convert to UTF-8 */
        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wdata, (int)wlen, NULL, 0, NULL, NULL);
        if (utf8Size > 0) {
            char* result = (char*)malloc(utf8Size + 1);
            if (result) {
                WideCharToMultiByte(CP_UTF8, 0, wdata, (int)wlen, result, utf8Size, NULL, NULL);
                result[utf8Size] = '\0';
                if (outSize) *outSize = utf8Size;
                if (swapped) free(swapped);
                return result;
            }
        }
        if (swapped) free(swapped);
    }
    
    if (encoding == ENCODING_ANSI) {
        /* Convert ANSI to UTF-8 via wide char */
        int wideSize = MultiByteToWideChar(CP_ACP, 0, data, (int)size, NULL, 0);
        if (wideSize > 0) {
            wchar_t* wide = (wchar_t*)malloc((wideSize + 1) * sizeof(wchar_t));
            if (wide) {
                MultiByteToWideChar(CP_ACP, 0, data, (int)size, wide, wideSize);
                wide[wideSize] = L'\0';
                
                int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wide, wideSize, NULL, 0, NULL, NULL);
                if (utf8Size > 0) {
                    char* result = (char*)malloc(utf8Size + 1);
                    if (result) {
                        WideCharToMultiByte(CP_UTF8, 0, wide, wideSize, result, utf8Size, NULL, NULL);
                        result[utf8Size] = '\0';
                        if (outSize) *outSize = utf8Size;
                        free(wide);
                        return result;
                    }
                }
                free(wide);
            }
        }
    }
    
    if (outSize) *outSize = 0;
    return NULL;
}

/* Convert from UTF-8 to other encoding */
char* ConvertFromUTF8(const char* data, size_t size, int encoding, size_t* outSize)
{
    if (encoding == ENCODING_UTF8) {
        char* result = (char*)malloc(size + 1);
        if (result) {
            memcpy(result, data, size);
            result[size] = '\0';
            if (outSize) *outSize = size;
        }
        return result;
    }
    
    if (encoding == ENCODING_UTF8_BOM) {
        char* result = (char*)malloc(size + 4);
        if (result) {
            memcpy(result, UTF8_BOM, 3);
            memcpy(result + 3, data, size);
            result[size + 3] = '\0';
            if (outSize) *outSize = size + 3;
        }
        return result;
    }
    
    /* Convert UTF-8 to wide char first */
    int wideSize = MultiByteToWideChar(CP_UTF8, 0, data, (int)size, NULL, 0);
    if (wideSize <= 0) {
        if (outSize) *outSize = 0;
        return NULL;
    }
    
    wchar_t* wide = (wchar_t*)malloc((wideSize + 1) * sizeof(wchar_t));
    if (!wide) {
        if (outSize) *outSize = 0;
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, 0, data, (int)size, wide, wideSize);
    wide[wideSize] = L'\0';
    
    if (encoding == ENCODING_UTF16_LE) {
        size_t resultSize = 2 + wideSize * 2;
        char* result = (char*)malloc(resultSize + 2);
        if (result) {
            memcpy(result, UTF16_LE_BOM, 2);
            memcpy(result + 2, wide, wideSize * 2);
            result[resultSize] = '\0';
            result[resultSize + 1] = '\0';
            if (outSize) *outSize = resultSize;
        }
        free(wide);
        return result;
    }
    
    if (encoding == ENCODING_UTF16_BE) {
        size_t resultSize = 2 + wideSize * 2;
        char* result = (char*)malloc(resultSize + 2);
        if (result) {
            memcpy(result, UTF16_BE_BOM, 2);
            /* Swap bytes */
            for (int i = 0; i < wideSize; i++) {
                result[2 + i * 2] = (wide[i] >> 8) & 0xFF;
                result[2 + i * 2 + 1] = wide[i] & 0xFF;
            }
            result[resultSize] = '\0';
            result[resultSize + 1] = '\0';
            if (outSize) *outSize = resultSize;
        }
        free(wide);
        return result;
    }
    
    if (encoding == ENCODING_ANSI) {
        int ansiSize = WideCharToMultiByte(CP_ACP, 0, wide, wideSize, NULL, 0, NULL, NULL);
        if (ansiSize > 0) {
            char* result = (char*)malloc(ansiSize + 1);
            if (result) {
                WideCharToMultiByte(CP_ACP, 0, wide, wideSize, result, ansiSize, NULL, NULL);
                result[ansiSize] = '\0';
                if (outSize) *outSize = ansiSize;
                free(wide);
                return result;
            }
        }
        free(wide);
    }
    
    if (outSize) *outSize = 0;
    return NULL;
}

/* Confirm save changes dialog */
int ConfirmSaveChanges(HWND parent, const char* fileName)
{
    char message[MAX_PATH + 100];
    sprintf(message, "Do you want to save changes to %s?", fileName);
    
    return MessageBox(parent, message, "Notepad+", 
                     MB_YESNOCANCEL | MB_ICONQUESTION);
}

/* Save tab to file */
BOOL SaveTabToFile(int tabIndex)
{
    TabInfo* tab = GetTab(tabIndex);
    if (!tab || !tab->editorHandle) {
        return FALSE;
    }
    
    /* Check if this is a new file */
    if (strncmp(tab->filePath, "New ", 4) == 0) {
        return SaveTabToFileAs(tabIndex);
    }
    
    /* Get text from editor */
    long textLength = (long)SendMessage(tab->editorHandle, SCI_GETLENGTH, 0, 0);
    char* buffer = (char*)malloc(textLength + 1);
    if (!buffer) {
        MessageBox(NULL, "Out of memory", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    SendMessage(tab->editorHandle, SCI_GETTEXT, textLength + 1, (LPARAM)buffer);
    
    /* Write to file */
    FILE* file = fopen(tab->filePath, "wb");
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
    
    /* Mark as saved */
    SendMessage(tab->editorHandle, SCI_SETSAVEPOINT, 0, 0);
    SetTabModified(tabIndex, FALSE);
    
    /* Add to recent files */
    AddRecentFile(tab->filePath);
    
    return TRUE;
}

/* Save tab to file with new name */
BOOL SaveTabToFileAs(int tabIndex)
{
    TabInfo* tab = GetTab(tabIndex);
    if (!tab || !tab->editorHandle) {
        return FALSE;
    }
    
    OPENFILENAME ofn;
    char filename[MAX_PATH] = "";

    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = GetParent(tab->editorHandle);
    ofn.lpstrFilter = g_fileFilters;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER;
    ofn.lpstrDefExt = "txt";
    
    /* Pre-fill with current filename if it exists */
    if (strncmp(tab->filePath, "New ", 4) != 0) {
        strcpy(filename, tab->filePath);
    }
    
    if (!GetSaveFileName(&ofn)) {
        return FALSE;  /* User cancelled */
    }
    
    /* Update file path */
    strcpy(tab->filePath, filename);
    
    /* Update display name */
    char* lastName = strrchr(filename, '\\');
    if (lastName) {
        strcpy(tab->displayName, lastName + 1);
    } else {
        strcpy(tab->displayName, filename);
    }
    
    /* Apply syntax highlighting for new file type */
    ApplySyntaxHighlightingForFile(tab->editorHandle, filename);
    
    /* Update file type in status bar */
    const char* extension = strrchr(filename, '.');
    UpdateFileType(GetFileTypeFromExtension(extension));
    
    /* Now save */
    return SaveTabToFile(tabIndex);
}

/* Load file to tab */
BOOL LoadFileToTab(int tabIndex, const char* filePath)
{
    TabInfo* tab = GetTab(tabIndex);
    if (!tab || !tab->editorHandle || !filePath) {
        return FALSE;
    }
    
    /* Open file */
    FILE* file = fopen(filePath, "rb");
    if (!file) {
        MessageBox(NULL, "Failed to open file", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    /* Get file size */
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    /* Read file */
    char* buffer = (char*)malloc(fileSize + 1);
    if (!buffer) {
        fclose(file);
        MessageBox(NULL, "Out of memory", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    size_t bytesRead = fread(buffer, 1, fileSize, file);
    buffer[bytesRead] = '\0';
    fclose(file);
    
    /* Detect and convert encoding */
    BOOL hasBOM;
    int encoding = DetectFileEncodingFromData(buffer, bytesRead, &hasBOM);
    
    /* Explicit check for UTF-16 LE BOM - ensure proper detection */
    if (bytesRead >= 2 && (unsigned char)buffer[0] == 0xFF && (unsigned char)buffer[1] == 0xFE) {
        encoding = ENCODING_UTF16_LE;
        hasBOM = TRUE;
    }
    /* Explicit check for UTF-16 BE BOM */
    else if (bytesRead >= 2 && (unsigned char)buffer[0] == 0xFE && (unsigned char)buffer[1] == 0xFF) {
        encoding = ENCODING_UTF16_BE;
        hasBOM = TRUE;
    }
    /* Explicit check for UTF-8 BOM */
    else if (bytesRead >= 3 && (unsigned char)buffer[0] == 0xEF && 
             (unsigned char)buffer[1] == 0xBB && (unsigned char)buffer[2] == 0xBF) {
        encoding = ENCODING_UTF8_BOM;
        hasBOM = TRUE;
    }
    
    /* Debug log to file */
    FILE* log = fopen("d:\\DEV\\notepad+\\debug_load.txt", "a");
    if (log) {
        fprintf(log, "File: %s\n", filePath);
        fprintf(log, "Encoding: %d (UTF8=0, UTF16LE=2)\n", encoding);
        fprintf(log, "HasBOM: %d\n", hasBOM);
        fprintf(log, "Bytes: %zu\n", bytesRead);
        fprintf(log, "First 4 bytes: %02X %02X %02X %02X\n\n", 
                (unsigned char)buffer[0], (unsigned char)buffer[1],
                (unsigned char)buffer[2], (unsigned char)buffer[3]);
        fclose(log);
    }
    
    char* utf8Data = buffer;
    size_t utf8Size = bytesRead;
    
    if (encoding != ENCODING_UTF8) {
        utf8Data = ConvertToUTF8(buffer, bytesRead, encoding, &utf8Size);
        if (!utf8Data) {
            free(buffer);
            MessageBox(NULL, "Failed to convert file encoding", "Error", MB_ICONERROR | MB_OK);
            return FALSE;
        }
        free(buffer);
        buffer = utf8Data;
    }
    
    /* Set text in editor */
    SendMessage(tab->editorHandle, SCI_SETTEXT, 0, (LPARAM)utf8Data);
    SendMessage(tab->editorHandle, SCI_SETSAVEPOINT, 0, 0);
    SendMessage(tab->editorHandle, SCI_EMPTYUNDOBUFFER, 0, 0);
    
    free(buffer);
    
    /* Update tab info */
    strcpy(tab->filePath, filePath);
    char* lastName = strrchr(filePath, '\\');
    if (lastName) {
        strcpy(tab->displayName, lastName + 1);
    } else {
        strcpy(tab->displayName, filePath);
    }
    tab->isModified = FALSE;
    
    /* Apply syntax highlighting */
    ApplySyntaxHighlightingForFile(tab->editorHandle, filePath);
    
    /* Update file type in status bar */
    const char* extension = strrchr(filePath, '.');
    UpdateFileType(GetFileTypeFromExtension(extension));
    
    /* Add to recent files */
    AddRecentFile(filePath);
    
    return TRUE;
}
