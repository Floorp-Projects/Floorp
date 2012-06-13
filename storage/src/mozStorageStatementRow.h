/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZSTORAGESTATEMENTROW_H
#define MOZSTORAGESTATEMENTROW_H

#include "mozIStorageStatementRow.h"
#include "nsIXPCScriptable.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace storage {

class Statement;

class StatementRow MOZ_FINAL : public mozIStorageStatementRow
                             , public nsIXPCScriptable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGESTATEMENTROW
  NS_DECL_NSIXPCSCRIPTABLE

  StatementRow(Statement *aStatement);
protected:

  Statement *mStatement;

  friend class Statement;
};

} // namespace storage
} // namespace mozilla

#endif /* MOZSTORAGESTATEMENTROW_H */
