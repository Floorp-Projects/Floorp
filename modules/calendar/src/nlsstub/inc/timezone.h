#ifndef timezone_h__
#define timezone_h__

#include "ptypes.h"

class TimeZone 
{

public:
  TimeZone();
  ~TimeZone();

  static TimeZone* createDefault();
  static TimeZone* createTimeZone(const UnicodeString& aID);
  void setID(const UnicodeString& aID);

};


#endif
