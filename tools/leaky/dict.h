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

#ifndef __dict_h_
#define __dict_h_

#include <sys/types.h>
#include "libmalloc.h"

// key is u_long
// value is malloc_log_entry*
struct MallocDict {
  MallocDict(int buckets);

  void rewind(void);
  malloc_log_entry* next(void);

  malloc_log_entry** find(u_long addr);
  void add(u_long addr, malloc_log_entry *log);
  void remove(u_long addr);

  struct MallocDictEntry {
    u_long addr;
    malloc_log_entry* logEntry;
    MallocDictEntry* next;
  } **buckets;

  int numBuckets;

  int iterNextBucket;
  MallocDictEntry* iterNextEntry;
};

#endif /* __dict_h_ */
