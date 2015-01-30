/*
 *  Copyright 2009 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/filelock.h"

#include "webrtc/base/fileutils.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/pathutils.h"
#include "webrtc/base/stream.h"

namespace rtc {

FileLock::FileLock(const std::string& path, FileStream* file)
    : path_(path), file_(file) {
}

FileLock::~FileLock() {
  MaybeUnlock();
}

void FileLock::Unlock() {
  LOG_F(LS_INFO);
  MaybeUnlock();
}

void FileLock::MaybeUnlock() {
  if (file_) {
    LOG(LS_INFO) << "Unlocking:" << path_;
    file_->Close();
    Filesystem::DeleteFile(path_);
    file_.reset();
  }
}

FileLock* FileLock::TryLock(const std::string& path) {
  FileStream* stream = new FileStream();
  bool ok = false;
#if defined(WEBRTC_WIN)
  // Open and lock in a single operation.
  ok = stream->OpenShare(path, "a", _SH_DENYRW, NULL);
#else // WEBRTC_LINUX && !WEBRTC_ANDROID and WEBRTC_MAC && !defined(WEBRTC_IOS)
  ok = stream->Open(path, "a", NULL) && stream->TryLock();
#endif
  if (ok) {
    return new FileLock(path, stream);
  } else {
    // Something failed, either we didn't succeed to open the
    // file or we failed to lock it. Anyway remove the heap
    // allocated object and then return NULL to indicate failure.
    delete stream;
    return NULL;
  }
}

}  // namespace rtc
