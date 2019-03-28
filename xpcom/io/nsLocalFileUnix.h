/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of nsIFile for ``Unixy'' systems.
 */

#ifndef _nsLocalFileUNIX_H_
#define _nsLocalFileUNIX_H_

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "nscore.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIClassInfoImpl.h"
#include "mozilla/Attributes.h"
#ifdef MOZ_WIDGET_COCOA
#  include "nsILocalFileMac.h"
#endif

/**
 *  we need these for statfs()
 */
#ifdef HAVE_SYS_STATVFS_H
#  if defined(__osf__) && defined(__DECCXX)
extern "C" int statvfs(const char*, struct statvfs*);
#  endif
#  include <sys/statvfs.h>
#endif

#ifdef HAVE_SYS_STATFS_H
#  include <sys/statfs.h>
#endif

#ifdef HAVE_SYS_VFS_H
#  include <sys/vfs.h>
#endif

#ifdef HAVE_SYS_MOUNT_H
#  include <sys/param.h>
#  include <sys/mount.h>
#endif

#if defined(HAVE_STATVFS64) && (!defined(LINUX) && !defined(__osf__))
#  define STATFS statvfs64
#  define F_BSIZE f_frsize
#elif defined(HAVE_STATVFS) && (!defined(LINUX) && !defined(__osf__))
#  define STATFS statvfs
#  define F_BSIZE f_frsize
#elif defined(HAVE_STATFS64)
#  define STATFS statfs64
#  define F_BSIZE f_bsize
#elif defined(HAVE_STATFS)
#  define STATFS statfs
#  define F_BSIZE f_bsize
#endif

// stat64 and lstat64 are deprecated on OS X. Normal stat and lstat are
// 64-bit by default on OS X 10.6+.
#if defined(HAVE_STAT64) && defined(HAVE_LSTAT64) && !defined(XP_DARWIN)
#  if defined(AIX)
#    if defined STAT
#      undef STAT
#    endif
#  endif
#  define STAT stat64
#  define LSTAT lstat64
#  define HAVE_STATS64 1
#else
#  define STAT stat
#  define LSTAT lstat
#endif

class nsLocalFile final
#ifdef MOZ_WIDGET_COCOA
    : public nsILocalFileMac
#else
    : public nsIFile
#endif
{
 public:
  NS_DEFINE_STATIC_CID_ACCESSOR(NS_LOCAL_FILE_CID)

  nsLocalFile();
  explicit nsLocalFile(const nsACString& aFilePath);

  static nsresult nsLocalFileConstructor(nsISupports* aOuter, const nsIID& aIID,
                                         void** aInstancePtr);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIFILE
#ifdef MOZ_WIDGET_COCOA
  NS_DECL_NSILOCALFILEMAC
#endif

 private:
  nsLocalFile(const nsLocalFile& aOther);
  ~nsLocalFile() {}

 protected:
  // This stat cache holds the *last stat* - it does not invalidate.
  // Call "FillStatCache" whenever you want to stat our file.
  struct STAT mCachedStat;
  nsCString mPath;

  void LocateNativeLeafName(nsACString::const_iterator&,
                            nsACString::const_iterator&);

  nsresult CopyDirectoryTo(nsIFile* aNewParent);
  nsresult CreateAllAncestors(uint32_t aPermissions);
  nsresult GetNativeTargetPathName(nsIFile* aNewParent,
                                   const nsACString& aNewName,
                                   nsACString& aResult);

  bool FillStatCache();

  nsresult CreateAndKeepOpen(uint32_t aType, int aFlags, uint32_t aPermissions,
                             PRFileDesc** aResult);
};

#endif /* _nsLocalFileUNIX_H_ */
