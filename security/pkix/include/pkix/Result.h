/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2013 Mozilla Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef mozilla_pkix__Result_h
#define mozilla_pkix__Result_h

#include "prerror.h"
#include "seccomon.h"
#include "secerr.h"

namespace mozilla { namespace pkix {

enum Result
{
  Success = 0,
  FatalError = -1,      // An error was encountered that caused path building
                        // to stop immediately. example: out-of-memory.
  RecoverableError = -2 // an error that will cause path building to continue
                        // searching for alternative paths. example: expired
                        // certificate.
};

// When returning errors, use this function instead of calling PR_SetError
// directly. This helps ensure that we always call PR_SetError when we return
// an error code. This is a useful place to set a breakpoint when a debugging
// a certificate verification failure.
inline Result
Fail(Result result, PRErrorCode errorCode)
{
  PR_ASSERT(result != Success);
  PR_SetError(errorCode, 0);
  return result;
}

inline Result
MapSECStatus(SECStatus srv)
{
  if (srv == SECSuccess) {
    return Success;
  }

  PRErrorCode error = PORT_GetError();
  switch (error) {
    case SEC_ERROR_EXTENSION_NOT_FOUND:
      return RecoverableError;

    case PR_INVALID_STATE_ERROR:
    case SEC_ERROR_INVALID_ARGS:
    case SEC_ERROR_LIBRARY_FAILURE:
    case SEC_ERROR_NO_MEMORY:
      return FatalError;
  }

  // TODO: PORT_Assert(false); // we haven't classified the error yet
  return RecoverableError;
}

} } // namespace mozilla::pkix

#endif // mozilla_pkix__Result_h
