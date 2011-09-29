/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Oleg Romashin <romaxa@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

