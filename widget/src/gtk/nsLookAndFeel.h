/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef __nsLookAndFeel
#define __nsLookAndFeel
#include "nsILookAndFeel.h"
#include "nsCOMPtr.h"
#include <gtk/gtk.h>

class nsLookAndFeel: public nsILookAndFeel {
  NS_DECL_ISUPPORTS

public:
  nsLookAndFeel();
  virtual ~nsLookAndFeel();

  NS_IMETHOD GetColor(const nsColorID aID, nscolor &aColor);
  NS_IMETHOD GetMetric(const nsMetricID aID, PRInt32 & aMetric);
  NS_IMETHOD GetMetric(const nsMetricFloatID aID, float & aMetric);

#ifdef NS_DEBUG
  // This method returns the actual (or nearest estimate) 
  // of the Navigator size for a given form control for a given font
  // and font size. This is used in NavQuirks mode to see how closely
  // we match its size
  NS_IMETHOD GetNavSize(const nsMetricNavWidgetID aWidgetID,
                        const nsMetricNavFontID   aFontID, 
                        const PRInt32             aFontSize, 
                        nsSize &aSize);
#endif

protected:
  GtkStyle *mStyle;
  GtkWidget *mWidget;
  nsCOMPtr<nsILookAndFeel> mXPLookAndFeel;

  // Cached colors, we have to create a dummy widget to actually
  // get the style
  static PRBool sHaveInfoColors;
  static nscolor sInfoBackground;
  static nscolor sInfoText;

  static void GetInfoColors();
};

#endif
