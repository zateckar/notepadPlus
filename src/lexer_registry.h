/*
 * Dynamic lexer registry for Notepad+
 * Provides runtime mapping of file extensions to Scintilla lexers
 * Eliminates the need for hardcoded language lists
 */

#ifndef LEXER_REGISTRY_H
#define LEXER_REGISTRY_H

#include <windows.h>

/* Maximum number of lexers to support */
#define MAX_LEXERS 200
#define MAX_EXTENSIONS_PER_LEXER 20
#define MAX_LEXER_NAME_LENGTH 64
#define MAX_DISPLAY_NAME_LENGTH 128
#define MAX_CATEGORY_LENGTH 64

/* Extension hash map parameters */
#define EXTENSION_MAP_SIZE 512
#define EXTENSION_HASH_PRIME 521

/* Forward declaration */
typedef struct LexerRegistry LexerRegistry;

/* Individual lexer information */
typedef struct {
    char lexerName[MAX_LEXER_NAME_LENGTH];        /* e.g., "python" */
    char displayName[MAX_DISPLAY_NAME_LENGTH];     /* e.g., "Python" */
    char extensions[MAX_EXTENSIONS_PER_LEXER][16]; /* e.g., [".py", ".pyw"] */
    int extensionCount;                            /* Number of extensions */
    char category[MAX_CATEGORY_LENGTH];            /* e.g., "Programming" */
    int sclexConstant;                             /* SCLEX_* constant from SciLexer.h */
    BOOL isAvailable;                              /* Whether lexer is available in Lexilla */
} LexerInfo;

/* Hash map entry for extension lookup */
typedef struct ExtensionMapEntry {
    char extension[16];    /* File extension (e.g., ".py") */
    char lexerName[MAX_LEXER_NAME_LENGTH]; /* Target lexer name */
    struct ExtensionMapEntry* next;        /* For collision handling */
} ExtensionMapEntry;

/* Dynamic lexer registry */
struct LexerRegistry {
    BOOL initialized;
    int lexerCount;
    LexerInfo lexers[MAX_LEXERS];
    
    /* Hash map for fast extension lookup */
    ExtensionMapEntry* extensionMap[EXTENSION_MAP_SIZE];
    
    /* Cache for frequently accessed lexers */
    char lastExtension[16];
    char lastLexerName[MAX_LEXER_NAME_LENGTH];
};

/* Registry initialization and cleanup */
BOOL InitializeLexerRegistry(LexerRegistry* registry);
void CleanupLexerRegistry(LexerRegistry* registry);
BOOL IsLexerRegistryInitialized(const LexerRegistry* registry);

/* Language detection by extension */
const char* DetectLexerByExtension(const LexerRegistry* registry, const char* filepath);
const char* DetectLexerByExtensionFast(const LexerRegistry* registry, const char* extension);

/* Lexer information lookup */
LexerInfo* GetLexerInfo(const LexerRegistry* registry, const char* lexerName);
LexerInfo* GetLexerInfoByExtension(const LexerRegistry* registry, const char* extension);

/* Dynamic file filter generation */
char* BuildDynamicFileFilters(const LexerRegistry* registry, int* filterCount);
char* BuildFileFilterForCategory(const LexerRegistry* registry, const char* category, int* filterCount);

/* Registry enumeration */
int GetLexerCount(const LexerRegistry* registry);
LexerInfo* GetLexerByIndex(const LexerRegistry* registry, int index);

/* Utility functions */
const char* GetDefaultExtensionForLexer(const LexerRegistry* registry, const char* lexerName);
BOOL IsExtensionSupported(const LexerRegistry* registry, const char* extension);
void GetSupportedExtensions(const LexerRegistry* registry, char* buffer, size_t bufferSize);

/* Backward compatibility with legacy LanguageType enum */
typedef enum {
    LANG_NONE = 0,
    LANG_C, LANG_PYTHON, LANG_JAVASCRIPT, LANG_HTML, LANG_CSS,
    LANG_XML, LANG_JSON, LANG_MARKDOWN, LANG_BATCH, LANG_SQL,
    /* Additional legacy types can be mapped here */
} LanguageType;

LanguageType LanguageTypeFromLexerName(const char* lexerName);
const char* LexerNameFromLanguageType(LanguageType lang);

/* Debug and diagnostics */
void DebugPrintLexerRegistry(const LexerRegistry* registry);
void ValidateLexerRegistry(const LexerRegistry* registry);

#endif /* LEXER_REGISTRY_H */