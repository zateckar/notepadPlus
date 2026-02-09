@echo off
setlocal EnableDelayedExpansion
REM Build script for Notepad+ on Windows
REM Statically links Scintilla and Lexilla libraries

echo Building Notepad+ ...

REM Create directories if they don't exist
if not exist obj mkdir obj
if not exist obj\scintilla mkdir obj\scintilla
if not exist obj\lexilla mkdir obj\lexilla
if not exist bin mkdir bin

REM Generate lexer mappings from GitHub Linguist data
REM Skip if running in CI (already generated in workflow) or if files already exist
echo Generating lexer mappings...
if exist src\lexer_mappings_generated.c (
    echo Lexer mappings already exist, skipping generation...
) else (
    python tools/generate_lexer_code.py
    if errorlevel 1 echo Error generating lexer mappings && exit /b 1
)

REM Compiler optimization flags:
REM   -O3: Maximum optimization for speed
REM   -flto: Link-time optimization for better cross-module inlining
REM   -s: Strip symbols for smaller binary
REM   -fomit-frame-pointer: Free up a register for better performance
REM   -ffast-math: Faster floating-point operations (safe for this app)
set CFLAGS=-O3 -flto -s -fomit-frame-pointer -Wall -Wextra -std=c99
set CXXFLAGS=-O3 -flto -s -fomit-frame-pointer -Wall -Wextra -std=c++17
set LDFLAGS=-flto -s

REM Read current source versions
set /p SCI_VERSION=<scintilla\version.txt
set /p LEX_VERSION=<lexilla\version.txt

REM Check if Scintilla needs rebuilding
echo Checking Scintilla static library...
set BUILD_SCINTILLA=0
if not exist obj\scintilla\libscintilla.a set BUILD_SCINTILLA=1
if exist obj\scintilla\version.txt (
    set /p BUILT_SCI_VERSION=<obj\scintilla\version.txt
    if not "!BUILT_SCI_VERSION!"=="!SCI_VERSION!" set BUILD_SCINTILLA=1
) else (
    set BUILD_SCINTILLA=1
)

if !BUILD_SCINTILLA! equ 1 (
    echo Building Scintilla static library ^(version !SCI_VERSION!^)...
    call :BuildScintilla
    if errorlevel 1 exit /b 1
    echo !SCI_VERSION!> obj\scintilla\version.txt
) else (
    echo Using existing Scintilla static library
)

REM Check if Lexilla needs rebuilding
echo Checking Lexilla static library...
set BUILD_LEXILLA=0
if not exist obj\lexilla\liblexilla.a set BUILD_LEXILLA=1
if exist obj\lexilla\version.txt (
    set /p BUILT_LEX_VERSION=<obj\lexilla\version.txt
    if not "!BUILT_LEX_VERSION!"=="!LEX_VERSION!" set BUILD_LEXILLA=1
) else (
    set BUILD_LEXILLA=1
)

if !BUILD_LEXILLA! equ 1 (
    echo Building Lexilla static library ^(version !LEX_VERSION!^)...
    call :BuildLexilla
    if errorlevel 1 exit /b 1
    echo !LEX_VERSION!> obj\lexilla\version.txt
) else (
    echo Using existing Lexilla static library
)

REM Compile resource file
echo Compiling resources...
windres resources/resources.rc -o obj/resources.o -I src
if errorlevel 1 echo Error compiling resources && exit /b 1

REM Compile Notepad+ source files
echo Compiling Notepad+ source files...

gcc -c src/main.c -o obj/main.o -Isrc %CFLAGS%
if errorlevel 1 echo Error compiling main.c && exit /b 1

gcc -c src/window.c -o obj/window.o -Isrc %CFLAGS%
if errorlevel 1 echo Error compiling window.c && exit /b 1

gcc -c src/editor.c -o obj/editor.o -Isrc %CFLAGS%
if errorlevel 1 echo Error compiling editor.c && exit /b 1

gcc -c src/resource.c -o obj/resource.o -Isrc %CFLAGS%
if errorlevel 1 echo Error compiling resource.c && exit /b 1

gcc -c src/tabs.c -o obj/tabs.o -Isrc %CFLAGS%
if errorlevel 1 echo Error compiling tabs.c && exit /b 1

gcc -c src/toolbar.c -o obj/toolbar.o -Isrc %CFLAGS%
if errorlevel 1 echo Error compiling toolbar.c && exit /b 1

gcc -c src/statusbar.c -o obj/statusbar.o -Isrc %CFLAGS%
if errorlevel 1 echo Error compiling statusbar.c && exit /b 1

gcc -c src/findreplace.c -o obj/findreplace.o -Isrc %CFLAGS%
if errorlevel 1 echo Error compiling findreplace.c && exit /b 1

gcc -c src/themes.c -o obj/themes.o -Isrc %CFLAGS%
if errorlevel 1 echo Error compiling themes.c && exit /b 1

gcc -c src/lexer_mappings_generated.c -o obj/lexer_mappings_generated.o -Isrc %CFLAGS%
if errorlevel 1 echo Error compiling lexer_mappings_generated.c && exit /b 1

gcc -c src/syntax.c -o obj/syntax.o -Isrc %CFLAGS%
if errorlevel 1 echo Error compiling syntax.c && exit /b 1

gcc -c src/config.c -o obj/config.o -Isrc %CFLAGS%
if errorlevel 1 echo Error compiling config.c && exit /b 1

gcc -c src/registry_config.c -o obj/registry_config.o -Isrc %CFLAGS%
if errorlevel 1 echo Error compiling registry_config.c && exit /b 1

gcc -c src/session.c -o obj/session.o -Isrc %CFLAGS%
if errorlevel 1 echo Error compiling session.c && exit /b 1

gcc -c src/fileops.c -o obj/fileops.o -Isrc %CFLAGS%
if errorlevel 1 echo Error compiling fileops.c && exit /b 1

gcc -c src/splitview.c -o obj/splitview.o -Isrc %CFLAGS%
if errorlevel 1 echo Error compiling splitview.c && exit /b 1

gcc -c src/gotoline.c -o obj/gotoline.o -Isrc %CFLAGS%
if errorlevel 1 echo Error compiling gotoline.c && exit /b 1

gcc -c src/preferences.c -o obj/preferences.o -Isrc %CFLAGS%
if errorlevel 1 echo Error compiling preferences.c && exit /b 1

gcc -c src/shellintegrate.c -o obj/shellintegrate.o -Isrc %CFLAGS%
if errorlevel 1 echo Error compiling shellintegrate.c && exit /b 1

REM Link the executable with static libraries
echo Linking executable with static libraries...
gcc %LDFLAGS% obj/main.o obj/window.o obj/editor.o obj/resource.o obj/tabs.o obj/toolbar.o obj/statusbar.o obj/findreplace.o obj/themes.o obj/lexer_mappings_generated.o obj/syntax.o obj/config.o obj/registry_config.o obj/session.o obj/fileops.o obj/splitview.o obj/gotoline.o obj/preferences.o obj/shellintegrate.o obj/resources.o obj/scintilla/libscintilla.a obj/lexilla/liblexilla.a -o bin/notepad+.exe -mwindows -lcomctl32 -lgdi32 -luser32 -lkernel32 -lshell32 -lcomdlg32 -ldwmapi -ladvapi32 -lshlwapi -lstdc++ -lole32 -luuid -loleaut32 -limm32
if errorlevel 1 echo Error linking executable && exit /b 1

echo Build completed successfully!
echo Executable: bin\notepad+.exe
echo.
echo Note: Scintilla and Lexilla are now statically linked.
echo No DLL files needed!
exit /b 0

REM ============================================================================
REM Build Scintilla static library
REM ============================================================================
:BuildScintilla
echo Compiling Scintilla source files...

set SCI_CFLAGS=-O3 -DNDEBUG -std=c++17 -Iscintilla/include -Iscintilla/src -Iscintilla/win32 -Wall -Wextra
set SCI_SRC=scintilla/src
set WIN32_SRC=scintilla/win32

REM Core Scintilla source files
g++ -c %SCI_SRC%/AutoComplete.cxx -o obj/scintilla/AutoComplete.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/CallTip.cxx -o obj/scintilla/CallTip.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/CaseConvert.cxx -o obj/scintilla/CaseConvert.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/CaseFolder.cxx -o obj/scintilla/CaseFolder.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/CellBuffer.cxx -o obj/scintilla/CellBuffer.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/ChangeHistory.cxx -o obj/scintilla/ChangeHistory.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/CharacterCategoryMap.cxx -o obj/scintilla/CharacterCategoryMap.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/CharacterType.cxx -o obj/scintilla/CharacterType.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/CharClassify.cxx -o obj/scintilla/CharClassify.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/ContractionState.cxx -o obj/scintilla/ContractionState.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/DBCS.cxx -o obj/scintilla/DBCS.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/Decoration.cxx -o obj/scintilla/Decoration.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/Document.cxx -o obj/scintilla/Document.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/EditModel.cxx -o obj/scintilla/EditModel.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/Editor.cxx -o obj/scintilla/Editor.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/EditView.cxx -o obj/scintilla/EditView.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/Geometry.cxx -o obj/scintilla/Geometry.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/Indicator.cxx -o obj/scintilla/Indicator.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/KeyMap.cxx -o obj/scintilla/KeyMap.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/LineMarker.cxx -o obj/scintilla/LineMarker.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/MarginView.cxx -o obj/scintilla/MarginView.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/PerLine.cxx -o obj/scintilla/PerLine.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/PositionCache.cxx -o obj/scintilla/PositionCache.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/RESearch.cxx -o obj/scintilla/RESearch.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/RunStyles.cxx -o obj/scintilla/RunStyles.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/Selection.cxx -o obj/scintilla/Selection.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/Style.cxx -o obj/scintilla/Style.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/UndoHistory.cxx -o obj/scintilla/UndoHistory.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/UniConversion.cxx -o obj/scintilla/UniConversion.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/UniqueString.cxx -o obj/scintilla/UniqueString.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/ViewStyle.cxx -o obj/scintilla/ViewStyle.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/XPM.cxx -o obj/scintilla/XPM.o %SCI_CFLAGS% || exit /b 1
g++ -c %SCI_SRC%/ScintillaBase.cxx -o obj/scintilla/ScintillaBase.o %SCI_CFLAGS% || exit /b 1

REM Windows-specific files
g++ -c %WIN32_SRC%/HanjaDic.cxx -o obj/scintilla/HanjaDic.o %SCI_CFLAGS% || exit /b 1
g++ -c %WIN32_SRC%/PlatWin.cxx -o obj/scintilla/PlatWin.o %SCI_CFLAGS% || exit /b 1
g++ -c %WIN32_SRC%/ListBox.cxx -o obj/scintilla/ListBox.o %SCI_CFLAGS% || exit /b 1
g++ -c %WIN32_SRC%/SurfaceGDI.cxx -o obj/scintilla/SurfaceGDI.o %SCI_CFLAGS% || exit /b 1
g++ -c %WIN32_SRC%/SurfaceD2D.cxx -o obj/scintilla/SurfaceD2D.o %SCI_CFLAGS% || exit /b 1
g++ -c %WIN32_SRC%/ScintillaWin.cxx -o obj/scintilla/ScintillaWin.o %SCI_CFLAGS% || exit /b 1

echo Creating Scintilla static library...
ar rcs obj/scintilla/libscintilla.a obj/scintilla/AutoComplete.o obj/scintilla/CallTip.o obj/scintilla/CaseConvert.o obj/scintilla/CaseFolder.o obj/scintilla/CellBuffer.o obj/scintilla/ChangeHistory.o obj/scintilla/CharacterCategoryMap.o obj/scintilla/CharacterType.o obj/scintilla/CharClassify.o obj/scintilla/ContractionState.o obj/scintilla/DBCS.o obj/scintilla/Decoration.o obj/scintilla/Document.o obj/scintilla/EditModel.o obj/scintilla/Editor.o obj/scintilla/EditView.o obj/scintilla/Geometry.o obj/scintilla/Indicator.o obj/scintilla/KeyMap.o obj/scintilla/LineMarker.o obj/scintilla/MarginView.o obj/scintilla/PerLine.o obj/scintilla/PositionCache.o obj/scintilla/RESearch.o obj/scintilla/RunStyles.o obj/scintilla/Selection.o obj/scintilla/Style.o obj/scintilla/UndoHistory.o obj/scintilla/UniConversion.o obj/scintilla/UniqueString.o obj/scintilla/ViewStyle.o obj/scintilla/XPM.o obj/scintilla/ScintillaBase.o obj/scintilla/HanjaDic.o obj/scintilla/PlatWin.o obj/scintilla/ListBox.o obj/scintilla/SurfaceGDI.o obj/scintilla/SurfaceD2D.o obj/scintilla/ScintillaWin.o
echo Scintilla static library built successfully
exit /b 0

REM ============================================================================
REM Build Lexilla static library
REM ============================================================================
:BuildLexilla
echo Compiling Lexilla source files...

set LEX_CFLAGS=-O3 -DNDEBUG -std=c++17 -Ilexilla/include -Iscintilla/include -Ilexilla/lexlib -Wall -Wextra
set LEX_SRC=lexilla/src
set LEXLIB_SRC=lexilla/lexlib
set LEXERS_SRC=lexilla/lexers

REM Initialize object file list
set LEX_OBJS=

REM Lexilla core
g++ -c %LEX_SRC%/Lexilla.cxx -o obj/lexilla/Lexilla.o %LEX_CFLAGS% || exit /b 1
set LEX_OBJS=!LEX_OBJS! obj/lexilla/Lexilla.o

REM Lexlib files
g++ -c %LEXLIB_SRC%/Accessor.cxx -o obj/lexilla/Accessor.o %LEX_CFLAGS% || exit /b 1
set LEX_OBJS=!LEX_OBJS! obj/lexilla/Accessor.o
g++ -c %LEXLIB_SRC%/CharacterCategory.cxx -o obj/lexilla/CharacterCategory.o %LEX_CFLAGS% || exit /b 1
set LEX_OBJS=!LEX_OBJS! obj/lexilla/CharacterCategory.o
g++ -c %LEXLIB_SRC%/CharacterSet.cxx -o obj/lexilla/CharacterSet.o %LEX_CFLAGS% || exit /b 1
set LEX_OBJS=!LEX_OBJS! obj/lexilla/CharacterSet.o
g++ -c %LEXLIB_SRC%/DefaultLexer.cxx -o obj/lexilla/DefaultLexer.o %LEX_CFLAGS% || exit /b 1
set LEX_OBJS=!LEX_OBJS! obj/lexilla/DefaultLexer.o
g++ -c %LEXLIB_SRC%/InList.cxx -o obj/lexilla/InList.o %LEX_CFLAGS% || exit /b 1
set LEX_OBJS=!LEX_OBJS! obj/lexilla/InList.o
g++ -c %LEXLIB_SRC%/LexAccessor.cxx -o obj/lexilla/LexAccessor.o %LEX_CFLAGS% || exit /b 1
set LEX_OBJS=!LEX_OBJS! obj/lexilla/LexAccessor.o
g++ -c %LEXLIB_SRC%/LexerBase.cxx -o obj/lexilla/LexerBase.o %LEX_CFLAGS% || exit /b 1
set LEX_OBJS=!LEX_OBJS! obj/lexilla/LexerBase.o
g++ -c %LEXLIB_SRC%/LexerModule.cxx -o obj/lexilla/LexerModule.o %LEX_CFLAGS% || exit /b 1
set LEX_OBJS=!LEX_OBJS! obj/lexilla/LexerModule.o
g++ -c %LEXLIB_SRC%/LexerSimple.cxx -o obj/lexilla/LexerSimple.o %LEX_CFLAGS% || exit /b 1
set LEX_OBJS=!LEX_OBJS! obj/lexilla/LexerSimple.o
g++ -c %LEXLIB_SRC%/PropSetSimple.cxx -o obj/lexilla/PropSetSimple.o %LEX_CFLAGS% || exit /b 1
set LEX_OBJS=!LEX_OBJS! obj/lexilla/PropSetSimple.o
g++ -c %LEXLIB_SRC%/StyleContext.cxx -o obj/lexilla/StyleContext.o %LEX_CFLAGS% || exit /b 1
set LEX_OBJS=!LEX_OBJS! obj/lexilla/StyleContext.o
g++ -c %LEXLIB_SRC%/WordList.cxx -o obj/lexilla/WordList.o %LEX_CFLAGS% || exit /b 1
set LEX_OBJS=!LEX_OBJS! obj/lexilla/WordList.o

REM Compile ALL lexer files (Lexilla.cxx references them all in its static table)
for %%f in (%LEXERS_SRC%\Lex*.cxx) do (
    g++ -c %%f -o obj/lexilla/%%~nf.o %LEX_CFLAGS% || exit /b 1
    set LEX_OBJS=!LEX_OBJS! obj/lexilla/%%~nf.o
)

echo Creating Lexilla static library...
ar rcs obj/lexilla/liblexilla.a !LEX_OBJS!
echo Lexilla static library built successfully
exit /b 0
