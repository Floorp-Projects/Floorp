/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteLookAndFeel.h"

#include "gfxFont.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "nsXULAppAPI.h"

#include <limits>
#include <type_traits>
#include <utility>

namespace mozilla::widget {

RemoteLookAndFeel::RemoteLookAndFeel(FullLookAndFeel&& aTables)
    : mTables(std::move(aTables)) {
  MOZ_ASSERT(XRE_IsContentProcess(),
             "Only content processes should be using a RemoteLookAndFeel");
}

RemoteLookAndFeel::~RemoteLookAndFeel() = default;

void RemoteLookAndFeel::SetDataImpl(FullLookAndFeel&& aTables) {
  MOZ_ASSERT(XRE_IsContentProcess(),
             "Only content processes should be using a RemoteLookAndFeel");
  MOZ_ASSERT(NS_IsMainThread());
  mTables = std::move(aTables);
}

namespace {

template <typename Item, typename UInt, typename ID>
Result<const Item*, nsresult> MapLookup(const nsTArray<Item>& aItems,
                                        const nsTArray<UInt>& aMap, ID aID,
                                        ID aMinimum = ID(0)) {
  UInt mapped = aMap[static_cast<size_t>(aID) - static_cast<size_t>(aMinimum)];

  if (mapped == std::numeric_limits<UInt>::max()) {
    return Err(NS_ERROR_NOT_IMPLEMENTED);
  }

  return &aItems[static_cast<size_t>(mapped)];
}

template <typename Item, typename UInt>
void AddToMap(nsTArray<Item>* aItems, nsTArray<UInt>* aMap,
              Maybe<Item>&& aNewItem) {
  if (aNewItem.isNothing()) {
    aMap->AppendElement(std::numeric_limits<UInt>::max());
    return;
  }

  size_t newIndex = aItems->Length();
  MOZ_ASSERT(newIndex < std::numeric_limits<UInt>::max());

  // Check if there is an existing value in aItems that we can point to.
  //
  // The arrays should be small enough and contain few enough unique
  // values that sequential search here is reasonable.
  for (size_t i = 0; i < newIndex; ++i) {
    if ((*aItems)[i] == aNewItem.ref()) {
      aMap->AppendElement(static_cast<UInt>(i));
      return;
    }
  }

  aItems->AppendElement(aNewItem.extract());
  aMap->AppendElement(static_cast<UInt>(newIndex));
}

}  // namespace

nsresult RemoteLookAndFeel::NativeGetColor(ColorID aID, nscolor& aResult) {
  const nscolor* result;
  MOZ_TRY_VAR(result, MapLookup(mTables.colors(), mTables.colorMap(), aID));
  aResult = *result;
  return NS_OK;
}

nsresult RemoteLookAndFeel::NativeGetInt(IntID aID, int32_t& aResult) {
  const int32_t* result;
  MOZ_TRY_VAR(result, MapLookup(mTables.ints(), mTables.intMap(), aID));
  aResult = *result;
  return NS_OK;
}

nsresult RemoteLookAndFeel::NativeGetFloat(FloatID aID, float& aResult) {
  const float* result;
  MOZ_TRY_VAR(result, MapLookup(mTables.floats(), mTables.floatMap(), aID));
  aResult = *result;
  return NS_OK;
}

bool RemoteLookAndFeel::NativeGetFont(FontID aID, nsString& aFontName,
                                      gfxFontStyle& aFontStyle) {
  auto result =
      MapLookup(mTables.fonts(), mTables.fontMap(), aID, FontID::MINIMUM);
  if (result.isErr()) {
    return false;
  }

  const LookAndFeelFont& font = *result.unwrap();
  MOZ_ASSERT(font.haveFont());
  aFontName = font.name();
  aFontStyle = gfxFontStyle();
  aFontStyle.size = font.size();
  aFontStyle.weight = FontWeight(font.weight());
  aFontStyle.style =
      font.italic() ? FontSlantStyle::Italic() : FontSlantStyle::Normal();

  return true;
}

char16_t RemoteLookAndFeel::GetPasswordCharacterImpl() {
  return static_cast<char16_t>(mTables.passwordChar());
}

bool RemoteLookAndFeel::GetEchoPasswordImpl() { return mTables.passwordEcho(); }

// static
const FullLookAndFeel* RemoteLookAndFeel::ExtractData() {
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Only parent processes should be extracting LookAndFeel data");

  if (sCachedTables) {
    return sCachedTables;
  }

  static bool sInitialized = false;
  if (!sInitialized) {
    sInitialized = true;
    ClearOnShutdown(&sCachedTables);
  }

  FullLookAndFeel* lf = new FullLookAndFeel{};
  nsXPLookAndFeel* impl = nsXPLookAndFeel::GetInstance();

  impl->WithThemeConfiguredForContent([&]() {
    for (auto id : MakeEnumeratedRange(IntID::End)) {
      int32_t theInt;
      nsresult rv = impl->NativeGetInt(id, theInt);
      AddToMap(&lf->ints(), &lf->intMap(),
               NS_SUCCEEDED(rv) ? Some(theInt) : Nothing{});
    }

    for (auto id : MakeEnumeratedRange(FloatID::End)) {
      float theFloat;
      nsresult rv = impl->NativeGetFloat(id, theFloat);
      AddToMap(&lf->floats(), &lf->floatMap(),
               NS_SUCCEEDED(rv) ? Some(theFloat) : Nothing{});
    }

    for (auto id : MakeEnumeratedRange(ColorID::End)) {
      nscolor theColor;
      nsresult rv = impl->NativeGetColor(id, theColor);
      AddToMap(&lf->colors(), &lf->colorMap(),
               NS_SUCCEEDED(rv) ? Some(theColor) : Nothing{});
    }

    for (auto id :
         MakeInclusiveEnumeratedRange(FontID::MINIMUM, FontID::MAXIMUM)) {
      LookAndFeelFont font{};
      gfxFontStyle fontStyle{};

      bool rv = impl->NativeGetFont(id, font.name(), fontStyle);
      Maybe<LookAndFeelFont> maybeFont;
      if (rv) {
        font.haveFont() = true;
        font.size() = fontStyle.size;
        font.weight() = fontStyle.weight.ToFloat();
        font.italic() = fontStyle.style.IsItalic();
        MOZ_ASSERT(fontStyle.style.IsNormal() || fontStyle.style.IsItalic(),
                   "Cannot handle oblique font style");
#ifdef DEBUG
        {
          // Assert that all the remaining font style properties have their
          // default values.
          gfxFontStyle candidate = fontStyle;
          gfxFontStyle defaults{};
          candidate.size = defaults.size;
          candidate.weight = defaults.weight;
          candidate.style = defaults.style;
          MOZ_ASSERT(candidate.Equals(defaults),
                     "Some font style properties not supported");
        }
#endif
        maybeFont = Some(std::move(font));
      }
      AddToMap(&lf->fonts(), &lf->fontMap(), std::move(maybeFont));
    }

    lf->passwordChar() = impl->GetPasswordCharacterImpl();
    lf->passwordEcho() = impl->GetEchoPasswordImpl();
  });

  // This assignment to sCachedTables must be done after the
  // WithThemeConfiguredForContent call, since it can end up calling RefreshImpl
  // on the LookAndFeel, which will clear out sCachedTables.
  sCachedTables = lf;
  return sCachedTables;
}

void RemoteLookAndFeel::ClearCachedData() {
  MOZ_ASSERT(XRE_IsParentProcess());
  sCachedTables = nullptr;
}

StaticAutoPtr<FullLookAndFeel> RemoteLookAndFeel::sCachedTables;

}  // namespace mozilla::widget
