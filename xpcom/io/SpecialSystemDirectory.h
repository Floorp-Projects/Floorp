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
#include "nsILocalFileMac.h"
#include "prenv.h"
#endif

enum SystemDirectories {
  OS_TemporaryDirectory     =   2,
  OS_CurrentProcessDirectory =  3,
  OS_CurrentWorkingDirectory =  4,

  Win_SystemDirectory       =   201,
  Win_WindowsDirectory      =   202,
  Win_HomeDirectory         =   203,
  Win_Programs              =   205,
  Win_Favorites             =   209,
  Win_Desktopdirectory      =   213,
  Win_Appdata               =   221,
  Win_Cookies               =   223,
  Win_LocalAppdata          =   224,
  Win_ProgramFiles          =   225,
  Win_Downloads             =   226,
#if defined(MOZ_THUNDERBIRD) || defined(MOZ_SUITE)
  Win_Documents             =   228,
#endif
#if defined(MOZ_CONTENT_SANDBOX)
  Win_LocalAppdataLow       =   232,
#endif

  Unix_HomeDirectory        =   303,
  Unix_XDG_Desktop          =   304,
  Unix_XDG_Download         =   306
};

nsresult
GetSpecialSystemDirectory(SystemDirectories aSystemSystemDirectory,
                          nsIFile** aFile);
#ifdef MOZ_WIDGET_COCOA
nsresult
GetOSXFolderType(short aDomain, OSType aFolderType, nsIFile** aLocalFile);
#endif

#endif
