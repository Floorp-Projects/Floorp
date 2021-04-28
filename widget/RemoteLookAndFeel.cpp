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
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/StaticPrefs_widget.h"
#include "nsLookAndFeel.h"
#include "nsXULAppAPI.h"

#include <limits>
#include <type_traits>
#include <utility>

namespace mozilla::widget {

// A cached copy of the data extracted by ExtractData.
//
// Storing this lets us avoid doing most of the work of ExtractData each
// time we create a new content process.
//
// Only used in the parent process.
static StaticAutoPtr<FullLookAndFeel> sCachedLookAndFeelData;

RemoteLookAndFeel::RemoteLookAndFeel(FullLookAndFeel&& aData)
    : mTables(std::move(aData.tables())) {
  MOZ_ASSERT(XRE_IsContentProcess(),
             "Only content processes should be using a RemoteLookAndFeel");

#ifdef MOZ_WIDGET_GTK
  if (!StaticPrefs::widget_non_native_theme_enabled()) {
    // Configure the theme in this content process with the Gtk theme that was
    // chosen by WithThemeConfiguredForContent in the parent process.
    nsLookAndFeel::ConfigureTheme(aData.theme());
  }
#endif
}

RemoteLookAndFeel::~RemoteLookAndFeel() = default;

void RemoteLookAndFeel::SetDataImpl(FullLookAndFeel&& aData) {
  MOZ_ASSERT(XRE_IsContentProcess(),
             "Only content processes should be using a RemoteLookAndFeel");
  MOZ_ASSERT(NS_IsMainThread());
  mTables = std::move(aData.tables());

#ifdef MOZ_WIDGET_GTK
  if (!StaticPrefs::widget_non_native_theme_enabled()) {
    // Configure the theme in this content process with the Gtk theme that was
    // chosen by WithThemeConfiguredForContent in the parent process.
    nsLookAndFeel::ConfigureTheme(aData.theme());
  }
#endif
}

namespace {

// Some lnf values are somewhat expensive to get, and are not needed in child
// processes, so we can avoid querying them.
bool IsNeededInChildProcess(LookAndFeel::IntID aId) {
  switch (aId) {
    case LookAndFeel::IntID::AlertNotificationOrigin:
      return false;  // see bug 1703205
    default:
      return true;
  }
}

template <typename Item, typename UInt, typename ID>
Result<const Item*, nsresult> MapLookup(const nsTArray<Item>& aItems,
                                        const nsTArray<UInt>& aMap, ID aID) {
  UInt mapped = aMap[static_cast<size_t>(aID)];

  if (mapped == std::numeric_limits<UInt>::max()) {
    return Err(NS_ERROR_NOT_IMPLEMENTED);
  }

  return &aItems[static_cast<size_t>(mapped)];
}

template <typename Item, typename UInt, typename Id>
void AddToMap(nsTArray<Item>& aItems, nsTArray<UInt>& aMap, Id aId,
              Maybe<Item>&& aNewItem) {
  auto mapIndex = size_t(aId);
  aMap.EnsureLengthAtLeast(mapIndex + 1);
  if (aNewItem.isNothing()) {
    aMap[mapIndex] = std::numeric_limits<UInt>::max();
    return;
  }

  size_t newIndex = aItems.Length();
  MOZ_ASSERT(newIndex < std::numeric_limits<UInt>::max());

  // Check if there is an existing value in aItems that we can point to.
  //
  // The arrays should be small enough and contain few enough unique
  // values that sequential search here is reasonable.
  for (size_t i = 0; i < newIndex; ++i) {
    if (aItems[i] == aNewItem.ref()) {
      aMap[mapIndex] = static_cast<UInt>(i);
      return;
    }
  }

  aItems.AppendElement(aNewItem.extract());
  aMap[mapIndex] = static_cast<UInt>(newIndex);
}

}  // namespace

nsresult RemoteLookAndFeel::NativeGetColor(ColorID aID, ColorScheme aScheme,
                                           nscolor& aResult) {
  const nscolor* result;
  const bool dark = aScheme == ColorScheme::Dark;
  MOZ_TRY_VAR(
      result,
      MapLookup(dark ? mTables.darkColors() : mTables.lightColors(),
                dark ? mTables.darkColorMap() : mTables.lightColorMap(), aID));
  aResult = *result;
  return NS_OK;
}

nsresult RemoteLookAndFeel::NativeGetInt(IntID aID, int32_t& aResult) {
  MOZ_DIAGNOSTIC_ASSERT(
      IsNeededInChildProcess(aID),
      "Querying value that we didn't bother getting from the parent process!");

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
  auto result = MapLookup(mTables.fonts(), mTables.fontMap(), aID);
  if (result.isErr()) {
    return false;
  }
  const LookAndFeelFont& font = *result.unwrap();
  return LookAndFeelFontToStyle(font, aFontName, aFontStyle);
}

char16_t RemoteLookAndFeel::GetPasswordCharacterImpl() {
  return static_cast<char16_t>(mTables.passwordChar());
}

bool RemoteLookAndFeel::GetEchoPasswordImpl() { return mTables.passwordEcho(); }

static bool AddIDsToMap(nsXPLookAndFeel* aImpl, FullLookAndFeel* aLf,
                        bool aDifferentTheme, bool aFromParentTheme) {
  using IntID = LookAndFeel::IntID;
  using FontID = LookAndFeel::FontID;
  using FloatID = LookAndFeel::FloatID;
  using ColorID = LookAndFeel::ColorID;
  using ColorScheme = LookAndFeel::ColorScheme;

  bool anyFromOtherTheme = false;
  for (auto id : MakeEnumeratedRange(IntID::End)) {
    if (!IsNeededInChildProcess(id)) {
      continue;
    }
    if (aDifferentTheme && aImpl->FromParentTheme(id) != aFromParentTheme) {
      anyFromOtherTheme = true;
      continue;
    }
    int32_t theInt;
    nsresult rv = aImpl->NativeGetInt(id, theInt);
    AddToMap(aLf->tables().ints(), aLf->tables().intMap(), id,
             NS_SUCCEEDED(rv) ? Some(theInt) : Nothing{});
  }

  for (auto id : MakeEnumeratedRange(ColorID::End)) {
    if (aDifferentTheme && aImpl->FromParentTheme(id) != aFromParentTheme) {
      anyFromOtherTheme = true;
      continue;
    }
    nscolor theColor;
    nsresult rv = aImpl->NativeGetColor(id, ColorScheme::Light, theColor);
    AddToMap(aLf->tables().lightColors(), aLf->tables().lightColorMap(), id,
             NS_SUCCEEDED(rv) ? Some(theColor) : Nothing{});
    rv = aImpl->NativeGetColor(id, ColorScheme::Dark, theColor);
    AddToMap(aLf->tables().darkColors(), aLf->tables().darkColorMap(), id,
             NS_SUCCEEDED(rv) ? Some(theColor) : Nothing{});
  }

  // The rest of IDs only come from the child content theme.
  if (aFromParentTheme) {
    return anyFromOtherTheme;
  }

  for (auto id : MakeEnumeratedRange(FloatID::End)) {
    float theFloat;
    nsresult rv = aImpl->NativeGetFloat(id, theFloat);
    AddToMap(aLf->tables().floats(), aLf->tables().floatMap(), id,
             NS_SUCCEEDED(rv) ? Some(theFloat) : Nothing{});
  }

  for (auto id : MakeEnumeratedRange(FontID::End)) {
    gfxFontStyle fontStyle{};

    nsString name;
    bool rv = aImpl->NativeGetFont(id, name, fontStyle);
    Maybe<LookAndFeelFont> maybeFont;
    if (rv) {
      maybeFont.emplace(
          nsXPLookAndFeel::StyleToLookAndFeelFont(name, fontStyle));
    }
    AddToMap(aLf->tables().fonts(), aLf->tables().fontMap(), id,
             std::move(maybeFont));
  }

  return anyFromOtherTheme;
}

// static
const FullLookAndFeel* RemoteLookAndFeel::ExtractData() {
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Only parent processes should be extracting LookAndFeel data");

  if (sCachedLookAndFeelData) {
    return sCachedLookAndFeelData;
  }

  static bool sInitialized = false;
  if (!sInitialized) {
    sInitialized = true;
    ClearOnShutdown(&sCachedLookAndFeelData);
  }

  FullLookAndFeel* lf = new FullLookAndFeel{};
  nsXPLookAndFeel* impl = nsXPLookAndFeel::GetInstance();

  bool anyFromParent = false;
  impl->WithThemeConfiguredForContent([&](const LookAndFeelTheme& aTheme,
                                          bool aDifferentTheme) {
    anyFromParent =
        AddIDsToMap(impl, lf, aDifferentTheme, /* aFromParentTheme = */ false);
    MOZ_ASSERT_IF(anyFromParent, aDifferentTheme);
    lf->tables().passwordChar() = impl->GetPasswordCharacterImpl();
    lf->tables().passwordEcho() = impl->GetEchoPasswordImpl();
#ifdef MOZ_WIDGET_GTK
    lf->theme() = aTheme;
#endif
  });

  if (anyFromParent) {
    AddIDsToMap(impl, lf, /* aDifferentTheme = */ true,
                /* aFromParentTheme = */ true);
  }

  // This assignment to sCachedLookAndFeelData must be done after the
  // WithThemeConfiguredForContent call, since it can end up calling RefreshImpl
  // on the LookAndFeel, which will clear out sCachedTables.
  sCachedLookAndFeelData = lf;
  return sCachedLookAndFeelData;
}

void RemoteLookAndFeel::ClearCachedData() {
  MOZ_ASSERT(XRE_IsParentProcess());
  sCachedLookAndFeelData = nullptr;
}

}  // namespace mozilla::widget
