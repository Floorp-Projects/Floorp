/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __nsXPLookAndFeel
#define __nsXPLookAndFeel

#include "nsILookAndFeel.h"
#include "nsCOMPtr.h"

class nsIPref;

#ifdef NS_DEBUG
struct nsSize;
#endif

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

#define CACHE_BLOCK(x)     ((x) >> 5)
#define CACHE_BIT(x)       (1 << ((x) & 31))

#define COLOR_CACHE_SIZE   (CACHE_BLOCK(nsILookAndFeel::eColor_LAST_COLOR) + 1)
#define IS_COLOR_CACHED(x) (CACHE_BIT(x) & nsXPLookAndFeel::sCachedColorBits[CACHE_BLOCK(x)])
#define CACHE_COLOR(x, y)  nsXPLookAndFeel::sCachedColors[(x)] = y; \
              nsXPLookAndFeel::sCachedColorBits[CACHE_BLOCK(x)] |= CACHE_BIT(x);

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
  nsresult InitColorFromPref(PRInt32 aIndex, nsIPref* aPrefService);
  virtual nsresult NativeGetColor(const nsColorID aID, nscolor& aColor) = 0;

  static PRBool sInitialized;
  static nsLookAndFeelIntPref sIntPrefs[];
  static nsLookAndFeelFloatPref sFloatPrefs[];
  static char* sColorPrefs[];
  static PRInt32 sCachedColors[nsILookAndFeel::eColor_LAST_COLOR];
  static PRInt32 sCachedColorBits[COLOR_CACHE_SIZE];

  friend int colorPrefChanged(const char* aPref, void* aData);
};

extern nsresult NS_NewXPLookAndFeel(nsILookAndFeel**);

#endif
