// TempFiles.h

#ifndef __TEMPFILES_H
#define __TEMPFILES_H

#include "Common/MyString.h"

class CTempFiles
{
  void Clear();
public:
  UStringVector Paths;
  ~CTempFiles() { Clear(); }
};

#endif
