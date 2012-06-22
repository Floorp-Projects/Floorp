/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_FILE_WRAPPER_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_FILE_WRAPPER_H_

#include <stddef.h>

#include "common_types.h"
#include "typedefs.h"

// Implementation of an InStream and OutStream that can read (exclusive) or
// write from/to a file.

namespace webrtc {

class FileWrapper : public InStream, public OutStream
{
public:
    static const size_t kMaxFileNameSize = 1024;

    // Factory method. Constructor disabled.
    static FileWrapper* Create();

    // Returns true if a file has been opened.
    virtual bool Open() const = 0;

    // Opens a file in read or write mode, decided by the readOnly parameter.
    virtual int OpenFile(const char* fileNameUTF8,
                         bool readOnly,
                         bool loop = false,
                         bool text = false) = 0;

    virtual int CloseFile() = 0;

    // Limits the file size to |bytes|. Writing will fail after the cap
    // is hit. Pass zero to use an unlimited size.
    virtual int SetMaxFileSize(size_t bytes)  = 0;

    // Flush any pending writes.
    virtual int Flush() = 0;

    // Returns the opened file's name in |fileNameUTF8|. Provide the size of
    // the buffer in bytes in |size|. The name will be truncated if |size| is
    // too small.
    virtual int FileName(char* fileNameUTF8,
                         size_t size) const = 0;

    // Write |format| to the opened file. Arguments are taken in the same manner
    // as printf. That is, supply a format string containing text and
    // specifiers. Returns the number of characters written or -1 on error.
    virtual int WriteText(const char* format, ...) = 0;

    // Inherited from Instream.
    // Reads |length| bytes from file to |buf|. Returns the number of bytes read
    // or -1 on error.
    virtual int Read(void* buf, int length) = 0;

    // Inherited from OutStream.
    // Writes |length| bytes from |buf| to file. The actual writing may happen
    // some time later. Call Flush() to force a write.
    virtual bool Write(const void *buf, int length) = 0;

    // Inherited from both Instream and OutStream.
    // Rewinds the file to the start. Only available when OpenFile() has been
    // called with |loop| == true or |readOnly| == true.
    virtual int Rewind() = 0;
};

} // namespace webrtc

#endif // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_FILE_WRAPPER_H_
