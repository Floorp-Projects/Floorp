/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _GTK_NSNATIVETHEMEGTK_H_
#define _GTK_NSNATIVETHEMEGTK_H_

#include "nsITheme.h"
#include "nsCOMPtr.h"
#include "nsAtom.h"
#include "nsIObserver.h"
#include "nsNativeTheme.h"
#include "nsStyleConsts.h"

#include <gtk/gtk.h>
#include "gtkdrawing.h"

class nsNativeThemeGTK final : private nsNativeTheme,
                               public nsITheme,
                               public nsIObserver {
public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIOBSERVER

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(gfxContext* aContext,
                                  nsIFrame* aFrame, WidgetType aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aDirtyRect) override;

  bool CreateWebRenderCommandsForWidget(mozilla::wr::DisplayListBuilder& aBuilder,
                                        mozilla::wr::IpcResourceUpdateQueue& aResources,
                                        const mozilla::layers::StackingContextHelper& aSc,
                                        mozilla::layers::WebRenderLayerManager* aManager,
                                        nsIFrame* aFrame,
                                        WidgetType aWidgetType,
                                        const nsRect& aRect) override;

  MOZ_MUST_USE LayoutDeviceIntMargin GetWidgetBorder(nsDeviceContext* aContext,
                                                     nsIFrame* aFrame,
                                                     WidgetType aWidgetType) override;

  bool GetWidgetPadding(nsDeviceContext* aContext,
                        nsIFrame* aFrame,
                        WidgetType aWidgetType,
                        LayoutDeviceIntMargin* aResult) override;

  virtual bool GetWidgetOverflow(nsDeviceContext* aContext,
                                 nsIFrame* aFrame,
                                 WidgetType aWidgetType,
                                 nsRect* aOverflowRect) override;

  NS_IMETHOD GetMinimumWidgetSize(nsPresContext* aPresContext,
                                  nsIFrame* aFrame, WidgetType aWidgetType,
                                  mozilla::LayoutDeviceIntSize* aResult,
                                  bool* aIsOverridable) override;

  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, WidgetType aWidgetType,
                                nsAtom* aAttribute,
                                bool* aShouldRepaint,
                                const nsAttrValue* aOldValue) override;

  NS_IMETHOD ThemeChanged() override;

  NS_IMETHOD_(bool) ThemeSupportsWidget(nsPresContext* aPresContext,
                                        nsIFrame* aFrame,
                                        WidgetType aWidgetType) override;

  NS_IMETHOD_(bool) WidgetIsContainer(WidgetType aWidgetType) override;

  NS_IMETHOD_(bool) ThemeDrawsFocusForWidget(WidgetType aWidgetType) override;

  virtual bool ThemeNeedsComboboxDropmarker() override;

  virtual Transparency GetWidgetTransparency(nsIFrame* aFrame,
                                             WidgetType aWidgetType) override;

  virtual bool WidgetAppearanceDependsOnWindowFocus(WidgetType aWidgetType) override;

  nsNativeThemeGTK();

protected:
  virtual ~nsNativeThemeGTK();

private:
  GtkTextDirection GetTextDirection(nsIFrame* aFrame);
  gint GetTabMarginPixels(nsIFrame* aFrame);
  bool GetGtkWidgetAndState(WidgetType aWidgetType, nsIFrame* aFrame,
                            WidgetNodeType& aGtkWidgetType,
                            GtkWidgetState* aState, gint* aWidgetFlags);
  bool GetExtraSizeForWidget(nsIFrame* aFrame, WidgetType aWidgetType,
                               nsIntMargin* aExtra);

  void RefreshWidgetWindow(nsIFrame* aFrame);
  WidgetNodeType NativeThemeToGtkTheme(WidgetType aWidgetType, nsIFrame* aFrame);

  uint8_t mDisabledWidgetTypes[(static_cast<size_t>(mozilla::StyleAppearance::Count) + 7) / 8];
  uint8_t mSafeWidgetStates[static_cast<size_t>(mozilla::StyleAppearance::Count) * 4]; // 32 bits per widget
  static const char* sDisabledEngines[];

  // Because moz_gtk_get_widget_border can be slow, we cache its results
  // by widget type.  Each bit in mBorderCacheValid says whether the
  // corresponding entry in mBorderCache is valid.
  void GetCachedWidgetBorder(nsIFrame* aFrame, WidgetType aWidgetType,
                             GtkTextDirection aDirection,
                             LayoutDeviceIntMargin* aResult);
  uint8_t mBorderCacheValid[(MOZ_GTK_WIDGET_NODE_COUNT + 7) / 8];
  LayoutDeviceIntMargin mBorderCache[MOZ_GTK_WIDGET_NODE_COUNT];
};

#endif
