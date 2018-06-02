/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeThemeAndroid_h_
#define nsNativeThemeAndroid_h_

#include "nsITheme.h"
#include "nsNativeTheme.h"

class nsNativeThemeAndroid final: private nsNativeTheme,
                                  public nsITheme
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(gfxContext* aContext,
                                  nsIFrame* aFrame, uint8_t aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aDirtyRect) override;

  MOZ_MUST_USE LayoutDeviceIntMargin GetWidgetBorder(nsDeviceContext* aContext,
                                                     nsIFrame* aFrame,
                                                     uint8_t aWidgetType) override;

  bool GetWidgetPadding(nsDeviceContext* aContext,
                        nsIFrame* aFrame,
                        uint8_t aWidgetType,
                        LayoutDeviceIntMargin* aResult) override;

  bool GetWidgetOverflow(nsDeviceContext* aContext,
                         nsIFrame* aFrame,
                         uint8_t aWidgetType,
                         nsRect* aOverflowRect) override;

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

  bool ThemeNeedsComboboxDropmarker() override;

  Transparency GetWidgetTransparency(nsIFrame* aFrame,
                                     uint8_t aWidgetType) override;

  nsNativeThemeAndroid() {}

protected:
  virtual ~nsNativeThemeAndroid() {}
};

#endif // nsNativeThemeAndroid_h_
