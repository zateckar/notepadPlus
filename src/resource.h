/*
 * Resource identifiers for Notepad+
 */

#ifndef RESOURCE_H
#define RESOURCE_H

/* Window class name */
#define WINDOW_CLASS_NAME "NotepadPlusWindowClass"

/* Menu resource IDs */
#define IDR_MAINMENU             100
#define IDR_ACCELERATORS         110
#define IDR_TOOLBAR              120

/* File menu */
#define ID_FILE_NEW              101
#define ID_FILE_OPEN             102
#define ID_FILE_SAVE             103
#define ID_FILE_SAVEAS           104
#define ID_FILE_SAVEALL          105
#define ID_FILE_EXIT             106
#define ID_FILE_RECENTFILES      107
#define ID_FILE_RECENT_BASE      150  /* IDs 150-159 for recent files */
#define ID_FILE_CLEARRECENT      160
#define ID_FILE_OPENFOLDER       161  /* Open containing folder */

/* Edit menu */
#define ID_EDIT_UNDO             201
#define ID_EDIT_REDO             202
#define ID_EDIT_CUT              203
#define ID_EDIT_COPY             204
#define ID_EDIT_PASTE            205
#define ID_EDIT_SELECTALL        206
#define ID_EDIT_FIND             207
#define ID_EDIT_REPLACE          208
#define ID_EDIT_FINDNEXT         209
#define ID_EDIT_FINDPREV         210
#define ID_EDIT_GOTOLINE         211

/* Common line operations */
#define ID_EDIT_DUPLICATE_LINE   220
#define ID_EDIT_DELETE_LINE      221
#define ID_EDIT_MOVE_LINE_UP     222
#define ID_EDIT_MOVE_LINE_DOWN   223
#define ID_EDIT_JOIN_LINES       224
#define ID_EDIT_SPLIT_LINES      225
#define ID_EDIT_TRIM_TRAILING    226
#define ID_EDIT_TRIM_LEADING     227
#define ID_EDIT_TRIM_BOTH        228

/* EOL conversion (already have IDs 460-462, but add menu item) */
#define ID_EDIT_EOL_CONVERSION   230

/* Encoding/Decoding */
#define ID_EDIT_BASE64_ENCODE    240
#define ID_EDIT_BASE64_DECODE    241
#define ID_EDIT_URL_ENCODE       242
#define ID_EDIT_URL_DECODE       243

/* Case conversion */
#define ID_EDIT_UPPERCASE        250
#define ID_EDIT_LOWERCASE        251
#define ID_EDIT_TITLECASE        252
#define ID_EDIT_SENTENCECASE     253
#define ID_EDIT_INVERTCASE       254

/* View menu */
#define ID_VIEW_TOOLBAR          301
#define ID_VIEW_STATUSBAR        302
#define ID_VIEW_LINE_NUMBERS     303
#define ID_VIEW_WORD_WRAP        304
#define ID_VIEW_ZOOMIN           305
#define ID_VIEW_ZOOMOUT          306
#define ID_VIEW_ZOOMRESET        307
#define ID_VIEW_MINIMAP          308
#define ID_VIEW_SPLITVIEW        309
#define ID_VIEW_CODEFOLDING      310
#define ID_VIEW_WHITESPACE       311
#define ID_VIEW_SPLITVIEW_LOADLEFT  312
#define ID_VIEW_SPLITVIEW_LOADRIGHT 313
#define ID_VIEW_CHANGEHISTORY       314

/* Options menu */
#define ID_OPTIONS_PREFERENCES   401
#define ID_OPTIONS_THEME_DARK    402
#define ID_OPTIONS_THEME_LIGHT   403
#define ID_OPTIONS_AUTOSAVE      404
#define ID_OPTIONS_AUTOINDENT    405
#define ID_OPTIONS_BRACKETMATCH  406
#define ID_OPTIONS_RESTORESESSION 407
#define ID_OPTIONS_INSTALL_SHELL       408
#define ID_OPTIONS_UNINSTALL_SHELL     409
#define ID_OPTIONS_REGISTER_FILES      410
#define ID_OPTIONS_UNREGISTER_FILES    411

/* Encoding menu */
#define ID_ENCODING_UTF8         450
#define ID_ENCODING_UTF8BOM      451
#define ID_ENCODING_UTF16LE      452
#define ID_ENCODING_UTF16BE      453
#define ID_ENCODING_ANSI         454

/* Line ending menu */
#define ID_LINEEND_CRLF          460
#define ID_LINEEND_LF            461
#define ID_LINEEND_CR            462

/* Help menu */
#define ID_HELP_ABOUT            501

/* Tab operations */
#define ID_TAB_CLOSE             601
#define ID_TAB_NEXT              602
#define ID_TAB_PREV              603
#define ID_TAB_CLOSEALL          604
#define ID_TAB_CLOSEOTHERS       605
#define ID_TAB_PIN               606
#define ID_TAB_UNPIN             607

/* Icon resource IDs */
#define IDI_APPICON              600
#define IDI_APPICONSMALL         601

/* Embedded DLL resource IDs */
#define IDR_SCINTILLA_DLL        700
#define IDR_LEXILLA_DLL          701

/* Toolbar button IDs (reuse menu IDs for simplicity) */
#define ID_TOOLBAR_NEW           ID_FILE_NEW
#define ID_TOOLBAR_OPEN          ID_FILE_OPEN
#define ID_TOOLBAR_SAVE          ID_FILE_SAVE
#define ID_TOOLBAR_CUT           ID_EDIT_CUT
#define ID_TOOLBAR_COPY          ID_EDIT_COPY
#define ID_TOOLBAR_PASTE         ID_EDIT_PASTE
#define ID_TOOLBAR_FIND          ID_EDIT_FIND
#define ID_TOOLBAR_REPLACE       ID_EDIT_REPLACE

/* Toolbar menu dropdown button IDs */
#define ID_TOOLBAR_MENU_FILE     801
#define ID_TOOLBAR_MENU_EDIT     802
#define ID_TOOLBAR_MENU_VIEW     803
#define ID_TOOLBAR_MENU_OPTIONS  804

/* String resources */
#define IDS_APP_TITLE            700
#define IDS_FILE_FILTER          701
#define IDS_ALL_FILES_FILTER     702

/* Status bar pane IDs */
#define SB_PANE_CURSOR           0
#define SB_PANE_ENCODING         1
#define SB_PANE_FILETYPE         2
#define SB_PANE_POSITION         3

/* Find/Replace dialog control IDs */
#define IDC_FIND_LABEL           1000
#define IDC_REGEX                1006
#define IDC_REPLACE              1011
#define IDC_REPLACE_ALL          1012
/* Other find/replace IDs are defined in findreplace.h with proper guards */

/* Go to line dialog control IDs */
#define IDC_GOTOLINE_LABEL       1100
#define IDC_GOTOLINE_EDIT        1101
#define IDC_GOTOLINE_OK          1102
#define IDC_GOTOLINE_CANCEL      1103
#define IDC_GOTOLINE_INFO        1104

/* Preferences dialog control IDs */
#define IDC_PREFS_TAB_CONTROL        1200
#define IDC_PREFS_OK                 1201
#define IDC_PREFS_CANCEL             1202
#define IDC_PREFS_APPLY              1203

/* General Tab controls (1210-1229) */
#define IDC_PREFS_GEN_GROUP          1210
#define IDC_PREFS_SINGLE_INSTANCE    1211
#define IDC_PREFS_CONFIRM_EXIT       1212
#define IDC_PREFS_RESTORE_SESSION    1213
#define IDC_PREFS_SAVE_ON_EXIT       1214
#define IDC_PREFS_BACKUP_ON_SAVE     1215
#define IDC_PREFS_AUTOSAVE_LABEL     1216
#define IDC_PREFS_AUTOSAVE_SPIN      1217
#define IDC_PREFS_AUTOSAVE_EDIT      1218

/* Editor Tab controls (1230-1259) */
#define IDC_PREFS_EDIT_GROUP         1230
#define IDC_PREFS_TAB_WIDTH_LABEL    1231
#define IDC_PREFS_TAB_WIDTH_SPIN     1232
#define IDC_PREFS_TAB_WIDTH_EDIT     1233
#define IDC_PREFS_USE_SPACES         1234
#define IDC_PREFS_AUTO_INDENT        1235
#define IDC_PREFS_SHOW_WHITESPACE    1236
#define IDC_PREFS_CARET_WIDTH_LABEL  1237
#define IDC_PREFS_CARET_WIDTH_SPIN   1238
#define IDC_PREFS_CARET_WIDTH_EDIT   1239
#define IDC_PREFS_HIGHLIGHT_LINE     1240

/* View Tab controls (1260-1279) */
#define IDC_PREFS_VIEW_GROUP         1260
#define IDC_PREFS_SHOW_STATUSBAR     1261
#define IDC_PREFS_SHOW_LINE_NUMBERS  1262
#define IDC_PREFS_WORD_WRAP          1263
#define IDC_PREFS_ZOOM_LABEL         1264
#define IDC_PREFS_ZOOM_SLIDER        1265
#define IDC_PREFS_ZOOM_VALUE         1266

/* Find/Replace Tab controls (1280-1299) */
#define IDC_PREFS_FIND_GROUP         1280
#define IDC_PREFS_MATCH_CASE         1281
#define IDC_PREFS_WHOLE_WORD         1282
#define IDC_PREFS_USE_REGEX          1283

/* Advanced Tab controls (1300-1319) */
#define IDC_PREFS_ADV_GROUP          1300
#define IDC_PREFS_THEME_LABEL        1301
#define IDC_PREFS_THEME_COMBO        1302
#define IDC_PREFS_SHELL_GROUP        1303
#define IDC_PREFS_SHELL_CTX_LABEL    1304
#define IDC_PREFS_SHELL_CTX_STATUS   1305
#define IDC_PREFS_SHELL_CTX_BUTTON   1306
#define IDC_PREFS_SHELL_FILE_LABEL   1307
#define IDC_PREFS_SHELL_FILE_STATUS  1308
#define IDC_PREFS_SHELL_FILE_BUTTON  1309
#define IDC_PREFS_SHELL_ADMIN_MSG    1310

/* New Tab Defaults controls (1320-1329) */
#define IDC_PREFS_DEFAULT_CODE_FOLDING    1320
#define IDC_PREFS_DEFAULT_BRACKET_MATCH   1321
#define IDC_PREFS_DEFAULT_CHANGE_HISTORY  1322

/* Font controls (1330-1339) */
#define IDC_PREFS_FONT_LABEL      1330
#define IDC_PREFS_FONT_EDIT       1331
#define IDC_PREFS_FONT_BUTTON     1332
#define IDC_PREFS_FONTSIZE_LABEL  1333
#define IDC_PREFS_FONTSIZE_EDIT   1334

/* Timer IDs */
#define IDT_AUTOSAVE             2001

#endif /* RESOURCE_H */
