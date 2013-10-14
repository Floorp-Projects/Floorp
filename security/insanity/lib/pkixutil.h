/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Copyright 2013 Mozilla Foundation
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

#ifndef insanity_pkix__pkixutil_h
#define insanity_pkix__pkixutil_h

#include "insanity/pkixtypes.h"
#include "prerror.h"
#include "seccomon.h"
#include "secerr.h"

namespace insanity { namespace pkix {

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

    case SEC_ERROR_LIBRARY_FAILURE:
    case SEC_ERROR_NO_MEMORY:
      return FatalError;
  }

  // TODO: PORT_Assert(false); // we haven't classified the error yet
  return RecoverableError;
}
} } // namespace insanity::pkix

#endif // insanity_pkix__pkixutil_h
