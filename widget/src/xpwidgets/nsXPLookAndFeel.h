/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __nsXPLookAndFeel
#define __nsXPLookAndFeel

#include "nsILookAndFeel.h"
#include "nsCOMPtr.h"

#ifdef NS_DEBUG
struct nsSize;
#endif

class nsLookAndFeel;

typedef enum {
  nsLookAndFeelTypeInt,
  nsLookAndFeelTypeFloat,
  nsLookAndFeelTypeColor
} nsLookAndFeelType;

struct nsLookAndFeelIntPref
{
  const char* name;
  nsILookAndFeel::nsMetricID id;
  PRPackedBool isSet;
  nsLookAndFeelType type;
  PRInt32 intVar;
};

struct nsLookAndFeelFloatPref
{
  const char* name;
  nsILookAndFeel::nsMetricFloatID id;
  PRPackedBool isSet;
  nsLookAndFeelType type;
  float floatVar;
};

#define CACHE_BLOCK(x)     ((x) >> 5)
#define CACHE_BIT(x)       (1 << ((x) & 31))

#define COLOR_CACHE_SIZE   (CACHE_BLOCK(nsILookAndFeel::eColor_LAST_COLOR) + 1)
#define IS_COLOR_CACHED(x) (CACHE_BIT(x) & nsXPLookAndFeel::sCachedColorBits[CACHE_BLOCK(x)])
#define CLEAR_COLOR_CACHE(x) nsXPLookAndFeel::sCachedColors[(x)] =0; \
              nsXPLookAndFeel::sCachedColorBits[CACHE_BLOCK(x)] &= ~(CACHE_BIT(x));
#define CACHE_COLOR(x, y)  nsXPLookAndFeel::sCachedColors[(x)] = y; \
              nsXPLookAndFeel::sCachedColorBits[CACHE_BLOCK(x)] |= CACHE_BIT(x);

class nsXPLookAndFeel: public nsILookAndFeel
{
public:
  nsXPLookAndFeel();
  virtual ~nsXPLookAndFeel();

  NS_DECL_ISUPPORTS

  static nsLookAndFeel* GetAddRefedInstance();
  static nsLookAndFeel* GetInstance();
  static void Shutdown();

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

  NS_IMETHOD LookAndFeelChanged();

#ifdef NS_DEBUG
  NS_IMETHOD GetNavSize(const nsMetricNavWidgetID aWidgetID,
                        const nsMetricNavFontID   aFontID, 
                        const PRInt32             aFontSize, 
                        nsSize &aSize);
#endif

protected:
  static void IntPrefChanged(nsLookAndFeelIntPref *data);
  static void FloatPrefChanged(nsLookAndFeelFloatPref *data);
  static void ColorPrefChanged(unsigned int index, const char *prefName);
  void InitFromPref(nsLookAndFeelIntPref* aPref);
  void InitFromPref(nsLookAndFeelFloatPref* aPref);
  void InitColorFromPref(PRInt32 aIndex);
  virtual nsresult NativeGetColor(const nsColorID aID, nscolor& aColor) = 0;
  PRBool IsSpecialColor(const nsColorID aID, nscolor &aColor);

  static int OnPrefChanged(const char* aPref, void* aClosure);

  static PRBool sInitialized;
  static nsLookAndFeelIntPref sIntPrefs[];
  static nsLookAndFeelFloatPref sFloatPrefs[];
  /* this length must not be shorter than the length of the longest string in the array
   * see nsXPLookAndFeel.cpp
   */
  static const char sColorPrefs[][38];
  static PRInt32 sCachedColors[nsILookAndFeel::eColor_LAST_COLOR];
  static PRInt32 sCachedColorBits[COLOR_CACHE_SIZE];
  static PRBool sUseNativeColors;

  static nsXPLookAndFeel* sInstance;
  static PRBool sShutdown;
};

#endif
