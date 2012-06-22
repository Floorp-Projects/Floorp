/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <string.h>
#include "video_buffer.h"

using namespace webrtc;

TestVideoBuffer::TestVideoBuffer():
_buffer(0),
_bufferSize(0),
_bufferLength(0),
_startOffset(0),
_timeStamp(0),
_width(0),
_height(0)
{
   //
}


TestVideoBuffer::~TestVideoBuffer()
{
    _timeStamp = 0;
    _startOffset = 0;
    _bufferLength = 0;
    _bufferSize = 0;

    if(_buffer)
    {
        delete [] _buffer;
        _buffer = 0;
    }
}

TestVideoBuffer::TestVideoBuffer(const TestVideoBuffer& rhs)
:
_buffer(0),
_bufferSize(rhs._bufferSize),
_bufferLength(rhs._bufferLength),
_startOffset(rhs._startOffset),
_timeStamp(rhs._timeStamp),
_width(rhs._width),
_height(rhs._height)
{
    // make sure that our buffer is big enough
    _buffer = new unsigned char[_bufferSize];

    // only copy required length
    memcpy(_buffer + _startOffset, rhs._buffer, _bufferLength);  // GetBuffer() includes _startOffset
}

void TestVideoBuffer::SetTimeStamp(unsigned int timeStamp)
{
    _timeStamp = timeStamp;
}

unsigned int
TestVideoBuffer::GetWidth() const
{
    return _width;
}

unsigned int
TestVideoBuffer::GetHeight() const
{
    return _height;
}

void
TestVideoBuffer::SetWidth(unsigned int width)
{
    _width = width;
}

void
TestVideoBuffer::SetHeight(unsigned int height)
{
    _height = height;
}


void TestVideoBuffer::Free()
{
    _timeStamp = 0;
    _startOffset = 0;
    _bufferLength = 0;
    _bufferSize = 0;
    _height = 0;
    _width = 0;

    if(_buffer)
    {
        delete [] _buffer;
        _buffer = 0;
    }
}

void TestVideoBuffer::VerifyAndAllocate(unsigned int minimumSize)
{
    if(minimumSize > _bufferSize)
    {
        // make sure that our buffer is big enough
        unsigned char * newBufferBuffer = new unsigned char[minimumSize];
        if(_buffer)
        {
            // copy the old data
            memcpy(newBufferBuffer, _buffer, _bufferSize);
            delete [] _buffer;
        }
        _buffer = newBufferBuffer;
        _bufferSize = minimumSize;
    }
}

int TestVideoBuffer::SetOffset(unsigned int length)
{
    if (length > _bufferSize ||
        length > _bufferLength)
    {
        return -1;
    }

    unsigned int oldOffset = _startOffset;

    if(oldOffset > length)
    {
        unsigned int newLength = _bufferLength + (oldOffset-length);// increase by the diff
        assert(newLength <= _bufferSize);
        _bufferLength = newLength;
    }
    if(oldOffset < length)
    {
        if(_bufferLength > (length-oldOffset))
        {
            _bufferLength -= (length-oldOffset); // decrease by the diff
        }
    }
    _startOffset = length; // update

    return 0;
}

void TestVideoBuffer::UpdateLength(unsigned int newLength)
{
    assert(newLength +_startOffset <= _bufferSize);
    _bufferLength = newLength;
}

void TestVideoBuffer::CopyBuffer(unsigned int length, const unsigned char* buffer)
{
    assert(length+_startOffset <= _bufferSize);
    memcpy(_buffer+_startOffset, buffer, length);
    _bufferLength = length;
}

void TestVideoBuffer::CopyBuffer(TestVideoBuffer& fromVideoBuffer)
{
    assert(fromVideoBuffer.GetLength() + fromVideoBuffer.GetStartOffset() <= _bufferSize);
    assert(fromVideoBuffer.GetSize() <= _bufferSize);

    _bufferLength = fromVideoBuffer.GetLength();
    _startOffset = fromVideoBuffer.GetStartOffset();
    _timeStamp = fromVideoBuffer.GetTimeStamp();
    _height = fromVideoBuffer.GetHeight();
    _width = fromVideoBuffer.GetWidth();

    // only copy required length
    memcpy(_buffer+_startOffset, fromVideoBuffer.GetBuffer(), fromVideoBuffer.GetLength());  // GetBuffer() includes _startOffset

}

void TestVideoBuffer::CopyPointer(const TestVideoBuffer& fromVideoBuffer)
{
    _bufferSize = fromVideoBuffer.GetSize();
    _bufferLength = fromVideoBuffer.GetLength();
    _startOffset = fromVideoBuffer.GetStartOffset();
    _timeStamp = fromVideoBuffer.GetTimeStamp();
    _height = fromVideoBuffer.GetHeight();
    _width = fromVideoBuffer.GetWidth();

    _buffer = fromVideoBuffer.GetBuffer();
}

void TestVideoBuffer::ClearPointer()
{
    _buffer = NULL;
}

void TestVideoBuffer::SwapBuffers(TestVideoBuffer& videoBuffer)
{
    unsigned char*  tempBuffer = _buffer;
    unsigned int    tempSize = _bufferSize;
    unsigned int    tempLength =_bufferLength;
    unsigned int    tempOffset = _startOffset;
    unsigned int    tempTime = _timeStamp;
    unsigned int    tempHeight = _height;
    unsigned int    tempWidth = _width;

    _buffer = videoBuffer.GetBuffer();
    _bufferSize = videoBuffer.GetSize();
    _bufferLength = videoBuffer.GetLength();
    _startOffset = videoBuffer.GetStartOffset();
    _timeStamp =  videoBuffer.GetTimeStamp();
    _height = videoBuffer.GetHeight();
    _width = videoBuffer.GetWidth();


    videoBuffer.Set(tempBuffer, tempSize, tempLength, tempOffset, tempTime);
    videoBuffer.SetHeight(tempHeight);
    videoBuffer.SetWidth(tempWidth);
}

void TestVideoBuffer::Set(unsigned char* tempBuffer,unsigned int tempSize,unsigned int tempLength, unsigned int tempOffset,unsigned int timeStamp)
{
    _buffer = tempBuffer;
    _bufferSize = tempSize;
    _bufferLength = tempLength;
    _startOffset = tempOffset;
    _timeStamp = timeStamp;
}

unsigned char* TestVideoBuffer::GetBuffer() const
{
    return _buffer+_startOffset;
}

unsigned int TestVideoBuffer::GetStartOffset() const
{
    return _startOffset;
}

unsigned int TestVideoBuffer::GetSize() const
{
    return _bufferSize;
}

unsigned int TestVideoBuffer::GetLength() const
{
    return _bufferLength;
}

unsigned int TestVideoBuffer::GetTimeStamp() const
{
    return _timeStamp;
}

/**
*   TestVideoEncodedBuffer
*
*/

TestVideoEncodedBuffer::TestVideoEncodedBuffer() :
    _captureWidth(0),
    _captureHeight(0),
    _frameRate(-1)
{
    _frameType = kDeltaFrame;
}

TestVideoEncodedBuffer::~TestVideoEncodedBuffer()
{
}

void TestVideoEncodedBuffer::SetCaptureWidth(unsigned short width)
{
    _captureWidth = width;
}

void TestVideoEncodedBuffer::SetCaptureHeight(unsigned short height)
{
    _captureHeight = height;
}

unsigned short TestVideoEncodedBuffer::GetCaptureWidth()
{
    return _captureWidth;
}

unsigned short TestVideoEncodedBuffer::GetCaptureHeight()
{
    return _captureHeight;
}

VideoFrameType TestVideoEncodedBuffer::GetFrameType()
{
    return _frameType;
}

void TestVideoEncodedBuffer::SetFrameType(VideoFrameType frametype)
{
    _frameType = frametype;
}

void TestVideoEncodedBuffer::Reset()
{
    _captureWidth = 0;
    _captureHeight = 0;
    _frameRate = -1;
    _frameType = kDeltaFrame;
}

void  TestVideoEncodedBuffer::SetFrameRate(float frameRate)
{
    _frameRate = frameRate;
}

float  TestVideoEncodedBuffer::GetFrameRate()
{
    return _frameRate;
}
