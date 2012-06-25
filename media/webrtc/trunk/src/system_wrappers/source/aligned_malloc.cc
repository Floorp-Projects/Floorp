/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "aligned_malloc.h"

#include <assert.h>
#include <memory.h>

#ifdef WEBRTC_ANDROID
#include <stdlib.h>
#endif

#if WEBRTC_MAC
  #include <malloc/malloc.h>
#else
  #include <malloc.h>
#endif

#if _WIN32
    #include <windows.h>
#else
    #include <stdint.h>
#endif

#include "typedefs.h"

// Ok reference on memory alignment:
// http://stackoverflow.com/questions/227897/solve-the-memory-alignment-in-c-interview-question-that-stumped-me

namespace webrtc
{
// TODO (hellner) better to create just one memory block and
//                           interpret the first sizeof(AlignedMemory) bytes as
//                           an AlignedMemory struct.
struct AlignedMemory
{
  void* alignedBuffer;
  void* memoryPointer;
};

void* AlignedMalloc(size_t size, size_t alignment)
{
    if(alignment == 0)
    {
        // Don't allow alignment 0 since it's undefined.
        return NULL;
    }
    // Make sure that the alignment is an integer power of two or fail.
    if(alignment & (alignment - 1))
    {
        return NULL;
    }

    AlignedMemory* returnValue = new AlignedMemory();
    if(returnValue == NULL)
    {
        return NULL;
    }

    // The memory is aligned towards the lowest address that so only
    // alignment - 1 bytes needs to be allocated.
    // A pointer to AlignedMemory must be stored so that it can be retreived for
    // deletion, ergo the sizeof(uintptr_t).
    returnValue->memoryPointer = malloc(size + sizeof(uintptr_t) +
                                        alignment - 1);
    if(returnValue->memoryPointer == NULL)
    {
        delete returnValue;
        return NULL;
    }

    // Alligning after the sizeof(header) bytes will leave room for the header
    // in the same memory block.
    uintptr_t alignStartPos = (uintptr_t)returnValue->memoryPointer;
    alignStartPos += sizeof(uintptr_t);

    // The buffer should be aligned with 'alignment' bytes. The - 1 guarantees
    // that we align towards the lowest address.
    uintptr_t alignedPos = (alignStartPos + alignment - 1) & ~(alignment - 1);

    // alignedPos is the address sought for.
    returnValue->alignedBuffer = (void*)alignedPos;

    // Store the address to the AlignedMemory struct in the header so that a
    // it's possible to reclaim all memory.
    uintptr_t headerPos = alignedPos;
    headerPos -= sizeof(uintptr_t);
    void* headerPtr = (void*) headerPos;
    uintptr_t headerValue = (uintptr_t)returnValue;
    memcpy(headerPtr,&headerValue,sizeof(uintptr_t));

    return returnValue->alignedBuffer;
}

void AlignedFree(void* memBlock)
{
    if(memBlock == NULL)
    {
        return;
    }
    uintptr_t alignedPos = (uintptr_t)memBlock;
    uintptr_t headerPos = alignedPos - sizeof(uintptr_t);

    // Read out the address of the AlignedMemory struct from the header.
    uintptr_t* headerPtr = (uintptr_t*)headerPos;
    AlignedMemory* deleteMemory = (AlignedMemory*) *headerPtr;

    if(deleteMemory->memoryPointer != NULL)
    {
        free(deleteMemory->memoryPointer);
    }
    delete deleteMemory;
}
} // namespace webrtc
