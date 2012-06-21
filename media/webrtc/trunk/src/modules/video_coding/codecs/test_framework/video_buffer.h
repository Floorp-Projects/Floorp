/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAMEWORK_VIDEO_BUFFER_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAMEWORK_VIDEO_BUFFER_H_

#include "typedefs.h"
#include "video_image.h"

class TestVideoBuffer
{
public:
    TestVideoBuffer();

    virtual ~TestVideoBuffer();

    TestVideoBuffer(const TestVideoBuffer& rhs);

    /**
    * Verifies that current allocated buffer size is larger than or equal to the input size.
    * If the current buffer size is smaller, a new allocation is made and the old buffer data is copied to the new buffer.
    */
    void VerifyAndAllocate(unsigned int minimumSize);

    void UpdateLength(unsigned int newLength);

    void SwapBuffers(TestVideoBuffer& videoBuffer);

    void CopyBuffer(unsigned int length, const unsigned char* fromBuffer);

    void CopyBuffer(TestVideoBuffer& fromVideoBuffer);

    // Use with care, and remember to call ClearPointer() when done.
    void CopyPointer(const TestVideoBuffer& fromVideoBuffer);

    void ClearPointer();

    int  SetOffset(unsigned int length);            // Sets offset to beginning of frame in buffer

    void Free();                                    // Deletes frame buffer and resets members to zero

    void SetTimeStamp(unsigned int timeStamp);      // Sets timestamp of frame (90kHz)

    /**
    *   Gets pointer to frame buffer
    */
    unsigned char* GetBuffer() const;

    /**
    *   Gets allocated buffer size
    */
    unsigned int	GetSize() const;

    /**
    *   Gets length of frame
    */
    unsigned int	GetLength() const;

    /**
    *   Gets timestamp of frame (90kHz)
    */
    unsigned int	GetTimeStamp() const;

    unsigned int	GetWidth() const;
    unsigned int	GetHeight() const;

    void            SetWidth(unsigned int width);
    void            SetHeight(unsigned int height);

private:
    TestVideoBuffer& operator=(const TestVideoBuffer& inBuffer);

private:
    void Set(unsigned char* buffer,unsigned int size,unsigned int length,unsigned int offset, unsigned int timeStamp);
    unsigned int GetStartOffset() const;

    unsigned char*		  _buffer;          // Pointer to frame buffer
    unsigned int		  _bufferSize;      // Allocated buffer size
    unsigned int		  _bufferLength;    // Length (in bytes) of frame
    unsigned int		  _startOffset;     // Offset (in bytes) to beginning of frame in buffer
    unsigned int		  _timeStamp;       // Timestamp of frame (90kHz)
    unsigned int          _width;
    unsigned int          _height;
};

class TestVideoEncodedBuffer: public TestVideoBuffer
{
public:
    TestVideoEncodedBuffer();
    ~TestVideoEncodedBuffer();

    void SetCaptureWidth(unsigned short width);
    void SetCaptureHeight(unsigned short height);
    unsigned short GetCaptureWidth();
    unsigned short GetCaptureHeight();

    webrtc::VideoFrameType GetFrameType();
    void SetFrameType(webrtc::VideoFrameType frametype);

    void Reset();

    void SetFrameRate(float frameRate);
    float GetFrameRate();

private:
    TestVideoEncodedBuffer& operator=(const TestVideoEncodedBuffer& inBuffer);

private:
    unsigned short			   _captureWidth;
    unsigned short			   _captureHeight;
    webrtc::VideoFrameType     _frameType;
    float                      _frameRate;
};

#endif // WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAMEWORK_VIDEO_BUFFER_H_
