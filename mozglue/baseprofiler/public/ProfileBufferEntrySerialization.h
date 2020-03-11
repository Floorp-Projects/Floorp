/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfileBufferEntrySerialization_h
#define ProfileBufferEntrySerialization_h

#include "mozilla/Assertions.h"
#include "mozilla/leb128iterator.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/ProfileBufferIndex.h"
#include "mozilla/Span.h"
#include "mozilla/Tuple.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/Unused.h"
#include "mozilla/Variant.h"

#include <string>
#include <tuple>

namespace mozilla {

class ProfileBufferEntryWriter;

class ProfileBufferEntryReader {
 public:
  // Class to be specialized for types to be read from a profile buffer entry.
  // See common specializations at the bottom of this header.
  // The following static functions must be provided:
  //   static void ReadInto(EntryReader aER&, T& aT)
  //   {
  //     /* Call `aER.ReadX(...)` function to deserialize into aT, be sure to
  //        read exactly `Bytes(aT)`! */
  //   }
  //   static T Read(EntryReader& aER) {
  //     /* Call `aER.ReadX(...)` function to deserialize and return a `T`, be
  //        sure to read exactly `Bytes(returned value)`! */
  //   }
  template <typename T>
  struct Deserializer;
};

class ProfileBufferEntryWriter {
 public:
  // Class to be specialized for types to be written in an entry.
  // See common specializations at the bottom of this header.
  // The following static functions must be provided:
  //   static Length Bytes(const T& aT) {
  //     /* Return number of bytes that will be written. */
  //   }
  //   static void Write(ProfileBufferEntryWriter& aEW,
  //                     const T& aT) {
  //     /* Call `aEW.WriteX(...)` functions to serialize aT, be sure to write
  //        exactly `Bytes(aT)` bytes! */
  //   }
  template <typename T>
  struct Serializer;
};

// ============================================================================
// Serializer and Deserializer ready-to-use specializations.

// ----------------------------------------------------------------------------
// Trivially-copyable types (default)

// The default implementation works for all trivially-copyable types (e.g.,
// PODs).
//
// Usage: `aEW.WriteObject(123);`.
//
// Raw pointers, though trivially-copyable, are explictly forbidden when writing
// (to avoid unexpected leaks/UAFs), instead use one of
// `WrapProfileBufferLiteralCStringPointer`, `WrapProfileBufferUnownedCString`,
// or `WrapProfileBufferRawPointer` as needed.
template <typename T>
struct ProfileBufferEntryWriter::Serializer {
  static_assert(std::is_trivially_copyable<T>::value,
                "Serializer only works with trivially-copyable types by "
                "default, use/add specialization for other types.");

  static constexpr Length Bytes(const T&) { return sizeof(T); }

  static void Write(ProfileBufferEntryWriter& aEW, const T& aT) {
    static_assert(!std::is_pointer<T>::value,
                  "Serializer won't write raw pointers by default, use "
                  "WrapProfileBufferRawPointer or other.");
    aEW.WriteBytes(&aT, sizeof(T));
  }
};

// Usage: `aER.ReadObject<int>();` or `int x; aER.ReadIntoObject(x);`.
template <typename T>
struct ProfileBufferEntryReader::Deserializer {
  static_assert(std::is_trivially_copyable<T>::value,
                "Deserializer only works with trivially-copyable types by "
                "default, use/add specialization for other types.");

  static void ReadInto(ProfileBufferEntryReader& aER, T& aT) {
    aER.ReadBytes(&aT, sizeof(T));
  }

  static T Read(ProfileBufferEntryReader& aER) {
    // Note that this creates a default `T` first, and then overwrites it with
    // bytes from the buffer. Trivially-copyable types support this without UB.
    T ob;
    ReadInto(aER, ob);
    return ob;
  }
};

// ----------------------------------------------------------------------------
// Strip const/volatile/reference from types.

// Automatically strip `const`.
template <typename T>
struct ProfileBufferEntryWriter::Serializer<const T>
    : public ProfileBufferEntryWriter::Serializer<T> {};

template <typename T>
struct ProfileBufferEntryReader::Deserializer<const T>
    : public ProfileBufferEntryReader::Deserializer<T> {};

// Automatically strip `volatile`.
template <typename T>
struct ProfileBufferEntryWriter::Serializer<volatile T>
    : public ProfileBufferEntryWriter::Serializer<T> {};

template <typename T>
struct ProfileBufferEntryReader::Deserializer<volatile T>
    : public ProfileBufferEntryReader::Deserializer<T> {};

// Automatically strip `lvalue-reference`.
template <typename T>
struct ProfileBufferEntryWriter::Serializer<T&>
    : public ProfileBufferEntryWriter::Serializer<T> {};

template <typename T>
struct ProfileBufferEntryReader::Deserializer<T&>
    : public ProfileBufferEntryReader::Deserializer<T> {};

// Automatically strip `rvalue-reference`.
template <typename T>
struct ProfileBufferEntryWriter::Serializer<T&&>
    : public ProfileBufferEntryWriter::Serializer<T> {};

template <typename T>
struct ProfileBufferEntryReader::Deserializer<T&&>
    : public ProfileBufferEntryReader::Deserializer<T> {};

// ----------------------------------------------------------------------------
// ProfileBufferBlockIndex

// ProfileBufferBlockIndex, serialized as the underlying value.
template <>
struct ProfileBufferEntryWriter::Serializer<ProfileBufferBlockIndex> {
  static constexpr Length Bytes(const ProfileBufferBlockIndex& aBlockIndex) {
    return sizeof(ProfileBufferBlockIndex);
  }

  static void Write(ProfileBufferEntryWriter& aEW,
                    const ProfileBufferBlockIndex& aBlockIndex) {
    aEW.WriteBytes(&aBlockIndex, sizeof(aBlockIndex));
  }
};

template <>
struct ProfileBufferEntryReader::Deserializer<ProfileBufferBlockIndex> {
  static void ReadInto(ProfileBufferEntryReader& aER,
                       ProfileBufferBlockIndex& aBlockIndex) {
    aER.ReadBytes(&aBlockIndex, sizeof(aBlockIndex));
  }

  static ProfileBufferBlockIndex Read(ProfileBufferEntryReader& aER) {
    ProfileBufferBlockIndex blockIndex;
    ReadInto(aER, blockIndex);
    return blockIndex;
  }
};

// ----------------------------------------------------------------------------
// Literal C string pointer

// Wrapper around a pointer to a literal C string.
template <size_t NonTerminalCharacters>
struct ProfileBufferLiteralCStringPointer {
  const char* mCString;
};

// Wrap a pointer to a literal C string.
template <size_t CharactersIncludingTerminal>
ProfileBufferLiteralCStringPointer<CharactersIncludingTerminal - 1>
WrapProfileBufferLiteralCStringPointer(
    const char (&aCString)[CharactersIncludingTerminal]) {
  return {aCString};
}

// Literal C strings, serialized as the raw pointer because it is unique and
// valid for the whole program lifetime.
//
// Usage: `aEW.WriteObject(WrapProfileBufferLiteralCStringPointer("hi"));`.
//
// No deserializer is provided for this type, instead it must be deserialized as
// a raw pointer: `aER.ReadObject<const char*>();`
template <size_t CharactersIncludingTerminal>
struct ProfileBufferEntryReader::Deserializer<
    ProfileBufferLiteralCStringPointer<CharactersIncludingTerminal>> {
  static constexpr Length Bytes(
      const ProfileBufferLiteralCStringPointer<CharactersIncludingTerminal>&) {
    // We're only storing a pointer, its size is independent from the pointer
    // value.
    return sizeof(const char*);
  }

  static void Write(
      ProfileBufferEntryWriter& aEW,
      const ProfileBufferLiteralCStringPointer<CharactersIncludingTerminal>&
          aWrapper) {
    // Write the pointer *value*, not the string contents.
    aEW.WriteBytes(aWrapper.mCString, sizeof(aWrapper.mCString));
  }
};

// ----------------------------------------------------------------------------
// C string contents

// Wrapper around a pointer to a C string whose contents will be serialized.
struct ProfileBufferUnownedCString {
  const char* mCString;
};

// Wrap a pointer to a C string whose contents will be serialized.
inline ProfileBufferUnownedCString WrapProfileBufferUnownedCString(
    const char* aCString) {
  return {aCString};
}

// The contents of a (probably) unowned C string are serialized as the number of
// characters (encoded as ULEB128) and all the characters in the string. The
// terminal '\0' is omitted.
//
// Usage: `aEW.WriteObject(WrapProfileBufferUnownedCString(str.c_str()))`.
//
// No deserializer is provided for this pointer type, instead it must be
// deserialized as one of the other string types that manages its contents,
// e.g.: `aER.ReadObject<std::string>();`
template <>
struct ProfileBufferEntryWriter::Serializer<ProfileBufferUnownedCString> {
  static Length Bytes(const ProfileBufferUnownedCString& aS) {
    const auto len = strlen(aS.mCString);
    return ULEB128Size(len) + len;
  }

  static void Write(ProfileBufferEntryWriter& aEW,
                    const ProfileBufferUnownedCString& aS) {
    const auto len = strlen(aS.mCString);
    aEW.WriteULEB128(len);
    aEW.WriteBytes(aS.mCString, len);
  }
};

// ----------------------------------------------------------------------------
// Raw pointers

// Wrapper around a pointer to be serialized as the raw pointer value.
template <typename T>
struct ProfileBufferRawPointer {
  T* mRawPointer;
};

// Wrap a pointer to be serialized as the raw pointer value.
template <typename T>
ProfileBufferRawPointer<T> WrapProfileBufferRawPointer(T* aRawPointer) {
  return {aRawPointer};
}

// Raw pointers are serialized as the raw pointer value.
//
// Usage: `aEW.WriteObject(WrapProfileBufferRawPointer(ptr));`
//
// The wrapper is compulsory when writing pointers (to avoid unexpected
// leaks/UAFs), but reading can be done straight into a raw pointer object,
// e.g.: `aER.ReadObject<Foo*>;`.
template <typename T>
struct ProfileBufferEntryWriter::Serializer<ProfileBufferRawPointer<T>> {
  template <typename U>
  static constexpr Length Bytes(const U&) {
    return sizeof(T*);
  }

  static void Write(ProfileBufferEntryWriter& aEW,
                    const ProfileBufferRawPointer<T>& aWrapper) {
    aEW.WriteBytes(&aWrapper.mRawPointer, sizeof(aWrapper.mRawPointer));
  }
};

// Usage: `aER.ReadObject<Foo*>;` or `Foo* p; aER.ReadIntoObject(p);`, no
// wrapper necessary.
template <typename T>
struct ProfileBufferEntryReader::Deserializer<ProfileBufferRawPointer<T>> {
  static void ReadInto(ProfileBufferEntryReader& aER, T*& aPtr) {
    aER.ReadBytes(&aPtr, sizeof(aPtr));
  }

  static T* Read(ProfileBufferEntryReader& aER) {
    T* ptr;
    ReadInto(aER, ptr);
    return ptr;
  }
};

// ----------------------------------------------------------------------------
// std::string contents

// std::string contents are serialized as the number of characters (encoded as
// ULEB128) and all the characters in the string. The terminal '\0' is omitted.
//
// Usage: `std::string s = ...; aEW.WriteObject(s);`
template <>
struct ProfileBufferEntryWriter::Serializer<std::string> {
  static Length Bytes(const std::string& aS) {
    const Length len = static_cast<Length>(aS.length());
    return ULEB128Size(len) + len;
  }

  static void Write(ProfileBufferEntryWriter& aEW, const std::string& aS) {
    const Length len = static_cast<Length>(aS.length());
    aEW.WriteULEB128(len);
    aEW.WriteBytes(aS.c_str(), len);
  }
};

// Usage: `std::string s = aEW.ReadObject<std::string>(s);` or
// `std::string s; aER.ReadIntoObject(s);`
template <>
struct ProfileBufferEntryReader::Deserializer<std::string> {
  static void ReadInto(ProfileBufferEntryReader& aER, std::string& aS) {
    const auto len = aER.ReadULEB128<std::string::size_type>();
    // Assign to `aS` by using iterators.
    // (`aER+0` so we get the same iterator type as `aER+len`.)
    aS.assign(aER, aER.EmptyIteratorAtOffset(len));
    aER += len;
  }

  static std::string Read(ProfileBufferEntryReader& aER) {
    const auto len = aER.ReadULEB128<std::string::size_type>();
    // Construct a string by using iterators.
    // (`aER+0` so we get the same iterator type as `aER+len`.)
    std::string s(aER, aER.EmptyIteratorAtOffset(len));
    aER += len;
    return s;
  }
};

// ----------------------------------------------------------------------------
// mozilla::UniqueFreePtr<CHAR>

// UniqueFreePtr<CHAR>, which points at a string allocated with `malloc`
// (typically generated by `strdup()`), is serialized as the number of
// *bytes* (encoded as ULEB128) and all the characters in the string. The
// null terminator is omitted.
// `CHAR` can be any type that has a specialization for
// `std::char_traits<CHAR>::length(const CHAR*)`.
//
// Note: A nullptr pointer will be serialized like an empty string, so when
// deserializing it will result in an allocated buffer only containing a
// single null terminator.
template <typename CHAR>
struct ProfileBufferEntryWriter::Serializer<UniqueFreePtr<CHAR>> {
  static Length Bytes(const UniqueFreePtr<CHAR>& aS) {
    if (!aS) {
      // Null pointer, store it as if it was an empty string (so: 0 bytes).
      return ULEB128Size(0u);
    }
    // Note that we store the size in *bytes*, not in number of characters.
    const auto bytes = std::char_traits<CHAR>::length(aS.get()) * sizeof(CHAR);
    return ULEB128Size(bytes) + bytes;
  }

  static void Write(ProfileBufferEntryWriter& aEW,
                    const UniqueFreePtr<CHAR>& aS) {
    if (!aS) {
      // Null pointer, store it as if it was an empty string (so we write a
      // length of 0 bytes).
      aEW.WriteULEB128(0u);
      return;
    }
    // Note that we store the size in *bytes*, not in number of characters.
    const auto bytes = std::char_traits<CHAR>::length(aS.get()) * sizeof(CHAR);
    aEW.WriteULEB128(bytes);
    aEW.WriteBytes(aS.get(), bytes);
  }
};

template <typename CHAR>
struct ProfileBufferEntryReader::Deserializer<UniqueFreePtr<CHAR>> {
  static void ReadInto(ProfileBufferEntryReader& aER, UniqueFreePtr<CHAR>& aS) {
    aS = Read(aER);
  }

  static UniqueFreePtr<CHAR> Read(ProfileBufferEntryReader& aER) {
    // Read the number of *bytes* that follow.
    const auto bytes = aER.ReadULEB128<size_t>();
    // We need a buffer of the non-const character type.
    using NC_CHAR = std::remove_const_t<CHAR>;
    // We allocate the required number of bytes, plus one extra character for
    // the null terminator.
    NC_CHAR* buffer = static_cast<NC_CHAR*>(malloc(bytes + sizeof(NC_CHAR)));
    // Copy the characters into the buffer.
    aER.ReadBytes(buffer, bytes);
    // And append a null terminator.
    buffer[bytes / sizeof(NC_CHAR)] = NC_CHAR(0);
    return UniqueFreePtr<CHAR>(buffer);
  }
};

// ----------------------------------------------------------------------------
// std::tuple

// std::tuple is serialized as a sequence of each recursively-serialized item.
//
// This is equivalent to manually serializing each item, so reading/writing
// tuples is equivalent to reading/writing their elements in order, e.g.:
// ```
// std::tuple<int, std::string> is = ...;
// aEW.WriteObject(is); // Write the tuple, equivalent to:
// aEW.WriteObject(/* int */ std::get<0>(is), /* string */ std::get<1>(is));
// ...
// // Reading back can be done directly into a tuple:
// auto is = aER.ReadObject<std::tuple<int, std::string>>();
// // Or each item could be read separately:
// auto i = aER.ReadObject<int>(); auto s = aER.ReadObject<std::string>();
// ```
template <typename... Ts>
struct ProfileBufferEntryWriter::Serializer<std::tuple<Ts...>> {
 private:
  template <size_t... Is>
  static Length TupleBytes(const std::tuple<Ts...>& aTuple,
                           std::index_sequence<Is...>) {
    Length bytes = 0;
    // This is pre-C++17 fold trick, using `initializer_list` to unpack the `Is`
    // variadic pack, and the comma operator to:
    // (work on each tuple item, generate an int for the list). Note that the
    // `initializer_list` is unused; the compiler will nicely optimize it away.
    // `std::index_sequence` is just a way to have `Is` be a variadic pack
    // containing numbers between 0 and (tuple size - 1).
    // The compiled end result is like doing:
    //   bytes += SumBytes(get<0>(aTuple));
    //   bytes += SumBytes(get<1>(aTuple));
    //   ...
    //   bytes += SumBytes(get<sizeof...(aTuple) - 1>(aTuple));
    // See https://articles.emptycrate.com/2016/05/14/folds_in_cpp11_ish.html
    // TODO: Replace with C++17 fold.
    Unused << std::initializer_list<int>{
        (bytes += SumBytes(std::get<Is>(aTuple)), 0)...};
    return bytes;
  }

  template <size_t... Is>
  static void TupleWrite(ProfileBufferEntryWriter& aEW,
                         const std::tuple<Ts...>& aTuple,
                         std::index_sequence<Is...>) {
    // TODO: Replace with C++17 fold.
    Unused << std::initializer_list<int>{
        (aEW.WriteObject(std::get<Is>(aTuple)), 0)...};
  }

 public:
  static Length Bytes(const std::tuple<Ts...>& aTuple) {
    // Generate a 0..N-1 index pack, we'll add the sizes of each item.
    return TupleBytes(aTuple, std::index_sequence_for<Ts...>());
  }

  static void Write(ProfileBufferEntryWriter& aEW,
                    const std::tuple<Ts...>& aTuple) {
    // Generate a 0..N-1 index pack, we'll write each item.
    TupleWrite(aEW, aTuple, std::index_sequence_for<Ts...>());
  }
};

template <typename... Ts>
struct ProfileBufferEntryReader::Deserializer<std::tuple<Ts...>> {
  static void ReadInto(ProfileBufferEntryReader& aER,
                       std::tuple<Ts...>& aTuple) {
    aER.ReadBytes(&aTuple, Bytes(aTuple));
  }

  static std::tuple<Ts...> Read(ProfileBufferEntryReader& aER) {
    // Note that this creates default `Ts` first, and then overwrites them.
    std::tuple<Ts...> ob;
    ReadInto(aER, ob);
    return ob;
  }
};

// ----------------------------------------------------------------------------
// mozilla::Tuple

// Tuple is serialized as a sequence of each recursively-serialized
// item.
//
// This is equivalent to manually serializing each item, so reading/writing
// tuples is equivalent to reading/writing their elements in order, e.g.:
// ```
// Tuple<int, std::string> is = ...;
// aEW.WriteObject(is); // Write the Tuple, equivalent to:
// aEW.WriteObject(/* int */ std::get<0>(is), /* string */ std::get<1>(is));
// ...
// // Reading back can be done directly into a Tuple:
// auto is = aER.ReadObject<Tuple<int, std::string>>();
// // Or each item could be read separately:
// auto i = aER.ReadObject<int>(); auto s = aER.ReadObject<std::string>();
// ```
template <typename... Ts>
struct ProfileBufferEntryWriter::Serializer<Tuple<Ts...>> {
 private:
  template <size_t... Is>
  static Length TupleBytes(const Tuple<Ts...>& aTuple,
                           std::index_sequence<Is...>) {
    Length bytes = 0;
    // TODO: Replace with C++17 fold.
    Unused << std::initializer_list<int>{
        (bytes += SumBytes(Get<Is>(aTuple)), 0)...};
    return bytes;
  }

  template <size_t... Is>
  static void TupleWrite(ProfileBufferEntryWriter& aEW,
                         const Tuple<Ts...>& aTuple,
                         std::index_sequence<Is...>) {
    // TODO: Replace with C++17 fold.
    Unused << std::initializer_list<int>{
        (aEW.WriteObject(Get<Is>(aTuple)), 0)...};
  }

 public:
  static Length Bytes(const Tuple<Ts...>& aTuple) {
    // Generate a 0..N-1 index pack, we'll add the sizes of each item.
    return TupleBytes(aTuple, std::index_sequence_for<Ts...>());
  }

  static void Write(ProfileBufferEntryWriter& aEW, const Tuple<Ts...>& aTuple) {
    // Generate a 0..N-1 index pack, we'll write each item.
    TupleWrite(aEW, aTuple, std::index_sequence_for<Ts...>());
  }
};

template <typename... Ts>
struct ProfileBufferEntryReader::Deserializer<Tuple<Ts...>> {
  static void ReadInto(ProfileBufferEntryReader& aER, Tuple<Ts...>& aTuple) {
    aER.ReadBytes(&aTuple, Bytes(aTuple));
  }

  static Tuple<Ts...> Read(ProfileBufferEntryReader& aER) {
    // Note that this creates default `Ts` first, and then overwrites them.
    Tuple<Ts...> ob;
    ReadInto(aER, ob);
    return ob;
  }
};

// ----------------------------------------------------------------------------
// mozilla::Span

// Span. All elements are serialized in sequence.
// The caller is assumed to know the number of elements (they may manually
// write&read it before the span if needed).
// Similar to tuples, reading/writing spans is equivalent to reading/writing
// their elements in order.
template <class T, size_t N>
struct ProfileBufferEntryWriter::Serializer<Span<T, N>> {
  static Length Bytes(const Span<T, N>& aSpan) {
    Length bytes = 0;
    for (const T& element : aSpan) {
      bytes += SumBytes(element);
    }
    return bytes;
  }

  static void Write(ProfileBufferEntryWriter& aEW, const Span<T, N>& aSpan) {
    for (const T& element : aSpan) {
      aEW.WriteObject(element);
    }
  }
};

template <class T, size_t N>
struct ProfileBufferEntryReader::Deserializer<Span<T, N>> {
  // Read elements back into span pointing at a pre-allocated buffer.
  static void ReadInto(ProfileBufferEntryReader& aER, Span<T, N>& aSpan) {
    for (T& element : aSpan) {
      aER.ReadIntoObject(element);
    }
  }

  // A Span does not own its data, this would probably leak so we forbid this.
  static Span<T, N> Read(ProfileBufferEntryReader& aER) = delete;
};

// ----------------------------------------------------------------------------
// mozilla::Maybe

// Maybe<T> is serialized as one byte containing either 'm' (Nothing),
// or 'M' followed by the recursively-serialized `T` object.
template <typename T>
struct ProfileBufferEntryWriter::Serializer<Maybe<T>> {
  static Length Bytes(const Maybe<T>& aMaybe) {
    // 1 byte to store nothing/something flag, then object size if present.
    return aMaybe.isNothing() ? 1 : (1 + SumBytes(aMaybe.ref()));
  }

  static void Write(ProfileBufferEntryWriter& aEW, const Maybe<T>& aMaybe) {
    // 'm'/'M' is just an arbitrary 1-byte value to distinguish states.
    if (aMaybe.isNothing()) {
      aEW.WriteObject<char>('m');
    } else {
      aEW.WriteObject<char>('M');
      // Use the Serializer for the contained type.
      aEW.WriteObject(aMaybe.ref());
    }
  }
};

template <typename T>
struct ProfileBufferEntryReader::Deserializer<Maybe<T>> {
  static void ReadInto(ProfileBufferEntryReader& aER, Maybe<T>& aMaybe) {
    char c = aER.ReadObject<char>();
    if (c == 'm') {
      aMaybe.reset();
    } else {
      MOZ_ASSERT(c == 'M');
      // If aMaybe is empty, create a default `T` first, to be overwritten.
      // Otherwise we'll just overwrite whatever was already there.
      if (aMaybe.isNothing()) {
        aMaybe.emplace();
      }
      // Use the Deserializer for the contained type.
      aER.ReadIntoObject(aMaybe.ref());
    }
  }

  static Maybe<T> Read(ProfileBufferEntryReader& aER) {
    Maybe<T> maybe;
    char c = aER.ReadObject<char>();
    MOZ_ASSERT(c == 'M' || c == 'm');
    if (c == 'M') {
      // Note that this creates a default `T` inside the Maybe first, and then
      // overwrites it.
      maybe = Some(T{});
      // Use the Deserializer for the contained type.
      aER.ReadIntoObject(maybe.ref());
    }
    return maybe;
  }
};

// ----------------------------------------------------------------------------
// mozilla::Variant

// Variant is serialized as the tag (0-based index of the stored type, encoded
// as ULEB128), and the recursively-serialized object.
template <typename... Ts>
struct ProfileBufferEntryWriter::Serializer<Variant<Ts...>> {
 private:
  // Called from the fold expression in `VariantBytes()`, only the selected
  // variant will write into `aOutBytes`.
  template <size_t I>
  static void VariantIBytes(const Variant<Ts...>& aVariantTs,
                            Length& aOutBytes) {
    if (aVariantTs.template is<I>()) {
      aOutBytes = ProfileBufferEntryWriter::ULEB128Size(I) +
                  SumBytes(aVariantTs.template as<I>());
    }
  }

  // Go through all variant tags, and let the selected one write the correct
  // number of bytes.
  template <size_t... Is>
  static Length VariantBytes(const Variant<Ts...>& aVariantTs,
                             std::index_sequence<Is...>) {
    Length bytes = 0;
    // TODO: Replace with C++17 fold.
    Unused << std::initializer_list<int>{
        (VariantIBytes<Is>(aVariantTs, bytes), 0)...};
    MOZ_ASSERT(bytes != 0);
    return bytes;
  }

  // Called from the fold expression in `VariantWrite()`, only the selected
  // variant will serialize the tag and object.
  template <size_t I>
  static void VariantIWrite(ProfileBufferEntryWriter& aEW,
                            const Variant<Ts...>& aVariantTs) {
    if (aVariantTs.template is<I>()) {
      aEW.WriteULEB128(I);
      // Use the Serializer for the contained type.
      aEW.WriteObject(aVariantTs.template as<I>());
    }
  }

  // Go through all variant tags, and let the selected one serialize the correct
  // tag and object.
  template <size_t... Is>
  static void VariantWrite(ProfileBufferEntryWriter& aEW,
                           const Variant<Ts...>& aVariantTs,
                           std::index_sequence<Is...>) {
    // TODO: Replace with C++17 fold.
    Unused << std::initializer_list<int>{
        (VariantIWrite<Is>(aEW, aVariantTs), 0)...};
  }

 public:
  static Length Bytes(const Variant<Ts...>& aVariantTs) {
    // Generate a 0..N-1 index pack, the selected variant will return its size.
    return VariantBytes(aVariantTs, std::index_sequence_for<Ts...>());
  }

  static void Write(ProfileBufferEntryWriter& aEW,
                    const Variant<Ts...>& aVariantTs) {
    // Generate a 0..N-1 index pack, the selected variant will serialize itself.
    VariantWrite(aEW, aVariantTs, std::index_sequence_for<Ts...>());
  }
};

template <typename... Ts>
struct ProfileBufferEntryReader::Deserializer<Variant<Ts...>> {
 private:
  // Called from the fold expression in `VariantReadInto()`, only the selected
  // variant will deserialize the object.
  template <size_t I>
  static void VariantIReadInto(ProfileBufferEntryReader& aER,
                               Variant<Ts...>& aVariantTs, unsigned aTag) {
    if (I == aTag) {
      // Ensure the variant contains the target type. Note that this may create
      // a default object.
      if (!aVariantTs.template is<I>()) {
        aVariantTs = Variant<Ts...>(VariantIndex<I>{});
      }
      aER.ReadIntoObject(aVariantTs.template as<I>());
    }
  }

  template <size_t... Is>
  static void VariantReadInto(ProfileBufferEntryReader& aER,
                              Variant<Ts...>& aVariantTs,
                              std::index_sequence<Is...>) {
    unsigned tag = aER.ReadULEB128<unsigned>();
    // TODO: Replace with C++17 fold.
    Unused << std::initializer_list<int>{
        (VariantIReadInto<Is>(aER, aVariantTs, tag), 0)...};
  }

 public:
  static void ReadInto(ProfileBufferEntryReader& aER,
                       Variant<Ts...>& aVariantTs) {
    // Generate a 0..N-1 index pack, the selected variant will deserialize
    // itself.
    VariantReadInto(aER, aVariantTs, std::index_sequence_for<Ts...>());
  }

  static Variant<Ts...> Read(ProfileBufferEntryReader& aER) {
    // Note that this creates a default `Variant` of the first type, and then
    // overwrites it. Consider using `ReadInto` for more control if needed.
    Variant<Ts...> variant(VariantIndex<0>{});
    ReadInto(aER, variant);
    return variant;
  }
};

}  // namespace mozilla

#endif  // ProfileBufferEntrySerialization_h
