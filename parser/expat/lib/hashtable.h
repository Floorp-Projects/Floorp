/*
The contents of this file are subject to the Mozilla Public License
Version 1.0 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
License for the specific language governing rights and limitations
under the License.

The Original Code is expat.

The Initial Developer of the Original Code is James Clark.
Portions created by James Clark are Copyright (C) 1998
James Clark. All Rights Reserved.

Contributor(s):
*/


#include <stddef.h>

#ifdef XML_UNICODE
#ifdef XML_UNICODE_WCHAR_T
typedef const wchar_t *KEY;
#else
typedef const unsigned short *KEY;
#endif
#else
typedef const char *KEY;
#endif

typedef struct {
  KEY name;
} NAMED;

typedef struct {
  NAMED **v;
  size_t size;
  size_t used;
  size_t usedLim;
} HASH_TABLE;

NAMED *lookup(HASH_TABLE *table, KEY name, size_t createSize);
void hashTableInit(HASH_TABLE *);
void hashTableDestroy(HASH_TABLE *);

typedef struct {
  NAMED **p;
  NAMED **end;
} HASH_TABLE_ITER;

void hashTableIterInit(HASH_TABLE_ITER *, const HASH_TABLE *);
NAMED *hashTableIterNext(HASH_TABLE_ITER *);
