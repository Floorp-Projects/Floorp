/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIWidgetListener.h"

#include "nsRegion.h"
#include "nsView.h"
#include "nsIPresShell.h"
#include "nsIWidget.h"
#include "nsIXULWindow.h"

#include "mozilla/BasicEvents.h"

using namespace mozilla;

nsIXULWindow*
nsIWidgetListener::GetXULWindow()
{
  return nullptr;
}

nsView*
nsIWidgetListener::GetView()
{
  return nullptr;
}

nsIPresShell*
nsIWidgetListener::GetPresShell()
{
  return nullptr;
}

bool
nsIWidgetListener::WindowMoved(nsIWidget* aWidget,
                               int32_t aX,
                               int32_t aY)
{
  return false;
}

bool
nsIWidgetListener::WindowResized(nsIWidget* aWidget,
                                 int32_t aWidth,
                                 int32_t aHeight)
{
  return false;
}

void
nsIWidgetListener::SizeModeChanged(nsSizeMode aSizeMode)
{
}

void
nsIWidgetListener::FullscreenChanged(bool aInFullscreen)
{
}

bool
nsIWidgetListener::ZLevelChanged(bool aImmediate,
                                 nsWindowZ* aPlacement,
                                 nsIWidget* aRequestBelow,
                                 nsIWidget** aActualBelow)
{
  return false;
}

void
nsIWidgetListener::WindowActivated()
{
}

void
nsIWidgetListener::WindowDeactivated()
{
}

void
nsIWidgetListener::OSToolbarButtonPressed()
{
}

bool
nsIWidgetListener::RequestWindowClose(nsIWidget* aWidget)
{
  return false;
}

void
nsIWidgetListener::WillPaintWindow(nsIWidget* aWidget)
{
}

bool
nsIWidgetListener::PaintWindow(nsIWidget* aWidget,
                               LayoutDeviceIntRegion aRegion)
{
  return false;
}

void
nsIWidgetListener::DidPaintWindow()
{
}

void
nsIWidgetListener::DidCompositeWindow(const TimeStamp& aCompositeStart,
                                      const TimeStamp& aCompositeEnd)
{
}

void
nsIWidgetListener::RequestRepaint()
{
}

nsEventStatus
nsIWidgetListener::HandleEvent(WidgetGUIEvent* aEvent,
                               bool aUseAttachedEvents)
{
  return nsEventStatus_eIgnore;
}
