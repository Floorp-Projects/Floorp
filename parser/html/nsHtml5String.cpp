/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5String.h"
#include "nsCharTraits.h"
#include "nsHtml5TreeBuilder.h"
#include "nsUTF8Utils.h"

void nsHtml5String::ToString(nsAString& aString) {
  switch (GetKind()) {
    case eStringBuffer:
      return AsStringBuffer()->ToString(Length(), aString);
    case eAtom:
      return AsAtom()->ToString(aString);
    case eEmpty:
      aString.Truncate();
      return;
    default:
      aString.Truncate();
      aString.SetIsVoid(true);
      return;
  }
}

void nsHtml5String::CopyToBuffer(char16_t* aBuffer) const {
  memcpy(aBuffer, AsPtr(), Length() * sizeof(char16_t));
}

bool nsHtml5String::LowerCaseEqualsASCII(const char* aLowerCaseLiteral) const {
  return !nsCharTraits<char16_t>::compareLowerCaseToASCIINullTerminated(
      AsPtr(), Length(), aLowerCaseLiteral);
}

bool nsHtml5String::EqualsASCII(const char* aLiteral) const {
  return !nsCharTraits<char16_t>::compareASCIINullTerminated(AsPtr(), Length(),
                                                             aLiteral);
}

bool nsHtml5String::LowerCaseStartsWithASCII(
    const char* aLowerCaseLiteral) const {
  const char* litPtr = aLowerCaseLiteral;
  const char16_t* strPtr = AsPtr();
  const char16_t* end = strPtr + Length();
  char16_t litChar;
  while ((litChar = *litPtr)) {
    MOZ_ASSERT(!(litChar >= 'A' && litChar <= 'Z'),
               "Literal isn't in lower case.");
    if (strPtr == end) {
      return false;
    }
    char16_t strChar = *strPtr;
    if (strChar >= 'A' && strChar <= 'Z') {
      strChar += 0x20;
    }
    if (litChar != strChar) {
      return false;
    }
    ++litPtr;
    ++strPtr;
  }
  return true;
}

bool nsHtml5String::Equals(nsHtml5String aOther) const {
  MOZ_ASSERT(operator bool());
  MOZ_ASSERT(aOther);
  if (Length() != aOther.Length()) {
    return false;
  }
  return !memcmp(AsPtr(), aOther.AsPtr(), Length() * sizeof(char16_t));
}

nsHtml5String nsHtml5String::Clone() {
  switch (GetKind()) {
    case eStringBuffer:
      AsStringBuffer()->AddRef();
      break;
    case eAtom:
      AsAtom()->AddRef();
      break;
    default:
      break;
  }
  return nsHtml5String(mBits);
}

void nsHtml5String::Release() {
  switch (GetKind()) {
    case eStringBuffer:
      AsStringBuffer()->Release();
      break;
    case eAtom:
      AsAtom()->Release();
      break;
    default:
      break;
  }
  mBits = eNull;
}

// static
nsHtml5String nsHtml5String::FromBuffer(char16_t* aBuffer, int32_t aLength,
                                        nsHtml5TreeBuilder* aTreeBuilder) {
  if (!aLength) {
    return nsHtml5String(eEmpty);
  }
  // Work with nsStringBuffer directly to make sure that storage is actually
  // nsStringBuffer and to make sure the allocation strategy matches
  // nsAttrValue::GetStringBuffer, so that it doesn't need to reallocate and
  // copy.
  RefPtr<nsStringBuffer> buffer = nsStringBuffer::Create(aBuffer, aLength);
  if (MOZ_UNLIKELY(!buffer)) {
    if (!aTreeBuilder) {
      MOZ_CRASH("Out of memory.");
    }
    aTreeBuilder->MarkAsBroken(NS_ERROR_OUT_OF_MEMORY);
    buffer = nsStringBuffer::Alloc(2 * sizeof(char16_t));
    if (!buffer) {
      MOZ_CRASH(
          "Out of memory so badly that couldn't even allocate placeholder.");
    }
    char16_t* data = reinterpret_cast<char16_t*>(buffer->Data());
    data[0] = 0xFFFD;
    data[1] = 0;
  }
  return nsHtml5String(reinterpret_cast<uintptr_t>(buffer.forget().take()) |
                       eStringBuffer);
}

// static
nsHtml5String nsHtml5String::FromLiteral(const char* aLiteral) {
  size_t length = std::strlen(aLiteral);
  if (!length) {
    return nsHtml5String(eEmpty);
  }
  // Work with nsStringBuffer directly to make sure that storage is actually
  // nsStringBuffer and to make sure the allocation strategy matches
  // nsAttrValue::GetStringBuffer, so that it doesn't need to reallocate and
  // copy.
  RefPtr<nsStringBuffer> buffer(
      nsStringBuffer::Alloc((length + 1) * sizeof(char16_t)));
  if (!buffer) {
    MOZ_CRASH("Out of memory.");
  }
  char16_t* data = reinterpret_cast<char16_t*>(buffer->Data());
  ConvertAsciitoUtf16(mozilla::Span(aLiteral, length),
                      mozilla::Span(data, length));
  data[length] = 0;
  return nsHtml5String(reinterpret_cast<uintptr_t>(buffer.forget().take()) |
                       eStringBuffer);
}

// static
nsHtml5String nsHtml5String::FromString(const nsAString& aString) {
  auto length = aString.Length();
  if (!length) {
    return nsHtml5String(eEmpty);
  }
  RefPtr<nsStringBuffer> buffer = nsStringBuffer::FromString(aString);
  if (buffer && (length == buffer->StorageSize() / sizeof(char16_t) - 1)) {
    return nsHtml5String(reinterpret_cast<uintptr_t>(buffer.forget().take()) |
                         eStringBuffer);
  }
  buffer = nsStringBuffer::Alloc((length + 1) * sizeof(char16_t));
  if (!buffer) {
    MOZ_CRASH("Out of memory.");
  }
  char16_t* data = reinterpret_cast<char16_t*>(buffer->Data());
  memcpy(data, aString.BeginReading(), length * sizeof(char16_t));
  data[length] = 0;
  return nsHtml5String(reinterpret_cast<uintptr_t>(buffer.forget().take()) |
                       eStringBuffer);
}

// static
nsHtml5String nsHtml5String::FromAtom(already_AddRefed<nsAtom> aAtom) {
  return nsHtml5String(reinterpret_cast<uintptr_t>(aAtom.take()) | eAtom);
}

// static
nsHtml5String nsHtml5String::EmptyString() { return nsHtml5String(eEmpty); }
