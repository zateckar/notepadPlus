/*
 * Syntax highlighting header for Notepad+
 * Automatic language detection and Scintilla lexer configuration
 * Comprehensive language support using build-time generated mappings
 */

#ifndef SYNTAX_H
#define SYNTAX_H

#include <windows.h>

/* Include auto-generated language type definitions */
#include "lexer_mappings_generated.h"

/* Initialize syntax highlighting system */
BOOL InitializeSyntax(void);

/* Cleanup syntax highlighting system */
void CleanupSyntax(void);

/* Detect language from file extension */
LanguageType DetectLanguage(const char* filePath);

/* Apply syntax highlighting to an editor window */
void ApplySyntaxHighlighting(HWND editor, LanguageType language);

/* Apply syntax highlighting based on file path */
void ApplySyntaxHighlightingForFile(HWND editor, const char* filePath);

/* Get language name for display */
const char* GetLanguageName(LanguageType language);

/* Get short language name for status bar */
const char* GetLanguageShortName(LanguageType language);

/* Set syntax colors based on current theme */
void ApplySyntaxColors(HWND editor, LanguageType language, BOOL isDarkTheme);

#endif /* SYNTAX_H */