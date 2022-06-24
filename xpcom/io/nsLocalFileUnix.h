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

#include "nscore.h"
#include "nsString.h"
#ifdef MOZ_WIDGET_COCOA
#  include "nsILocalFileMac.h"
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

#if defined(HAVE_SYS_QUOTA_H) && defined(HAVE_LINUX_QUOTA_H)
#  define USE_LINUX_QUOTACTL
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

  static nsresult nsLocalFileConstructor(const nsIID& aIID,
                                         void** aInstancePtr);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIFILE
#ifdef MOZ_WIDGET_COCOA
  NS_DECL_NSILOCALFILEMAC
#endif

 private:
  nsLocalFile(const nsLocalFile& aOther);
  ~nsLocalFile() = default;

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
                             bool aSkipAncestors, PRFileDesc** aResult);

  nsresult SetLastModifiedTimeImpl(PRTime aLastModTime, bool aFollowLinks);
  nsresult GetLastModifiedTimeImpl(PRTime* aLastModTime, bool aFollowLinks);
  nsresult GetCreationTimeImpl(PRTime* aCreationTime, bool aFollowLinks);

#if defined(USE_LINUX_QUOTACTL)
  template <typename StatInfoFunc, typename QuotaInfoFunc>
  nsresult GetDiskInfo(StatInfoFunc&& aStatInfoFunc,
                       QuotaInfoFunc&& aQuotaInfoFunc, int64_t* aResult);
#else
  template <typename StatInfoFunc>
  nsresult GetDiskInfo(StatInfoFunc&& aStatInfoFunc, int64_t* aResult);
#endif
};

#endif /* _nsLocalFileUNIX_H_ */
