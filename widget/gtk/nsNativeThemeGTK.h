/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _GTK_NSNATIVETHEMEGTK_H_
#define _GTK_NSNATIVETHEMEGTK_H_

#include "nsITheme.h"
#include "nsCOMPtr.h"
#include "nsAtom.h"
#include "nsNativeTheme.h"
#include "nsStyleConsts.h"
#include "nsNativeBasicTheme.h"
#include "ScrollbarDrawingGTK.h"

#include <gtk/gtk.h>
#include "gtkdrawing.h"

class nsNativeThemeGTK final : public nsNativeBasicTheme {
 public:
  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                  StyleAppearance aAppearance,
                                  const nsRect& aRect, const nsRect& aDirtyRect,
                                  DrawOverflow) override;

  bool CreateWebRenderCommandsForWidget(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const mozilla::layers::StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager, nsIFrame*,
      StyleAppearance, const nsRect& aRect) override;

  [[nodiscard]] LayoutDeviceIntMargin GetWidgetBorder(
      nsDeviceContext* aContext, nsIFrame* aFrame,
      StyleAppearance aAppearance) override;

  bool GetWidgetPadding(nsDeviceContext* aContext, nsIFrame* aFrame,
                        StyleAppearance aAppearance,
                        LayoutDeviceIntMargin* aResult) override;

  bool GetWidgetOverflow(nsDeviceContext* aContext, nsIFrame* aFrame,
                         StyleAppearance aAppearance,
                         nsRect* aOverflowRect) override;

  // Whether we draw a non-native widget.
  //
  // We always draw scrollbars as non-native so that all of Firefox has
  // consistent scrollbar styles both in chrome and content (plus, the
  // non-native scrollbars support scrollbar-width, auto-darkening...).
  //
  // We draw other widgets as non-native when their color-scheme doesn't match
  // the current GTK theme's color-scheme. We do that because frequently
  // switching GTK themes at runtime is prohibitively expensive. In that case
  // (`BecauseColorMismatch`) we don't call into the non-native theme for sizing
  // information (GetWidgetPadding/Border and GetMinimumWidgetSize), to avoid
  // subtle sizing changes. The non-native theme can basically draw at any size,
  // so we prefer to have consistent sizing information.
  enum class NonNative { No, Always, BecauseColorMismatch };
  NonNative IsWidgetNonNative(nsIFrame*, StyleAppearance);

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

  bool ThemeDrawsFocusForWidget(nsIFrame*, StyleAppearance) override;

  bool ThemeNeedsComboboxDropmarker() override;
  Transparency GetWidgetTransparency(nsIFrame*, StyleAppearance) override;
  ScrollbarSizes GetScrollbarSizes(nsPresContext*, StyleScrollbarWidth,
                                   Overlay) override;

  explicit nsNativeThemeGTK(
      mozilla::UniquePtr<ScrollbarDrawing>&& aScrollbarDrawingGTK);

 protected:
  virtual ~nsNativeThemeGTK();

 private:
  GtkTextDirection GetTextDirection(nsIFrame* aFrame);
  gint GetTabMarginPixels(nsIFrame* aFrame);
  bool GetGtkWidgetAndState(StyleAppearance aAppearance, nsIFrame* aFrame,
                            WidgetNodeType& aGtkWidgetType,
                            GtkWidgetState* aState, gint* aWidgetFlags);
  bool GetExtraSizeForWidget(nsIFrame* aFrame, StyleAppearance aAppearance,
                             nsIntMargin* aExtra);
  bool IsWidgetVisible(StyleAppearance aAppearance);

  void RefreshWidgetWindow(nsIFrame* aFrame);
  WidgetNodeType NativeThemeToGtkTheme(StyleAppearance aAppearance,
                                       nsIFrame* aFrame);

  uint8_t mDisabledWidgetTypes
      [(static_cast<size_t>(mozilla::StyleAppearance::Count) + 7) / 8];
  uint8_t
      mSafeWidgetStates[static_cast<size_t>(mozilla::StyleAppearance::Count) *
                        4];  // 32 bits per widget
  static const char* sDisabledEngines[];

  // Because moz_gtk_get_widget_border can be slow, we cache its results
  // by widget type.  Each bit in mBorderCacheValid says whether the
  // corresponding entry in mBorderCache is valid.
  void GetCachedWidgetBorder(nsIFrame* aFrame, StyleAppearance aAppearance,
                             GtkTextDirection aDirection,
                             LayoutDeviceIntMargin* aResult);
  uint8_t mBorderCacheValid[(MOZ_GTK_WIDGET_NODE_COUNT + 7) / 8];
  LayoutDeviceIntMargin mBorderCache[MOZ_GTK_WIDGET_NODE_COUNT];
};

#endif
