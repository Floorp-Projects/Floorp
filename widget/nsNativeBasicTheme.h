/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeBasicTheme_h
#define nsNativeBasicTheme_h

#include "nsITheme.h"
#include "nsNativeTheme.h"

class nsNativeBasicTheme : private nsNativeTheme, public nsITheme {
 public:
  nsNativeBasicTheme() = default;

  NS_DECL_ISUPPORTS_INHERITED

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                  StyleAppearance aAppearance,
                                  const nsRect& aRect,
                                  const nsRect& aDirtyRect) override;
  /*bool CreateWebRenderCommandsForWidget(mozilla::wr::DisplayListBuilder&
     aBuilder, mozilla::wr::IpcResourceUpdateQueue& aResources, const
     mozilla::layers::StackingContextHelper& aSc,
                                        mozilla::layers::RenderRootStateManager*
     aManager, nsIFrame* aFrame, StyleAppearance aAppearance, const nsRect&
     aRect) override;*/
  [[nodiscard]] LayoutDeviceIntMargin GetWidgetBorder(
      nsDeviceContext* aContext, nsIFrame* aFrame,
      StyleAppearance aAppearance) override;
  bool GetWidgetPadding(nsDeviceContext* aContext, nsIFrame* aFrame,
                        StyleAppearance aAppearance,
                        LayoutDeviceIntMargin* aResult) override;
  virtual bool GetWidgetOverflow(nsDeviceContext* aContext, nsIFrame* aFrame,
                                 StyleAppearance aAppearance,
                                 nsRect* aOverflowRect) override;
  NS_IMETHOD GetMinimumWidgetSize(nsPresContext* aPresContext, nsIFrame* aFrame,
                                  StyleAppearance aAppearance,
                                  mozilla::LayoutDeviceIntSize* aResult,
                                  bool* aIsOverridable) override;
  virtual Transparency GetWidgetTransparency(
      nsIFrame* aFrame, StyleAppearance aAppearance) override;
  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, StyleAppearance aAppearance,
                                nsAtom* aAttribute, bool* aShouldRepaint,
                                const nsAttrValue* aOldValue) override;
  NS_IMETHOD ThemeChanged() override;
  virtual bool WidgetAppearanceDependsOnWindowFocus(
      StyleAppearance aAppearance) override;
  /*virtual bool NeedToClearBackgroundBehindWidget(nsIFrame* aFrame,
                                                 StyleAppearance aAppearance)
     override;*/
  virtual ThemeGeometryType ThemeGeometryTypeForWidget(
      nsIFrame* aFrame, StyleAppearance aAppearance) override;
  bool ThemeSupportsWidget(nsPresContext* aPresContext, nsIFrame* aFrame,
                           StyleAppearance aAppearance) override;
  bool WidgetIsContainer(StyleAppearance aAppearance) override;
  bool ThemeDrawsFocusForWidget(StyleAppearance aAppearance) override;
  bool ThemeNeedsComboboxDropmarker() override;

 protected:
  virtual ~nsNativeBasicTheme() = default;
};

#endif
