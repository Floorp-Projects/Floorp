/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

DIR_ATOM(sCurrentProcess, NS_XPCOM_CURRENT_PROCESS_DIR)
DIR_ATOM(sGRE_Directory, NS_GRE_DIR)
DIR_ATOM(sGRE_BinDirectory, NS_GRE_BIN_DIR)
DIR_ATOM(sOS_TemporaryDirectory, NS_OS_TEMP_DIR)
DIR_ATOM(sOS_CurrentProcessDirectory, NS_OS_CURRENT_PROCESS_DIR)
DIR_ATOM(sOS_CurrentWorkingDirectory, NS_OS_CURRENT_WORKING_DIR)
// This one is commented out because nsGkAtoms::Home also exists, and we don't
// allow duplicate static atoms.
//DIR_ATOM(sOS_HomeDirectory, NS_OS_HOME_DIR)
DIR_ATOM(sOS_DesktopDirectory, NS_OS_DESKTOP_DIR)
DIR_ATOM(sInitCurrentProcess_dummy, NS_XPCOM_INIT_CURRENT_PROCESS_DIR)
#if defined (MOZ_WIDGET_COCOA)
DIR_ATOM(sDirectory, NS_OS_SYSTEM_DIR)
DIR_ATOM(sUserLibDirectory, NS_MAC_USER_LIB_DIR)
DIR_ATOM(sDefaultDownloadDirectory, NS_OSX_DEFAULT_DOWNLOAD_DIR)
DIR_ATOM(sLocalApplicationsDirectory, NS_OSX_LOCAL_APPLICATIONS_DIR)
DIR_ATOM(sUserPreferencesDirectory, NS_OSX_USER_PREFERENCES_DIR)
DIR_ATOM(sPictureDocumentsDirectory, NS_OSX_PICTURE_DOCUMENTS_DIR)
#elif defined (XP_WIN)
DIR_ATOM(sSystemDirectory, NS_OS_SYSTEM_DIR)
DIR_ATOM(sWindowsDirectory, NS_WIN_WINDOWS_DIR)
DIR_ATOM(sWindowsProgramFiles, NS_WIN_PROGRAM_FILES_DIR)
DIR_ATOM(sPrograms, NS_WIN_PROGRAMS_DIR)
DIR_ATOM(sFavorites, NS_WIN_FAVORITES_DIR)
DIR_ATOM(sAppdata, NS_WIN_APPDATA_DIR)
DIR_ATOM(sLocalAppdata, NS_WIN_LOCAL_APPDATA_DIR)
#if defined(MOZ_CONTENT_SANDBOX)
DIR_ATOM(sLocalAppdataLow, NS_WIN_LOCAL_APPDATA_LOW_DIR)
DIR_ATOM(sLowIntegrityTempBase, NS_WIN_LOW_INTEGRITY_TEMP_BASE)
#endif
DIR_ATOM(sWinCookiesDirectory, NS_WIN_COOKIES_DIR)
DIR_ATOM(sDefaultDownloadDirectory, NS_WIN_DEFAULT_DOWNLOAD_DIR)
#elif defined (XP_UNIX)
DIR_ATOM(sDefaultDownloadDirectory, NS_UNIX_DEFAULT_DOWNLOAD_DIR)
#endif
