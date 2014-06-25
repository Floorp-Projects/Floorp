/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _RemoteOpenFileChild_h
#define _RemoteOpenFileChild_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/net/PRemoteOpenFileChild.h"
#include "nsICachedFileDescriptorListener.h"
#include "nsILocalFile.h"
#include "nsIRemoteOpenFileListener.h"

class nsILoadContext;

namespace mozilla {

namespace ipc {
class FileDescriptor;
}

namespace net {

/**
 * RemoteOpenFileChild: a thin wrapper around regular nsIFile classes that does
 * IPC to open a file handle on parent instead of child.  Used when we can't
 * open file handle on child (don't have permission), but we don't want the
 * overhead of shipping all I/O traffic across IPDL.  Example: JAR files.
 *
 * To open a file handle with this class, AsyncRemoteFileOpen() must be called
 * first.  After the listener's OnRemoteFileOpenComplete() is called, if the
 * result is NS_OK, nsIFile.OpenNSPRFileDesc() may be called--once--to get the
 * file handle.
 *
 * Note that calling Clone() on this class results in the filehandle ownership
 * being passed on to the new RemoteOpenFileChild. I.e. if
 * OnRemoteFileOpenComplete is called and then Clone(), OpenNSPRFileDesc() will
 * work in the cloned object, but not in the original.
 *
 * This class should only be instantiated in a child process.
 *
 */
class RemoteOpenFileChild MOZ_FINAL
  : public PRemoteOpenFileChild
  , public nsIFile
  , public nsIHashable
  , public nsICachedFileDescriptorListener
{
  typedef mozilla::dom::TabChild TabChild;
  typedef mozilla::ipc::FileDescriptor FileDescriptor;

  virtual ~RemoteOpenFileChild();

public:
  RemoteOpenFileChild()
    : mNSPRFileDesc(nullptr)
    , mAsyncOpenCalled(false)
    , mNSPROpenCalled(false)
  {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIFILE
  NS_DECL_NSIHASHABLE

  // aRemoteOpenUri must be scheme 'remoteopenfile://': otherwise looks like
  // a file:// uri.
  nsresult Init(nsIURI* aRemoteOpenUri, nsIURI* aAppUri);

  // Send message to parent to tell it to open file handle for file.
  // TabChild is required, for IPC security.
  // Note: currently only PR_RDONLY is supported for 'flags'
  nsresult AsyncRemoteFileOpen(int32_t aFlags,
                               nsIRemoteOpenFileListener* aListener,
                               nsITabChild* aTabChild,
                               nsILoadContext *aLoadContext);

  void ReleaseIPDLReference()
  {
    Release();
  }

private:
  RemoteOpenFileChild(const RemoteOpenFileChild& other);

protected:
  void AddIPDLReference()
  {
    AddRef();
  }

  virtual bool Recv__delete__(const FileDescriptor&) MOZ_OVERRIDE;

  virtual void OnCachedFileDescriptor(const nsAString& aPath,
                                      const FileDescriptor& aFD) MOZ_OVERRIDE;

  void HandleFileDescriptorAndNotifyListener(const FileDescriptor&,
                                             bool aFromRecvDelete);

  void NotifyListener(nsresult aResult);

  // regular nsIFile object, that we forward most calls to.
  nsCOMPtr<nsIFile> mFile;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIURI> mAppURI;
  nsCOMPtr<nsIRemoteOpenFileListener> mListener;
  nsRefPtr<TabChild> mTabChild;
  PRFileDesc* mNSPRFileDesc;
  bool mAsyncOpenCalled;
  bool mNSPROpenCalled;
};

} // namespace net
} // namespace mozilla

#endif // _RemoteOpenFileChild_h
