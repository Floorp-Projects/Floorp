/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "critical_section_wrapper.h"
#include "ref_count.h"

namespace webrtc {

namespace voe {

RefCount::RefCount() :
    _count(0),
    _crit(*CriticalSectionWrapper::CreateCriticalSection())
{
}

RefCount::~RefCount()
{
    delete &_crit;
}

RefCount&
RefCount::operator++(int)
{
    CriticalSectionScoped lock(&_crit);
    _count++;
    return *this;
}
    
RefCount&
RefCount::operator--(int)
{
    CriticalSectionScoped lock(&_crit);
    _count--;
    return *this;
}
  
void 
RefCount::Reset()
{
    CriticalSectionScoped lock(&_crit);
    _count = 0;
}

int 
RefCount::GetCount() const
{
    return _count;
}

}  // namespace voe

}  //  namespace webrtc
