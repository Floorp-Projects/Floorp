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
// The Initial Developer of the Original Code is James L. Nance.

#ifndef jprof_h___
#define jprof_h___
#include "nscore.h"

#ifdef _IMPL_JPPROF_API
#define JPROF_API(type) NS_EXPORT_(type)
#else
#define JPROF_API(type) NS_IMPORT_(type)
#endif

JPROF_API(void) setupProfilingStuff(void);

#endif /* jprof_h___ */
