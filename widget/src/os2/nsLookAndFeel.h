/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#ifndef  _nsLookAndFeel_h
#define  _nsLookAndFeel_h

#include "nsILookAndFeel.h"
#include "nsCOMPtr.h"

class nsLookAndFeel: public nsILookAndFeel
{
 public:
   nsLookAndFeel();

   NS_DECL_ISUPPORTS

   NS_IMETHOD GetColor( const nsColorID aID, nscolor &aColor);
   NS_IMETHOD GetMetric( const nsMetricID aID, PRInt32 &aMetric);
   NS_IMETHOD GetMetric( const nsMetricFloatID aID, float &aMetric);

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
  nsCOMPtr<nsILookAndFeel> mXPLookAndFeel;
};

#endif
