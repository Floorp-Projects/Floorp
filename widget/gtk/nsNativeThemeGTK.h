/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _GTK_NSNATIVETHEMEGTK_H_
#define _GTK_NSNATIVETHEMEGTK_H_

#include "nsITheme.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsIObserver.h"
#include "nsNativeTheme.h"

#include <gtk/gtk.h>
#include "gtkdrawing.h"

class nsNativeThemeGTK: private nsNativeTheme,
                        public nsITheme,
                        public nsIObserver {
public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIOBSERVER

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(nsRenderingContext* aContext,
                                  nsIFrame* aFrame, uint8_t aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aDirtyRect) override;

  NS_IMETHOD GetWidgetBorder(nsDeviceContext* aContext, nsIFrame* aFrame,
                             uint8_t aWidgetType,
                             nsIntMargin* aResult) override;

  virtual bool GetWidgetPadding(nsDeviceContext* aContext,
                                nsIFrame* aFrame,
                                uint8_t aWidgetType,
                                nsIntMargin* aResult) override;

  virtual bool GetWidgetOverflow(nsDeviceContext* aContext,
                                 nsIFrame* aFrame,
                                 uint8_t aWidgetType,
                                 nsRect* aOverflowRect) override;

  NS_IMETHOD GetMinimumWidgetSize(nsPresContext* aPresContext,
                                  nsIFrame* aFrame, uint8_t aWidgetType,
                                  mozilla::LayoutDeviceIntSize* aResult,
                                  bool* aIsOverridable) override;

  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, uint8_t aWidgetType, 
                                nsIAtom* aAttribute,
                                bool* aShouldRepaint,
                                const nsAttrValue* aOldValue) override;

  NS_IMETHOD ThemeChanged() override;

  NS_IMETHOD_(bool) ThemeSupportsWidget(nsPresContext* aPresContext,
                                        nsIFrame* aFrame,
                                        uint8_t aWidgetType) override;

  NS_IMETHOD_(bool) WidgetIsContainer(uint8_t aWidgetType) override;
  
  NS_IMETHOD_(bool) ThemeDrawsFocusForWidget(uint8_t aWidgetType) override;

  virtual bool ThemeNeedsComboboxDropmarker() override;

  virtual Transparency GetWidgetTransparency(nsIFrame* aFrame,
                                             uint8_t aWidgetType) override;

  nsNativeThemeGTK();

protected:
  virtual ~nsNativeThemeGTK();

private:
  gint GetTabMarginPixels(nsIFrame* aFrame);
  bool GetGtkWidgetAndState(uint8_t aWidgetType, nsIFrame* aFrame,
                            WidgetNodeType& aGtkWidgetType,
                            GtkWidgetState* aState, gint* aWidgetFlags);
  bool GetExtraSizeForWidget(nsIFrame* aFrame, uint8_t aWidgetType,
                               nsIntMargin* aExtra);

  void RefreshWidgetWindow(nsIFrame* aFrame);
  WidgetNodeType NativeThemeToGtkTheme(uint8_t aWidgetType, nsIFrame* aFrame);

  uint8_t mDisabledWidgetTypes[32];
  uint8_t mSafeWidgetStates[1024];    // 256 widgets * 32 bits per widget
  static const char* sDisabledEngines[];
};

#endif
