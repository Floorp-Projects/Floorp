/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef __nsXPLookAndFeel
#define __nsXPLookAndFeel

#include "nsILookAndFeel.h"

#include "nsCOMPtr.h"
#include "nsIPref.h"

typedef enum {
  nsLookAndFeelTypeInt,
  nsLookAndFeelTypeFloat,
  nsLookAndFeelTypeColor
} nsLookAndFeelType;

struct nsLookAndFeelIntPref
{
  char* name;
  nsILookAndFeel::nsMetricID id;
  PRPackedBool isSet;
  nsLookAndFeelType type;
  PRInt32 intVar;
};

struct nsLookAndFeelFloatPref
{
  char* name;
  nsILookAndFeel::nsMetricFloatID id;
  PRPackedBool isSet;
  nsLookAndFeelType type;
  float floatVar;
};

struct nsLookAndFeelColorPref
{
  char* name;
  nsILookAndFeel::nsColorID id;
  PRPackedBool isSet;
  nsLookAndFeelType type;
  nscolor colorVar;
};

class nsXPLookAndFeel: public nsILookAndFeel
{
public:
  nsXPLookAndFeel();
  virtual ~nsXPLookAndFeel();

  NS_DECL_ISUPPORTS

  void Init();

  //
  // All these routines will return NS_OK if they have a value,
  // in which case the nsLookAndFeel should use that value;
  // otherwise we'll return NS_ERROR_NOT_AVAILABLE, in which case, the
  // platform-specific nsLookAndFeel should use its own values instead.
  //
  NS_IMETHOD GetColor(const nsColorID aID, nscolor &aColor);
  NS_IMETHOD GetMetric(const nsMetricID aID, PRInt32 & aMetric);
  NS_IMETHOD GetMetric(const nsMetricFloatID aID, float & aMetric);

#ifdef NS_DEBUG
  NS_IMETHOD GetNavSize(const nsMetricNavWidgetID aWidgetID,
                        const nsMetricNavFontID   aFontID, 
                        const PRInt32             aFontSize, 
                        nsSize &aSize);
#endif

protected:
  nsresult InitFromPref(nsLookAndFeelIntPref* aPref, nsIPref* aPrefService);
  nsresult InitFromPref(nsLookAndFeelFloatPref* aPref, nsIPref* aPrefService);
  nsresult InitFromPref(nsLookAndFeelColorPref* aPref, nsIPref* aPrefService);

  static PRBool sInitialized;

  static nsLookAndFeelIntPref sIntPrefs[];
  static nsLookAndFeelFloatPref sFloatPrefs[];
  static nsLookAndFeelColorPref sColorPrefs[];
};

extern nsresult NS_NewXPLookAndFeel(nsILookAndFeel**);

#endif
