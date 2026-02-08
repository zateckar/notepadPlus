/*
 * Syntax highlighting implementation for Notepad+
 * Uses Scintilla lexers via Lexilla for language support
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "syntax.h"
#include "Scintilla.h"
#include "SciLexer.h"
#include "Lexilla.h"
#include "themes.h"

/* For static linking, CreateLexer is directly available from Lexilla.h */
/* The CreateLexer function is declared in Lexilla.h and linked statically */

/* C/C++ keywords */
static const char* g_CKeywords = 
    "auto break case char const continue default do double else enum extern "
    "float for goto if inline int long register restrict return short signed "
    "sizeof static struct switch typedef union unsigned void volatile while "
    "_Bool _Complex _Imaginary bool true false NULL";

static const char* g_CppKeywords = 
    "class public private protected virtual override final new delete this "
    "template typename namespace using try catch throw noexcept constexpr "
    "nullptr static_cast dynamic_cast const_cast reinterpret_cast explicit "
    "friend mutable operator";

/* Python keywords */
static const char* g_PythonKeywords = 
    "and as assert async await break class continue def del elif else except "
    "finally for from global if import in is lambda None not or pass raise "
    "return try while with yield True False";

/* JavaScript keywords */
static const char* g_JSKeywords = 
    "break case catch class const continue debugger default delete do else "
    "export extends finally for function if import in instanceof let new "
    "return super switch this throw try typeof var void while with yield "
    "async await static true false null undefined NaN Infinity";

/* HTML tags */
static const char* g_HTMLKeywords = 
    "a abbr address area article aside audio b base bdi bdo blockquote body "
    "br button canvas caption cite code col colgroup data datalist dd del "
    "details dfn dialog div dl dt em embed fieldset figcaption figure footer "
    "form h1 h2 h3 h4 h5 h6 head header hr html i iframe img input ins kbd "
    "label legend li link main map mark meta meter nav noscript object ol "
    "optgroup option output p param picture pre progress q rp rt ruby s samp "
    "script section select small source span strong style sub summary sup "
    "table tbody td template textarea tfoot th thead time title tr track u "
    "ul var video wbr";

/* CSS keywords */
static const char* g_CSSKeywords = 
    "color background font margin padding border width height display position "
    "top left right bottom float clear text-align vertical-align line-height "
    "font-size font-family font-weight overflow visibility z-index opacity "
    "transform transition animation flex grid justify-content align-items";

/* SQL keywords */
static const char* g_SQLKeywords = 
    "SELECT FROM WHERE AND OR NOT IN IS NULL AS ORDER BY GROUP HAVING JOIN "
    "LEFT RIGHT INNER OUTER ON INSERT INTO VALUES UPDATE SET DELETE CREATE "
    "TABLE ALTER DROP INDEX VIEW TRIGGER PROCEDURE FUNCTION BEGIN END IF ELSE "
    "WHILE FOR CASE WHEN THEN DISTINCT COUNT SUM AVG MIN MAX UNION ALL";

/* Initialize syntax highlighting system */
BOOL InitializeSyntax(void)
{
    /* For static linking, Lexilla is already linked in.
     * CreateLexer() is available directly from Lexilla.h */
    return TRUE;
}

/* Cleanup syntax highlighting system */
void CleanupSyntax(void)
{
    /* Nothing to cleanup for static linking */
}

/* Detect language from file extension using generated mappings */
LanguageType DetectLanguage(const char* filePath)
{
    /* Validate input */
    if (!filePath || filePath[0] == '\0') {
        return LANG_NONE;
    }

    /* Find the last dot in the filename */
    const char* ext = strrchr(filePath, '.');
    if (!ext) {
        return LANG_NONE;
    }

    ext++; /* Skip the dot */
    
    /* Validate extension is not empty (filename ending with dot) */
    if (ext[0] == '\0') {
        return LANG_NONE;
    }

    /* Convert to lowercase for comparison - with bounds checking */
    char extLower[32] = {0};  /* Initialize to zero */
    int i;
    for (i = 0; ext[i] && i < 31; i++) {
        char c = ext[i];
        /* Only convert A-Z to lowercase */
        if (c >= 'A' && c <= 'Z') {
            extLower[i] = c + ('a' - 'A');
        } else {
            extLower[i] = c;
        }
    }
    extLower[i] = '\0';

    /* Add dot prefix for lookup - properly initialized */
    char extWithDot[64] = {0};  /* Initialize entire buffer to zero */
    extWithDot[0] = '.';
    
    /* Safe copy of extension */
    size_t srcLen = strlen(extLower);
    if (srcLen > 0 && srcLen < 62) {
        memcpy(extWithDot + 1, extLower, srcLen);
        /* extWithDot is already null-terminated due to buffer initialization */
    }

    /* Lookup in generated extension mappings with bounds check */
    for (size_t i = 0; i < EXTENSION_MAPPING_COUNT && i < 500; i++) {
        /* Validate entry before accessing */
        if (!g_extensionMappings[i].extension) {
            continue;
        }
        if (strcmp(extWithDot, g_extensionMappings[i].extension) == 0) {
            return g_extensionMappings[i].language;
        }
    }

    return LANG_NONE;
}

/* Apply syntax colors based on current theme */
void ApplySyntaxColors(HWND editor, LanguageType language, BOOL isDarkTheme)
{
    if (!editor) return;
    
    /* Define color schemes */
    COLORREF keyword, string, comment, number, operator_c, preprocessor;
    COLORREF type, function;
    
    if (isDarkTheme) {
        /* Dark theme colors */
        keyword = RGB(86, 156, 214);       /* Blue */
        string = RGB(206, 145, 120);       /* Orange-brown */
        comment = RGB(106, 153, 85);       /* Green */
        number = RGB(181, 206, 168);       /* Light green */
        operator_c = RGB(212, 212, 212);   /* Light gray */
        preprocessor = RGB(155, 89, 182);  /* Purple */
        type = RGB(78, 201, 176);          /* Teal */
        function = RGB(220, 220, 170);     /* Light yellow */
    } else {
        /* Light theme colors */
        keyword = RGB(0, 0, 255);          /* Blue */
        string = RGB(163, 21, 21);         /* Dark red */
        comment = RGB(0, 128, 0);          /* Green */
        number = RGB(9, 134, 88);          /* Teal */
        operator_c = RGB(0, 0, 0);         /* Black */
        preprocessor = RGB(111, 0, 138);   /* Purple */
        type = RGB(38, 127, 153);          /* Cyan */
        function = RGB(116, 83, 31);       /* Brown */
    }
    
    /* Apply colors based on language */
    switch (language) {
        case LANG_C:
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_WORD, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_WORD2, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_STRING, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_CHARACTER, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_COMMENT, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_COMMENTLINE, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_COMMENTDOC, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_NUMBER, number);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_OPERATOR, operator_c);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_PREPROCESSOR, preprocessor);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_C_WORD, TRUE);
            break;
            
        case LANG_PYTHON:
            SendMessage(editor, SCI_STYLESETFORE, SCE_P_WORD, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_P_WORD2, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_P_STRING, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_P_TRIPLE, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_P_TRIPLEDOUBLE, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_P_CHARACTER, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_P_COMMENTLINE, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_P_NUMBER, number);
            SendMessage(editor, SCI_STYLESETFORE, SCE_P_OPERATOR, operator_c);
            SendMessage(editor, SCI_STYLESETFORE, SCE_P_DEFNAME, function);
            SendMessage(editor, SCI_STYLESETFORE, SCE_P_CLASSNAME, type);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_P_WORD, TRUE);
            break;
            
        case LANG_JAVASCRIPT:
            /* JavaScript uses C-like lexer styles */
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_WORD, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_WORD2, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_STRING, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_CHARACTER, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_COMMENT, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_COMMENTLINE, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_COMMENTDOC, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_NUMBER, number);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_OPERATOR, operator_c);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_C_WORD, TRUE);
            break;
            
        case LANG_HTML:
        case LANG_XML:
            SendMessage(editor, SCI_STYLESETFORE, SCE_H_TAG, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_H_TAGUNKNOWN, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_H_ATTRIBUTE, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_H_ATTRIBUTEUNKNOWN, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_H_DOUBLESTRING, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_H_SINGLESTRING, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_H_COMMENT, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_H_NUMBER, number);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_H_TAG, TRUE);
            break;
            
        case LANG_CSS:
            SendMessage(editor, SCI_STYLESETFORE, SCE_CSS_TAG, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_CSS_CLASS, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_CSS_PSEUDOCLASS, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_CSS_IDENTIFIER, function);
            SendMessage(editor, SCI_STYLESETFORE, SCE_CSS_DOUBLESTRING, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_CSS_SINGLESTRING, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_CSS_COMMENT, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_CSS_VALUE, number);
            break;
            
        case LANG_SQL:
            SendMessage(editor, SCI_STYLESETFORE, SCE_SQL_WORD, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_SQL_STRING, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_SQL_CHARACTER, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_SQL_COMMENT, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_SQL_COMMENTLINE, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_SQL_NUMBER, number);
            SendMessage(editor, SCI_STYLESETFORE, SCE_SQL_OPERATOR, operator_c);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_SQL_WORD, TRUE);
            break;
            
        case LANG_JSON:
            /* JSON uses its own lexer styles */
            SendMessage(editor, SCI_STYLESETFORE, SCE_JSON_STRING, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_JSON_NUMBER, number);
            SendMessage(editor, SCI_STYLESETFORE, SCE_JSON_KEYWORD, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_JSON_OPERATOR, operator_c);
            SendMessage(editor, SCI_STYLESETFORE, SCE_JSON_PROPERTYNAME, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_JSON_LINECOMMENT, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_JSON_BLOCKCOMMENT, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_JSON_ESCAPESEQUENCE, preprocessor);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_JSON_KEYWORD, TRUE);
            break;
            
        case LANG_POWERSHELL:
            /* PowerShell styles */
            SendMessage(editor, SCI_STYLESETFORE, SCE_POWERSHELL_KEYWORD, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_POWERSHELL_CMDLET, function);
            SendMessage(editor, SCI_STYLESETFORE, SCE_POWERSHELL_ALIAS, function);
            SendMessage(editor, SCI_STYLESETFORE, SCE_POWERSHELL_FUNCTION, function);
            SendMessage(editor, SCI_STYLESETFORE, SCE_POWERSHELL_STRING, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_POWERSHELL_CHARACTER, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_POWERSHELL_HERE_STRING, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_POWERSHELL_HERE_CHARACTER, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_POWERSHELL_COMMENT, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_POWERSHELL_COMMENTSTREAM, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_POWERSHELL_COMMENTDOCKEYWORD, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_POWERSHELL_NUMBER, number);
            SendMessage(editor, SCI_STYLESETFORE, SCE_POWERSHELL_VARIABLE, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_POWERSHELL_OPERATOR, operator_c);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_POWERSHELL_KEYWORD, TRUE);
            break;
            
        case LANG_BATCH:
        case LANG_BATCHFILE:
            /* Batch/CMD styles */
            SendMessage(editor, SCI_STYLESETFORE, SCE_BAT_WORD, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_BAT_COMMAND, function);
            SendMessage(editor, SCI_STYLESETFORE, SCE_BAT_COMMENT, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_BAT_LABEL, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_BAT_IDENTIFIER, preprocessor);
            SendMessage(editor, SCI_STYLESETFORE, SCE_BAT_OPERATOR, operator_c);
            SendMessage(editor, SCI_STYLESETFORE, SCE_BAT_HIDE, comment);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_BAT_WORD, TRUE);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_BAT_LABEL, TRUE);
            break;
            
        case LANG_MARKDOWN:
            /* Markdown styles */
            SendMessage(editor, SCI_STYLESETFORE, SCE_MARKDOWN_HEADER1, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MARKDOWN_HEADER2, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MARKDOWN_HEADER3, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MARKDOWN_HEADER4, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MARKDOWN_HEADER5, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MARKDOWN_HEADER6, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MARKDOWN_STRONG1, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MARKDOWN_STRONG2, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MARKDOWN_EM1, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MARKDOWN_EM2, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MARKDOWN_CODE, preprocessor);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MARKDOWN_CODE2, preprocessor);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MARKDOWN_CODEBK, preprocessor);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MARKDOWN_LINK, function);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MARKDOWN_ULIST_ITEM, number);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MARKDOWN_OLIST_ITEM, number);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MARKDOWN_BLOCKQUOTE, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MARKDOWN_STRIKEOUT, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MARKDOWN_HRULE, operator_c);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_MARKDOWN_HEADER1, TRUE);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_MARKDOWN_HEADER2, TRUE);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_MARKDOWN_STRONG1, TRUE);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_MARKDOWN_STRONG2, TRUE);
            break;
            
        case LANG_SHELL:
            /* Shell/Bash styles */
            SendMessage(editor, SCI_STYLESETFORE, SCE_SH_WORD, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_SH_STRING, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_SH_CHARACTER, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_SH_COMMENTLINE, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_SH_NUMBER, number);
            SendMessage(editor, SCI_STYLESETFORE, SCE_SH_OPERATOR, operator_c);
            SendMessage(editor, SCI_STYLESETFORE, SCE_SH_SCALAR, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_SH_PARAM, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_SH_BACKTICKS, preprocessor);
            SendMessage(editor, SCI_STYLESETFORE, SCE_SH_HERE_DELIM, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_SH_HERE_Q, string);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_SH_WORD, TRUE);
            break;
            
        case LANG_RUBY:
            /* Ruby styles */
            SendMessage(editor, SCI_STYLESETFORE, SCE_RB_WORD, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RB_WORD_DEMOTED, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RB_STRING, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RB_CHARACTER, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RB_STRING_Q, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RB_STRING_QQ, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RB_COMMENTLINE, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RB_POD, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RB_NUMBER, number);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RB_OPERATOR, operator_c);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RB_SYMBOL, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RB_CLASSNAME, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RB_DEFNAME, function);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RB_MODULE_NAME, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RB_INSTANCE_VAR, preprocessor);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RB_CLASS_VAR, preprocessor);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RB_GLOBAL, preprocessor);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RB_REGEX, string);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_RB_WORD, TRUE);
            break;
            
        case LANG_LUA:
            /* Lua styles */
            SendMessage(editor, SCI_STYLESETFORE, SCE_LUA_WORD, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_LUA_WORD2, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_LUA_WORD3, function);
            SendMessage(editor, SCI_STYLESETFORE, SCE_LUA_WORD4, function);
            SendMessage(editor, SCI_STYLESETFORE, SCE_LUA_STRING, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_LUA_CHARACTER, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_LUA_LITERALSTRING, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_LUA_COMMENT, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_LUA_COMMENTLINE, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_LUA_COMMENTDOC, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_LUA_NUMBER, number);
            SendMessage(editor, SCI_STYLESETFORE, SCE_LUA_OPERATOR, operator_c);
            SendMessage(editor, SCI_STYLESETFORE, SCE_LUA_PREPROCESSOR, preprocessor);
            SendMessage(editor, SCI_STYLESETFORE, SCE_LUA_LABEL, type);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_LUA_WORD, TRUE);
            break;
            
        case LANG_PERL:
            /* Perl styles */
            SendMessage(editor, SCI_STYLESETFORE, SCE_PL_WORD, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_PL_STRING, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_PL_CHARACTER, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_PL_STRING_Q, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_PL_STRING_QQ, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_PL_COMMENTLINE, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_PL_POD, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_PL_POD_VERB, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_PL_NUMBER, number);
            SendMessage(editor, SCI_STYLESETFORE, SCE_PL_OPERATOR, operator_c);
            SendMessage(editor, SCI_STYLESETFORE, SCE_PL_PREPROCESSOR, preprocessor);
            SendMessage(editor, SCI_STYLESETFORE, SCE_PL_SCALAR, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_PL_ARRAY, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_PL_HASH, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_PL_REGEX, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_PL_REGSUBST, string);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_PL_WORD, TRUE);
            break;
            
        case LANG_RUST:
            /* Rust styles */
            SendMessage(editor, SCI_STYLESETFORE, SCE_RUST_WORD, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RUST_WORD2, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RUST_STRING, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RUST_STRINGR, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RUST_CHARACTER, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RUST_COMMENTBLOCK, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RUST_COMMENTLINE, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RUST_NUMBER, number);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RUST_OPERATOR, operator_c);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RUST_LIFETIME, preprocessor);
            SendMessage(editor, SCI_STYLESETFORE, SCE_RUST_MACRO, preprocessor);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_RUST_WORD, TRUE);
            break;
            
        case LANG_GO:
        case LANG_JAVA:
        case LANG_KOTLIN:
        case LANG_SCALA:
        case LANG_SWIFT:
        case LANG_TYPESCRIPT:
        case LANG_CS:
        case LANG_OBJECTIVE_C:
        case LANG_OBJECTIVE_CPP:
        case LANG_CPP:
        case LANG_D:
            /* C-like languages */
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_WORD, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_WORD2, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_STRING, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_CHARACTER, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_COMMENT, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_COMMENTLINE, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_COMMENTDOC, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_NUMBER, number);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_OPERATOR, operator_c);
            SendMessage(editor, SCI_STYLESETFORE, SCE_C_PREPROCESSOR, preprocessor);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_C_WORD, TRUE);
            break;
            
        case LANG_YAML:
            /* YAML styles */
            SendMessage(editor, SCI_STYLESETFORE, SCE_YAML_KEYWORD, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_YAML_IDENTIFIER, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_YAML_TEXT, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_YAML_COMMENT, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_YAML_NUMBER, number);
            SendMessage(editor, SCI_STYLESETFORE, SCE_YAML_OPERATOR, operator_c);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_YAML_KEYWORD, TRUE);
            break;
            
        case LANG_MAKEFILE:
            /* Makefile styles */
            SendMessage(editor, SCI_STYLESETFORE, SCE_MAKE_TARGET, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MAKE_IDENTIFIER, function);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MAKE_PREPROCESSOR, preprocessor);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MAKE_COMMENT, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_MAKE_OPERATOR, operator_c);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_MAKE_TARGET, TRUE);
            break;
            
        case LANG_DIFF:
            /* Diff styles */
            SendMessage(editor, SCI_STYLESETFORE, SCE_DIFF_COMMAND, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_DIFF_HEADER, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_DIFF_COMMENT, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_DIFF_ADDED, function);
            SendMessage(editor, SCI_STYLESETFORE, SCE_DIFF_DELETED, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_DIFF_CHANGED, number);
            break;
            
        case LANG_INI:
            /* INI/Properties styles */
            SendMessage(editor, SCI_STYLESETFORE, SCE_PROPS_SECTION, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_PROPS_KEY, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_PROPS_DEFVAL, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_PROPS_COMMENT, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_PROPS_ASSIGNMENT, operator_c);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_PROPS_SECTION, TRUE);
            break;

        case LANG_WINDOWS_REGISTRY:
            /* Windows Registry file styles */
            SendMessage(editor, SCI_STYLESETFORE, SCE_REG_VALUENAME, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_REG_STRING, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_REG_HEXDIGIT, number);
            SendMessage(editor, SCI_STYLESETFORE, SCE_REG_VALUETYPE, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_REG_COMMENT, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_REG_ADDEDKEY, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_REG_DELETEDKEY, string);
            SendMessage(editor, SCI_STYLESETFORE, SCE_REG_KEYPATH_GUID, preprocessor);
            SendMessage(editor, SCI_STYLESETFORE, SCE_REG_STRING_GUID, preprocessor);
            SendMessage(editor, SCI_STYLESETFORE, SCE_REG_OPERATOR, operator_c);
            SendMessage(editor, SCI_STYLESETFORE, SCE_REG_ESCAPED, operator_c);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_REG_VALUENAME, TRUE);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_REG_ADDEDKEY, TRUE);
            break;
            
        case LANG_TEX:
            /* TeX/LaTeX styles */
            SendMessage(editor, SCI_STYLESETFORE, SCE_L_COMMAND, keyword);
            SendMessage(editor, SCI_STYLESETFORE, SCE_L_TAG, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_L_TAG2, type);
            SendMessage(editor, SCI_STYLESETFORE, SCE_L_MATH, function);
            SendMessage(editor, SCI_STYLESETFORE, SCE_L_MATH2, function);
            SendMessage(editor, SCI_STYLESETFORE, SCE_L_COMMENT, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_L_COMMENT2, comment);
            SendMessage(editor, SCI_STYLESETFORE, SCE_L_SPECIAL, preprocessor);
            SendMessage(editor, SCI_STYLESETBOLD, SCE_L_COMMAND, TRUE);
            break;
            
        default:
            /* Plain text - no special colors */
            break;
    }
}

/* Apply syntax highlighting to an editor window using generated configurations */
void ApplySyntaxHighlighting(HWND editor, LanguageType language)
{
    if (!editor) return;

    /* Get current theme */
    Theme theme = GetCurrentTheme();
    BOOL isDark = (theme == THEME_DARK);

    /* Lookup configuration in generated lexer configs */
    const char* lexerName = NULL;
    const char* keywords1 = NULL;
    const char* keywords2 = NULL;

    for (size_t i = 0; i < LEXER_CONFIG_COUNT; i++) {
        if (g_lexerConfigs[i].language == language) {
            lexerName = g_lexerConfigs[i].lexerName;
            keywords1 = g_lexerConfigs[i].keywords1;
            keywords2 = g_lexerConfigs[i].keywords2;
            break;
        }
    }

    /* Try to set up lexer if Lexilla is available */
    /* For static linking, CreateLexer is directly available */
    if (lexerName) {
        /* Create and set the lexer via Lexilla */
        void* lexer = CreateLexer(lexerName);
        if (lexer) {
            SendMessage(editor, SCI_SETILEXER, 0, (LPARAM)lexer);

            /* Set keywords */
            if (keywords1) {
                SendMessage(editor, SCI_SETKEYWORDS, 0, (LPARAM)keywords1);
            }
            if (keywords2) {
                SendMessage(editor, SCI_SETKEYWORDS, 1, (LPARAM)keywords2);
            }

            /* Apply syntax colors */
            ApplySyntaxColors(editor, language, isDark);

            /* Refresh the display to apply colors */
            SendMessage(editor, SCI_COLOURISE, 0, -1);
            return;
        }
    }

    /* Fallback: Apply colors without lexer for basic keyword highlighting */
    /* This works even without Lexilla.dll by setting up manual keyword lists */
    if (language != LANG_NONE) {
        /* Set up basic keyword highlighting manually */
        SendMessage(editor, SCI_SETILEXER, 0, 0);  /* Clear any previous lexer */

        /* For C-like languages, we can use the built-in container lexer */
        if (language == LANG_C || language == LANG_CPP || language == LANG_JAVASCRIPT) {
            /* Set keywords manually */
            if (keywords1) {
                SendMessage(editor, SCI_SETKEYWORDS, 0, (LPARAM)keywords1);
            }
            if (keywords2) {
                SendMessage(editor, SCI_SETKEYWORDS, 1, (LPARAM)keywords2);
            }
        } else if (language == LANG_PYTHON) {
            if (keywords1) {
                SendMessage(editor, SCI_SETKEYWORDS, 0, (LPARAM)keywords1);
            }
        }

        /* Apply syntax colors even without lexer */
        ApplySyntaxColors(editor, language, isDark);

        /* Force a re-colorization */
        SendMessage(editor, SCI_COLOURISE, 0, -1);
    }
}

/* Apply syntax highlighting based on file path */
void ApplySyntaxHighlightingForFile(HWND editor, const char* filePath)
{
    LanguageType lang = DetectLanguage(filePath);
    ApplySyntaxHighlighting(editor, lang);
}
