#include "HangStack.h"

namespace mozilla {

HangStack::HangStack(const HangStack& aOther)
{
  if (NS_WARN_IF(!mBuffer.reserve(aOther.mBuffer.length()) ||
                 !mImpl.reserve(aOther.mImpl.length()))) {
    return;
  }
  // XXX: I should be able to just memcpy the other stack's mImpl and mBuffer,
  // and then just re-offset pointers.
  for (size_t i = 0; i < aOther.length(); ++i) {
    const char* s = aOther[i];
    if (aOther.IsInBuffer(s)) {
      InfallibleAppendViaBuffer(s, strlen(s));
    } else {
      infallibleAppend(s);
    }
  }
  MOZ_ASSERT(mImpl.length() == aOther.mImpl.length());
  MOZ_ASSERT(mBuffer.length() == aOther.mBuffer.length());
}

const char*
HangStack::InfallibleAppendViaBuffer(const char* aText, size_t aLength)
{
  MOZ_ASSERT(this->canAppendWithoutRealloc(1));
  // Include null-terminator in length count.
  MOZ_ASSERT(mBuffer.canAppendWithoutRealloc(aLength + 1));

  const char* const entry = mBuffer.end();
  mBuffer.infallibleAppend(aText, aLength);
  mBuffer.infallibleAppend('\0'); // Explicitly append null-terminator
  this->infallibleAppend(entry);
  return entry;
}

const char*
HangStack::AppendViaBuffer(const char* aText, size_t aLength)
{
  if (!this->reserve(this->length() + 1)) {
    return nullptr;
  }

  // Keep track of the previous buffer in case we need to adjust pointers later.
  const char* const prevStart = mBuffer.begin();
  const char* const prevEnd = mBuffer.end();

  // Include null-terminator in length count.
  if (!mBuffer.reserve(mBuffer.length() + aLength + 1)) {
    return nullptr;
  }

  if (prevStart != mBuffer.begin()) {
    // The buffer has moved; we have to adjust pointers in the stack.
    for (auto & entry : *this) {
      if (entry >= prevStart && entry < prevEnd) {
        // Move from old buffer to new buffer.
        entry += mBuffer.begin() - prevStart;
      }
    }
  }

  return InfallibleAppendViaBuffer(aText, aLength);
}

} // namespace mozilla

namespace IPC {

void
ParamTraits<mozilla::HangStack>::Write(Message* aMsg, const mozilla::HangStack& aParam)
{
  size_t length = aParam.length();
  WriteParam(aMsg, length);
  for (size_t i = 0; i < length; ++i) {
    nsDependentCString str(aParam[i]);
    WriteParam(aMsg, static_cast<nsACString&>(str));
  }
}

bool
ParamTraits<mozilla::HangStack>::Read(const Message* aMsg,
                                      PickleIterator* aIter,
                                      mozilla::HangStack* aResult)
{
  size_t length;
  if (!ReadParam(aMsg, aIter, &length)) {
    return false;
  }

  for (size_t i = 0; i < length; ++i) {
    nsAutoCString str;
    if (!ReadParam(aMsg, aIter, &str)) {
      return false;
    }
    aResult->AppendViaBuffer(str.get(), str.Length());
  }
  return true;
}

} // namespace IPC
