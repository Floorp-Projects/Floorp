/*
 *  Copyright 2009 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_FILELOCK_H_
#define WEBRTC_BASE_FILELOCK_H_

#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/scoped_ptr.h"

namespace rtc {

class FileStream;

// Implements a very simple cross process lock based on a file.
// When Lock(...) is called we try to open/create the file in read/write
// mode without any sharing. (Or locking it with flock(...) on Unix)
// If the process crash the OS will make sure that the file descriptor
// is released and another process can accuire the lock.
// This doesn't work on ancient OSX/Linux versions if used on NFS.
// (Nfs-client before: ~2.6 and Linux Kernel < 2.6.)
class FileLock {
 public:
  virtual ~FileLock();

  // Attempts to lock the file. The caller owns the returned
  // lock object. Returns NULL if the file already was locked.
  static FileLock* TryLock(const std::string& path);
  void Unlock();

 protected:
  FileLock(const std::string& path, FileStream* file);

 private:
  void MaybeUnlock();

  std::string path_;
  scoped_ptr<FileStream> file_;

  DISALLOW_EVIL_CONSTRUCTORS(FileLock);
};

}  // namespace rtc

#endif  // WEBRTC_BASE_FILELOCK_H_
