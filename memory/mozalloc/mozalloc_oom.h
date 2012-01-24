/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
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
 * Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Jones <jones.chris.g@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef mozilla_mozalloc_oom_h
#define mozilla_mozalloc_oom_h

#include "mozalloc.h"

#if defined(MOZALLOC_EXPORT)
// do nothing: it's been defined to __declspec(dllexport) by
// mozalloc*.cpp on platforms where that's required
#elif defined(XP_WIN) || (defined(XP_OS2) && defined(__declspec))
#  define MOZALLOC_EXPORT __declspec(dllimport)
#elif defined(HAVE_VISIBILITY_ATTRIBUTE)
/* Make sure symbols are still exported even if we're wrapped in a
 * |visibility push(hidden)| blanket. */
#  define MOZALLOC_EXPORT __attribute__ ((visibility ("default")))
#else
#  define MOZALLOC_EXPORT
#endif


/**
 * Called when memory is critically low.  Returns iff it was able to 
 * remedy the critical memory situation; if not, it will abort().
 * 
 * We have to re-#define MOZALLOC_EXPORT because this header can be
 * used indepedently of mozalloc.h.
 */
MOZALLOC_EXPORT void mozalloc_handle_oom(size_t requestedSize);

/**
 * Called by embedders (specifically Mozilla breakpad) which wants to be
 * notified of an intentional abort, to annotate any crash report with
 * the size of the allocation on which we aborted.
 */
typedef void (*mozalloc_oom_abort_handler)(size_t size);
MOZALLOC_EXPORT void mozalloc_set_oom_abort_handler(mozalloc_oom_abort_handler handler);

/* TODO: functions to query system memory usage and register
 * critical-memory handlers. */


#endif /* ifndef mozilla_mozalloc_oom_h */
