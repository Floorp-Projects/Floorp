#ifndef locid_h__
#define locid_h__

#include "ptypes.h"

class UnicodeString;

class Locale 
{

public:
  Locale();
  ~Locale();

  static const Locale& getDefault();
  UnicodeString& getName(UnicodeString&	aName) const;

};

#endif
