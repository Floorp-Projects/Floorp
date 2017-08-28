#include "HangStack.h"
#include "shared-libraries.h"

namespace mozilla {

HangStack::HangStack(const HangStack& aOther)
  : mModules(aOther.mModules)
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

namespace {

// Sorting comparator used by ReadModuleInformation. Sorts PC Frames by their
// PC.
struct PCFrameComparator {
  bool LessThan(HangStack::Frame* const& a, HangStack::Frame* const& b) const {
    return a->AsPC() < b->AsPC();
  }
  bool Equals(HangStack::Frame* const& a, HangStack::Frame* const& b) const {
    return a->AsPC() == b->AsPC();
  }
};

} // anonymous namespace

void
HangStack::ReadModuleInformation()
{
  // mModules should be empty when we start filling it.
  mModules.Clear();

  // Create a sorted list of the PCs in the current stack.
  AutoTArray<Frame*, 100> frames;
  for (auto& frame : *this) {
    if (frame.GetKind() == Frame::Kind::PC) {
      frames.AppendElement(&frame);
    }
  }
  PCFrameComparator comparator;
  frames.Sort(comparator);

  SharedLibraryInfo rawModules = SharedLibraryInfo::GetInfoForSelf();
  rawModules.SortByAddress();

  size_t frameIdx = 0;
  for (size_t i = 0; i < rawModules.GetSize(); ++i) {
    const SharedLibrary& info = rawModules.GetEntry(i);
    uintptr_t moduleStart = info.GetStart();
    uintptr_t moduleEnd = info.GetEnd() - 1;
    // the interval is [moduleStart, moduleEnd)

    bool moduleReferenced = false;
    for (; frameIdx < frames.Length(); ++frameIdx) {
      auto& frame = frames[frameIdx];
      // We've moved past this frame, let's go to the next one.
      if (frame->AsPC() >= moduleEnd) {
        break;
      }
      if (frame->AsPC() >= moduleStart) {
        uint64_t offset = frame->AsPC() - moduleStart;
        if (NS_WARN_IF(offset > UINT32_MAX)) {
          continue; // module/offset can only hold 32-bit offsets into shared libraries.
        }

        // If we found the module, rewrite the Frame entry to instead be a
        // ModOffset one. mModules.Length() will be the index of the module when
        // we append it below, and we set moduleReferenced to true to ensure
        // that we do.
        moduleReferenced = true;
        uint32_t module = mModules.Length();
        ModOffset modOffset = {
          module,
          static_cast<uint32_t>(offset)
        };
        *frame = Frame(modOffset);
      }
    }

    if (moduleReferenced) {
      nsDependentCString cstr(info.GetBreakpadId().c_str());
      Module module = {
        info.GetDebugName(),
        cstr
      };
      mModules.AppendElement(module);
    }
  }
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
ParamTraits<mozilla::HangStack::Module>::Write(Message* aMsg, const mozilla::HangStack::Module& aParam)
{
  WriteParam(aMsg, aParam.mName);
  WriteParam(aMsg, aParam.mBreakpadId);
}

bool
ParamTraits<mozilla::HangStack::Module>::Read(const Message* aMsg, PickleIterator* aIter,
                                              mozilla::HangStack::Module* aResult)
{
  if (!ReadParam(aMsg, aIter, &aResult->mName)) {
    return false;
  }
  if (!ReadParam(aMsg, aIter, &aResult->mBreakpadId)) {
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
      case Frame::Kind::CONTENT:
      case Frame::Kind::WASM:
      case Frame::Kind::JIT:
      case Frame::Kind::SUPPRESSED: {
        // NOTE: no associated data.
        break;
      }
      default: {
        MOZ_RELEASE_ASSERT(false, "Invalid kind for HangStack Frame");
        break;
      }
    }
  }

  WriteParam(aMsg, aParam.GetModules());
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

  if (!aResult->reserve(length)) {
    return false;
  }

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
      case Frame::Kind::CONTENT:
        aResult->infallibleAppend(Frame::Content());
        break;
      case Frame::Kind::WASM:
        aResult->infallibleAppend(Frame::Wasm());
        break;
      case Frame::Kind::JIT:
        aResult->infallibleAppend(Frame::Jit());
        break;
      case Frame::Kind::SUPPRESSED:
        aResult->infallibleAppend(Frame::Suppressed());
        break;
      default:
        // We can't deserialize other kinds!
        return false;
    }
  }

  if (!ReadParam(aMsg, aIter, &aResult->GetModules())) {
    return false;
  }

  return true;
}

} // namespace IPC
