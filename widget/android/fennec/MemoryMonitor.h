/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MemoryMonitor_h
#define MemoryMonitor_h

#include "FennecJNINatives.h"
#include "nsMemoryPressure.h"

namespace mozilla {

class MemoryMonitor final
    : public java::MemoryMonitor::Natives<MemoryMonitor>
{
public:
    static void
    DispatchMemoryPressure()
    {
        NS_DispatchMemoryPressure(MemoryPressureState::MemPressure_New);
    }

    static void
    DispatchMemoryPressureStop()
    {
        NS_DispatchMemoryPressure(MemoryPressureState::MemPressure_Stopping);
    }
};

} // namespace mozilla

#endif // MemoryMonitor_h
