/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_FILE_IMPL_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_FILE_IMPL_H_

#include "file_wrapper.h"

#include <stdio.h>

namespace webrtc {

class FileWrapperImpl : public FileWrapper
{
public:
    FileWrapperImpl();
    virtual ~FileWrapperImpl();

    virtual int FileName(char* fileNameUTF8,
                         size_t size) const;

    virtual bool Open() const;

    virtual int OpenFile(const char* fileNameUTF8,
                         bool readOnly,
                         bool loop = false,
                         bool text = false);

    virtual int CloseFile();
    virtual int SetMaxFileSize(size_t bytes);
    virtual int Flush();

    virtual int Read(void* buf, int length);
    virtual bool Write(const void *buf, int length);
    virtual int WriteText(const char* format, ...);
    virtual int Rewind();

private:
    FILE* _id;
    bool _open;
    bool _looping;
    bool _readOnly;
    size_t _maxSizeInBytes; // -1 indicates file size limitation is off
    size_t _sizeInBytes;
    char _fileNameUTF8[kMaxFileNameSize];
};

} // namespace webrtc

#endif // WEBRTC_SYSTEM_WRAPPERS_SOURCE_FILE_IMPL_H_
