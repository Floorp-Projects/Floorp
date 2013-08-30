/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <QTimer>
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "nsNativeAppSupportQt.h"
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"

#ifdef MOZ_ENABLE_QMSYSTEM2
void
nsNativeAppSupportQt::activityChanged(MeeGo::QmActivity::Activity activity)
{
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (!os)
        return;

    if (MeeGo::QmActivity::Inactive == activity) {
        os->NotifyObservers(nullptr, "system-idle", nullptr);
    } else {
        os->NotifyObservers(nullptr, "system-active", nullptr);
    }
}

void
nsNativeAppSupportQt::displayStateChanged(MeeGo::QmDisplayState::DisplayState state)
{
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (!os)
        return;

    switch (state) {
    case MeeGo::QmDisplayState::On:
        os->NotifyObservers(nullptr, "system-display-on", nullptr);
        break;
    case MeeGo::QmDisplayState::Off:
        os->NotifyObservers(nullptr, "system-display-off", nullptr);
        break;
    case MeeGo::QmDisplayState::Dimmed:
        os->NotifyObservers(nullptr, "system-display-dimmed", nullptr);
        break;
    default:
        NS_WARNING("Unknown display state");
        break;
    }
}

void nsNativeAppSupportQt::deviceModeChanged(MeeGo::QmDeviceMode::DeviceMode mode)
{
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (!os)
        return;

    switch (mode) {
    case MeeGo::QmDeviceMode::DeviceMode::Normal:
        os->NotifyObservers(nullptr, "profile-change-net-restore", nullptr);
        break;
    case MeeGo::QmDeviceMode::DeviceMode::Flight:
        os->NotifyObservers(nullptr, "profile-change-net-teardown", nullptr);
        break;
    case MeeGo::QmDeviceMode::DeviceMode::Error:
    default:
        NS_WARNING("Unknown DeviceMode");
        break;
    }
}

void nsNativeAppSupportQt::RefreshStates()
{
  activityChanged(mActivity.get());
  displayStateChanged(mDisplayState.get());
  deviceModeChanged(mDeviceMode.getMode());
}
#endif

NS_IMETHODIMP
nsNativeAppSupportQt::Start(bool* aRetVal)
{
  NS_ASSERTION(gAppData, "gAppData must not be null.");

#ifdef MOZ_ENABLE_QMSYSTEM2
  connect(&mActivity, SIGNAL(activityChanged(MeeGo::QmActivity::Activity)), this, SLOT(activityChanged(MeeGo::QmActivity::Activity)));
  connect(&mDeviceMode, SIGNAL(deviceModeChanged(MeeGo::QmDeviceMode::DeviceMode)), this, SLOT(deviceModeChanged(MeeGo::QmDeviceMode::DeviceMode)));
  connect(&mDisplayState, SIGNAL(displayStateChanged(MeeGo::QmDisplayState::DisplayState)), this, SLOT(displayStateChanged(MeeGo::QmDisplayState::DisplayState)));
  // Init states withing next event loop iteration
  QTimer::singleShot(0, this, SLOT(RefreshStates()));
#endif

  *aRetVal = true;
  return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportQt::Stop(bool* aResult)
{
  NS_ENSURE_ARG(aResult);
  *aResult = true;

  return NS_OK;
}

nsresult
NS_CreateNativeAppSupport(nsINativeAppSupport** aResult)
{
  nsNativeAppSupportBase* native = new nsNativeAppSupportQt();
  if (!native)
    return NS_ERROR_OUT_OF_MEMORY;

  *aResult = native;
  NS_ADDREF(*aResult);

  return NS_OK;
}
