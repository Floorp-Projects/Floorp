#ifndef datefmt_h__
#define datefmt_h__

#include "prtypes.h"
#include "unistring.h"

class ParsePosition;
class Format;

class DateFormat
{

public:
  DateFormat();
  ~DateFormat();

  virtual void setLenient(PRBool aLenient);
  virtual void setTimeZone(const TimeZone& aZone);
  virtual Date parse(const UnicodeString& aUnicodeString, ErrorCode& aStatus) const;
  virtual Date parse(const UnicodeString& aUnicodeString, ParsePosition& aPosition) const = 0;

  virtual PRBool operator==(const Format&) const;

};


#endif
