/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <QObject>
#ifdef MOZ_ENABLE_QMSYSTEM2
#include "qmdevicemode.h"
#include "qmdisplaystate.h"
#include "qmactivity.h"
#endif
#include "nsNativeAppSupportBase.h"
#include "nsString.h"

#ifdef MOZ_ENABLE_LIBCONIC
#include <glib-object.h>
#endif

#if (MOZ_PLATFORM_MAEMO == 5)
#include <libosso.h>
#endif

class nsNativeAppSupportQt : public QObject, public nsNativeAppSupportBase
{
  Q_OBJECT
public:
  NS_IMETHOD Start(bool* aRetVal);
  NS_IMETHOD Stop(bool* aResult);
#if (MOZ_PLATFORM_MAEMO == 5)
  // Osso context must be initialized for maemo5 otherwise we will be killed in ~20 seconds
  osso_context_t *m_osso_context;
#endif
#ifdef MOZ_ENABLE_QMSYSTEM2
public Q_SLOTS:
  void activityChanged(MeeGo::QmActivity::Activity activity);
  void deviceModeChanged(MeeGo::QmDeviceMode::DeviceMode mode);
  void displayStateChanged(MeeGo::QmDisplayState::DisplayState state);
  void RefreshStates();

private:
  MeeGo::QmDeviceMode mDeviceMode;
  MeeGo::QmDisplayState mDisplayState;
  MeeGo::QmActivity mActivity;
#endif
};

