/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __strset_h_
#define __strset_h_

struct StrSet {
  StrSet();

  void add(const char* string);
  int contains(const char* string);
  bool IsEmpty() const { return 0 == numstrings; }

  char** strings;
  int numstrings;
};

#endif /* __strset_h_ */
