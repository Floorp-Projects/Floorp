/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/WidgetUtils.h"

#include "nsAppShell.h"

#include "nsLookAndFeel.h"
#include "nsAppShellSingleton.h"

nsresult nsWidgetAndroidModuleCtor() { return nsAppShellInit(); }

void nsWidgetAndroidModuleDtor() {
  // Shutdown all XP level widget classes.
  mozilla::widget::WidgetUtils::Shutdown();

  nsLookAndFeel::Shutdown();
  nsAppShellShutdown();
}
