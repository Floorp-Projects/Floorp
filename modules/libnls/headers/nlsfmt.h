/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#ifndef _NLSFMT_H
#define _NLSFMT_H


#include "nlsxp.h"
#include "nlsuni.h"
#include "nlsloc.h"
#include "nlscal.h"

#ifdef NLS_CPLUSPLUS
#include "format.h"
#include "fmtable.h"
#include "datefmt.h"
#include "numfmt.h"
#include "decimfmt.h"
#endif


NLS_BEGIN_PROTOS

/******************** Formatting Data Types ************************/

#ifndef NLS_CPLUSPLUS
typedef struct _Formattable				Formattable;
typedef struct _DateFormat				DateFormat;
typedef struct _NumberFormat			NumberFormat;
typedef struct _DecimalFormat			DecimalFormat;
typedef struct _DecimalFormatSymbols	DecimalFormatSymbols;
#endif

typedef int NLS_FormattableType;
enum _NLS_FormattableType {
	NLS_FORMAT_TYPE_DATE,		/* Date */
	NLS_FORMAT_TYPE_DOUBLE,		/* double */
	NLS_FORMAT_TYPE_LONG,		/* long */
	NLS_FORMAT_TYPE_STRING,		/* UnicodeString */
	NLS_FORMAT_TYPE_ARRAY,		/* Formattable[] */
	NLS_FORMAT_TYPE_CHOICE		/* Choice Pattern */
};


NLSFMTAPI_PUBLIC(void)
NLS_FmtTerminate(void);

/******************** Simple Formatting functions ************************/

NLSFMTAPI_PUBLIC(const DateFormat *)
NLS_GetDefaultTimeFormat();

NLSFMTAPI_PUBLIC(const DateFormat *)
NLS_GetDefaultDateFormat();

NLSFMTAPI_PUBLIC(const DateFormat *)
NLS_GetDefaultDateTimeFormat();

NLSFMTAPI_PUBLIC(const NumberFormat *)
NLS_GetDefaultNumberFormat();

NLSFMTAPI_PUBLIC(const NumberFormat *)
NLS_GetDefaultCurrencyFormat();

NLSFMTAPI_PUBLIC(const NumberFormat *)
NLS_GetDefaultPercentFormat();

NLSFMTAPI_PUBLIC(const DecimalFormat *)
NLS_GetDefaultDecimalFormat();

/******************** Formatting Constructor functions ************************/

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NewTimeFormat(DateFormat ** result, NLS_DateTimeFormatType	style, const Locale* locale);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NewDateFormat(DateFormat ** result, NLS_DateTimeFormatType	style, const Locale* locale);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NewDateTimeFormat(DateFormat ** result, NLS_DateTimeFormatType	dateStyle, NLS_DateTimeFormatType timeStyle, const Locale* locale);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NewDateFormatCopy(DateFormat ** result, const DateFormat * copyDateFormat);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NewNumberFormat(NumberFormat ** result, const Locale* locale);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NewNumberFormatCopy(NumberFormat ** result, const NumberFormat * copyNumberFormat);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NewCurrencyFormat(NumberFormat ** result, const Locale* locale);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NewPercentFormat(NumberFormat ** result, const Locale* locale);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NewDecimalFormatSymbols(DecimalFormatSymbols ** result, const Locale* locale);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NewDecimalFormatSymbolsCopy(DecimalFormatSymbols ** result, const DecimalFormatSymbols* symbols);

NLSFMTAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewDecimalFormat(DecimalFormat ** result, const UnicodeString* pattern, const DecimalFormatSymbols* symbols);

NLSFMTAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewDecimalFormatCopy(DecimalFormat ** result, const DecimalFormat * decimalFormat);

NLSFMTAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewFormattable(Formattable ** result);

NLSFMTAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewFormattableFromDate(Formattable ** result, Date date);

NLSFMTAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewFormattableFromDouble(Formattable ** result, double value);

NLSFMTAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewFormattableFromLong(Formattable ** result, long value);

NLSFMTAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewFormattableFromString(Formattable ** result, const UnicodeString* string);

NLSFMTAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewFormattableCopy(Formattable ** result, const Formattable * formattable);

/********************* Formatting Destructor functions *******************************/

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DeleteDateFormat(DateFormat * that);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DeleteNumberFormat(NumberFormat * that);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DeleteDecimalFormat(DecimalFormat * that);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DeleteDecimalFormatSymbols(DecimalFormatSymbols * that);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DeleteFormattable(Formattable * that);

/******************** Date Formatting API ************************/

NLSFMTAPI_PUBLIC(nlsBool)
NLS_DateFormatEqual(const DateFormat* format1, const DateFormat* format2);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_FormatDate(const DateFormat* format, Date dateToFormat, UnicodeString* str);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DateFormatParse(const DateFormat* format, const UnicodeString* textToParse, Date *date);

NLSFMTAPI_PUBLIC(const NumberFormat*)
NLS_DateFormatGetNumberFormat(const DateFormat* format);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DateFormatSetNumberFormat(DateFormat* format, const NumberFormat* numberFormat);

NLSFMTAPI_PUBLIC(const Calendar*)
NLS_DateFormatGetCalendar(const DateFormat* format);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DateFormatSetCalendar(DateFormat* format, const Calendar* calendar);

NLSFMTAPI_PUBLIC(const TimeZone*)
NLS_DateFormatGetTimeZone(const DateFormat* format);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DateFormatSetTimeZone(DateFormat* format, const TimeZone* timeZone);

/******************** Number Formatting API ************************/

NLSFMTAPI_PUBLIC(nlsBool)
NLS_NumberFormatEquals(const NumberFormat* format1, const NumberFormat* format2);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NumberFormatParse(const NumberFormat* format, const UnicodeString* text, Formattable* result);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NumberFormatDouble(const NumberFormat* format, double number, UnicodeString* result);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NumberFormatLong(const NumberFormat* format, long number, UnicodeString* result);

NLSFMTAPI_PUBLIC(nlsBool)
NLS_NumberFormatIsParseIntegerOnly(const NumberFormat* format);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NumberFormatSetParseIntegerOnly(NumberFormat* format, nlsBool value);

NLSFMTAPI_PUBLIC(nlsBool)
NLS_NumberFormatIsGroupingUsed(const NumberFormat* format);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NumberFormatSetGroupingUsed(NumberFormat* format, nlsBool value);

NLSFMTAPI_PUBLIC(uint8)
NLS_NumberFormatGetMinimumIntegerDigits(const NumberFormat* format);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NumberFormatSetMinimumIntegerDigits(NumberFormat* format, uint8 value);

NLSFMTAPI_PUBLIC(uint8)
NLS_NumberFormatGetMaximumIntegerDigits(const NumberFormat* format);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NumberFormatSetMaximumIntegerDigits(NumberFormat* format, uint8 value);

NLSFMTAPI_PUBLIC(uint8)
NLS_NumberFormatGetMinimumFractionDigits(const NumberFormat* format);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NumberFormatSetMinimumFractionDigits(NumberFormat* format, uint8 value);

NLSFMTAPI_PUBLIC(uint8)
NLS_NumberFormatGetMaximumFractionDigits(const NumberFormat* format);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NumberFormatSetMaximumFractionDigits(NumberFormat* format, uint8 value);

/******************** Decimal Format Symbols API ************************/

NLSFMTAPI_PUBLIC(nlsBool)
NLS_DecimalFormatSymbolsEqual(const DecimalFormatSymbols* object, const DecimalFormatSymbols* objectToCompareTo);

NLSFMTAPI_PUBLIC(UniChar)
NLS_DecimalFormatSymbolsGetZeroDigit(const DecimalFormatSymbols* object);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatSymbolsSetZeroDigit(DecimalFormatSymbols* object, UniChar value);

NLSFMTAPI_PUBLIC(UniChar)
NLS_DecimalFormatSymbolsGetGroupingSeparator(const DecimalFormatSymbols* object);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatSymbolsSetGroupingSeparator(DecimalFormatSymbols* object, UniChar value);

NLSFMTAPI_PUBLIC(UniChar)
NLS_DecimalFormatSymbolsGetDecimalSeparator(const DecimalFormatSymbols* object);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatSymbolsSetDecimalSeparator(DecimalFormatSymbols* object, UniChar value);

NLSFMTAPI_PUBLIC(UniChar)
NLS_DecimalFormatSymbolsGetPerMill(const DecimalFormatSymbols* object);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatSymbolsSetPerMill(DecimalFormatSymbols* object, UniChar value);

NLSFMTAPI_PUBLIC(UniChar)
NLS_DecimalFormatSymbolsGetPercent(const DecimalFormatSymbols* object);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatSymbolsSetPercent(DecimalFormatSymbols* object, UniChar value);

NLSFMTAPI_PUBLIC(UniChar)
NLS_DecimalFormatSymbolsGetDigit(const DecimalFormatSymbols* object);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatSymbolsSetDigit(DecimalFormatSymbols* object, UniChar value);

NLSFMTAPI_PUBLIC(UniChar)
NLS_DecimalFormatSymbolsGetMinusSign(const DecimalFormatSymbols* object);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatSymbolsSetMinusSign(DecimalFormatSymbols* object, UniChar value);

NLSFMTAPI_PUBLIC(UniChar)
NLS_DecimalFormatSymbolsGetExponential(const DecimalFormatSymbols* object);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatSymbolsSetExponential(DecimalFormatSymbols* object, UniChar value);

NLSFMTAPI_PUBLIC(UniChar)
NLS_DecimalFormatSymbolsGetPatternSeparator(const DecimalFormatSymbols* object);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatSymbolsSetPatternSeparator(DecimalFormatSymbols* object, UniChar value);

/******************** Decimal Formatting API ************************/

NLSFMTAPI_PUBLIC(nlsBool)
NLS_DecimalFormatEqual(const DecimalFormat* object, const DecimalFormat* objectToCompareTo);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatDouble(const DecimalFormat* object, double number, UnicodeString* toAppendTo);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatLong(const DecimalFormat* object, long number, UnicodeString* toAppendTo);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatParse(const DecimalFormat* object, const UnicodeString* text, Formattable* result);

NLSFMTAPI_PUBLIC(const DecimalFormatSymbols*)
NLS_DecimalFormatGetDecimalFormatSymbols(const DecimalFormat* object);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatSetDecimalFormatSymbols(DecimalFormat* object, const DecimalFormatSymbols* symbolsToAdopt);

NLSFMTAPI_PUBLIC(const UnicodeString*)
NLS_DecimalFormatGetPositivePrefix(const DecimalFormat* object, UnicodeString* result);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatSetPositivePrefix(DecimalFormat* object, const UnicodeString* value);

NLSFMTAPI_PUBLIC(const UnicodeString*)
NLS_DecimalFormatGetNegativePrefix(const DecimalFormat* object, UnicodeString* result);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatSetNegativePrefix(DecimalFormat* object, const UnicodeString* value);

NLSFMTAPI_PUBLIC(const UnicodeString*)
NLS_DecimalFormatGetPositiveSuffix(const DecimalFormat* object, UnicodeString* result);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatSetPositiveSuffix(DecimalFormat* object, const UnicodeString* value);

NLSFMTAPI_PUBLIC(const UnicodeString*)
NLS_DecimalFormatGetNegativeSuffix(const DecimalFormat* object, UnicodeString* result);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatSetNegativeSuffix(DecimalFormat* object, const UnicodeString* value);

NLSFMTAPI_PUBLIC(int32)
NLS_DecimalFormatGetMultiplier(const DecimalFormat* object);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatSetMultiplier(DecimalFormat* object, int32 multiplier);

NLSFMTAPI_PUBLIC(uint8)
NLS_DecimalFormatGetGroupingSize(const DecimalFormat* object);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatSetGroupingSize(DecimalFormat* object, uint8 groupingSize);

NLSFMTAPI_PUBLIC(nlsBool)
NLS_DecimalFormatGetDecimalSeparatorAlwaysShown(const DecimalFormat* object);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatSetDecimalSeparatorAlwaysShown(DecimalFormat* object, nlsBool alwaysShown);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatGetPattern(const DecimalFormat* object, UnicodeString* value);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatSetPattern(DecimalFormat* object, const UnicodeString* value);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatGetLocalizedPattern(const DecimalFormat* object, UnicodeString* value);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DecimalFormatSetLocalizedPattern(DecimalFormat* object, const UnicodeString* value);


/******************** Formattable API ************************/

NLSFMTAPI_PUBLIC(nlsBool)
NLS_FormattableEqual(const Formattable* object, const Formattable* objectToCompareTo);

NLSFMTAPI_PUBLIC(NLS_FormattableType)
NLS_FormattableGetType(const Formattable* object);

NLSFMTAPI_PUBLIC(double)
NLS_FormattableGetDouble(const Formattable* object);

NLSFMTAPI_PUBLIC(long)
NLS_FormattableGetLong(const Formattable* object);

NLSFMTAPI_PUBLIC(Date)
NLS_FormattableGetDate(const Formattable* object);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_FormattableGetString(const Formattable* object, UnicodeString* result);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_FormattableSetDouble(Formattable* object, double value);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_FormattableSetLong(Formattable* object, long value);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_FormattableSetDate(Formattable* object, Date date);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_FormattableSetString(Formattable* object, const UnicodeString* stringToCopy);



/************************ End ******************************/

NLS_END_PROTOS
#endif /* _NLSFMT_H */

