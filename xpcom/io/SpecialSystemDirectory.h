/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SPECIALSYSTEMDIRECTORY_H_
#define _SPECIALSYSTEMDIRECTORY_H_

#include "nscore.h"
#include "nsIFile.h"

#ifdef MOZ_WIDGET_COCOA
#  include "nsILocalFileMac.h"
#  include "prenv.h"
#endif

enum SystemDirectories {
  OS_TemporaryDirectory = 2,
  // 3 Used to be OS_CurrentProcessDirectory, which we never actually
  //   supported getting...
  OS_CurrentWorkingDirectory = 4,

  Mac_SystemDirectory = 101,
  Mac_UserLibDirectory = 102,
  Mac_HomeDirectory = 103,
  Mac_DefaultDownloadDirectory = 104,
  Mac_UserDesktopDirectory = 105,
  Mac_LocalApplicationsDirectory = 106,
  Mac_UserPreferencesDirectory = 107,
  Mac_PictureDocumentsDirectory = 108,
  Mac_DefaultScreenshotDirectory = 109,
  Mac_UserDocumentsDirectory = 110,

  Win_SystemDirectory = 201,
  Win_WindowsDirectory = 202,
  Win_HomeDirectory = 203,
  Win_Programs = 205,
  Win_Favorites = 209,
  Win_Desktopdirectory = 213,
  Win_Appdata = 221,
  Win_Cookies = 223,
  Win_LocalAppdata = 224,
  Win_ProgramFiles = 225,
  Win_Downloads = 226,
  Win_Documents = 228,

  Unix_HomeDirectory = 303,
  Unix_XDG_Desktop = 304,
  Unix_XDG_Documents = 305,
  Unix_XDG_Download = 306,
  Unix_SystemConfigDirectory = 307,
};

nsresult GetSpecialSystemDirectory(SystemDirectories aSystemSystemDirectory,
                                   nsIFile** aFile);
#ifdef MOZ_WIDGET_COCOA
nsresult GetOSXFolderType(short aDomain, OSType aFolderType,
                          nsIFile** aLocalFile);
#endif

#endif
