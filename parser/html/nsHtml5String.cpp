/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5String.h"
#include "nsCharTraits.h"
#include "nsUTF8Utils.h"
#include "nsHtml5TreeBuilder.h"

nsHtml5String::nsHtml5String(already_AddRefed<nsStringBuffer> aBuffer,
                             uint32_t aLength)
  : mBuffer(aBuffer.take())
  , mLength(aLength)
{
  if (mBuffer) {
    MOZ_ASSERT(aLength);
  } else {
    MOZ_ASSERT(!aLength || aLength == UINT32_MAX);
  }
}

void
nsHtml5String::ToString(nsAString& aString)
{
  if (mBuffer) {
    mBuffer->ToString(mLength, aString);
  } else {
    aString.Truncate();
    if (mLength) {
      aString.SetIsVoid(true);
    }
  }
}

void
nsHtml5String::CopyToBuffer(char16_t* aBuffer)
{
  if (mBuffer) {
    memcpy(aBuffer, mBuffer->Data(), mLength * sizeof(char16_t));
  }
}

bool
nsHtml5String::LowerCaseEqualsASCII(const char* aLowerCaseLiteral)
{
  if (!mBuffer) {
    if (mLength) {
      // This string is null
      return false;
    }
    // this string is empty
    return !(*aLowerCaseLiteral);
  }
  return !nsCharTraits<char16_t>::compareLowerCaseToASCIINullTerminated(
    reinterpret_cast<char16_t*>(mBuffer->Data()), Length(), aLowerCaseLiteral);
}

bool
nsHtml5String::EqualsASCII(const char* aLiteral)
{
  if (!mBuffer) {
    if (mLength) {
      // This string is null
      return false;
    }
    // this string is empty
    return !(*aLiteral);
  }
  return !nsCharTraits<char16_t>::compareASCIINullTerminated(
    reinterpret_cast<char16_t*>(mBuffer->Data()), Length(), aLiteral);
}

bool
nsHtml5String::LowerCaseStartsWithASCII(const char* aLowerCaseLiteral)
{
  if (!mBuffer) {
    if (mLength) {
      // This string is null
      return false;
    }
    // this string is empty
    return !(*aLowerCaseLiteral);
  }
  const char* litPtr = aLowerCaseLiteral;
  const char16_t* strPtr = reinterpret_cast<char16_t*>(mBuffer->Data());
  const char16_t* end = strPtr + Length();
  char16_t litChar;
  while ((litChar = *litPtr) && (strPtr != end)) {
    MOZ_ASSERT(!(litChar >= 'A' && litChar <= 'Z'),
               "Literal isn't in lower case.");
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

bool
nsHtml5String::Equals(nsHtml5String aOther)
{
  MOZ_ASSERT(operator bool());
  MOZ_ASSERT(aOther);
  if (mLength != aOther.mLength) {
    return false;
  }
  if (!mBuffer) {
    return true;
  }
  MOZ_ASSERT(aOther.mBuffer);
  return !memcmp(
    mBuffer->Data(), aOther.mBuffer->Data(), Length() * sizeof(char16_t));
}

nsHtml5String
nsHtml5String::Clone()
{
  MOZ_ASSERT(operator bool());
  RefPtr<nsStringBuffer> ref(mBuffer);
  return nsHtml5String(ref.forget(), mLength);
}

void
nsHtml5String::Release()
{
  if (mBuffer) {
    mBuffer->Release();
    mBuffer = nullptr;
  }
  mLength = UINT32_MAX;
}

// static
nsHtml5String
nsHtml5String::FromBuffer(char16_t* aBuffer,
                          int32_t aLength,
                          nsHtml5TreeBuilder* aTreeBuilder)
{
  if (!aLength) {
    return nsHtml5String(nullptr, 0U);
  }
  // Work with nsStringBuffer directly to make sure that storage is actually
  // nsStringBuffer and to make sure the allocation strategy matches
  // nsAttrValue::GetStringBuffer, so that it doesn't need to reallocate and
  // copy.
  RefPtr<nsStringBuffer> buffer(
    nsStringBuffer::Alloc((aLength + 1) * sizeof(char16_t)));
  if (!buffer) {
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
    return nsHtml5String(buffer.forget(), 1);
  }
  char16_t* data = reinterpret_cast<char16_t*>(buffer->Data());
  memcpy(data, aBuffer, aLength * sizeof(char16_t));
  data[aLength] = 0;
  return nsHtml5String(buffer.forget(), aLength);
}

// static
nsHtml5String
nsHtml5String::FromLiteral(const char* aLiteral)
{
  size_t length = std::strlen(aLiteral);
  if (!length) {
    return nsHtml5String(nullptr, 0U);
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
  LossyConvertEncoding8to16 converter(data);
  converter.write(aLiteral, length);
  data[length] = 0;
  return nsHtml5String(buffer.forget(), length);
}

// static
nsHtml5String
nsHtml5String::FromString(const nsAString& aString)
{
  auto length = aString.Length();
  if (!length) {
    return nsHtml5String(nullptr, 0U);
  }
  RefPtr<nsStringBuffer> buffer = nsStringBuffer::FromString(aString);
  if (buffer) {
    return nsHtml5String(buffer.forget(), length);
  }
  buffer = nsStringBuffer::Alloc((length + 1) * sizeof(char16_t));
  if (!buffer) {
    MOZ_CRASH("Out of memory.");
  }
  char16_t* data = reinterpret_cast<char16_t*>(buffer->Data());
  memcpy(data, aString.BeginReading(), length * sizeof(char16_t));
  data[length] = 0;
  return nsHtml5String(buffer.forget(), length);
}

// static
nsHtml5String
nsHtml5String::EmptyString()
{
  return nsHtml5String(nullptr, 0U);
}
