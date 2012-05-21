/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
                                  nsIFrame* aFrame, PRUint8 aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aDirtyRect);

  NS_IMETHOD GetWidgetBorder(nsDeviceContext* aContext, nsIFrame* aFrame,
                             PRUint8 aWidgetType, nsIntMargin* aResult);

  virtual NS_HIDDEN_(bool) GetWidgetPadding(nsDeviceContext* aContext,
                                              nsIFrame* aFrame,
                                              PRUint8 aWidgetType,
                                              nsIntMargin* aResult);

  virtual NS_HIDDEN_(bool) GetWidgetOverflow(nsDeviceContext* aContext,
                                               nsIFrame* aFrame,
                                               PRUint8 aWidgetType,
                                               nsRect* aOverflowRect);

  NS_IMETHOD GetMinimumWidgetSize(nsRenderingContext* aContext,
                                  nsIFrame* aFrame, PRUint8 aWidgetType,
                                  nsIntSize* aResult, bool* aIsOverridable);

  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                nsIAtom* aAttribute, bool* aShouldRepaint);

  NS_IMETHOD ThemeChanged();

  NS_IMETHOD_(bool) ThemeSupportsWidget(nsPresContext* aPresContext,
                                          nsIFrame* aFrame,
                                          PRUint8 aWidgetType);

  NS_IMETHOD_(bool) WidgetIsContainer(PRUint8 aWidgetType);
  
  NS_IMETHOD_(bool) ThemeDrawsFocusForWidget(nsPresContext* aPresContext,
                                               nsIFrame* aFrame, PRUint8 aWidgetType);

  bool ThemeNeedsComboboxDropmarker();

  virtual Transparency GetWidgetTransparency(nsIFrame* aFrame,
                                             PRUint8 aWidgetType);

  nsNativeThemeGTK();
  virtual ~nsNativeThemeGTK();

private:
  gint GetTabMarginPixels(nsIFrame* aFrame);
  bool GetGtkWidgetAndState(PRUint8 aWidgetType, nsIFrame* aFrame,
                              GtkThemeWidgetType& aGtkWidgetType,
                              GtkWidgetState* aState, gint* aWidgetFlags);
  bool GetExtraSizeForWidget(nsIFrame* aFrame, PRUint8 aWidgetType,
                               nsIntMargin* aExtra);

  void RefreshWidgetWindow(nsIFrame* aFrame);

  PRUint8 mDisabledWidgetTypes[32];
  PRUint8 mSafeWidgetStates[1024];    // 256 widgets * 32 bits per widget
  static const char* sDisabledEngines[];
};
