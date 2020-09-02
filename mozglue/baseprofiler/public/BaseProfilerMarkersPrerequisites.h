/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This header contains basic definitions required to create marker types, and
// to add markers to the profiler buffers.
//
// In most cases, #include "mozilla/BaseProfilerMarkers.h" instead, or
// #include "mozilla/BaseProfilerMarkerTypes.h" for common marker types.

#ifndef BaseProfilerMarkersPrerequisites_h
#define BaseProfilerMarkersPrerequisites_h

#ifdef MOZ_GECKO_PROFILER

#  include "mozilla/ProfileChunkedBuffer.h"

#  include <string_view>
#  include <string>
#  include <type_traits>
#  include <utility>

namespace mozilla {

// Return a NotNull<const CHAR*> pointing at the literal empty string `""`.
template <typename CHAR>
constexpr const CHAR* LiteralEmptyStringPointer() {
  static_assert(std::is_same_v<CHAR, char> || std::is_same_v<CHAR, char16_t>,
                "Only char and char16_t are supported in Firefox");
  if constexpr (std::is_same_v<CHAR, char>) {
    return "";
  }
  if constexpr (std::is_same_v<CHAR, char16_t>) {
    return u"";
  }
}

// Return a string_view<CHAR> pointing at the literal empty string.
template <typename CHAR>
constexpr std::basic_string_view<CHAR> LiteralEmptyStringView() {
  static_assert(std::is_same_v<CHAR, char> || std::is_same_v<CHAR, char16_t>,
                "Only char and char16_t are supported in Firefox");
  // Use `operator""sv()` from <string_view>.
  using namespace std::literals::string_view_literals;
  if constexpr (std::is_same_v<CHAR, char>) {
    return ""sv;
  }
  if constexpr (std::is_same_v<CHAR, char16_t>) {
    return u""sv;
  }
}

// General string view, optimized for short on-stack life before serialization,
// and between deserialization and JSON-streaming.
template <typename CHAR>
class MOZ_STACK_CLASS ProfilerStringView {
 public:
  // Default constructor points at "" (literal empty string).
  constexpr ProfilerStringView() = default;

  // Don't allow copy.
  ProfilerStringView(const ProfilerStringView&) = delete;
  ProfilerStringView& operator=(const ProfilerStringView&) = delete;

  // Allow move. For consistency the moved-from string is always reset to "".
  constexpr ProfilerStringView(ProfilerStringView&& aOther)
      : mStringView(std::move(aOther.mStringView)),
        mOwnership(aOther.mOwnership) {
    if (mOwnership == Ownership::OwnedThroughStringView) {
      // We now own the buffer, make the other point at the literal "".
      aOther.mStringView = LiteralEmptyStringView<CHAR>();
      aOther.mOwnership = Ownership::Literal;
    }
  }
  constexpr ProfilerStringView& operator=(ProfilerStringView&& aOther) {
    mStringView = std::move(aOther.mStringView);
    mOwnership = aOther.mOwnership;
    if (mOwnership == Ownership::OwnedThroughStringView) {
      // We now own the buffer, make the other point at the literal "".
      aOther.mStringView = LiteralEmptyStringView<CHAR>();
      aOther.mOwnership = Ownership::Literal;
    }
    return *this;
  }

  ~ProfilerStringView() {
    if (MOZ_UNLIKELY(mOwnership == Ownership::OwnedThroughStringView)) {
      // We own the buffer pointed at by mStringView, destroy it.
      // This is only used between deserialization and streaming.
      delete mStringView.data();
    }
  }

  // Implicit construction from nullptr, points at "" (literal empty string).
  constexpr MOZ_IMPLICIT ProfilerStringView(decltype(nullptr)) {}

  // Implicit constructor from a literal string.
  template <size_t Np1>
  constexpr MOZ_IMPLICIT ProfilerStringView(const CHAR (&aLiteralString)[Np1])
      : ProfilerStringView(aLiteralString, Np1 - 1, Ownership::Literal) {}

  // Constructor from a non-literal string.
  constexpr ProfilerStringView(const CHAR* aString, size_t aLength)
      : ProfilerStringView(aString, aLength, Ownership::Reference) {}

  // Implicit constructor from a string_view.
  constexpr MOZ_IMPLICIT ProfilerStringView(
      const std::basic_string_view<CHAR>& aStringView)
      : ProfilerStringView(aStringView.data(), aStringView.length(),
                           Ownership::Reference) {}

  // Implicit constructor from an expiring string_view. We assume that the
  // pointed-at string will outlive this ProfilerStringView.
  constexpr MOZ_IMPLICIT ProfilerStringView(
      std::basic_string_view<CHAR>&& aStringView)
      : ProfilerStringView(aStringView.data(), aStringView.length(),
                           Ownership::Reference) {}

  // Implicit constructor from std::string.
  constexpr MOZ_IMPLICIT ProfilerStringView(const std::string& aString)
      : ProfilerStringView(aString.data(), aString.length(),
                           Ownership::Reference) {}

  // Construction from a raw pointer to a null-terminated string.
  // This is a named class-static function to make it more obvious where work is
  // being done (to determine the string length), and encourage users to instead
  // provide a length, if already known.
  // TODO: Find callers and convert them to constructor instead if possible.
  static constexpr ProfilerStringView WrapNullTerminatedString(
      const CHAR* aString) {
    return ProfilerStringView(
        aString, aString ? std::char_traits<char>::length(aString) : 0,
        Ownership::Reference);
  }

  // Implicit constructor for an object with member functions `Data()`
  // `Length()`, and `IsLiteral()`, common in xpcom strings.
  template <
      typename String,
      typename DataReturnType = decltype(std::declval<const String>().Data()),
      typename LengthReturnType =
          decltype(std::declval<const String>().Length()),
      typename IsLiteralReturnType =
          decltype(std::declval<const String>().IsLiteral()),
      typename =
          std::enable_if_t<std::is_convertible_v<DataReturnType, const CHAR*> &&
                           std::is_integral_v<LengthReturnType> &&
                           std::is_same_v<IsLiteralReturnType, bool>>>
  constexpr MOZ_IMPLICIT ProfilerStringView(const String& aString)
      : ProfilerStringView(
            static_cast<const CHAR*>(aString.Data()), aString.Length(),
            aString.IsLiteral() ? Ownership::Literal : Ownership::Reference) {}

  [[nodiscard]] constexpr const std::basic_string_view<CHAR>& StringView()
      const {
    return mStringView;
  }

  [[nodiscard]] constexpr const CHAR* Data() const {
    return mStringView.data();
  }

  [[nodiscard]] constexpr size_t Length() const { return mStringView.length(); }

  [[nodiscard]] constexpr bool IsLiteral() const {
    return mOwnership == Ownership::Literal;
  }
  [[nodiscard]] constexpr bool IsReference() const {
    return mOwnership == Ownership::Reference;
  }
  // No `IsOwned...()` because it's a secret, only used internally!

  [[nodiscard]] std::basic_string<CHAR> String() const {
    return std::basic_string<CHAR>(mStringView);
  }

 private:
  enum class Ownership { Literal, Reference, OwnedThroughStringView };

  // Allow deserializer to store anything here.
  friend ProfileBufferEntryReader::Deserializer<ProfilerStringView>;

  constexpr ProfilerStringView(const CHAR* aString, size_t aLength,
                               Ownership aOwnership)
      : mStringView(aString ? std::basic_string_view<CHAR>(aString, aLength)
                            : LiteralEmptyStringView<CHAR>()),
        mOwnership(aString ? aOwnership : Ownership::Literal) {}

  // String view to an outside string (literal or reference).
  // We may actually own the pointed-at buffer, but it is only used internally
  // between deserialization and JSON streaming.
  std::basic_string_view<CHAR> mStringView = LiteralEmptyStringView<CHAR>();

  Ownership mOwnership = Ownership::Literal;
};

using ProfilerString8View = ProfilerStringView<char>;
using ProfilerString16View = ProfilerStringView<char16_t>;

// The classes below are all embedded in a `MarkerOptions` object.
class MarkerOptions;

// This compulsory marker option contains the required category information.
class MarkerCategory {
 public:
  // Constructor from category pair (aka sub-category) and category.
  constexpr MarkerCategory(baseprofiler::ProfilingCategoryPair aCategoryPair,
                           baseprofiler::ProfilingCategory aCategory)
      : mCategoryPair(aCategoryPair), mCategory(aCategory) {}

  constexpr baseprofiler::ProfilingCategoryPair CategoryPair() const {
    return mCategoryPair;
  }

  constexpr baseprofiler::ProfilingCategory Category() const {
    return mCategory;
  }

 private:
  // The default constructor is only used during deserialization of
  // MarkerOptions.
  friend MarkerOptions;
  constexpr MarkerCategory() = default;

  friend ProfileBufferEntryReader::Deserializer<MarkerCategory>;

  baseprofiler::ProfilingCategoryPair mCategoryPair =
      baseprofiler::ProfilingCategoryPair::OTHER;
  baseprofiler::ProfilingCategory mCategory =
      baseprofiler::ProfilingCategory::OTHER;
};

namespace baseprofiler::category {
// Each category-pair (aka subcategory) name constructs a MarkerCategory.
// E.g.: mozilla::baseprofiler::category::OTHER_Profiling
// Profiler macros will take the category name alone.
// E.g.: `PROFILER_MARKER_UNTYPED("name", OTHER_Profiling)`
#  define CATEGORY_ENUM_BEGIN_CATEGORY(name, labelAsString, color)
#  define CATEGORY_ENUM_SUBCATEGORY(supercategory, name, labelAsString) \
    static constexpr MarkerCategory name{ProfilingCategoryPair::name,   \
                                         ProfilingCategory::supercategory};
#  define CATEGORY_ENUM_END_CATEGORY
MOZ_PROFILING_CATEGORY_LIST(CATEGORY_ENUM_BEGIN_CATEGORY,
                            CATEGORY_ENUM_SUBCATEGORY,
                            CATEGORY_ENUM_END_CATEGORY)
#  undef CATEGORY_ENUM_BEGIN_CATEGORY
#  undef CATEGORY_ENUM_SUBCATEGORY
#  undef CATEGORY_ENUM_END_CATEGORY
}  // namespace baseprofiler::category

}  // namespace mozilla

#endif  // MOZ_GECKO_PROFILER

#endif  // BaseProfilerMarkersPrerequisites_h
