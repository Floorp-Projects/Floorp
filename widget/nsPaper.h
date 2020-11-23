/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPaper_h__
#define nsPaper_h__

#include "mozilla/dom/ToJSValue.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/Maybe.h"
#include "nsIPaper.h"
#include "nsISupportsImpl.h"
#include "js/TypeDecls.h"
#include "nsString.h"

struct JSContext;

namespace mozilla {

// Simple struct that can be used off the main thread to hold all the info from
// an nsPaper instance.
struct PaperInfo {
  using MarginDouble = mozilla::gfx::MarginDouble;
  using SizeDouble = mozilla::gfx::SizeDouble;

  PaperInfo() = default;
  PaperInfo(const nsAString& aId, const nsAString& aName,
            const SizeDouble& aSize,
            const Maybe<MarginDouble>& aUnwriteableMargin)
      : mId(aId),
        mName(aName),
        mSize(aSize),
        mUnwriteableMargin(aUnwriteableMargin) {}

  nsString mId;
  nsString mName;

  SizeDouble mSize;

  // The margins may not be known by some back-ends.
  Maybe<MarginDouble> mUnwriteableMargin{Nothing()};
};

/**
 * Plain struct used for commonly used, hard-coded paper sizes.
 *
 * Used to construct PaperInfo at runtime by localizing the name.
 */
struct CommonPaperSize final {
  // The standardized PWG name, which should be used as the PaperInfo id
  nsLiteralString mPWGName;
  // The name key to localize the name of this paper size using strings in
  // printUI.ftl
  nsLiteralCString mLocalizableNameKey;
  // Size is in points, same as PaperInfo
  gfx::SizeDouble mSize;
};

}  // namespace mozilla

class nsPrinterBase;

class nsPaper final : public nsIPaper {
  using Promise = mozilla::dom::Promise;
  using CommonPaperSize = mozilla::CommonPaperSize;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsPaper)
  NS_DECL_NSIPAPER

  nsPaper() = delete;
  explicit nsPaper(const mozilla::PaperInfo&);
  nsPaper(nsPrinterBase&, const mozilla::PaperInfo&);

  // This list is used for both our fallback paper sizes (used when a printer
  // does not provide a list of available paper sizes), and for localizing
  // common paper sizes to avoid querying the printer for extra information in
  // the common case, as well as to provide more uniform paper names in the
  // frontend.
  // If we need to separate these two uses, we will need to either split this
  // into two lists, or add a flag to indicate what the size is used for.
#define mm *72.0 / 25.4
#define in *72.0
  static constexpr CommonPaperSize kCommonPaperSizes[] = {
      CommonPaperSize{u"iso_a5"_ns, "a5"_ns, {148 mm, 210 mm}},
      CommonPaperSize{u"iso_a4"_ns, "a4"_ns, {210 mm, 297 mm}},
      CommonPaperSize{u"iso_a3"_ns, "a3"_ns, {297 mm, 420 mm}},
      CommonPaperSize{u"iso_b5"_ns, "b5"_ns, {176 mm, 250 mm}},
      CommonPaperSize{u"iso_b4"_ns, "b4"_ns, {250 mm, 353 mm}},
      CommonPaperSize{u"jis_b5"_ns, "jis-b5"_ns, {182 mm, 257 mm}},
      CommonPaperSize{u"jis_b4"_ns, "jis-b4"_ns, {257 mm, 364 mm}},
      CommonPaperSize{u"na_letter"_ns, "letter"_ns, {8.5 in, 11 in}},
      CommonPaperSize{u"na_legal"_ns, "legal"_ns, {8.5 in, 14 in}},
      CommonPaperSize{u"na_ledger"_ns, "tabloid"_ns, {11 in, 17 in}}};
#undef mm
#undef in
  static constexpr size_t kNumCommonPaperSizes =
      mozilla::ArrayLength(kCommonPaperSizes);

 private:
  ~nsPaper();

  // null if not associated with a printer (for "Save-to-PDF" paper sizes)
  RefPtr<nsPrinterBase> mPrinter;

  RefPtr<Promise> mMarginPromise;
  const mozilla::PaperInfo mInfo;
};

namespace mozilla {

// Used to allow fixed-sized arrays of PWG paper info to be ref-counted.
class CommonPaperInfoArray
    : public Array<PaperInfo, nsPaper::kNumCommonPaperSizes> {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CommonPaperInfoArray);
  CommonPaperInfoArray() = default;

 private:
  ~CommonPaperInfoArray() = default;
};

}  // namespace mozilla

#endif /* nsPaper_h__ */
