/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
