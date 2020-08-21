/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfileBufferEntrySerializationGeckoExtensions_h
#define ProfileBufferEntrySerializationGeckoExtensions_h

#include "mozilla/ProfileBufferEntrySerialization.h"

#include "js/AllocPolicy.h"
#include "js/Utility.h"
#include "nsString.h"

namespace mozilla {

// ----------------------------------------------------------------------------
// ns[C]String

// nsString or nsCString contents are serialized as the number of bytes (encoded
// as ULEB128) and all the characters in the string. The terminal '\0' is
// omitted.
// Make sure you write and read with the same character type!
//
// Usage: `nsCString s = ...; aEW.WriteObject(s);`
template <typename CHAR>
struct ProfileBufferEntryWriter::Serializer<nsTString<CHAR>> {
  static Length Bytes(const nsTString<CHAR>& aS) {
    const auto length = aS.Length();
    return ProfileBufferEntryWriter::ULEB128Size(length) +
           static_cast<Length>(length * sizeof(CHAR));
  }

  static void Write(ProfileBufferEntryWriter& aEW, const nsTString<CHAR>& aS) {
    const auto length = aS.Length();
    aEW.WriteULEB128(length);
    // Copy the bytes from the string's buffer.
    aEW.WriteBytes(aS.Data(), length * sizeof(CHAR));
  }
};

template <typename CHAR>
struct ProfileBufferEntryReader::Deserializer<nsTString<CHAR>> {
  static void ReadInto(ProfileBufferEntryReader& aER, nsTString<CHAR>& aS) {
    aS = Read(aER);
  }

  static nsTString<CHAR> Read(ProfileBufferEntryReader& aER) {
    const Length length = aER.ReadULEB128<Length>();
    nsTString<CHAR> s;
    // BulkWrite is the most efficient way to copy bytes into the target string.
    auto writerOrErr = s.BulkWrite(length, 0, true);
    MOZ_RELEASE_ASSERT(!writerOrErr.isErr());

    auto writer = writerOrErr.unwrap();

    aER.ReadBytes(writer.Elements(), length * sizeof(CHAR));
    writer.Finish(length, true);
    return s;
  }
};

// ----------------------------------------------------------------------------
// nsAuto[C]String

// nsAuto[C]String contents are serialized as the number of bytes (encoded as
// ULEB128) and all the characters in the string. The terminal '\0' is omitted.
// Make sure you write and read with the same character type!
//
// Usage: `nsAutoCString s = ...; aEW.WriteObject(s);`
template <typename CHAR, size_t N>
struct ProfileBufferEntryWriter::Serializer<nsTAutoStringN<CHAR, N>> {
  static Length Bytes(const nsTAutoStringN<CHAR, N>& aS) {
    const auto length = aS.Length();
    return ProfileBufferEntryWriter::ULEB128Size(length) +
           static_cast<Length>(length * sizeof(CHAR));
  }

  static void Write(ProfileBufferEntryWriter& aEW,
                    const nsTAutoStringN<CHAR, N>& aS) {
    const auto length = aS.Length();
    aEW.WriteULEB128(length);
    // Copy the bytes from the string's buffer.
    aEW.WriteBytes(aS.BeginReading(), length * sizeof(CHAR));
  }
};

template <typename CHAR, size_t N>
struct ProfileBufferEntryReader::Deserializer<nsTAutoStringN<CHAR, N>> {
  static void ReadInto(ProfileBufferEntryReader& aER,
                       nsTAutoStringN<CHAR, N>& aS) {
    aS = Read(aER);
  }

  static nsTAutoStringN<CHAR, N> Read(ProfileBufferEntryReader& aER) {
    const auto length = aER.ReadULEB128<Length>();
    nsTAutoStringN<CHAR, N> s;
    // BulkWrite is the most efficient way to copy bytes into the target string.
    auto writerOrErr = s.BulkWrite(length, 0, true);
    MOZ_RELEASE_ASSERT(!writerOrErr.isErr());

    auto writer = writerOrErr.unwrap();
    aER.ReadBytes(writer.Elements(), length * sizeof(CHAR));
    writer.Finish(length, true);
    return s;
  }
};

// ----------------------------------------------------------------------------
// JS::UniqueChars

// JS::UniqueChars contents are serialized as the number of bytes (encoded as
// ULEB128) and all the characters in the string. The terminal '\0' is omitted.
// Note: A nullptr pointer will be serialized like an empty string, so when
// deserializing it will result in an allocated buffer only containing a
// single null terminator.
//
// Usage: `JS::UniqueChars s = ...; aEW.WriteObject(s);`
template <>
struct ProfileBufferEntryWriter::Serializer<JS::UniqueChars> {
  static Length Bytes(const JS::UniqueChars& aS) {
    if (!aS) {
      return ProfileBufferEntryWriter::ULEB128Size<Length>(0);
    }
    const auto len = static_cast<Length>(strlen(aS.get()));
    return ProfileBufferEntryWriter::ULEB128Size(len) + len;
  }

  static void Write(ProfileBufferEntryWriter& aEW, const JS::UniqueChars& aS) {
    if (!aS) {
      aEW.WriteULEB128<Length>(0);
      return;
    }
    const auto len = static_cast<Length>(strlen(aS.get()));
    aEW.WriteULEB128(len);
    aEW.WriteBytes(aS.get(), len);
  }
};

template <>
struct ProfileBufferEntryReader::Deserializer<JS::UniqueChars> {
  static void ReadInto(ProfileBufferEntryReader& aER, JS::UniqueChars& aS) {
    aS = Read(aER);
  }

  static JS::UniqueChars Read(ProfileBufferEntryReader& aER) {
    const auto len = aER.ReadULEB128<Length>();
    // Use the same allocation policy as JS_smprintf.
    char* buffer =
        static_cast<char*>(js::SystemAllocPolicy{}.pod_malloc<char>(len + 1));
    aER.ReadBytes(buffer, len);
    buffer[len] = '\0';
    return JS::UniqueChars(buffer);
  }
};

}  // namespace mozilla

#endif  // ProfileBufferEntrySerializationGeckoExtensions_h
