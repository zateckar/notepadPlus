/*
 * Shell integration for Notepad+
 * Handles Windows Explorer context menu integration and file associations
 */

#ifndef SHELLINTEGRATE_H
#define SHELLINTEGRATE_H

#include <windows.h>

/* Utility Functions */

/*
 * Get full path to the current executable
 * Returns TRUE on success, FALSE on failure
 */
BOOL GetExecutablePath(char* path, int size);

/*
 * Get directory containing the current executable
 * Returns TRUE on success, FALSE on failure
 */
BOOL GetExecutableDirectory(char* path, int size);

/*
 * Check if the application is running with administrator privileges
 * Returns TRUE if running as admin, FALSE otherwise
 */
BOOL IsRunningAsAdministrator(void);

/*
 * Request UAC elevation by restarting the application as administrator
 * Returns TRUE if restart initiated, FALSE on failure or user cancellation
 */
BOOL RequestAdministratorPrivileges(void);

/* Shell Integration Functions */

/*
 * Install Windows Explorer context menu entries
 * Returns TRUE on success, FALSE on failure
 */
BOOL InstallContextMenu(void);

/*
 * Remove Windows Explorer context menu entries
 * Returns TRUE on success, FALSE on failure
 */
BOOL UninstallContextMenu(void);

/*
 * Check if context menu integration is currently installed
 * Returns TRUE if installed, FALSE otherwise
 */
BOOL IsContextMenuInstalled(void);

/* File Association Functions */

/*
 * Register file type associations with Windows
 * Returns TRUE on success, FALSE on failure
 */
BOOL RegisterFileAssociations(void);

/*
 * Remove file type associations from Windows
 * Returns TRUE on success, FALSE on failure
 */
BOOL UnregisterFileAssociations(void);

/*
 * Check if file associations are currently registered
 * Returns TRUE if registered, FALSE otherwise
 */
BOOL AreFileAssociationsRegistered(void);

/* Combined Operations */

/*
 * Install both context menu and file associations
 * showProgress: If TRUE, displays progress messages
 * Returns TRUE on success, FALSE on failure
 */
BOOL InstallShellIntegration(BOOL showProgress);

/*
 * Uninstall both context menu and file associations
 * showProgress: If TRUE, displays progress messages
 * Returns TRUE on success, FALSE on failure
 */
BOOL UninstallShellIntegration(BOOL showProgress);

#endif /* SHELLINTEGRATE_H */