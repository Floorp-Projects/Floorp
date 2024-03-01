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

  NS_IMETHOD CopyFromWithValidation(nsIPrintSettings*, JSContext*,
                                    Promise**) override;
  NS_IMETHOD GetSupportsDuplex(JSContext*, Promise**) final;
  NS_IMETHOD GetSupportsColor(JSContext*, Promise**) final;
  NS_IMETHOD GetSupportsMonochrome(JSContext*, Promise**) final;
  NS_IMETHOD GetSupportsCollation(JSContext*, Promise**) final;
  NS_IMETHOD GetPrinterInfo(JSContext*, Promise**) final;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsPrinterBase)

  // We require the paper info array
  nsPrinterBase() = delete;

  // No copy or move, we're an identity.
  nsPrinterBase(const nsPrinterBase&) = delete;
  nsPrinterBase(nsPrinterBase&&) = delete;

  void QueryMarginsForPaper(Promise&, const nsString& aPaperId);

  /**
   * Caches the argument by copying it into mPrintSettingsInitializer.
   * If mPrintSettingsInitializer is already populated this is a no-op.
   */
  void CachePrintSettingsInitializer(
      const PrintSettingsInitializer& aInitializer);

  // Data to pass through to create the nsPrinterInfo
  struct PrinterInfo {
    nsTArray<mozilla::PaperInfo> mPaperList;
    PrintSettingsInitializer mDefaultSettings;
  };

 private:
  enum class AsyncAttribute {
    // If you change this list you must update attributeKeys in
    // nsPrinterBase::AsyncPromiseAttributeGetter.
    SupportsDuplex = 0,
    SupportsColor,
    SupportsMonochrome,
    SupportsCollation,
    PrinterInfo,
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

 protected:
  nsPrinterBase(const mozilla::CommonPaperInfoArray* aPaperInfoArray);
  virtual ~nsPrinterBase();

  // Implementation-specific methods. These must not make assumptions about
  // which thread they run on.
  virtual bool SupportsDuplex() const = 0;
  virtual bool SupportsColor() const = 0;
  virtual bool SupportsMonochrome() const = 0;
  virtual bool SupportsCollation() const = 0;
  virtual MarginDouble GetMarginsForPaper(nsString aPaperId) const = 0;
  virtual PrinterInfo CreatePrinterInfo() const = 0;
  // Searches our built-in list of commonly used PWG paper sizes for a matching,
  // localized PaperInfo. Returns null if there is no matching size.
  const mozilla::PaperInfo* FindCommonPaperSize(
      const mozilla::gfx::SizeDouble& aSize) const;

 private:
  mozilla::EnumeratedArray<AsyncAttribute, RefPtr<Promise>,
                           size_t(AsyncAttribute::Last)>
      mAsyncAttributePromises;
  // List of built-in, commonly used paper sizes.
  const RefPtr<const mozilla::CommonPaperInfoArray> mCommonPaperInfo;
};

#endif
