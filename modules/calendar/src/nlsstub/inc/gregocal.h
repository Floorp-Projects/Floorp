#ifndef gregocal_h__
#define gregocal_h__

#include "ptypes.h"
#include "calendar.h"

class TimeZone;
class Locale;

class GregorianCalendar : public Calendar
{

public:
  GregorianCalendar();
  ~GregorianCalendar();

  GregorianCalendar(ErrorCode& aSuccess);
  GregorianCalendar(TimeZone* aZoneToAdopt, ErrorCode& aSuccess);
  GregorianCalendar(const TimeZone& aZone, ErrorCode& aSuccess);
  GregorianCalendar(const Locale& aLocale, ErrorCode& aSuccess);
  GregorianCalendar(TimeZone* aZoneToAdopt, const Locale& aLocale, ErrorCode& aSuccess);
  GregorianCalendar(const TimeZone& aZone, const Locale& aLocale, ErrorCode& aSuccess);
  GregorianCalendar(PRInt32 aYear, PRInt32 aMonth, PRInt32 aDate, ErrorCode& aSuccess);
  GregorianCalendar(PRInt32 aYear, PRInt32 aMonth, PRInt32 aDate, PRInt32 aHour, PRInt32 aMinute, ErrorCode& aSuccess);
  GregorianCalendar(PRInt32 aYear, PRInt32 aMonth, PRInt32 aDate, PRInt32 aHour, PRInt32 aMinute, PRInt32 aSecond, ErrorCode& aSuccess);
  
  virtual void add(EDateFields aField, PRInt32 aAmount, ErrorCode& aStatus);

};




#endif
