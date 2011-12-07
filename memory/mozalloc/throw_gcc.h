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

#ifndef mozilla_throw_gcc_h
#define mozilla_throw_gcc_h

#include <stdio.h>              // snprintf
#include <string.h>             // strerror

// For gcc, we define these inline to abort so that we're absolutely
// certain that (i) no exceptions are thrown from Gecko; (ii) these
// errors are always terminal and caught by breakpad.

#include "mozilla/mozalloc_abort.h"

namespace std {

// NB: user code is not supposed to touch the std:: namespace.  We're
// doing this after careful review because we want to define our own
// exception throwing semantics.  Don't try this at home!

NS_ALWAYS_INLINE inline void NS_NORETURN
__throw_bad_exception(void)
{
    mozalloc_abort("fatal: STL threw bad_exception");
}

NS_ALWAYS_INLINE inline void NS_NORETURN
__throw_bad_alloc(void)
{
    mozalloc_abort("fatal: STL threw bad_alloc");
}

NS_ALWAYS_INLINE inline void NS_NORETURN
__throw_bad_cast(void)
{
    mozalloc_abort("fatal: STL threw bad_cast");
}

NS_ALWAYS_INLINE inline void NS_NORETURN
__throw_bad_typeid(void)
{
    mozalloc_abort("fatal: STL threw bad_typeid");
}

NS_ALWAYS_INLINE inline void NS_NORETURN
__throw_logic_error(const char* msg)
{
    mozalloc_abort(msg);
}

NS_ALWAYS_INLINE inline void NS_NORETURN
__throw_domain_error(const char* msg)
{
    mozalloc_abort(msg);
}

NS_ALWAYS_INLINE inline void NS_NORETURN
__throw_invalid_argument(const char* msg) 
{
    mozalloc_abort(msg);
}

NS_ALWAYS_INLINE inline void NS_NORETURN
__throw_length_error(const char* msg) 
{
    mozalloc_abort(msg);
}

NS_ALWAYS_INLINE inline void NS_NORETURN
__throw_out_of_range(const char* msg) 
{
    mozalloc_abort(msg);
}

NS_ALWAYS_INLINE inline void NS_NORETURN
__throw_runtime_error(const char* msg) 
{
    mozalloc_abort(msg);
}

NS_ALWAYS_INLINE inline void NS_NORETURN
__throw_range_error(const char* msg) 
{
    mozalloc_abort(msg);
}

NS_ALWAYS_INLINE inline void NS_NORETURN
__throw_overflow_error(const char* msg) 
{
    mozalloc_abort(msg);
}

NS_ALWAYS_INLINE inline void NS_NORETURN
__throw_underflow_error(const char* msg) 
{
    mozalloc_abort(msg);
}

NS_ALWAYS_INLINE inline void NS_NORETURN
__throw_ios_failure(const char* msg) 
{
    mozalloc_abort(msg);
}

NS_ALWAYS_INLINE inline void NS_NORETURN
__throw_system_error(int err) 
{
    char error[128];
    snprintf(error, sizeof(error)-1,
             "fatal: STL threw system_error: %s (%d)", strerror(err), err);
    mozalloc_abort(error);
}

} // namespace std

#endif  // mozilla_throw_gcc_h
