/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RustRegex_h
#define mozilla_RustRegex_h

#include "nsPrintfCString.h"
#include "nsTArray.h"
#include "rure.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

// This header is a thin wrapper around the `rure.h` header file, which declares
// the C API for interacting with the rust `regex` crate. This is intended to
// make the type more ergonomic to use with mozilla types.

class RustRegex;
class RustRegexSet;
class RustRegexOptions;
class RustRegexCaptures;
class RustRegexIter;
class RustRegexIterCaptureNames;

using RustRegexMatch = rure_match;

/*
 * RustRegexCaptures represents storage for sub-capture locations of a match.
 *
 * Computing the capture groups of a match can carry a significant performance
 * penalty, so their use in the API is optional.
 *
 * A RustRegexCaptures value may outlive its corresponding RustRegex and can be
 * freed independently.
 *
 * It is not safe to use from multiple threads simultaneously.
 */
class RustRegexCaptures final {
 public:
  RustRegexCaptures() = default;

  // Check if the `RustRegexCaptures` object is valid.
  bool IsValid() const { return mPtr != nullptr; }
  explicit operator bool() const { return IsValid(); }

  /*
   * CaptureAt returns Some if and only if the capturing group at the
   * index given was part of the match. If so, the returned RustRegexMatch
   * object contains the start and end offsets (in bytes) of the match.
   *
   * If no capture group with the index aIdx exists, or the group was not part
   * of the match, then Nothing is returned.  (A capturing group exists if and
   * only if aIdx is less than Length().)
   *
   * Note that index 0 corresponds to the full match.
   */
  Maybe<RustRegexMatch> CaptureAt(size_t aIdx) const {
    RustRegexMatch match;
    if (mPtr && rure_captures_at(mPtr.get(), aIdx, &match)) {
      return Some(match);
    }
    return Nothing();
  }
  Maybe<RustRegexMatch> operator[](size_t aIdx) const {
    return CaptureAt(aIdx);
  }

  /*
   * Returns the number of capturing groups in this `RustRegexCaptures`.
   */
  size_t Length() const { return mPtr ? rure_captures_len(mPtr.get()) : 0; }

 private:
  friend class RustRegex;
  friend class RustRegexIter;

  explicit RustRegexCaptures(rure* aRe)
      : mPtr(aRe ? rure_captures_new(aRe) : nullptr) {}

  struct Deleter {
    void operator()(rure_captures* ptr) const { rure_captures_free(ptr); }
  };
  UniquePtr<rure_captures, Deleter> mPtr;
};

/*
 * RustRegexIterCaptureNames is an iterator over the list of capture group names
 * in this particular RustRegex.
 *
 * A RustRegexIterCaptureNames value may not outlive its corresponding
 * RustRegex, and should be destroyed before its corresponding RustRegex is
 * destroyed.
 *
 * It is not safe to use from multiple threads simultaneously.
 */
class RustRegexIterCaptureNames {
 public:
  RustRegexIterCaptureNames() = delete;

  // Check if the `RustRegexIterCaptureNames` object is valid.
  bool IsValid() const { return mPtr != nullptr; }
  explicit operator bool() const { return IsValid(); }

  /*
   * Advances the iterator and returns true if and only if another capture group
   * name exists.
   *
   * The value of the capture group name is written to the provided pointer.
   */
  mozilla::Maybe<const char*> Next() {
    char* next = nullptr;
    if (mPtr && rure_iter_capture_names_next(mPtr.get(), &next)) {
      return Some(next);
    }
    return Nothing();
  }

 private:
  friend class RustRegex;

  explicit RustRegexIterCaptureNames(rure* aRe)
      : mPtr(aRe ? rure_iter_capture_names_new(aRe) : nullptr) {}

  struct Deleter {
    void operator()(rure_iter_capture_names* ptr) const {
      rure_iter_capture_names_free(ptr);
    }
  };
  UniquePtr<rure_iter_capture_names, Deleter> mPtr;
};

/*
 * RustRegexIter is an iterator over successive non-overlapping matches in a
 * particular haystack.
 *
 * A RustRegexIter value may not outlive its corresponding RustRegex and should
 * be destroyed before its corresponding RustRegex is destroyed.
 *
 * It is not safe to use from multiple threads simultaneously.
 */
class RustRegexIter {
 public:
  RustRegexIter() = delete;

  // Check if the `RustRegexIter` object is valid.
  bool IsValid() const { return mPtr != nullptr; }
  explicit operator bool() const { return IsValid(); }

  /*
   * Next() returns Some if and only if this regex matches anywhere in haystack.
   * The returned RustRegexMatch object contains the start and end offsets (in
   * bytes) of the match.
   *
   * If no match is found, then subsequent calls will return Nothing()
   * indefinitely.
   *
   * Next() should be preferred to NextCaptures() since it may be faster.
   *
   * N.B. The performance of this search is not impacted by the presence of
   * capturing groups in your regular expression.
   */
  mozilla::Maybe<RustRegexMatch> Next() {
    RustRegexMatch match{};
    if (mPtr &&
        rure_iter_next(mPtr.get(), mHaystackPtr, mHaystackSize, &match)) {
      return Some(match);
    }
    return Nothing();
  }

  /*
   * NextCaptures returns a valid RustRegexCaptures if and only if this regex
   * matches anywhere in haystack. If a match is found, then all of its capture
   * locations are stored in the returned RustRegexCaptures object.
   *
   * If no match is found, then subsequent calls will return an invalid
   * `RustRegexCaptures` indefinitely.
   *
   * Only use this function if you specifically need access to capture
   * locations. It is not necessary to use this function just because your
   * regular expression contains capturing groups.
   *
   * Capture locations can be accessed using the methods on RustRegexCaptures.
   *
   * N.B. The performance of this search can be impacted by the number of
   * capturing groups. If you're using this function, it may be beneficial to
   * use non-capturing groups (e.g., `(?:re)`) where possible.
   */
  RustRegexCaptures NextCaptures() {
    RustRegexCaptures captures(mRe);
    if (mPtr && rure_iter_next_captures(mPtr.get(), mHaystackPtr, mHaystackSize,
                                        captures.mPtr.get())) {
      return captures;
    }
    return {};
  }

 private:
  friend class RustRegex;
  RustRegexIter(rure* aRe, const std::string_view& aHaystack)
      : mRe(aRe),
        mHaystackPtr(reinterpret_cast<const uint8_t*>(aHaystack.data())),
        mHaystackSize(aHaystack.size()),
        mPtr(aRe ? rure_iter_new(aRe) : nullptr) {}

  rure* MOZ_NON_OWNING_REF mRe;
  const uint8_t* MOZ_NON_OWNING_REF mHaystackPtr;
  size_t mHaystackSize;

  struct Deleter {
    void operator()(rure_iter* ptr) const { rure_iter_free(ptr); }
  };
  UniquePtr<rure_iter, Deleter> mPtr;
};

/*
 * RustRegexOptions is the set of configuration options for compiling a regular
 * expression.
 *
 * All flags on this type can be used to set default flags while compiling, and
 * can be toggled in the expression itself using standard syntax, e.g. `(?i)`
 * turns case-insensitive matching on, and `(?-i)` disables it.
 *
 * In addition, two non-flag options are available: setting the size limit of
 * the compiled program and setting the size limit of the cache of states that
 * the DFA uses while searching.
 *
 * For most uses, the default settings will work fine, and a default-constructed
 * RustRegexOptions can be passed.
 */
class RustRegexOptions {
 public:
  RustRegexOptions() = default;

  /*
   * Set the value for the case insensitive (i) flag.
   *
   * When enabled, letters in the pattern will match both upper case and lower
   * case variants.
   */
  RustRegexOptions& CaseInsensitive(bool aYes) {
    return SetFlag(aYes, RURE_FLAG_CASEI);
  }

  /*
   * Set the value for the multi-line matching (m) flag.
   *
   * When enabled, ^ matches the beginning of lines and $ matches the end of
   * lines.
   *
   * By default, they match beginning/end of the input.
   */
  RustRegexOptions& MultiLine(bool aYes) {
    return SetFlag(aYes, RURE_FLAG_MULTI);
  }

  /*
   * Set the value for the any character (s) flag, where in . matches anything
   * when s is set and matches anything except for new line when it is not set
   * (the default).
   *
   * N.B. “matches anything” means “any byte” when Unicode is disabled and means
   * “any valid UTF-8 encoding of any Unicode scalar value” when Unicode is
   * enabled.
   */
  RustRegexOptions& DotMatchesNewLine(bool aYes) {
    return SetFlag(aYes, RURE_FLAG_DOTNL);
  }

  /*
   * Set the value for the greedy swap (U) flag.
   *
   * When enabled, a pattern like a* is lazy (tries to find shortest match) and
   * a*? is greedy (tries to find longest match).
   *
   * By default, a* is greedy and a*? is lazy.
   */
  RustRegexOptions& SwapGreed(bool aYes) {
    return SetFlag(aYes, RURE_FLAG_SWAP_GREED);
  }

  /*
   * Set the value for the ignore whitespace (x) flag.
   *
   * When enabled, whitespace such as new lines and spaces will be ignored
   * between expressions of the pattern, and # can be used to start a comment
   * until the next new line.
   */
  RustRegexOptions& IgnoreWhitespace(bool aYes) {
    return SetFlag(aYes, RURE_FLAG_SPACE);
  }

  /*
   * Set the value for the Unicode (u) flag.
   *
   * Enabled by default. When disabled, character classes such as \w only match
   * ASCII word characters instead of all Unicode word characters.
   */
  RustRegexOptions& Unicode(bool aYes) {
    return SetFlag(aYes, RURE_FLAG_UNICODE);
  }

  /*
   * SizeLimit sets the appoximate size limit of the compiled regular
   * expression.
   *
   * This size limit roughly corresponds to the number of bytes occupied by
   * a single compiled program. If the program would exceed this number,
   * then an invalid RustRegex will be constructed.
   */
  RustRegexOptions& SizeLimit(size_t aLimit) {
    mSizeLimit = Some(aLimit);
    return *this;
  }

  /*
   * DFASizeLimit sets the approximate size of the cache used by the DFA during
   * search.
   *
   * This roughly corresponds to the number of bytes that the DFA will use while
   * searching.
   *
   * Note that this is a *per thread* limit. There is no way to set a global
   * limit. In particular, if a regular expression is used from multiple threads
   * simultaneously, then each thread may use up to the number of bytes
   * specified here.
   */
  RustRegexOptions& DFASizeLimit(size_t aLimit) {
    mDFASizeLimit = Some(aLimit);
    return *this;
  }

 private:
  friend class RustRegex;
  friend class RustRegexSet;

  struct OptionsDeleter {
    void operator()(rure_options* ptr) const { rure_options_free(ptr); }
  };

  UniquePtr<rure_options, OptionsDeleter> GetOptions() const {
    UniquePtr<rure_options, OptionsDeleter> options;
    if (mSizeLimit || mDFASizeLimit) {
      options.reset(rure_options_new());
      if (mSizeLimit) {
        rure_options_size_limit(options.get(), *mSizeLimit);
      }
      if (mDFASizeLimit) {
        rure_options_dfa_size_limit(options.get(), *mDFASizeLimit);
      }
    }
    return options;
  }

  uint32_t GetFlags() const { return mFlags; }

  RustRegexOptions& SetFlag(bool aYes, uint32_t aFlag) {
    if (aYes) {
      mFlags |= aFlag;
    } else {
      mFlags &= ~aFlag;
    }
    return *this;
  }

  uint32_t mFlags = RURE_DEFAULT_FLAGS;
  Maybe<size_t> mSizeLimit;
  Maybe<size_t> mDFASizeLimit;
};

/*
 * RustRegex is the type of a compiled regular expression.
 *
 * A RustRegex can be safely used from multiple threads simultaneously.
 *
 * When calling the matching methods on this type, they will generally have the
 * following parameters:
 *
 * aHaystack
 *   may contain arbitrary bytes, but ASCII compatible text is more useful.
 *   UTF-8 is even more useful. Other text encodings aren't supported.
 *
 * aStart
 *   the position in bytes at which to start searching. Note that setting the
 *   start position is distinct from using a substring for `aHaystack`, since
 *   the regex engine may look at bytes before the start position to determine
 *   match information. For example, if the start position is greater than 0,
 *   then the \A ("begin text") anchor can never match.
 */
class RustRegex final {
 public:
  // Create a new invalid RustRegex object
  RustRegex() = default;

  /*
   * Compiles the given pattern into a regular expression. The pattern must be
   * valid UTF-8 and the length corresponds to the number of bytes in the
   * pattern.
   *
   * If an error occurs, the constructed RustRegex will be `!IsValid()`.
   *
   * The compiled expression returned may be used from multiple threads
   * simultaneously.
   */
  explicit RustRegex(const std::string_view& aPattern,
                     const RustRegexOptions& aOptions = {}) {
#ifdef DEBUG
    rure_error* error = rure_error_new();
#else
    rure_error* error = nullptr;
#endif
    mPtr.reset(rure_compile(reinterpret_cast<const uint8_t*>(aPattern.data()),
                            aPattern.size(), aOptions.GetFlags(),
                            aOptions.GetOptions().get(), error));
#ifdef DEBUG
    if (!mPtr) {
      NS_WARNING(nsPrintfCString("RustRegex compile failed: %s",
                                 rure_error_message(error))
                     .get());
    }
    rure_error_free(error);
#endif
  }

  // Check if the compiled `RustRegex` is valid.
  bool IsValid() const { return mPtr != nullptr; }
  explicit operator bool() const { return IsValid(); }

  /*
   * IsMatch returns true if and only if this regex matches anywhere in
   * aHaystack.
   *
   * See the type-level comment for details on aHaystack and aStart.
   *
   * IsMatch() should be preferred to Find() since it may be faster.
   *
   * N.B. The performance of this search is not impacted by the presence of
   * capturing groups in your regular expression.
   */
  bool IsMatch(const std::string_view& aHaystack, size_t aStart = 0) const {
    return mPtr &&
           rure_is_match(mPtr.get(),
                         reinterpret_cast<const uint8_t*>(aHaystack.data()),
                         aHaystack.size(), aStart);
  }

  /*
   * Find returns Some if and only if this regex matches anywhere in
   * haystack. The returned RustRegexMatch object contains the start and end
   * offsets (in bytes) of the match.
   *
   * See the type-level comment for details on aHaystack and aStart.
   *
   * Find() should be preferred to FindCaptures() since it may be faster.
   *
   * N.B. The performance of this search is not impacted by the presence of
   * capturing groups in your regular expression.
   */
  Maybe<RustRegexMatch> Find(const std::string_view& aHaystack,
                             size_t aStart = 0) const {
    RustRegexMatch match{};
    if (mPtr && rure_find(mPtr.get(),
                          reinterpret_cast<const uint8_t*>(aHaystack.data()),
                          aHaystack.size(), aStart, &match)) {
      return Some(match);
    }
    return Nothing();
  }

  /*
   * FindCaptures() returns a valid RustRegexCaptures if and only if this
   * regex matches anywhere in haystack. If a match is found, then all of its
   * capture locations are stored in the returned RustRegexCaptures object.
   *
   * See the type-level comment for details on aHaystack and aStart.
   *
   * Only use this function if you specifically need access to capture
   * locations. It is not necessary to use this function just because your
   * regular expression contains capturing groups.
   *
   * Capture locations can be accessed using the methods on RustRegexCaptures.
   *
   * N.B. The performance of this search can be impacted by the number of
   * capturing groups. If you're using this function, it may be beneficial to
   * use non-capturing groups (e.g., `(?:re)`) where possible.
   */
  RustRegexCaptures FindCaptures(const std::string_view& aHaystack,
                                 size_t aStart = 0) const {
    RustRegexCaptures captures(mPtr.get());
    if (mPtr &&
        rure_find_captures(mPtr.get(),
                           reinterpret_cast<const uint8_t*>(aHaystack.data()),
                           aHaystack.size(), aStart, captures.mPtr.get())) {
      return captures;
    }
    return {};
  }

  /*
   * ShortestMatch() returns Some if and only if this regex matches anywhere
   * in haystack. If a match is found, then its end location is stored in the
   * pointer given. The end location is the place at which the regex engine
   * determined that a match exists, but may occur before the end of the
   * proper leftmost-first match.
   *
   * See the type-level comment for details on aHaystack and aStart.
   *
   * ShortestMatch should be preferred to Find since it may be faster.
   *
   * N.B. The performance of this search is not impacted by the presence of
   * capturing groups in your regular expression.
   */
  Maybe<size_t> ShortestMatch(const std::string_view& aHaystack,
                              size_t aStart = 0) const {
    size_t end = 0;
    if (mPtr &&
        rure_shortest_match(mPtr.get(),
                            reinterpret_cast<const uint8_t*>(aHaystack.data()),
                            aHaystack.size(), aStart, &end)) {
      return Some(end);
    }
    return Nothing();
  }

  /*
   * Create an iterator over all successive non-overlapping matches of this
   * regex in aHaystack.
   *
   * See the type-level comment for details on aHaystack.
   *
   * Both aHaystack and this regex must remain valid until the returned
   * `RustRegexIter` is destroyed.
   */
  RustRegexIter IterMatches(const std::string_view& aHaystack) const {
    return RustRegexIter(mPtr.get(), aHaystack);
  }

  /*
   * Returns the capture index for the name given. If no such named capturing
   * group exists in this regex, then -1 is returned.
   *
   * The capture index may be used with RustRegexCaptures::CaptureAt.
   *
   * This function never returns 0 since the first capture group always
   * corresponds to the entire match and is always unnamed.
   */
  int32_t CaptureNameIndex(const char* aName) const {
    return mPtr ? rure_capture_name_index(mPtr.get(), aName) : -1;
  }

  /*
   * Create an iterator over the list of capture group names in this particular
   * regex.
   *
   * This regex must remain valid until the returned `RustRegexIterCaptureNames`
   * is destroyed.
   */
  RustRegexIterCaptureNames IterCaptureNames() const {
    return RustRegexIterCaptureNames(mPtr.get());
  }

  /*
   * Count the number of successive non-overlapping matches of this regex in
   * aHaystack.
   *
   * See the type-level comment for details on aHaystack.
   */
  size_t CountMatches(const std::string_view& aHaystack) const {
    size_t count = 0;
    auto iter = IterMatches(aHaystack);
    while (iter.Next()) {
      count++;
    }
    return count;
  }

 private:
  struct Deleter {
    void operator()(rure* ptr) const { rure_free(ptr); }
  };
  UniquePtr<rure, Deleter> mPtr;
};

/*
 * RustRegexSet is the type of a set of compiled regular expression.
 *
 * A RustRegexSet can be safely used from multiple threads simultaneously.
 *
 * When calling the matching methods on this type, they will generally have the
 * following parameters:
 *
 * aHaystack
 *   may contain arbitrary bytes, but ASCII compatible text is more useful.
 *   UTF-8 is even more useful. Other text encodings aren't supported.
 *
 * aStart
 *   the position in bytes at which to start searching. Note that setting the
 *   start position is distinct from using a substring for `aHaystack`, since
 *   the regex engine may look at bytes before the start position to determine
 *   match information. For example, if the start position is greater than 0,
 *   then the \A ("begin text") anchor can never match.
 */
class RustRegexSet final {
 public:
  /*
   * Compiles the given range of patterns into a single regular expression which
   * can be matched in a linear-scan. Each pattern in aPatterns must be valid
   * UTF-8, and implicitly coerce to `std::string_view`.
   *
   * If an error occurs, the constructed RustRegexSet will be `!IsValid()`.
   *
   * The compiled expression returned may be used from multiple threads
   * simultaneously.
   */
  template <typename Patterns>
  explicit RustRegexSet(Patterns&& aPatterns,
                        const RustRegexOptions& aOptions = {}) {
#ifdef DEBUG
    rure_error* error = rure_error_new();
#else
    rure_error* error = nullptr;
#endif
    AutoTArray<const uint8_t*, 4> patternPtrs;
    AutoTArray<size_t, 4> patternSizes;
    for (auto&& pattern : std::forward<Patterns>(aPatterns)) {
      std::string_view view = pattern;
      patternPtrs.AppendElement(reinterpret_cast<const uint8_t*>(view.data()));
      patternSizes.AppendElement(view.size());
    }
    mPtr.reset(rure_compile_set(patternPtrs.Elements(), patternSizes.Elements(),
                                patternPtrs.Length(), aOptions.GetFlags(),
                                aOptions.GetOptions().get(), error));
#ifdef DEBUG
    if (!mPtr) {
      NS_WARNING(nsPrintfCString("RustRegexSet compile failed: %s",
                                 rure_error_message(error))
                     .get());
    }
    rure_error_free(error);
#endif
  }

  // Check if the `RustRegexSet` object is valid.
  bool IsValid() const { return mPtr != nullptr; }
  explicit operator bool() const { return IsValid(); }

  /*
   * IsMatch returns true if and only if any regexes within the set
   * match anywhere in the haystack. Once a match has been located, the
   * matching engine will quit immediately.
   *
   * See the type-level comment for details on aHaystack and aStart.
   */
  bool IsMatch(const std::string_view& aHaystack, size_t aStart = 0) const {
    return mPtr &&
           rure_set_is_match(mPtr.get(),
                             reinterpret_cast<const uint8_t*>(aHaystack.data()),
                             aHaystack.size(), aStart);
  }

  struct SetMatches {
    bool matchedAny = false;
    nsTArray<bool> matches;
  };

  /*
   * Matches() compares each regex in the set against the haystack and
   * returns a list with the match result of each pattern. Match results are
   * ordered in the same way as the regex set was compiled. For example, index 0
   * of matches corresponds to the first pattern passed to the constructor.
   *
   * See the type-level comment for details on aHaystack and aStart.
   *
   * Only use this function if you specifically need to know which regexes
   * matched within the set. To determine if any of the regexes matched without
   * caring which, use IsMatch.
   */
  SetMatches Matches(const std::string_view& aHaystack,
                     size_t aStart = 0) const {
    nsTArray<bool> matches;
    matches.SetLength(Length());
    bool any = mPtr && rure_set_matches(
                           mPtr.get(),
                           reinterpret_cast<const uint8_t*>(aHaystack.data()),
                           aHaystack.size(), aStart, matches.Elements());
    return SetMatches{any, std::move(matches)};
  }

  /*
   * Returns the number of patterns the regex set was compiled with.
   */
  size_t Length() const { return mPtr ? rure_set_len(mPtr.get()) : 0; }

 private:
  struct Deleter {
    void operator()(rure_set* ptr) const { rure_set_free(ptr); }
  };
  UniquePtr<rure_set, Deleter> mPtr;
};

}  // namespace mozilla

#endif  // mozilla_RustRegex_h
