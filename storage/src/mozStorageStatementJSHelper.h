/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZSTORAGESTATEMENTJSHELPER_H
#define MOZSTORAGESTATEMENTJSHELPER_H

#include "nsIXPCScriptable.h"
#include "nsIXPConnect.h"

class Statement;

namespace mozilla {
namespace storage {

class StatementJSHelper : public nsIXPCScriptable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPCSCRIPTABLE

private:
  nsresult getRow(Statement *, JSContext *, JSObject *, JS::Value *);
  nsresult getParams(Statement *, JSContext *, JSObject *, JS::Value *);
};

/**
 * Wrappers used to clean up the references JS helpers hold to the statement.
 * For cycle-avoidance reasons they do not hold reference-counted references,
 * so it is important we do this.
 */
class StatementJSObjectHolder : public nsIXPConnectJSObjectHolder
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPCONNECTJSOBJECTHOLDER

  explicit StatementJSObjectHolder(nsIXPConnectJSObjectHolder* aHolder);

protected:
  virtual ~StatementJSObjectHolder() {};
  nsCOMPtr<nsIXPConnectJSObjectHolder> mHolder;
};

class StatementParamsHolder final: public StatementJSObjectHolder {
public:
  explicit StatementParamsHolder(nsIXPConnectJSObjectHolder* aHolder)
    : StatementJSObjectHolder(aHolder) {
  }

private:
  virtual ~StatementParamsHolder();
};

class StatementRowHolder final: public StatementJSObjectHolder {
public:
  explicit StatementRowHolder(nsIXPConnectJSObjectHolder* aHolder)
    : StatementJSObjectHolder(aHolder) {
  }

private:
  virtual ~StatementRowHolder();
};

} // namespace storage
} // namespace mozilla

#endif // MOZSTORAGESTATEMENTJSHELPER_H
