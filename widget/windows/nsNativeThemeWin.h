/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tim Hill (tim@prismelite.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
