/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsITheme.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsNativeTheme.h"
#include <windows.h>

struct nsIntRect;
struct nsIntSize;

class nsNativeThemeWin : private nsNativeTheme,
                         public nsITheme {
public:
  NS_DECL_ISUPPORTS_INHERITED

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(nsRenderingContext* aContext,
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aDirtyRect);

  NS_IMETHOD GetWidgetBorder(nsDeviceContext* aContext, 
                             nsIFrame* aFrame,
                             PRUint8 aWidgetType,
                             nsIntMargin* aResult);

  virtual bool GetWidgetPadding(nsDeviceContext* aContext,
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsIntMargin* aResult);

  virtual bool GetWidgetOverflow(nsDeviceContext* aContext,
                                   nsIFrame* aFrame,
                                   PRUint8 aWidgetType,
                                   nsRect* aOverflowRect);

  NS_IMETHOD GetMinimumWidgetSize(nsRenderingContext* aContext, nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsIntSize* aResult,
                                  bool* aIsOverridable);

  virtual Transparency GetWidgetTransparency(nsIFrame* aFrame, PRUint8 aWidgetType);

  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                nsIAtom* aAttribute, bool* aShouldRepaint);

  NS_IMETHOD ThemeChanged();

  bool ThemeSupportsWidget(nsPresContext* aPresContext, 
                             nsIFrame* aFrame,
                             PRUint8 aWidgetType);

  bool WidgetIsContainer(PRUint8 aWidgetType);

  bool ThemeDrawsFocusForWidget(nsPresContext* aPresContext, nsIFrame* aFrame, PRUint8 aWidgetType);

  bool ThemeNeedsComboboxDropmarker();

  nsNativeThemeWin();
  virtual ~nsNativeThemeWin();

protected:
  HANDLE GetTheme(PRUint8 aWidgetType);
  nsresult GetThemePartAndState(nsIFrame* aFrame, PRUint8 aWidgetType,
                                PRInt32& aPart, PRInt32& aState);
  nsresult ClassicGetThemePartAndState(nsIFrame* aFrame, PRUint8 aWidgetType,
                                   PRInt32& aPart, PRInt32& aState, bool& aFocused);
  nsresult ClassicDrawWidgetBackground(nsRenderingContext* aContext,
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aClipRect);
  nsresult ClassicGetWidgetBorder(nsDeviceContext* aContext, 
                             nsIFrame* aFrame,
                             PRUint8 aWidgetType,
                             nsIntMargin* aResult);

  bool ClassicGetWidgetPadding(nsDeviceContext* aContext,
                            nsIFrame* aFrame,
                            PRUint8 aWidgetType,
                            nsIntMargin* aResult);

  nsresult ClassicGetMinimumWidgetSize(nsRenderingContext* aContext, nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsIntSize* aResult,
                                  bool* aIsOverridable);

  bool ClassicThemeSupportsWidget(nsPresContext* aPresContext, 
                             nsIFrame* aFrame,
                             PRUint8 aWidgetType);

  void DrawCheckedRect(HDC hdc, const RECT& rc, PRInt32 fore, PRInt32 back,
                       HBRUSH defaultBack);

  PRUint32 GetWidgetNativeDrawingFlags(PRUint8 aWidgetType);

  PRInt32 StandardGetState(nsIFrame* aFrame, PRUint8 aWidgetType, bool wantFocused);

  bool IsMenuActive(nsIFrame* aFrame, PRUint8 aWidgetType);
};

// Creator function
extern NS_METHOD NS_NewNativeThemeWin(nsISupports *aOuter, REFNSIID aIID, void **aResult);
