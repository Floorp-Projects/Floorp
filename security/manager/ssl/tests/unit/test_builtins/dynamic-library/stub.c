/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "pkcs11.h"

// see notes in ipcclientcerts/dynamic-library/stub.c

CK_RV BUILTINSC_GetFunctionList(CK_FUNCTION_LIST_PTR_PTR ppFunctionList);

CK_RV C_GetFunctionList(CK_FUNCTION_LIST_PTR_PTR ppFunctionList) {
  return BUILTINSC_GetFunctionList(ppFunctionList);
}

#ifdef __MINGW32__
#  include "mozilla/Assertions.h"
void _Unwind_Resume() { MOZ_CRASH("Unexpected call to _Unwind_*"); }
void _Unwind_GetDataRelBase() { _Unwind_Resume(); }
void _Unwind_GetTextRelBase() { _Unwind_Resume(); }
void _Unwind_GetLanguageSpecificData() { _Unwind_Resume(); }
void _Unwind_GetIPInfo() { _Unwind_Resume(); }
void _Unwind_GetRegionStart() { _Unwind_Resume(); }
void _Unwind_SetGR() { _Unwind_Resume(); }
void _Unwind_SetIP() { _Unwind_Resume(); }
void _GCC_specific_handler() { _Unwind_Resume(); }
#endif
