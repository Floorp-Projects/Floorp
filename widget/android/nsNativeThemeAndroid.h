/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeThemeAndroid_h_
#define nsNativeThemeAndroid_h_

#include "nsITheme.h"
#include "nsNativeTheme.h"

class nsNativeThemeAndroid final : private nsNativeTheme, public nsITheme {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                  StyleAppearance aAppearance,
                                  const nsRect& aRect,
                                  const nsRect& aDirtyRect) override;

  [[nodiscard]] LayoutDeviceIntMargin GetWidgetBorder(
      nsDeviceContext* aContext, nsIFrame* aFrame,
      StyleAppearance aAppearance) override;

  bool GetWidgetPadding(nsDeviceContext* aContext, nsIFrame* aFrame,
                        StyleAppearance aAppearance,
                        LayoutDeviceIntMargin* aResult) override;

  bool GetWidgetOverflow(nsDeviceContext* aContext, nsIFrame* aFrame,
                         StyleAppearance aAppearance,
                         nsRect* aOverflowRect) override;

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

  Transparency GetWidgetTransparency(nsIFrame* aFrame,
                                     StyleAppearance aAppearance) override;

  nsNativeThemeAndroid() {}

 protected:
  virtual ~nsNativeThemeAndroid() {}
};

#endif  // nsNativeThemeAndroid_h_
