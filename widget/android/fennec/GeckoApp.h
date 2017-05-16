/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_GeckoApp_h__
#define mozilla_GeckoApp_h__

#include "FennecJNINatives.h"
#include "nsPluginInstanceOwner.h"

namespace mozilla {

class GeckoApp final
    : public java::GeckoApp::Natives<GeckoApp>
{
    GeckoApp() = delete;

public:
    static void OnFullScreenPluginHidden(jni::Object::Param aView)
    {
        nsPluginInstanceOwner::ExitFullScreen(aView.Get());
    }
};

} // namespace mozilla

#endif // mozilla_GeckoApp_h__
