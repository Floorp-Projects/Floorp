/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_HeadlessThemeGTK_h
#define mozilla_widget_HeadlessThemeGTK_h

#include "nsITheme.h"
#include "nsNativeTheme.h"

namespace mozilla {
namespace widget {

class HeadlessThemeGTK final : private nsNativeTheme,
                               public nsITheme {
public:
  NS_DECL_ISUPPORTS_INHERITED

  HeadlessThemeGTK() = default;
  NS_IMETHOD DrawWidgetBackground(gfxContext* aContext,
                                  nsIFrame* aFrame, uint8_t aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aDirtyRect) override;

  NS_IMETHOD GetWidgetBorder(nsDeviceContext* aContext, nsIFrame* aFrame,
                             uint8_t aWidgetType,
                             mozilla::LayoutDeviceIntMargin* aResult) override;

  bool GetWidgetPadding(nsDeviceContext* aContext,
                        nsIFrame* aFrame,
                        uint8_t aWidgetType,
                        mozilla::LayoutDeviceIntMargin* aResult) override;
  NS_IMETHOD GetMinimumWidgetSize(nsPresContext* aPresContext,
                                  nsIFrame* aFrame, uint8_t aWidgetType,
                                  mozilla::LayoutDeviceIntSize* aResult,
                                  bool* aIsOverridable) override;

  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, uint8_t aWidgetType,
                                nsAtom* aAttribute,
                                bool* aShouldRepaint,
                                const nsAttrValue* aOldValue) override;

  NS_IMETHOD ThemeChanged() override;

  NS_IMETHOD_(bool) ThemeSupportsWidget(nsPresContext* aPresContext,
                                        nsIFrame* aFrame,
                                        uint8_t aWidgetType) override;

  NS_IMETHOD_(bool) WidgetIsContainer(uint8_t aWidgetType) override;

  NS_IMETHOD_(bool) ThemeDrawsFocusForWidget(uint8_t aWidgetType) override;

  virtual bool ThemeNeedsComboboxDropmarker() override;

protected:
  virtual ~HeadlessThemeGTK() { }
};

} // namespace widget
} // namespace mozilla

#endif // mozilla_widget_HeadlessThemeGTK_h
