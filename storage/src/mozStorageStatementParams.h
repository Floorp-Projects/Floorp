/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZSTORAGESTATEMENTPARAMS_H
#define MOZSTORAGESTATEMENTPARAMS_H

#include "mozIStorageStatementParams.h"
#include "nsIXPCScriptable.h"

class mozIStorageStatement;

namespace mozilla {
namespace storage {

class Statement;

class StatementParams : public mozIStorageStatementParams
                      , public nsIXPCScriptable
{
public:
  StatementParams(mozIStorageStatement *aStatement);

  // interfaces
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGESTATEMENTPARAMS
  NS_DECL_NSIXPCSCRIPTABLE

protected:
  mozIStorageStatement *mStatement;
  PRUint32 mParamCount;

  friend class Statement;
};

} // namespace storage
} // namespace mozilla

#endif /* MOZSTORAGESTATEMENTPARAMS_H */
