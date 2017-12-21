/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_gtk_InProcessGtkCompositorWidgetParent_h
#define widget_gtk_InProcessGtkCompositorWidgetParent_h

#include "GtkCompositorWidget.h"

class nsWindow;

namespace mozilla {
namespace widget {

class InProcessGtkCompositorWidget final : public GtkCompositorWidget
{
public:
  InProcessGtkCompositorWidget(const GtkCompositorWidgetInitData& aInitData,
                               const layers::CompositorOptions& aOptions,
                               nsWindow* aWindow);

  // CompositorWidgetDelegate

  void ObserveVsync(VsyncObserver* aObserver) override;
};

} // namespace widget
} // namespace mozilla

#endif // widget_gtk_InProcessGtkCompositorWidgetParent_h
