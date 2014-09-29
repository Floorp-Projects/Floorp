/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsLocalFileWIN_H_
#define _nsLocalFileWIN_H_

#include "nscore.h"
#include "nsError.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIFile.h"
#include "nsIFactory.h"
#include "nsILocalFileWin.h"
#include "nsIHashable.h"
#include "nsIClassInfoImpl.h"
#include "prio.h"

#include "mozilla/Attributes.h"

#include "windows.h"
#include "shlobj.h"

#include <sys/stat.h>

class nsLocalFile MOZ_FINAL
  : public nsILocalFileWin
  , public nsIHashable
{
public:
  NS_DEFINE_STATIC_CID_ACCESSOR(NS_LOCAL_FILE_CID)

  nsLocalFile();

  static nsresult nsLocalFileConstructor(nsISupports* aOuter,
                                         const nsIID& aIID,
                                         void** aInstancePtr);

  // nsISupports interface
  NS_DECL_THREADSAFE_ISUPPORTS

  // nsIFile interface
  NS_DECL_NSIFILE

  // nsILocalFile interface
  NS_DECL_NSILOCALFILE

  // nsILocalFileWin interface
  NS_DECL_NSILOCALFILEWIN

  // nsIHashable interface
  NS_DECL_NSIHASHABLE

public:
  static void GlobalInit();
  static void GlobalShutdown();

private:
  // CopyMove and CopySingleFile constants for |options| parameter:
  enum CopyFileOption {
    FollowSymlinks          = 1u << 0,
    Move                    = 1u << 1,
    SkipNtfsAclReset        = 1u << 2,
    Rename                  = 1u << 3
  };

  nsLocalFile(const nsLocalFile& aOther);
  ~nsLocalFile()
  {
  }

  bool mDirty;            // cached information can only be used when this is false
  bool mResolveDirty;
  bool mFollowSymlinks;   // should we follow symlinks when working on this file

  // this string will always be in native format!
  nsString mWorkingPath;

  // this will be the resolved path of shortcuts, it will *NEVER*
  // be returned to the user
  nsString mResolvedPath;

  // this string, if not empty, is the *short* pathname that represents
  // mWorkingPath
  nsString mShortWorkingPath;

  PRFileInfo64 mFileInfo64;

  void MakeDirty()
  {
    mDirty = true;
    mResolveDirty = true;
    mShortWorkingPath.Truncate();
  }

  nsresult ResolveAndStat();
  nsresult Resolve();
  nsresult ResolveShortcut();

  void EnsureShortPath();

  nsresult CopyMove(nsIFile* aNewParentDir, const nsAString& aNewName,
                    uint32_t aOptions);
  nsresult CopySingleFile(nsIFile* aSource, nsIFile* aDest,
                          const nsAString& aNewName, uint32_t aOptions);

  nsresult SetModDate(int64_t aLastModifiedTime, const wchar_t* aFilePath);
  nsresult HasFileAttribute(DWORD aFileAttrib, bool* aResult);
  nsresult AppendInternal(const nsAFlatString& aNode,
                          bool aMultipleComponents);

  nsresult OpenNSPRFileDescMaybeShareDelete(int32_t aFlags,
                                            int32_t aMode,
                                            bool aShareDelete,
                                            PRFileDesc** aResult);
};

#endif
