#ifndef smpdtfmt_h__
#define smpdtfmt_h__

#include "prtypes.h"
#include "datefmt.h"

class UnicodeString;
class Formattable;
class ParsePosition;
class Format;

class FieldPosition
{
public:
  FieldPosition();
  ~FieldPosition();
  FieldPosition(PRInt32 aField);
};

class SimpleDateFormat : public DateFormat 
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
