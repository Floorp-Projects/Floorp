/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrinterBase_h__
#define nsPrinterBase_h__

#include "nsIPrinter.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/Result.h"

namespace mozilla {

struct PaperInfo;

namespace dom {
class Promise;
}

}  // namespace mozilla

class nsPrinterBase : public nsIPrinter {
 public:
  using Promise = mozilla::dom::Promise;

  NS_IMETHOD GetSupportsDuplex(JSContext*, Promise**) final;
  NS_IMETHOD GetSupportsColor(JSContext*, Promise**) final;
  NS_IMETHOD GetPaperList(JSContext*, Promise**) final;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsPrinterBase)

  // No copy or move, we're an identity.
  nsPrinterBase(const nsPrinterBase&) = delete;
  nsPrinterBase(nsPrinterBase&&) = delete;

 private:
  enum class AsyncAttribute {
    SupportsDuplex = 0,
    SupportsColor,
    PaperList,
    // Just a guard.
    Last,
  };

  template <typename T>
  using AsyncAttributeBackgroundTask = T (nsPrinterBase::*)() const;

  // Resolves an async attribute via a background task.
  template <typename T>
  nsresult AsyncPromiseAttributeGetter(JSContext* aCx, Promise** aResultPromise,
                                       AsyncAttribute,
                                       AsyncAttributeBackgroundTask<T>);

 protected:
  nsPrinterBase();
  virtual ~nsPrinterBase();

  // Implementation-specific methods. These must not make assumptions about
  // which thread they run on.
  virtual bool SupportsDuplex() const = 0;
  virtual bool SupportsColor() const = 0;
  virtual nsTArray<mozilla::PaperInfo> PaperList() const = 0;

 private:
  mozilla::EnumeratedArray<AsyncAttribute, AsyncAttribute::Last,
                           RefPtr<Promise>>
      mAsyncAttributePromises;
};

#endif
