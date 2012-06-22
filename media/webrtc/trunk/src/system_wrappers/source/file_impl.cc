/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "file_impl.h"

#include <assert.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <stdarg.h>
#include <string.h>
#endif

namespace webrtc {

FileWrapper* FileWrapper::Create()
{
    return new FileWrapperImpl();
}

FileWrapperImpl::FileWrapperImpl()
    : _id(NULL),
      _open(false),
      _looping(false),
      _readOnly(false),
      _maxSizeInBytes(0),
      _sizeInBytes(0)
{
    memset(_fileNameUTF8, 0, kMaxFileNameSize);
}

FileWrapperImpl::~FileWrapperImpl()
{
    if (_id != NULL)
    {
        fclose(_id);
    }
}

int FileWrapperImpl::CloseFile()
{
    if (_id != NULL)
    {
        fclose(_id);
        _id = NULL;
    }
    memset(_fileNameUTF8, 0, kMaxFileNameSize);
    _open = false;
    return 0;
}

int FileWrapperImpl::Rewind()
{
    if(_looping || !_readOnly)
    {
        if (_id != NULL)
        {
            _sizeInBytes = 0;
            return fseek(_id, 0, SEEK_SET);
        }
    }
    return -1;
}

int FileWrapperImpl::SetMaxFileSize(size_t bytes)
{
    _maxSizeInBytes = bytes;
    return 0;
}

int FileWrapperImpl::Flush()
{
    if (_id != NULL)
    {
        return fflush(_id);
    }
    return -1;
}

int FileWrapperImpl::FileName(char* fileNameUTF8,
                              size_t size) const
{
    size_t length = strlen(_fileNameUTF8);
    if(length > kMaxFileNameSize)
    {
        assert(false);
        return -1;
    }
    if(length < 1)
    {
        return -1;
    }

    // Make sure to NULL terminate
    if(size < length)
    {
        length = size - 1;
    }
    memcpy(fileNameUTF8, _fileNameUTF8, length);
    fileNameUTF8[length] = 0;
    return 0;
}

bool FileWrapperImpl::Open() const
{
    return _open;
}

int FileWrapperImpl::OpenFile(const char *fileNameUTF8, bool readOnly,
                              bool loop, bool text)
{
    size_t length = strlen(fileNameUTF8);
    if (length > kMaxFileNameSize - 1)
    {
        return -1;
    }

    _readOnly = readOnly;

    FILE *tmpId = NULL;
#if defined _WIN32
    wchar_t wideFileName[kMaxFileNameSize];
    wideFileName[0] = 0;

    MultiByteToWideChar(CP_UTF8,
                        0 /*UTF8 flag*/,
                        fileNameUTF8,
                        -1 /*Null terminated string*/,
                        wideFileName,
                        kMaxFileNameSize);
    if(text)
    {
        if(readOnly)
        {
            tmpId = _wfopen(wideFileName, L"rt");
        } else {
            tmpId = _wfopen(wideFileName, L"wt");
        }
    } else {
        if(readOnly)
        {
            tmpId = _wfopen(wideFileName, L"rb");
        } else {
            tmpId = _wfopen(wideFileName, L"wb");
        }
    }
#else
    if(text)
    {
        if(readOnly)
        {
            tmpId = fopen(fileNameUTF8, "rt");
        } else {
            tmpId = fopen(fileNameUTF8, "wt");
        }
    } else {
        if(readOnly)
        {
            tmpId = fopen(fileNameUTF8, "rb");
        } else {
            tmpId = fopen(fileNameUTF8, "wb");
        }
    }
#endif

    if (tmpId != NULL)
    {
        // +1 comes from copying the NULL termination character.
        memcpy(_fileNameUTF8, fileNameUTF8, length + 1);
        if (_id != NULL)
        {
            fclose(_id);
        }
        _id = tmpId;
        _looping = loop;
        _open = true;
        return 0;
    }
    return -1;
}

int FileWrapperImpl::Read(void* buf, int length)
{
    if (length < 0)
        return -1;

    if (_id == NULL)
        return -1;

    int bytes_read = static_cast<int>(fread(buf, 1, length, _id));
    if (bytes_read != length && !_looping)
    {
        CloseFile();
    }
    return bytes_read;
}

int FileWrapperImpl::WriteText(const char* format, ...)
{
    if (format == NULL)
        return -1;

    if (_readOnly)
        return -1;

    if (_id == NULL)
        return -1;

    va_list args;
    va_start(args, format);
    int num_chars = vfprintf(_id, format, args);
    va_end(args);

    if (num_chars >= 0)
    {
        return num_chars;
    }
    else
    {
        CloseFile();
        return -1;
    }
}

bool FileWrapperImpl::Write(const void* buf, int length)
{
    if (buf == NULL)
        return false;

    if (length < 0)
        return false;

    if (_readOnly)
        return false;

    if (_id == NULL)
        return false;

    // Check if it's time to stop writing.
    if (_maxSizeInBytes > 0 && (_sizeInBytes + length) > _maxSizeInBytes)
    {
        Flush();
        return false;
    }

    size_t num_bytes = fwrite(buf, 1, length, _id);
    if (num_bytes > 0)
    {
        _sizeInBytes += num_bytes;
        return true;
    }

    CloseFile();
    return false;
}

} // namespace webrtc
