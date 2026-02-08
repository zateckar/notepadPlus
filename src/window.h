/*
 * Window management header for Notepad+
 */

#ifndef WINDOW_H
#define WINDOW_H

#include <windows.h>

/* Window management functions */
BOOL InitializeWindow(HINSTANCE hInstance, int nCmdShow);
void CleanupWindow(void);
HWND GetMainWindow(void);
void HandleEditorResize(int width, int height);
void HandleWindowResize(int width, int height);

/* Window title update */
void UpdateWindowTitle(const char* filePath);

#endif /* WINDOW_H */
