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
    const Frame& frame = aOther[i];

    // If the source string is a reference to aOther's buffer, we have to append
    // via buffer, otherwise we can just copy the entry over.
    if (frame.GetKind() == Frame::Kind::STRING) {
      const char* s = frame.AsString();
      if (aOther.IsInBuffer(s)) {
        InfallibleAppendViaBuffer(s, strlen(s));
        continue;
      }
    }

    infallibleAppend(frame);
  }
  MOZ_ASSERT(mImpl.length() == aOther.mImpl.length());
  MOZ_ASSERT(mBuffer.length() == aOther.mBuffer.length());
}

void
HangStack::InfallibleAppendViaBuffer(const char* aText, size_t aLength)
{
  MOZ_ASSERT(this->canAppendWithoutRealloc(1));
  // Include null-terminator in length count.
  MOZ_ASSERT(mBuffer.canAppendWithoutRealloc(aLength + 1));

  const char* const entry = mBuffer.end();
  mBuffer.infallibleAppend(aText, aLength);
  mBuffer.infallibleAppend('\0'); // Explicitly append null-terminator

  this->infallibleAppend(Frame(entry));
}

bool
HangStack::AppendViaBuffer(const char* aText, size_t aLength)
{
  if (!this->reserve(this->length() + 1)) {
    return false;
  }

  // Keep track of the previous buffer in case we need to adjust pointers later.
  const char* const prevStart = mBuffer.begin();
  const char* const prevEnd = mBuffer.end();

  // Include null-terminator in length count.
  if (!mBuffer.reserve(mBuffer.length() + aLength + 1)) {
    return false;
  }

  if (prevStart != mBuffer.begin()) {
    // The buffer has moved; we have to adjust pointers in the stack.
    for (auto & frame : *this) {
      if (frame.GetKind() == Frame::Kind::STRING) {
        const char*& entry = frame.AsString();
        if (entry >= prevStart && entry < prevEnd) {
          // Move from old buffer to new buffer.
          entry += mBuffer.begin() - prevStart;
        }
      }
    }
  }

  InfallibleAppendViaBuffer(aText, aLength);
  return true;
}

} // namespace mozilla

namespace IPC {

void
ParamTraits<mozilla::HangStack::ModOffset>::Write(Message* aMsg, const mozilla::HangStack::ModOffset& aParam)
{
  WriteParam(aMsg, aParam.mModule);
  WriteParam(aMsg, aParam.mOffset);
}

bool
ParamTraits<mozilla::HangStack::ModOffset>::Read(const Message* aMsg,
                                                 PickleIterator* aIter,
                                                 mozilla::HangStack::ModOffset* aResult)
{
  if (!ReadParam(aMsg, aIter, &aResult->mModule)) {
    return false;
  }
  if (!ReadParam(aMsg, aIter, &aResult->mOffset)) {
    return false;
  }
  return true;
}

void
ParamTraits<mozilla::HangStack>::Write(Message* aMsg, const mozilla::HangStack& aParam)
{
  typedef mozilla::HangStack::Frame Frame;

  size_t length = aParam.length();
  WriteParam(aMsg, length);
  for (size_t i = 0; i < length; ++i) {
    const Frame& frame = aParam[i];
    WriteParam(aMsg, frame.GetKind());

    switch (frame.GetKind()) {
      case Frame::Kind::STRING: {
        nsDependentCString str(frame.AsString());
        WriteParam(aMsg, static_cast<nsACString&>(str));
        break;
      }
      case Frame::Kind::MODOFFSET: {
        WriteParam(aMsg, frame.AsModOffset());
        break;
      }
      case Frame::Kind::PC: {
        WriteParam(aMsg, frame.AsPC());
        break;
      }
      default: {
        MOZ_RELEASE_ASSERT(false, "Invalid kind for HangStack Frame");
        break;
      }
    }
  }
}

bool
ParamTraits<mozilla::HangStack>::Read(const Message* aMsg,
                                      PickleIterator* aIter,
                                      mozilla::HangStack* aResult)
{
  // Shorten the name of Frame
  typedef mozilla::HangStack::Frame Frame;

  size_t length;
  if (!ReadParam(aMsg, aIter, &length)) {
    return false;
  }

  aResult->reserve(length);
  for (size_t i = 0; i < length; ++i) {
    Frame::Kind kind;
    if (!ReadParam(aMsg, aIter, &kind)) {
      return false;
    }

    switch (kind) {
      case Frame::Kind::STRING: {
        nsAutoCString str;
        if (!ReadParam(aMsg, aIter, static_cast<nsACString*>(&str))) {
          return false;
        }
        aResult->AppendViaBuffer(str.get(), str.Length());
        break;
      }
      case Frame::Kind::MODOFFSET: {
        mozilla::HangStack::ModOffset modOff;
        if (!ReadParam(aMsg, aIter, &modOff)) {
          return false;
        }
        aResult->infallibleAppend(Frame(modOff));
        break;
      }
      case Frame::Kind::PC: {
        uintptr_t pc;
        if (!ReadParam(aMsg, aIter, &pc)) {
          return false;
        }
        aResult->infallibleAppend(Frame(pc));
        break;
      }
      default:
        // We can't deserialize other kinds!
        return false;
    }
  }
  return true;
}

} // namespace IPC
