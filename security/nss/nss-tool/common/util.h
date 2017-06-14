/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef util_h__
#define util_h__

#include "nspr.h"
#include "scoped_ptrs.h"

#include <secmodt.h>
#include <string>
#include <vector>

#ifndef PORT_Malloc
#define PORT_Malloc PR_Malloc
#endif

enum PwDataType { PW_NONE = 0, PW_FROMFILE = 1, PW_PLAINTEXT = 2 };
typedef struct {
  PwDataType source;
  char *data;
} PwData;

bool InitSlotPassword(void);
bool ChangeSlotPassword(void);
bool DBLoginIfNeeded(const ScopedPK11SlotInfo &slot);
std::string StringToHex(const ScopedSECItem &input);
std::vector<uint8_t> ReadInputData(std::string dataPath);
std::istream &GetStreamFromFileOrStdin(std::string &path, std::ifstream &ifs);

#endif  // util_h__
