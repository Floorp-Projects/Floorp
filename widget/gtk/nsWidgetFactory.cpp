/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWidgetFactory.h"

#include "mozilla/Components.h"
#include "mozilla/WidgetUtils.h"
#include "NativeKeyBindings.h"
#include "nsWidgetsCID.h"
#include "nsAppShell.h"
#include "nsAppShellSingleton.h"
#include "nsBaseWidget.h"
#include "nsGtkKeyUtils.h"
#include "nsLookAndFeel.h"
#include "nsWindow.h"
#include "nsHTMLFormatConverter.h"
#include "HeadlessClipboard.h"
#include "IMContextWrapper.h"
#include "nsClipboard.h"
#include "TaskbarProgress.h"
#include "nsFilePicker.h"
#include "nsSound.h"
#include "nsGTKToolkit.h"
#include "WakeLockListener.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/widget/ScreenManager.h"
#include <gtk/gtk.h>

using namespace mozilla;
using namespace mozilla::widget;

NS_IMPL_COMPONENT_FACTORY(nsIClipboard) {
  nsCOMPtr<nsIClipboard> inst;
  if (gfxPlatform::IsHeadless()) {
    inst = new HeadlessClipboard();
  } else {
    auto clipboard = MakeRefPtr<nsClipboard>();
    if (NS_FAILED(clipboard->Init())) {
      return nullptr;
    }
    inst = std::move(clipboard);
  }

  return inst.forget().downcast<nsISupports>();
}

nsresult nsWidgetGtk2ModuleCtor() { return nsAppShellInit(); }

void nsWidgetGtk2ModuleDtor() {
  // Shutdown all XP level widget classes.
  WidgetUtils::Shutdown();

  NativeKeyBindings::Shutdown();
  nsLookAndFeel::Shutdown();
  nsFilePicker::Shutdown();
  nsSound::Shutdown();
  nsWindow::ReleaseGlobals();
  IMContextWrapper::Shutdown();
  KeymapWrapper::Shutdown();
  nsGTKToolkit::Shutdown();
  nsAppShellShutdown();
  WakeLockListener::Shutdown();
}
