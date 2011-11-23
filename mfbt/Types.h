/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
 * NB: This header must be both valid C and C++.  It must be
 * include-able by code embedding SpiderMonkey *and* Gecko.
 */

#ifndef mozilla_Types_h_
#define mozilla_Types_h_

/* 
 * mfbt is logically "lower level" than js/src, but needs basic
 * definitions of numerical types and macros for compiler/linker
 * directives.  js/src already goes through some pain to provide them
 * on numerous platforms, so instead of moving all that goop here,
 * this header makes use of the fact that for the foreseeable future
 * mfbt code will be part and parcel with libmozjs, static or not.
 *
 * For now, the policy is to use jstypes definitions but add a layer
 * of indirection on top of them in case a Great Refactoring ever
 * happens.
 */
#include "jstypes.h"

/*
 * The numerical types provided by jstypes.h that are allowed within
 * mfbt code are
 *
 *   stddef types:  size_t, ptrdiff_t, etc.
 *   stdin [sic] types:  int8, uint32, etc.
 *
 * stdint types (int8_t etc.), are available for use here, but doing
 * so would change SpiderMonkey's and Gecko's contracts with
 * embedders: stdint types have not yet appeared in public APIs.
 */

#define MOZ_EXPORT_API(type_)  JS_EXPORT_API(type_)
#define MOZ_IMPORT_API(type_)  JS_IMPORT_API(type_)

/*
 * mfbt definitions need to see export declarations when built, but
 * other code needs to see import declarations when using mfbt.
 */
#if defined(IMPL_MFBT)
#  define MFBT_API(type_)       MOZ_EXPORT_API(type_)
#else
#  define MFBT_API(type_)       MOZ_IMPORT_API(type_)
#endif


#define MOZ_BEGIN_EXTERN_C     JS_BEGIN_EXTERN_C
#define MOZ_END_EXTERN_C       JS_END_EXTERN_C

#endif  /* mozilla_Types_h_ */
