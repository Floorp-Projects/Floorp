#ifndef __mozilla_GkRustUtils_h
#define __mozilla_GkRustUtils_h

#include "nsString.h"

class GkRustUtils {
 public:
  static void GenerateUUID(nsACString& aResult);
};

#endif
