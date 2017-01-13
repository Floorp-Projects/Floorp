/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_gtk_InProcessX11CompositorWidgetParent_h
#define widget_gtk_InProcessX11CompositorWidgetParent_h

#include "X11CompositorWidget.h"

class nsWindow;

namespace mozilla {
namespace widget {

class InProcessX11CompositorWidget final : public X11CompositorWidget
{
public:
  InProcessX11CompositorWidget(const CompositorWidgetInitData& aInitData,
                               const layers::CompositorOptions& aOptions,
                               nsWindow* aWindow);

  // CompositorWidgetDelegate

  void ObserveVsync(VsyncObserver* aObserver) override;
};

} // namespace widget
} // namespace mozilla

#endif // widget_gtk_InProcessX11CompositorWidgetParent_h
