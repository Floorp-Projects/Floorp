// Common/ListFileUtils.h

#ifndef __COMMON_LISTFILEUTILS_H
#define __COMMON_LISTFILEUTILS_H

#include "MyString.h"
#include "Types.h"

bool ReadNamesFromListFile(LPCWSTR fileName, UStringVector &strings, UINT codePage = CP_OEMCP);

#endif
