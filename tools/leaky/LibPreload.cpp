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

#include "TestPreload.h"
#include "config.h"
#include <malloc.h>

// This is a fake implementation of malloc that layers on top of the
// native platforms malloc routine (ideally using weak symbols). What
// it does if given a specail size request it returns a fake address
// result, otherwise it uses the underlying malloc.
void* malloc(size_t aSize)
{
  if (aSize == LD_PRELOAD_TEST_MALLOC_SIZE) {
    return (void*) LD_PRELOAD_TEST_VALUE;
  }
  else {
    return REAL_MALLOC(aSize);
  }
}
