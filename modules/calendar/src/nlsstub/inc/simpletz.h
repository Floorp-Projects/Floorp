#ifndef simpletz_h__
#define simpletz_h__

#include "timezone.h"

class SimpleTimeZone : public TimeZone
{

public:
  SimpleTimeZone();
  ~SimpleTimeZone();
  SimpleTimeZone(PRInt32 aRawOffset, const UnicodeString& aID);

  SimpleTimeZone(PRInt32 aRawOffset, const UnicodeString& aID,
      PRInt8  aStartMonth,      PRInt8 aStartDayOfWeekInMonth,
      PRInt8  aStartDayOfWeek,  PRInt32 aStartTime,
      PRInt8  aEndMonth,        PRInt8 aEndDayOfWeekInMonth,
      PRInt8  aEndDayOfWeek,    PRInt32 aEndTime,
      PRInt32 aDstSavings = kMillisPerHour);

  virtual TimeZone* clone() const;
  virtual void setRawOffset(PRInt32 aOffsetMillis);
  void setStartRule(PRInt32 aMonth, PRInt32 aDayOfWeekInMonth, PRInt32 aDayOfWeek, PRInt32 aTime);
  void setEndRule(PRInt32 aMonth, PRInt32 aDayOfWeekInMonth, PRInt32 aDayOfWeek, PRInt32 aTime);

};

#endif
