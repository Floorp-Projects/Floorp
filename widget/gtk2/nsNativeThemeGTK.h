/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>  (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
