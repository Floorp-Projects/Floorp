/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Simon Fraser <sfraser@netscape.com>
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



#include <StringCompare.h>

#include "nsAEUtils.h"
#include "nsAECompare.h"

// ----------------------------------------------------------------------------------------

Boolean AEComparisons::CompareTexts(DescType oper, const AEDesc *desc1, const AEDesc *desc2)
{
	Boolean	result = false;
	
	short	compareResult;
	Handle  lhsHandle = 0, rhsHandle = 0;
	char  	*lhs;
	char  	*rhs;
	long		lhsSize;
	long		rhsSize;
	
	// FIXME:  this can leak lhsHandle if second conversion fails.
	if (DescToTextHandle(desc1, &lhsHandle) != noErr || DescToTextHandle(desc2, &rhsHandle) != noErr)
	    goto fail;
	
	lhsSize = GetHandleSize(lhsHandle);
	HLock(lhsHandle);
	lhs = *(lhsHandle);
	
	rhsSize = GetHandleSize(rhsHandle);
	HLock(rhsHandle);
	rhs = *(rhsHandle);

	compareResult = ::CompareText(lhs, rhs, lhsSize, rhsSize, nil);

	switch (oper) 
	{
		case kAEEquals:
			result = (compareResult == 0);
			break;
		
		case kAELessThan:
			result = (compareResult < 0);
			break;
		
		case kAELessThanEquals:
			result = (compareResult <= 0);
			break;
		
		case kAEGreaterThan:
			result = (compareResult > 0);
			break;
		
		case kAEGreaterThanEquals:
			result = (compareResult >= 0);
			break;
		
		case kAEBeginsWith:
			if (rhsSize > lhsSize)
			{
				result = false;
			}
			else
			{
				// compare only the number of characters in rhs
				// begin comparing at the beginning of lhs
				compareResult = CompareText(lhs, rhs, rhsSize, rhsSize, nil);
				result = (compareResult == 0);
			}
			break;
			
		case kAEEndsWith:
			if (rhsSize > lhsSize)
			{
				result = false;
			}
			else
			{
				// compare only the number of characters in rhs
				// begin comparing rhsSize characters from the end of lhs
				// start 
				
				lhs += (lhsSize - rhsSize);
				compareResult = CompareText(lhs, rhs, rhsSize, rhsSize, nil);
				result = (compareResult == 0);
			}
			break;

		case kAEContains:
			// Here I use an inefficient search strategy, but we're dealing with small amounts
			// of text and by using CompareText(), we're doing the same style of comparison
			// as in the other cases above.
			
			result = false;
			while (lhsSize >= rhsSize)
			{
				compareResult = CompareText(lhs, rhs, rhsSize, rhsSize, nil);
				if (compareResult == 0)
				{
					result = true;
					break;
				}
				lhs++;
				lhsSize--;
			}
			break;

		default:
			ThrowOSErr(errAEBadTestKey);
	}

fail:
    if (lhsHandle) DisposeHandle(lhsHandle);
    if (rhsHandle) DisposeHandle(rhsHandle);
	
	return result;
}

// ----------------------------------------------------------------------------------------

Boolean AEComparisons::CompareEnumeration(DescType oper, const AEDesc *desc1, const AEDesc *desc2)
{
	OSErr		err	= noErr;
	Boolean		result = false;
	long	 		lhs;
	long   		rhs;
	StAEDesc 		charDesc;
	
	// Make each number is a long integer (in case it's a short integer or other integer type) 
	// before extracting the data */
	
	err = AECoerceDesc(desc1, typeChar, &charDesc);
	ThrowIfOSErr(err);

	lhs = **(long **)(charDesc.dataHandle);
	AEDisposeDesc(&charDesc);
	
	err = AECoerceDesc(desc2, typeChar, &charDesc);
	ThrowIfOSErr(err);

	rhs = **(long **)charDesc.dataHandle;
	AEDisposeDesc(&charDesc);
	
	switch (oper) 
	{
		case kAEEquals:
			result = (lhs == rhs);	// equality is the only test that makes sense for enumerators
			break;
		
		default:
			ThrowOSErr(errAEBadTestKey);
	}

	return result;
}

// ----------------------------------------------------------------------------------------

Boolean AEComparisons::CompareInteger(DescType oper, const AEDesc *desc1, const AEDesc *desc2)
{
	OSErr		err	= noErr;
	Boolean		result = false;
	long	 		lhs;
	long   		rhs;
	StAEDesc 		longDesc;
	
	// Make each number is a long integer (in case it's a short integer or other integer type) 
	// before extracting the data */
	
	err = AECoerceDesc(desc1, typeLongInteger, &longDesc);
	ThrowIfOSErr(err);

	lhs = **(long **)(longDesc.dataHandle);
	AEDisposeDesc(&longDesc);
	
	err = AECoerceDesc(desc2, typeLongInteger, &longDesc);
	ThrowIfOSErr(err);

	rhs = **(long **)longDesc.dataHandle;
	AEDisposeDesc(&longDesc);
	
	switch (oper) 
	{
		case kAEEquals:
			result = (lhs == rhs);
			break;
		
		case kAELessThan:
			result = (lhs < rhs);
			break;
		
		case kAELessThanEquals:
			result = (lhs <= rhs);
			break;
		
		case kAEGreaterThan:
			result = (lhs > rhs);
			break;
		
		case kAEGreaterThanEquals:
			result = (lhs >= rhs);
			break;
		
		default:
			ThrowOSErr(errAEBadTestKey);
	}

	return result;
}

// ----------------------------------------------------------------------------------------

Boolean AEComparisons::CompareFixed(DescType oper, const AEDesc *desc1, const AEDesc *desc2)
{
	OSErr		err	= noErr;
	Boolean		result = false;
	Fixed		lhs;
	Fixed 		rhs;
	
	err = DescToFixed(desc1, &lhs);
	ThrowIfOSErr(err);
		
	err = DescToFixed(desc2, &rhs);
	ThrowIfOSErr(err);
	
	switch (oper) 
	{
		case kAEEquals:
			result = (lhs == rhs);
			break;
		
		case kAELessThan:
			result = (lhs < rhs);
			break;
		
		case kAELessThanEquals:
			result = (lhs <= rhs);
			break;
		
		case kAEGreaterThan:
			result = (lhs > rhs);
			break;
		
			case kAEGreaterThanEquals:
			result = (lhs >= rhs);
			break;
		
		default:
			ThrowOSErr(errAEBadTestKey);
	}
	
	return result;
}

// ----------------------------------------------------------------------------------------

Boolean AEComparisons::CompareFloat(DescType oper, const AEDesc *desc1, const AEDesc *desc2)
{
	OSErr	err	= noErr;
	Boolean	result = false;
	float		 lhs;
	float 	 rhs;
	
	err = DescToFloat(desc1, &lhs);
	ThrowIfOSErr(err);
		
	err = DescToFloat(desc2, &rhs);
	ThrowIfOSErr(err);
	
	switch (oper) 
	{
		case kAEEquals:
			result = (lhs == rhs);
			break;
		
		case kAELessThan:
			result = (lhs < rhs);
			break;
		
		case kAELessThanEquals:
			result = (lhs <= rhs);
			break;
		
		case kAEGreaterThan:
			result = (lhs > rhs);
			break;
		
			case kAEGreaterThanEquals:
			result = (lhs >= rhs);
			break;
		
		default:
			ThrowOSErr(errAEBadTestKey);
	}
	
	return result;
}

// ----------------------------------------------------------------------------------------
// Apple events defines a boolean as a 1-byte value containing 1 for TRUE and 0 for FALSE

Boolean AEComparisons::CompareBoolean(DescType oper, const AEDesc *desc1, const AEDesc *desc2)
{
	OSErr	err 	= noErr;
	Boolean	result = false;
	
	Boolean 	bool1	= ((**(char **)desc1->dataHandle) != 0);
	Boolean 	bool2	= ((**(char **)desc2->dataHandle) != 0);
		
	if (oper == kAEEquals) 
		result = (bool1 == bool2);
	else
		ThrowOSErr(errAEBadTestKey);		// No other tests make sense

	return result;
}

// ----------------------------------------------------------------------------------------

Boolean AEComparisons::CompareRGBColor(DescType oper, const AEDesc *desc1, const AEDesc *desc2)
{
	OSErr	err = noErr;
	Boolean	result = false;
	
	RGBColor lhs;
	RGBColor rhs;
	
	err = DescToRGBColor(desc1, &lhs);
	ThrowIfOSErr(err);
		
	err = DescToRGBColor(desc2, &rhs);
	ThrowIfOSErr(err);

	if (oper == kAEEquals) 
		result = EqualRGB(lhs, rhs);
	else
		ThrowOSErr(errAEBadTestKey);		// No other tests make sense

	return result;
}

// ----------------------------------------------------------------------------------------

Boolean AEComparisons::ComparePoint(DescType oper, const AEDesc *desc1, const AEDesc *desc2)
{
	OSErr	err = noErr;
	Boolean	result = false;
	
	Point		lhs;
	Point		rhs;
	
	err = DescToPoint(desc1, &lhs);
	ThrowIfOSErr(err);
		
	err = DescToPoint(desc2, &rhs);
	ThrowIfOSErr(err);
		
	switch (oper)
	{
		case kAEEquals:
			result = (lhs.h = rhs.h && lhs.v == rhs.v);
			break;
			
		case kAELessThan:
			result = (lhs.h < rhs.h && lhs.v < rhs.v);
			break;
		
		case kAELessThanEquals:
			result = (lhs.h <= rhs.h && lhs.v <= rhs.v);
			break;
		
		case kAEGreaterThan:
			result = (lhs.h > rhs.h && lhs.v > rhs.v);
			break;
		
		case kAEGreaterThanEquals:
			result = (lhs.h >= rhs.h && lhs.v >= rhs.v);
			break;
		
		default:
			ThrowOSErr(errAEBadTestKey);		// No other tests make sense
	}

	return result;
}

// ----------------------------------------------------------------------------------------

Boolean AEComparisons::CompareRect(DescType oper, const AEDesc *desc1, const AEDesc *desc2)
{
	OSErr	err = noErr;
	Boolean	result = false;
	Rect		lhs;
	Rect		rhs;
	
	err = DescToRect(desc1, &lhs);
	ThrowIfOSErr(err);
		
	err = DescToRect(desc2, &rhs);
	ThrowIfOSErr(err);
		
	switch (oper)
	{
		// compare size AND location
		case kAEEquals:	
			result = ((lhs.top == rhs.top) && (lhs.left == rhs.left) && (lhs.bottom == rhs.bottom) && (lhs.right == rhs.right));
			break;
			
		// compare size only on the rest of the tests
		case kAELessThan:	
			result = (((lhs.bottom - lhs.top) < (rhs.bottom - rhs.top)) && ((lhs.right - lhs.left) < (lhs.right - rhs.left)));
			break;
		
		case kAELessThanEquals:
			result = (((lhs.bottom - lhs.top) < (rhs.bottom - rhs.top)) && ((lhs.right - lhs.left) < (lhs.right - rhs.left)));
			break;
		
		case kAEGreaterThan:
			result = (((lhs.bottom - lhs.top) < (rhs.bottom - rhs.top)) && ((lhs.right - lhs.left) < (lhs.right - rhs.left)));
			break;
		
		case kAEGreaterThanEquals:
			result = (((lhs.bottom - lhs.top) < (rhs.bottom - rhs.top)) && ((lhs.right - lhs.left) < (lhs.right - rhs.left)));
			break;
		
		case kAEContains:
			// Note: two identical Rects contain each other, according to this test:
			result = ((lhs.top <= rhs.top) && (lhs.left <= rhs.left) && (lhs.bottom >= rhs.bottom) && (lhs.right >= rhs.right));
			break;
			
		default:
			ThrowOSErr(errAEBadTestKey);		// No other tests make sense
	}

	return result;
}

#pragma mark -

/*----------------------------------------------------------------------------
	TryPrimitiveComparison 
	
----------------------------------------------------------------------------*/

Boolean AEComparisons::TryPrimitiveComparison(DescType comparisonOperator, const AEDesc *desc1, const AEDesc *desc2)
{
	Boolean 	result = false;
	
	// This has to handle all the data types used in the application's
	// object model implementation.
	switch (desc1->descriptorType) 
	{
		case typeChar:
			result = CompareTexts(comparisonOperator, desc1, desc2);
			break;
		
		case typeShortInteger:		// also covers typeSMInt		'shor'
		case typeLongInteger:		// also covers typeInteger	'long'
		case typeMagnitude:			//						'magn'
			result = CompareInteger(comparisonOperator, desc1, desc2);
			break;

		case typeEnumerated:
			result = CompareEnumeration(comparisonOperator, desc1, desc2);
			break;
		
		case typeFixed:
			result = CompareFixed(comparisonOperator, desc1, desc2);
			break;

		case typeFloat:
			result = CompareFloat(comparisonOperator, desc1, desc2);
			break;
		
		case typeBoolean:
			result = CompareBoolean(comparisonOperator, desc1, desc2);
			break;
				
		case typeRGBColor:
			result = CompareRGBColor(comparisonOperator, desc1, desc2);
			break;
				
		case typeQDRectangle:
			result = CompareRect(comparisonOperator, desc1, desc2);
			break;
				
		case typeQDPoint:
			result = ComparePoint(comparisonOperator, desc1, desc2);
			break;
				
		default:
			ThrowOSErr(errAEWrongDataType);
	}

	return result;
}
