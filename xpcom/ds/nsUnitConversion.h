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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#ifndef nsUnitConversion_h__
#define nsUnitConversion_h__

#include "nscore.h"
#include "nsCoord.h"
#include <math.h>
#include <float.h>

/// handy constants
#define TWIPS_PER_POINT_INT           20
#define TWIPS_PER_POINT_FLOAT         20.0f
#define ROUND_CONST_FLOAT             0.5f

/*
 * Coord Rounding Functions
 */
inline nscoord NSToCoordFloor(float aValue)
{
  return nscoord(floor(aValue));
}

inline nscoord NSToCoordCeil(float aValue)
{
  return nscoord(ceil(aValue));
}

inline nscoord NSToCoordRound(float aValue)
{
#ifdef NS_COORD_IS_FLOAT
  return float(floor(aValue + ROUND_CONST_FLOAT));
#else
  return ((0.0f <= aValue) ? nscoord(aValue + ROUND_CONST_FLOAT) : nscoord(aValue - ROUND_CONST_FLOAT));
#endif
}

/*
 * Int Rounding Functions
 */
inline PRInt32 NSToIntFloor(float aValue)
{
  return PRInt32(floor(aValue));
}

inline PRInt32 NSToIntCeil(float aValue)
{
  return PRInt32(ceil(aValue));
}

inline PRInt32 NSToIntRound(float aValue)
{
  return ((0.0f <= aValue) ? PRInt32(aValue + ROUND_CONST_FLOAT) : PRInt32(aValue - ROUND_CONST_FLOAT));
}

/* 
 * App Unit/Pixel conversions
 */
inline nscoord NSFloatPixelsToAppUnits(float aPixels, PRInt32 aAppUnitsPerPixel)
{
  nscoord r = NSToCoordRound(aPixels * aAppUnitsPerPixel);
  VERIFY_COORD(r);
  return r;
}

inline nscoord NSIntPixelsToAppUnits(PRInt32 aPixels, PRInt32 aAppUnitsPerPixel)
{
  // The cast to nscoord makes sure we don't overflow if we ever change
  // nscoord to float
  nscoord r = aPixels * (nscoord)aAppUnitsPerPixel;
  VERIFY_COORD(r);
  return r;
}

inline float NSAppUnitsToFloatPixels(nscoord aAppUnits, PRInt32 aAppUnitsPerPixel)
{
  return (float(aAppUnits) / aAppUnitsPerPixel);
}

inline PRInt32 NSAppUnitsToIntPixels(nscoord aAppUnits, PRInt32 aAppUnitsPerPixel)
{
  return NSToIntRound(float(aAppUnits) / aAppUnitsPerPixel);
}

/* 
 * Twips/unit conversions
 */
inline nscoord NSUnitsToTwips(float aValue, float aPointsPerUnit)
{
  return NSToCoordRound(aValue * aPointsPerUnit * TWIPS_PER_POINT_FLOAT);
}

inline float NSTwipsToUnits(nscoord aTwips, float aUnitsPerPoint)
{
  return (aTwips * (aUnitsPerPoint / TWIPS_PER_POINT_FLOAT));
}


/// Unit conversion macros
//@{
#define NS_POINTS_TO_TWIPS(x)         NSUnitsToTwips((x), 1.0f)
#define NS_INCHES_TO_TWIPS(x)         NSUnitsToTwips((x), 72.0f)                      // 72 points per inch
#define NS_FEET_TO_TWIPS(x)           NSUnitsToTwips((x), (72.0f * 12.0f))            // 12 inches per foot
#define NS_MILES_TO_TWIPS(x)          NSUnitsToTwips((x), (72.0f * 12.0f * 5280.0f))  // 5280 feet per mile

#define NS_MILLIMETERS_TO_TWIPS(x)    NSUnitsToTwips((x), (72.0f * 0.03937f))
#define NS_CENTIMETERS_TO_TWIPS(x)    NSUnitsToTwips((x), (72.0f * 0.3937f))
#define NS_METERS_TO_TWIPS(x)         NSUnitsToTwips((x), (72.0f * 39.37f))
#define NS_KILOMETERS_TO_TWIPS(x)     NSUnitsToTwips((x), (72.0f * 39370.0f))

#define NS_PICAS_TO_TWIPS(x)          NSUnitsToTwips((x), 12.0f)                      // 12 points per pica
#define NS_DIDOTS_TO_TWIPS(x)         NSUnitsToTwips((x), (16.0f / 15.0f))            // 15 didots per 16 points
#define NS_CICEROS_TO_TWIPS(x)        NSUnitsToTwips((x), (12.0f * (16.0f / 15.0f)))  // 12 didots per cicero

#define NS_TWIPS_TO_POINTS(x)         NSTwipsToUnits((x), 1.0f)
#define NS_TWIPS_TO_INCHES(x)         NSTwipsToUnits((x), 1.0f / 72.0f)
#define NS_TWIPS_TO_FEET(x)           NSTwipsToUnits((x), 1.0f / (72.0f * 12.0f))
#define NS_TWIPS_TO_MILES(x)          NSTwipsToUnits((x), 1.0f / (72.0f * 12.0f * 5280.0f))

#define NS_TWIPS_TO_MILLIMETERS(x)    NSTwipsToUnits((x), 1.0f / (72.0f * 0.03937f))
#define NS_TWIPS_TO_CENTIMETERS(x)    NSTwipsToUnits((x), 1.0f / (72.0f * 0.3937f))
#define NS_TWIPS_TO_METERS(x)         NSTwipsToUnits((x), 1.0f / (72.0f * 39.37f))
#define NS_TWIPS_TO_KILOMETERS(x)     NSTwipsToUnits((x), 1.0f / (72.0f * 39370.0f))

#define NS_TWIPS_TO_PICAS(x)          NSTwipsToUnits((x), 1.0f / 12.0f)
#define NS_TWIPS_TO_DIDOTS(x)         NSTwipsToUnits((x), 1.0f / (16.0f / 15.0f))
#define NS_TWIPS_TO_CICEROS(x)        NSTwipsToUnits((x), 1.0f / (12.0f * (16.0f / 15.0f)))
//@}

#endif
