/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef smpdtfmt_h__
#define smpdtfmt_h__

#include "prtypes.h"
#include "datefmt.h"

class UnicodeString;
class Formattable;
class ParsePosition;
class Format;

class NS_NLS FieldPosition
{
public:
  FieldPosition();
  ~FieldPosition();
  FieldPosition(PRInt32 aField);
};

class NS_NLS SimpleDateFormat : public DateFormat 
{
public:
  SimpleDateFormat();
  ~SimpleDateFormat();
  SimpleDateFormat(ErrorCode& aStatus);

  SimpleDateFormat(const UnicodeString& aPattern, const Locale& aLocale, ErrorCode& aStatus);

 virtual UnicodeString& format(Date aDate, UnicodeString& aAppendTo, FieldPosition& aPosition) const;
 virtual UnicodeString& format(const Formattable& aObject, UnicodeString& aAppendTo, FieldPosition& aPosition, ErrorCode& aStatus) const;
 virtual void applyLocalizedPattern(const UnicodeString& aPattern, ErrorCode& aStatus);
 virtual Date parse(const UnicodeString& aUnicodeString, ParsePosition& aPosition) const;

 virtual PRBool operator==(const Format& other) const;

};
#endif
