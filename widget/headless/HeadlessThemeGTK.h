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

class HeadlessThemeGTK final : private nsNativeTheme, public nsITheme {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  HeadlessThemeGTK() = default;
  NS_IMETHOD DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                  StyleAppearance aAppearance,
                                  const nsRect& aRect, const nsRect& aDirtyRect,
                                  DrawOverflow) override;

  [[nodiscard]] LayoutDeviceIntMargin GetWidgetBorder(
      nsDeviceContext* aContext, nsIFrame* aFrame,
      StyleAppearance aAppearance) override;

  bool GetWidgetPadding(nsDeviceContext* aContext, nsIFrame* aFrame,
                        StyleAppearance aAppearance,
                        LayoutDeviceIntMargin* aResult) override;
  NS_IMETHOD GetMinimumWidgetSize(nsPresContext* aPresContext, nsIFrame* aFrame,
                                  StyleAppearance aAppearance,
                                  mozilla::LayoutDeviceIntSize* aResult,
                                  bool* aIsOverridable) override;

  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, StyleAppearance aAppearance,
                                nsAtom* aAttribute, bool* aShouldRepaint,
                                const nsAttrValue* aOldValue) override;

  NS_IMETHOD ThemeChanged() override;

  NS_IMETHOD_(bool)
  ThemeSupportsWidget(nsPresContext* aPresContext, nsIFrame* aFrame,
                      StyleAppearance aAppearance) override;

  NS_IMETHOD_(bool) WidgetIsContainer(StyleAppearance aAppearance) override;

  NS_IMETHOD_(bool)
  ThemeDrawsFocusForWidget(StyleAppearance aAppearance) override;

  bool ThemeNeedsComboboxDropmarker() override;

  ScrollbarSizes GetScrollbarSizes(nsPresContext*, StyleScrollbarWidth,
                                   Overlay) override;

 protected:
  virtual ~HeadlessThemeGTK() = default;
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_HeadlessThemeGTK_h
