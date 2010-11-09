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
 * The Original Code is Mozilla Code
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
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

#include <stdio.h>

#if defined(XP_WIN) || (defined(XP_OS2)
#  define MOZALLOC_EXPORT __declspec(dllexport)
#endif

#define MOZALLOC_DONT_WRAP_RAISE_FUNCTIONS
#include "mozilla/throw_msvc.h"

__declspec(noreturn) static void abort_from_exception(const char* const which,
                                                      const char* const what);
static void
abort_from_exception(const char* const which,  const char* const what)
{
    fprintf(stderr, "fatal: STL threw %s: ", which);
    mozalloc_abort(what);
}

namespace std {

// NB: user code is not supposed to touch the std:: namespace.  We're
// doing this after careful review because we want to define our own
// exception throwing semantics.  Don't try this at home!

void
moz_Xinvalid_argument(const char* what)
{
    abort_from_exception("invalid_argument", what);
}

void
moz_Xlength_error(const char* what)
{
    abort_from_exception("length_error", what);
}

void
moz_Xout_of_range(const char* what)
{
    abort_from_exception("out_of_range", what);
}

void
moz_Xoverflow_error(const char* what)
{
    abort_from_exception("overflow_error", what);
}

void
moz_Xruntime_error(const char* what)
{
    abort_from_exception("runtime_error", what);
}

}  // namespace std
