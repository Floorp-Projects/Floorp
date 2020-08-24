/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrinterBase_h__
#define nsPrinterBase_h__

#include "mozilla/gfx/Rect.h"
#include "nsIPrinter.h"
#include "nsTArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "nsPrintSettingsImpl.h"
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
  using MarginDouble = mozilla::gfx::MarginDouble;
  using PrintSettingsInitializer = mozilla::PrintSettingsInitializer;

  NS_IMETHOD CreateDefaultSettings(JSContext*, Promise**) final;
  NS_IMETHOD GetSupportsDuplex(JSContext*, Promise**) final;
  NS_IMETHOD GetSupportsColor(JSContext*, Promise**) final;
  NS_IMETHOD GetSupportsCollation(JSContext*, Promise**) final;
  NS_IMETHOD GetPaperList(JSContext*, Promise**) final;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsPrinterBase)

  // No copy or move, we're an identity.
  nsPrinterBase(const nsPrinterBase&) = delete;
  nsPrinterBase(nsPrinterBase&&) = delete;

  void QueryMarginsForPaper(Promise&, uint64_t aPaperId);

  /**
   * Caches the argument by copying it into mPrintSettingsInitializer.
   * If mPrintSettingsInitializer is already populated this is a no-op.
   */
  void CachePrintSettingsInitializer(
      const PrintSettingsInitializer& aInitializer);

 private:
  enum class AsyncAttribute {
    SupportsDuplex = 0,
    SupportsColor,
    SupportsCollation,
    PaperList,
    // Just a guard.
    Last,
  };

  template <typename Result, typename... Args>
  using BackgroundTask = Result (nsPrinterBase::*)(Args...) const;

  // Resolves an async attribute via a background task.
  template <typename T, typename... Args>
  nsresult AsyncPromiseAttributeGetter(JSContext*, Promise**, AsyncAttribute,
                                       BackgroundTask<T, Args...>,
                                       Args... aArgs);

  /**
   * A cache to store the result of DefaultSettings() to ensure
   * that subsequent calls to CreateDefaultSettings() will not
   * have to spawn another background task to fetch the same info.
   */
  Maybe<PrintSettingsInitializer> mPrintSettingsInitializer;

 protected:
  nsPrinterBase();
  virtual ~nsPrinterBase();

  // Implementation-specific methods. These must not make assumptions about
  // which thread they run on.
  virtual PrintSettingsInitializer DefaultSettings() const = 0;
  virtual bool SupportsDuplex() const = 0;
  virtual bool SupportsColor() const = 0;
  virtual bool SupportsCollation() const = 0;
  virtual nsTArray<mozilla::PaperInfo> PaperList() const = 0;
  virtual MarginDouble GetMarginsForPaper(uint64_t aPaperId) const = 0;

 private:
  mozilla::EnumeratedArray<AsyncAttribute, AsyncAttribute::Last,
                           RefPtr<Promise>>
      mAsyncAttributePromises;
};

#endif
