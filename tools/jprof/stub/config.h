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
#ifndef config_h___
#define config_h___

#define MAX_STACK_CRAWL 500
#define M_LOGFILE "jprof-log"
#define M_MAPFILE "jprof-map"

#if defined(linux) || defined(NTO)
#define USE_BFD
#undef NEED_WRAPPERS

#endif /* linux */

#endif /* config_h___ */
