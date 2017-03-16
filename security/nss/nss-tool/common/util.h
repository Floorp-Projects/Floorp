/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef util_h__
#define util_h__

#include "scoped_ptrs.h"

#include <secmodt.h>
#include <string>
#include <vector>

enum PwDataType { PW_NONE = 0, PW_FROMFILE = 1, PW_PLAINTEXT = 2 };
typedef struct {
  PwDataType source;
  char *data;
} PwData;

bool InitSlotPassword(void);
bool ChangeSlotPassword(void);
bool DBLoginIfNeeded(const ScopedPK11SlotInfo &slot);
std::string StringToHex(const ScopedSECItem &input);
std::vector<char> ReadInputData(std::string &dataPath);

#endif  // util_h__
