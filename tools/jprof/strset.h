// The contents of this file are subject to the Mozilla Public License
// Version 1.1 (the "License"); you may not use this file except in
// compliance with the License. You may obtain a copy of the License
// at http://www.mozilla.org/MPL/
//
// Software distributed under the License is distributed on an "AS IS"
// basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
// the License for the specific language governing rights and
// limitations under the License.
//
// The Initial Developer of the Original Code is Kipp E.B. Hickman.

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
