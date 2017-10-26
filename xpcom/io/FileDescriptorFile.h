/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _FileDescriptorFile_h
#define _FileDescriptorFile_h

#include "mozilla/Attributes.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsIURI.h"
#include "private/pprio.h"

namespace mozilla {
namespace net {

/**
 * A limited implementation of nsIFile that wraps a FileDescriptor object
 * allowing the file to be read from. Added to allow a child process to use
 * an nsIFile object for a file it does not have access to on the filesystem
 * but has been provided a FileDescriptor for from the parent. Many nsIFile
 * methods are not implemented and this is not intended to be a general
 * purpose file implementation.
 */
class FileDescriptorFile final : public nsIFile
{
  typedef mozilla::ipc::FileDescriptor FileDescriptor;

public:
  FileDescriptorFile(const FileDescriptor& aFD, nsIFile* aFile);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIFILE

private:
  ~FileDescriptorFile()
  {}

  FileDescriptorFile(const FileDescriptorFile& other);

  // regular nsIFile object, that we forward most calls to.
  nsCOMPtr<nsIFile> mFile;
  FileDescriptor mFD;
};

} // namespace net
} // namespace mozilla

#endif // _FileDescriptorFile_h
