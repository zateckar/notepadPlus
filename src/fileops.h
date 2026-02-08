/*
 * File operations header for Notepad+
 * Handles file save, load, and encoding operations
 */

#ifndef FILEOPS_H
#define FILEOPS_H

#include <windows.h>

/* NOTE: FileEncoding and LineEnding types are defined in tabs.h */
/* This header uses those types - include tabs.h before this header */

/* File information structure */
typedef struct {
    char filePath[MAX_PATH];
    int encoding;      /* FileEncoding from tabs.h */
    int lineEnding;    /* LineEnding from tabs.h */
    BOOL hasBOM;
    BOOL isReadOnly;
} FileInfo;

/* File operations */
BOOL SaveTabToFile(int tabIndex);
BOOL SaveTabToFileAs(int tabIndex);
BOOL LoadFileToTab(int tabIndex, const char* filePath);

/* Encoding detection and conversion (internal use - different signatures) */
int DetectFileEncodingFromData(const char* data, size_t size, BOOL* hasBOM);
int DetectLineEndingFromData(const char* data, size_t size);
char* ConvertToUTF8(const char* data, size_t size, int encoding, size_t* outSize);
char* ConvertFromUTF8(const char* data, size_t size, int encoding, size_t* outSize);

/* Close confirmation */
int ConfirmSaveChanges(HWND parent, const char* fileName);

#endif /* FILEOPS_H */
