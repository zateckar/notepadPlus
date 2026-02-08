# Notepad+ - A Lightweight Text Editor

A fast, lightweight text editor written in C using Win32 API and Scintilla, focused on achieving the fastest possible startup time while maintaining comprehensive features.

## Features

- **Core Editing**: Full text editing with Scintilla component
- **File Operations**: New, Open, Save, Save As functionality
- **Menu System**: Complete menu structure with keyboard shortcuts
- **UTF-8 Support**: Full Unicode text support
- **Line Numbers**: Optional line numbering
- **Syntax Highlighting**: Automatic detection and highlighting for:
  - C/C++, Python, JavaScript, HTML, CSS, XML, JSON, SQL, Batch
- **Theme System**: Dark and Light themes (editor-only dark mode)
- **Tab System**: Multi-document interface with close buttons
- **Configuration System**: User settings stored in Windows Registry
- **Session Management**: Restore previous session on startup
- **Word Highlighting**: Double-click to highlight all occurrences
- **HiDPI Support**: Per-monitor DPI awareness for sharp text
- **Fast Startup**: Optimized for quick launch (<0ms target)
- **Application Icon**: Custom blue "N+" icon for executable, taskbar, and file associations
- **Shell Integration**: Context menu integration ("Open with Notepad+")
- **File Associations**: Register as default editor for text and programming files

## Download & Installation

### Download Latest Release

Download the latest release from the [GitHub Releases](../../releases) page.

### Installation Methods

#### Option 1: Portable (No Installation Required)
1. Download `NotepadPlus-Windows.zip` from the latest release
2. Extract to any folder
3. Run `notepad+.exe` directly

#### Option 2: Full Installation (With Shell Integration)
1. Download `NotepadPlus-Windows.zip` from the latest release
2. Extract to your desired installation folder (e.g., `C:\Program Files\Notepad+`)
3. Run `notepad+.exe` and use the Options menu to install shell integration:
   - **Options → Install Shell Integration** - Adds "Open with Notepad+" to context menus
   - **Options → Register File Associations** - Associates file types with Notepad+

### Uninstall

To remove shell integration and file associations:
1. Open Notepad+
2. Use the Options menu:
   - **Options → Uninstall Shell Integration**
   - **Options → Unregister File Associations**

Then simply delete the application folder.

## Building from Source

### Prerequisites

- Windows 7 SP1 or later
- MinGW-w64 GCC compiler
- Python 3.x (for lexer mapping generation)

### Build Instructions

1. Clone the repository:
```bash
git clone https://github.com/yourusername/notepad+.git
cd notepad+
```

2. Open a terminal in the project directory

3. Run the build script:
```cmd
.\build.bat
```

4. The executable will be created in `bin\notepad+.exe`

### Build System Details

The build system:
- Statically links Scintilla and Lexilla libraries (no DLL dependencies)
- Generates lexer mappings from GitHub Linguist data
- Uses incremental builds (rebuilds only changed components)
- Creates optimized release builds with `-O3 -flto` flags

## Configuration

Notepad+ stores all settings in the Windows Registry at:
```
HKEY_CURRENT_USER\Software\Notepad+
```

**20 User Settings** organized in 6 categories:
- **View** (4 settings): ShowStatusBar, ShowLineNumbers, WordWrap, ZoomLevel
- **Editor** (6 settings): TabWidth, UseSpaces, ShowWhitespace, AutoIndent, CaretWidth, CaretLineVisible
- **Theme** (1 setting): Theme
- **Find** (3 settings): MatchCase, WholeWord, UseRegex
- **Session** (2 settings): RestoreSession, SaveOnExit
- **Behavior** (4 settings): SingleInstance, ConfirmExit, AutoSaveInterval, BackupOnSave

All settings can be managed through the **Preferences Dialog** (Edit → Preferences) without manual registry editing.

## Project Structure

```
notepad+/
├── .github/workflows/           # GitHub Actions CI/CD
├── src/                         # Source code
│   ├── main.c                   # Application entry point
│   ├── window.c/.h              # Window management
│   ├── editor.c/.h              # Scintilla editor wrapper
│   ├── tabs.c/.h                # Tab system
│   ├── toolbar.c/.h             # Toolbar management
│   ├── statusbar.c/.h           # Status bar
│   ├── menu.c/.h                # Menu system
│   ├── themes.c/.h              # Theme system
│   ├── session.c/.h             # Session management
│   ├── findreplace.c/.h         # Find/Replace functionality
│   ├── syntax.c/.h              # Syntax highlighting
│   ├── config.c/.h              # Configuration system
│   └── ...
├── resources/                   # Resources (icons, manifests)
├── scintilla/                   # Scintilla source (statically linked)
├── lexilla/                     # Lexilla source (statically linked)
├── tools/                       # Build tools
├── bin/                         # Output directory
├── obj/                         # Object files
├── build.bat                    # Build script
└── README.md                    # This file
```

## Keyboard Shortcuts

### File Operations
- `Ctrl+N` - New file
- `Ctrl+O` - Open file
- `Ctrl+S` - Save file
- `Ctrl+Shift+S` - Save As
- `Ctrl+Q` - Exit

### Edit Operations
- `Ctrl+Z` - Undo
- `Ctrl+Y` - Redo
- `Ctrl+X` - Cut
- `Ctrl+C` - Copy
- `Ctrl+V` - Paste
- `Ctrl+A` - Select All
- `Ctrl+F` - Find
- `Ctrl+H` - Replace
- `Ctrl+G` - Go to line

### Tab Operations
- `Ctrl+W` - Close current tab
- `Ctrl+Tab` - Next tab
- `Ctrl+Shift+Tab` - Previous tab

### View Operations
- `Ctrl++` - Zoom in
- `Ctrl+-` - Zoom out
- `Ctrl+0` - Reset zoom
- `F11` - Fullscreen toggle

## Supported File Types

When installed, Notepad+ registers associations for:

- **Text**: `.txt`, `.log`, `.md`
- **C/C++**: `.c`, `.cpp`, `.h`, `.hpp`
- **Web**: `.html`, `.htm`, `.css`, `.js`, `.json`, `.xml`
- **Scripts**: `.bat`, `.sh`, `.ps1`, `.py`, `.java`
- **Config**: `.ini`, `.cfg`, `.yml`, `.yaml`

## Technical Details

### Compiler Flags
- **Release**: `-O3 -march=native -mtune=native -flto -Wall -Wextra`
- All optimization flags are in `build.bat`

### Dependencies
- **Scintilla**: Text editing component (statically linked)
- **Lexilla**: Lexer library (statically linked)
- **Win32 API**: Windowing system
- **Common Controls**: UI elements

### Performance Targets
- **Startup Time**: <40ms on typical hardware
- **Memory Usage**: <50MB for normal usage
- **File Size**: Standalone executable with no DLL dependencies

## CI/CD

This project uses GitHub Actions for continuous integration and deployment:

- **Build**: Triggered on every push and pull request
- **Release**: Automatic release creation when pushing version tags (`v*`)
- **Code Quality**: Automated cppcheck static analysis

### Creating a Release

To create a new release:

1. Update version information if needed
2. Create and push a tag:
```bash
git tag -a v1.0.0 -m "Release version 1.0.0"
git push origin v1.0.0
```
3. GitHub Actions will automatically build and create a release

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- [Scintilla](https://www.scintilla.org/) - Text editing component
- [Lexilla](https://www.scintilla.org/Lexilla.html) - Syntax highlighting lexers
- [GitHub Linguist](https://github.com/github/linguist) - Language detection heuristics