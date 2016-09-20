/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ANRReporter_h__
#define ANRReporter_h__

#include "FennecJNINatives.h"

namespace mozilla {

class ANRReporter : public java::ANRReporter::Natives<ANRReporter>
{
private:
    ANRReporter();

public:
    static bool RequestNativeStack(bool aUnwind);
    static jni::String::LocalRef GetNativeStack();
    static void ReleaseNativeStack();
};

} // namespace

#endif // ANRReporter_h__
