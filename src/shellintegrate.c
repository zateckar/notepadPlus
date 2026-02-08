/*
 * Shell integration implementation for Notepad+
 * Handles Windows Explorer context menu integration and file associations
 */

#include "shellintegrate.h"
#include <windows.h>
#include <stdio.h>
#include <shlwapi.h>

/* Supported file extensions for file associations */
static const char* g_fileExtensions[] = {
    ".txt", ".log", ".md",                    /* Text files */
    ".c", ".cpp", ".h", ".hpp",              /* C/C++ */
    ".java",                                  /* Java */
    ".py",                                    /* Python */
    ".js", ".json",                          /* JavaScript */
    ".html", ".htm", ".css", ".xml",         /* Web */
    ".bat", ".sh", ".ps1",                   /* Scripts */
    ".ini", ".cfg",                          /* Config */
    NULL
};

/* Helper function to create registry key and set value */
static BOOL RegCreateKeyAndSetValue(HKEY hRoot, const char* subKey, const char* valueName, const char* value)
{
    HKEY hKey;
    LONG result;
    
    result = RegCreateKeyEx(hRoot, subKey, 0, NULL, REG_OPTION_NON_VOLATILE,
                           KEY_WRITE, NULL, &hKey, NULL);
    
    if (result != ERROR_SUCCESS) {
        return FALSE;
    }
    
    if (valueName == NULL) {
        /* Set default value */
        result = RegSetValueEx(hKey, "", 0, REG_SZ, (const BYTE*)value, (DWORD)(strlen(value) + 1));
    } else {
        result = RegSetValueEx(hKey, valueName, 0, REG_SZ, (const BYTE*)value, (DWORD)(strlen(value) + 1));
    }
    
    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS);
}

/* Helper function to delete registry key recursively */
static BOOL RegDeleteKeyRecursive(HKEY hRoot, const char* subKey)
{
    LONG result = SHDeleteKey(hRoot, subKey);
    return (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
}

/* Helper function to check if registry key exists */
static BOOL RegKeyExists(HKEY hRoot, const char* subKey)
{
    HKEY hKey;
    LONG result = RegOpenKeyEx(hRoot, subKey, 0, KEY_READ, &hKey);
    
    if (result == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return TRUE;
    }
    
    return FALSE;
}

/* Get full path to the current executable */
BOOL GetExecutablePath(char* path, int size)
{
    if (!path || size <= 0) {
        return FALSE;
    }
    
    DWORD len = GetModuleFileName(NULL, path, size);
    if (len == 0 || len >= (DWORD)size) {
        return FALSE;
    }
    
    return TRUE;
}

/* Get directory containing the current executable */
BOOL GetExecutableDirectory(char* path, int size)
{
    if (!GetExecutablePath(path, size)) {
        return FALSE;
    }
    
    char* lastSlash = strrchr(path, '\\');
    if (lastSlash) {
        *lastSlash = '\0';
        return TRUE;
    }
    
    return FALSE;
}

/* Check if the application is running with administrator privileges */
BOOL IsRunningAsAdministrator(void)
{
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    
    if (AllocateAndInitializeSid(&ntAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &adminGroup)) {
        
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    
    return isAdmin;
}

/* Request UAC elevation by restarting the application as administrator */
BOOL RequestAdministratorPrivileges(void)
{
    char exePath[MAX_PATH];
    
    if (!GetModuleFileName(NULL, exePath, MAX_PATH)) {
        return FALSE;
    }
    
    /* Execute with "runas" verb to trigger UAC */
    SHELLEXECUTEINFO sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFO);
    sei.lpVerb = "runas";
    sei.lpFile = exePath;
    sei.lpParameters = "";
    sei.nShow = SW_NORMAL;
    sei.fMask = SEE_MASK_FLAG_NO_UI;
    
    if (!ShellExecuteEx(&sei)) {
        if (GetLastError() == ERROR_CANCELLED) {
            /* User cancelled UAC prompt */
            return FALSE;
        }
        return FALSE;
    }
    
    /* New process started, close current instance */
    PostQuitMessage(0);
    return TRUE;
}

/* Install Windows Explorer context menu entries */
BOOL InstallContextMenu(void)
{
    char exePath[MAX_PATH];
    char commandLine[MAX_PATH + 20];
    char iconPath[MAX_PATH + 10];
    
    if (!GetExecutablePath(exePath, MAX_PATH)) {
        MessageBox(NULL, "Failed to get executable path", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    /* Prepare command line and icon path */
    sprintf(commandLine, "\"%s\" \"%%1\"", exePath);
    sprintf(iconPath, "%s,0", exePath);
    
    /* Install context menu for all files (*) */
    if (!RegCreateKeyAndSetValue(HKEY_CLASSES_ROOT, "*\\shell\\OpenWithNotepadPlus", NULL, "Open with Notepad+")) {
        MessageBox(NULL, "Failed to create context menu entry for files", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    if (!RegCreateKeyAndSetValue(HKEY_CLASSES_ROOT, "*\\shell\\OpenWithNotepadPlus", "Icon", iconPath)) {
        return FALSE;
    }
    
    if (!RegCreateKeyAndSetValue(HKEY_CLASSES_ROOT, "*\\shell\\OpenWithNotepadPlus\\command", NULL, commandLine)) {
        return FALSE;
    }
    
    /* Install context menu for directories */
    if (!RegCreateKeyAndSetValue(HKEY_CLASSES_ROOT, "Directory\\shell\\OpenWithNotepadPlus", NULL, "Open with Notepad+")) {
        MessageBox(NULL, "Failed to create context menu entry for directories", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    if (!RegCreateKeyAndSetValue(HKEY_CLASSES_ROOT, "Directory\\shell\\OpenWithNotepadPlus", "Icon", iconPath)) {
        return FALSE;
    }
    
    if (!RegCreateKeyAndSetValue(HKEY_CLASSES_ROOT, "Directory\\shell\\OpenWithNotepadPlus\\command", NULL, commandLine)) {
        return FALSE;
    }
    
    /* Install context menu for directory background */
    sprintf(commandLine, "\"%s\" \"%%V\"", exePath);
    
    if (!RegCreateKeyAndSetValue(HKEY_CLASSES_ROOT, "Directory\\Background\\shell\\OpenWithNotepadPlus", NULL, "Open Notepad+ here")) {
        MessageBox(NULL, "Failed to create context menu entry for directory background", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    if (!RegCreateKeyAndSetValue(HKEY_CLASSES_ROOT, "Directory\\Background\\shell\\OpenWithNotepadPlus", "Icon", iconPath)) {
        return FALSE;
    }
    
    if (!RegCreateKeyAndSetValue(HKEY_CLASSES_ROOT, "Directory\\Background\\shell\\OpenWithNotepadPlus\\command", NULL, commandLine)) {
        return FALSE;
    }
    
    return TRUE;
}

/* Remove Windows Explorer context menu entries */
BOOL UninstallContextMenu(void)
{
    BOOL success = TRUE;
    
    /* Remove context menu for files */
    if (!RegDeleteKeyRecursive(HKEY_CLASSES_ROOT, "*\\shell\\OpenWithNotepadPlus")) {
        success = FALSE;
    }
    
    /* Remove context menu for directories */
    if (!RegDeleteKeyRecursive(HKEY_CLASSES_ROOT, "Directory\\shell\\OpenWithNotepadPlus")) {
        success = FALSE;
    }
    
    /* Remove context menu for directory background */
    if (!RegDeleteKeyRecursive(HKEY_CLASSES_ROOT, "Directory\\Background\\shell\\OpenWithNotepadPlus")) {
        success = FALSE;
    }
    
    if (!success) {
        MessageBox(NULL, "Some context menu entries could not be removed.\nThey may have already been removed.", 
                  "Warning", MB_ICONWARNING | MB_OK);
    }
    
    return success;
}

/* Check if context menu integration is currently installed */
BOOL IsContextMenuInstalled(void)
{
    return RegKeyExists(HKEY_CLASSES_ROOT, "*\\shell\\OpenWithNotepadPlus");
}

/* Register file type associations with Windows */
BOOL RegisterFileAssociations(void)
{
    char exePath[MAX_PATH];
    char commandLine[MAX_PATH + 20];
    char iconPath[MAX_PATH + 10];
    char exeDir[MAX_PATH];
    
    if (!GetExecutablePath(exePath, MAX_PATH)) {
        MessageBox(NULL, "Failed to get executable path", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    if (!GetExecutableDirectory(exeDir, MAX_PATH)) {
        MessageBox(NULL, "Failed to get executable directory", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    /* Prepare command line and icon path */
    sprintf(commandLine, "\"%s\" \"%%1\"", exePath);
    sprintf(iconPath, "%s,0", exePath);
    
    /* Register application with Windows */
    if (!RegCreateKeyAndSetValue(HKEY_CLASSES_ROOT, "Applications\\notepad+.exe", "FriendlyAppName", "Notepad+")) {
        MessageBox(NULL, "Failed to register application", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    if (!RegCreateKeyAndSetValue(HKEY_CLASSES_ROOT, "Applications\\notepad+.exe\\DefaultIcon", NULL, iconPath)) {
        return FALSE;
    }
    
    if (!RegCreateKeyAndSetValue(HKEY_CLASSES_ROOT, "Applications\\notepad+.exe\\shell\\open\\command", NULL, commandLine)) {
        return FALSE;
    }
    
    /* Add to OpenWithList for all files */
    if (!RegCreateKeyAndSetValue(HKEY_CLASSES_ROOT, "*\\OpenWithList\\notepad+.exe", NULL, "")) {
        MessageBox(NULL, "Failed to add to Open With list", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    /* Register each supported file extension */
    for (int i = 0; g_fileExtensions[i] != NULL; i++) {
        char keyPath[MAX_PATH];
        sprintf(keyPath, "%s\\OpenWithList\\notepad+.exe", g_fileExtensions[i]);
        
        if (!RegCreateKeyAndSetValue(HKEY_CLASSES_ROOT, keyPath, NULL, "")) {
            /* Continue on error - some extensions may already be registered */
        }
    }
    
    /* Register in App Paths for easy command-line access */
    if (!RegCreateKeyAndSetValue(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\notepad+.exe", 
                                 NULL, exePath)) {
        /* Not critical - continue */
    }
    
    if (!RegCreateKeyAndSetValue(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\notepad+.exe", 
                                 "Path", exeDir)) {
        /* Not critical - continue */
    }
    
    /* Store installation information */
    if (!RegCreateKeyAndSetValue(HKEY_LOCAL_MACHINE, "SOFTWARE\\Notepad+", "InstallPath", exePath)) {
        /* Not critical - continue */
    }
    
    if (!RegCreateKeyAndSetValue(HKEY_LOCAL_MACHINE, "SOFTWARE\\Notepad+", "Version", "1.0")) {
        /* Not critical - continue */
    }
    
    return TRUE;
}

/* Remove file type associations from Windows */
BOOL UnregisterFileAssociations(void)
{
    BOOL success = TRUE;
    
    /* Remove application registration */
    if (!RegDeleteKeyRecursive(HKEY_CLASSES_ROOT, "Applications\\notepad+.exe")) {
        success = FALSE;
    }
    
    /* Remove from OpenWithList for all files */
    RegDeleteKeyRecursive(HKEY_CLASSES_ROOT, "*\\OpenWithList\\notepad+.exe");
    
    /* Remove from OpenWithList for each file extension */
    for (int i = 0; g_fileExtensions[i] != NULL; i++) {
        char keyPath[MAX_PATH];
        sprintf(keyPath, "%s\\OpenWithList\\notepad+.exe", g_fileExtensions[i]);
        RegDeleteKeyRecursive(HKEY_CLASSES_ROOT, keyPath);
    }
    
    /* Remove from App Paths */
    RegDeleteKeyRecursive(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\notepad+.exe");
    
    /* Remove installation information */
    RegDeleteKeyRecursive(HKEY_LOCAL_MACHINE, "SOFTWARE\\Notepad+");
    
    if (!success) {
        MessageBox(NULL, "Some file associations could not be removed.\nThey may have already been removed.", 
                  "Warning", MB_ICONWARNING | MB_OK);
    }
    
    return success;
}

/* Check if file associations are currently registered */
BOOL AreFileAssociationsRegistered(void)
{
    return RegKeyExists(HKEY_CLASSES_ROOT, "Applications\\notepad+.exe");
}

/* Install both context menu and file associations */
BOOL InstallShellIntegration(BOOL showProgress)
{
    BOOL contextMenuSuccess = FALSE;
    BOOL fileAssocSuccess = FALSE;
    
    if (showProgress) {
        MessageBox(NULL, "Installing shell integration...\nThis may take a moment.", 
                  "Notepad+ Shell Integration", MB_ICONINFORMATION | MB_OK);
    }
    
    /* Install context menu */
    contextMenuSuccess = InstallContextMenu();
    
    /* Install file associations */
    fileAssocSuccess = RegisterFileAssociations();
    
    if (contextMenuSuccess && fileAssocSuccess) {
        return TRUE;
    } else if (contextMenuSuccess || fileAssocSuccess) {
        MessageBox(NULL, "Shell integration was partially installed.\nSome features may not work correctly.", 
                  "Partial Installation", MB_ICONWARNING | MB_OK);
        return FALSE;
    } else {
        MessageBox(NULL, "Failed to install shell integration.\nPlease ensure you are running as administrator.", 
                  "Installation Failed", MB_ICONERROR | MB_OK);
        return FALSE;
    }
}

/* Uninstall both context menu and file associations */
BOOL UninstallShellIntegration(BOOL showProgress)
{
    BOOL contextMenuSuccess = FALSE;
    BOOL fileAssocSuccess = FALSE;
    
    if (showProgress) {
        MessageBox(NULL, "Uninstalling shell integration...\nThis may take a moment.", 
                  "Notepad+ Shell Integration", MB_ICONINFORMATION | MB_OK);
    }
    
    /* Uninstall context menu */
    contextMenuSuccess = UninstallContextMenu();
    
    /* Uninstall file associations */
    fileAssocSuccess = UnregisterFileAssociations();
    
    if (contextMenuSuccess && fileAssocSuccess) {
        return TRUE;
    } else {
        /* Even if some operations failed, consider it partially successful */
        return TRUE;
    }
}