/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *  The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jason Duell <jduell.mcbugs@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef mozilla_net_NeckoCommon_h
#define mozilla_net_NeckoCommon_h

#include "nsXULAppAPI.h"
#include "prenv.h"

#if defined(DEBUG) || defined(ENABLE_TESTS)
# define NECKO_ERRORS_ARE_FATAL_DEFAULT true
#else
# define NECKO_ERRORS_ARE_FATAL_DEFAULT false
#endif 

// TODO: Eventually remove NECKO_MAYBE_ABORT and DROP_DEAD (bug 575494).
// Still useful for catching listener interfaces we don't yet support across
// processes, etc.

#define NECKO_MAYBE_ABORT(msg)                                                 \
  do {                                                                         \
    bool abort = NECKO_ERRORS_ARE_FATAL_DEFAULT;                               \
    const char *e = PR_GetEnv("NECKO_ERRORS_ARE_FATAL");                       \
    if (e)                                                                     \
      abort = (*e == '0') ? false : true;                                      \
    if (abort) {                                                               \
      msg.Append(" (set NECKO_ERRORS_ARE_FATAL=0 in your environment to "      \
                      "convert this error into a warning.)");                  \
      NS_RUNTIMEABORT(msg.get());                                              \
    } else {                                                                   \
      msg.Append(" (set NECKO_ERRORS_ARE_FATAL=1 in your environment to "      \
                      "convert this warning into a fatal error.)");            \
      NS_WARNING(msg.get());                                                   \
    }                                                                          \
  } while (0)

#define DROP_DEAD()                                                            \
  do {                                                                         \
    nsPrintfCString msg(1000,"NECKO ERROR: '%s' UNIMPLEMENTED",                \
                        __FUNCTION__);                                         \
    NECKO_MAYBE_ABORT(msg);                                                    \
    return NS_ERROR_NOT_IMPLEMENTED;                                           \
  } while (0)

#define ENSURE_CALLED_BEFORE_ASYNC_OPEN()                                      \
  if (mIsPending || mWasOpened) {                                              \
    nsPrintfCString msg(1000, "'%s' called after AsyncOpen: %s +%d",           \
                        __FUNCTION__, __FILE__, __LINE__);                     \
    NECKO_MAYBE_ABORT(msg);                                                    \
  }                                                                            \
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);                           \
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

namespace mozilla {
namespace net {

inline bool 
IsNeckoChild() 
{
  static bool didCheck = false;
  static bool amChild = false;

  if (!didCheck) {
    // This allows independent necko-stacks (instead of single stack in chrome)
    // to still be run.  
    // TODO: Remove eventually when no longer supported (bug 571126)
    const char * e = PR_GetEnv("NECKO_SEPARATE_STACKS");
    if (!e) 
      amChild = (XRE_GetProcessType() == GeckoProcessType_Content);
    didCheck = true;
  }
  return amChild;
}


} // namespace net
} // namespace mozilla

#endif // mozilla_net_NeckoCommon_h

