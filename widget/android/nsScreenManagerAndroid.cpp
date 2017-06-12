/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set sw=4 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define MOZ_FATAL_ASSERTIONS_FOR_THREAD_SAFETY

#include "mozilla/SyncRunnable.h"
#include "nsScreenManagerAndroid.h"
#include "nsServiceManagerUtils.h"
#include "AndroidRect.h"
#include "GeneratedJNINatives.h"
#include "nsAppShell.h"
#include "nsThreadUtils.h"

#include <android/log.h>
#include <mozilla/jni/Refs.h>

#define ALOG(args...) __android_log_print(ANDROID_LOG_INFO, "nsScreenManagerAndroid", ## args)

using namespace mozilla;
using namespace mozilla::java;

static uint32_t sScreenId = 0;
const uint32_t PRIMARY_SCREEN_ID = 0;

nsScreenAndroid::nsScreenAndroid(DisplayType aDisplayType, nsIntRect aRect)
    : mId(sScreenId++)
    , mDisplayType(aDisplayType)
    , mRect(aRect)
    , mDensity(0.0)
{
    // ensure that the ID of the primary screen would be PRIMARY_SCREEN_ID.
    if (mDisplayType == DisplayType::DISPLAY_PRIMARY) {
        mId = PRIMARY_SCREEN_ID;
    }
}

nsScreenAndroid::~nsScreenAndroid()
{
}

float
nsScreenAndroid::GetDensity() {
    if (mDensity != 0.0) {
        return mDensity;
    }
    if (mDisplayType == DisplayType::DISPLAY_PRIMARY) {
        mDensity = mozilla::jni::IsAvailable() ? GeckoAppShell::GetDensity()
                                               : 1.0; // xpcshell most likely
        return mDensity;
    }
    return 1.0;
}

NS_IMETHODIMP
nsScreenAndroid::GetRect(int32_t *outLeft, int32_t *outTop, int32_t *outWidth, int32_t *outHeight)
{
    if (mDisplayType != DisplayType::DISPLAY_PRIMARY) {
        *outLeft   = mRect.x;
        *outTop    = mRect.y;
        *outWidth  = mRect.width;
        *outHeight = mRect.height;

        return NS_OK;
    }

    if (!mozilla::jni::IsAvailable()) {
      // xpcshell most likely
      *outLeft = *outTop = *outWidth = *outHeight = 0;
      return NS_ERROR_FAILURE;
    }

    java::sdk::Rect::LocalRef rect = java::GeckoAppShell::GetScreenSize();
    *outLeft = rect->Left();
    *outTop = rect->Top();
    *outWidth = rect->Width();
    *outHeight = rect->Height();

    return NS_OK;
}


NS_IMETHODIMP
nsScreenAndroid::GetAvailRect(int32_t *outLeft, int32_t *outTop, int32_t *outWidth, int32_t *outHeight)
{
    return GetRect(outLeft, outTop, outWidth, outHeight);
}



NS_IMETHODIMP
nsScreenAndroid::GetPixelDepth(int32_t *aPixelDepth)
{
    if (!mozilla::jni::IsAvailable()) {
      // xpcshell most likely
      *aPixelDepth = 16;
      return NS_ERROR_FAILURE;
    }

    *aPixelDepth = java::GeckoAppShell::GetScreenDepth();
    return NS_OK;
}


NS_IMETHODIMP
nsScreenAndroid::GetColorDepth(int32_t *aColorDepth)
{
    return GetPixelDepth(aColorDepth);
}

class nsScreenManagerAndroid::ScreenManagerHelperSupport final
    : public ScreenManagerHelper::Natives<ScreenManagerHelperSupport>
{
public:
    typedef ScreenManagerHelper::Natives<ScreenManagerHelperSupport> Base;

    static int32_t AddDisplay(int32_t aDisplayType, int32_t aWidth, int32_t aHeight, float aDensity) {
        int32_t screenId = -1; // return value
        nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
        SyncRunnable::DispatchToThread(mainThread, NS_NewRunnableFunction(
            "nsScreenManagerAndroid::ScreenManagerHelperSupport::AddDisplay",
            [&aDisplayType, &aWidth, &aHeight, &aDensity, &screenId] {
                MOZ_ASSERT(NS_IsMainThread());
                nsCOMPtr<nsIScreenManager> screenMgr =
                do_GetService("@mozilla.org/gfx/screenmanager;1");
                MOZ_ASSERT(screenMgr, "Failed to get nsIScreenManager");

                RefPtr<nsScreenManagerAndroid> screenMgrAndroid =
                (nsScreenManagerAndroid*) screenMgr.get();
                RefPtr<nsScreenAndroid> screen =
                screenMgrAndroid->AddScreen(static_cast<DisplayType>(aDisplayType),
                                            nsIntRect(0, 0, aWidth, aHeight));
                MOZ_ASSERT(screen);
                screen->SetDensity(aDensity);
                screenId = static_cast<int32_t>(screen->GetId());
            }).take());
        return screenId;
    }

    static void RemoveDisplay(int32_t aScreenId) {
        nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
        SyncRunnable::DispatchToThread(mainThread, NS_NewRunnableFunction(
            "nsScreenManagerAndroid::ScreenManagerHelperSupport::RemoveDisplay",
            [&aScreenId] {
                MOZ_ASSERT(NS_IsMainThread());
                nsCOMPtr<nsIScreenManager> screenMgr =
                    do_GetService("@mozilla.org/gfx/screenmanager;1");
                MOZ_ASSERT(screenMgr, "Failed to get nsIScreenManager");

                RefPtr<nsScreenManagerAndroid> screenMgrAndroid =
                    (nsScreenManagerAndroid*) screenMgr.get();
                screenMgrAndroid->RemoveScreen(aScreenId);
            }).take());
    }
};

NS_IMPL_ISUPPORTS(nsScreenManagerAndroid, nsIScreenManager)

nsScreenManagerAndroid::nsScreenManagerAndroid()
{
    if (mozilla::jni::IsAvailable()) {
        ScreenManagerHelperSupport::Base::Init();
    }
    nsCOMPtr<nsIScreen> screen = AddScreen(DisplayType::DISPLAY_PRIMARY);
    MOZ_ASSERT(screen);
}

nsScreenManagerAndroid::~nsScreenManagerAndroid()
{
}

NS_IMETHODIMP
nsScreenManagerAndroid::GetPrimaryScreen(nsIScreen **outScreen)
{
    RefPtr<nsScreenAndroid> screen = ScreenForId(PRIMARY_SCREEN_ID);
    if (screen) {
        screen.forget(outScreen);
    }
    return NS_OK;
}

already_AddRefed<nsScreenAndroid>
nsScreenManagerAndroid::ScreenForId(uint32_t aId)
{
    for (size_t i = 0; i < mScreens.Length(); ++i) {
        if (aId == mScreens[i]->GetId()) {
            RefPtr<nsScreenAndroid> screen = mScreens[i];
            return screen.forget();
        }
    }

    return nullptr;
}

NS_IMETHODIMP
nsScreenManagerAndroid::ScreenForRect(int32_t inLeft,
                                      int32_t inTop,
                                      int32_t inWidth,
                                      int32_t inHeight,
                                      nsIScreen **outScreen)
{
    // Not support to query non-primary screen with rect.
    return GetPrimaryScreen(outScreen);
}

already_AddRefed<nsScreenAndroid>
nsScreenManagerAndroid::AddScreen(DisplayType aDisplayType, nsIntRect aRect)
{
    ALOG("nsScreenManagerAndroid: add %s screen",
        (aDisplayType == DisplayType::DISPLAY_PRIMARY  ? "PRIMARY"  :
        (aDisplayType == DisplayType::DISPLAY_EXTERNAL ? "EXTERNAL" :
                                                         "VIRTUAL")));
    RefPtr<nsScreenAndroid> screen = new nsScreenAndroid(aDisplayType, aRect);
    mScreens.AppendElement(screen);
    return screen.forget();
}

void
nsScreenManagerAndroid::RemoveScreen(uint32_t aScreenId)
{
    for (size_t i = 0; i < mScreens.Length(); i++) {
        if (aScreenId == mScreens[i]->GetId()) {
            mScreens.RemoveElementAt(i);
        }
    }
}
