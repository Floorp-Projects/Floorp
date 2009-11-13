// Windows/Time.h

#ifndef __WINDOWS_TIME_H
#define __WINDOWS_TIME_H

#include "Common/Types.h"

namespace NWindows {
namespace NTime {

bool DosTimeToFileTime(UInt32 dosTime, FILETIME &fileTime);
bool FileTimeToDosTime(const FILETIME &fileTime, UInt32 &dosTime);
void UnixTimeToFileTime(UInt32 unixTime, FILETIME &fileTime);
bool FileTimeToUnixTime(const FILETIME &fileTime, UInt32 &unixTime);
bool GetSecondsSince1601(unsigned year, unsigned month, unsigned day,
  unsigned hour, unsigned min, unsigned sec, UInt64 &resSeconds);
void GetCurUtcFileTime(FILETIME &ft);

}}

#endif
