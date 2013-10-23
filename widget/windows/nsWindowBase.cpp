/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWindowBase.h"

#include "mozilla/MiscEvents.h"
#include "npapi.h"

using namespace mozilla;

bool
nsWindowBase::DispatchPluginEvent(const MSG& aMsg)
{
  if (!PluginHasFocus()) {
    return false;
  }
  WidgetPluginEvent pluginEvent(true, NS_PLUGIN_INPUT_EVENT, this);
  nsIntPoint point(0, 0);
  InitEvent(pluginEvent, &point);
  NPEvent npEvent;
  npEvent.event = aMsg.message;
  npEvent.wParam = aMsg.wParam;
  npEvent.lParam = aMsg.lParam;
  pluginEvent.pluginEvent = &npEvent;
  pluginEvent.retargetToFocusedDocument = true;
  return DispatchWindowEvent(&pluginEvent);
}
