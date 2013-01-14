/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "timestamp_map.h"
#include <stdlib.h>
#include <assert.h>

namespace webrtc {

// Constructor. Optional parameter specifies maximum number of
// coexisting timers.
VCMTimestampMap::VCMTimestampMap(WebRtc_Word32 length):
    _nextAddIx(0),
    _nextPopIx(0)
{
    if (length <= 0)
    {
        // default
        length = 10;
    }

    _map = new VCMTimestampDataTuple[length];
    _length = length;
}

// Destructor.
VCMTimestampMap::~VCMTimestampMap()
{
    delete [] _map;
}

// Empty the list of timers.
void
VCMTimestampMap::Reset()
{
    _nextAddIx = 0;
    _nextPopIx = 0;
}

WebRtc_Word32
VCMTimestampMap::Add(WebRtc_UWord32 timestamp, void* data)
{
    _map[_nextAddIx].timestamp = timestamp;
    _map[_nextAddIx].data = data;
    _nextAddIx = (_nextAddIx + 1) % _length;

    if (_nextAddIx == _nextPopIx)
    {
        // Circular list full; forget oldest entry
        _nextPopIx = (_nextPopIx + 1) % _length;
        return -1;
    }
    return 0;
}

void*
VCMTimestampMap::Pop(WebRtc_UWord32 timestamp)
{
    while (!IsEmpty())
    {
        if (_map[_nextPopIx].timestamp == timestamp)
        {
            // found start time for this timestamp
            void* data = _map[_nextPopIx].data;
            _map[_nextPopIx].data = NULL;
            _nextPopIx = (_nextPopIx + 1) % _length;
            return data;
        }
        else if (_map[_nextPopIx].timestamp > timestamp)
        {
            // the timestamp we are looking for is not in the list
            assert(_nextPopIx < _length && _nextPopIx >= 0);
            return NULL;
        }

        // not in this position, check next (and forget this position)
        _nextPopIx = (_nextPopIx + 1) % _length;
    }

    // could not find matching timestamp in list
    assert(_nextPopIx < _length && _nextPopIx >= 0);
    return NULL;
}

// Check if no timers are currently running
bool
VCMTimestampMap::IsEmpty() const
{
    return (_nextAddIx == _nextPopIx);
}

}
