/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HangStack_h
#define mozilla_HangStack_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/ProcessedStack.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Move.h"
#include "nsTArray.h"
#include "nsIHangDetails.h"

namespace mozilla {

/* A native stack is a simple list of pointers, so rather than building a
   wrapper type, we typdef the type here. */
typedef std::vector<uintptr_t> NativeHangStack;

/* HangStack stores an array of const char pointers,
   with optional internal storage for strings. */
class HangStack
{
public:
  static const size_t sMaxInlineStorage = 8;

  // The maximum depth for the native stack frames that we might collect.
  // XXX: Consider moving this to a different object?
  static const size_t sMaxNativeFrames = 150;

  struct ModOffset {
    uint32_t mModule;
    uint32_t mOffset;

    bool operator==(const ModOffset& aOther) const {
      return mModule == aOther.mModule && mOffset == aOther.mOffset;
    }
  };

  // A HangStack frame is one of the following types:
  // * Kind::STRING(const char*) : A string representing a pseudostack or chrome JS stack frame.
  // * Kind::MODOFFSET(ModOffset) : A module index and offset into that module.
  // * Kind::PC(uintptr_t) : A raw program counter which has not been mapped to a module.
  // * Kind::CONTENT: A hidden "(content script)" frame.
  // * Kind::JIT : An unprocessed  "(jit frame)".
  // * Kind::WASM : An unprocessed "(wasm)" frame.
  // * Kind::SUPPRESSED : A JS frame while profiling was suppressed.
  //
  // NOTE: A manually rolled tagged enum is used instead of mozilla::Variant
  // here because we cannot use mozilla::Variant's IPC serialization directly.
  // Unfortunately the const char* variant needs the context of the HangStack
  // which it is in order to correctly deserialize. For this reason, a Frame by
  // itself does not have a ParamTraits implementation.
  class Frame
  {
  public:
    enum class Kind {
      STRING,
      MODOFFSET,
      PC,
      CONTENT,
      JIT,
      WASM,
      SUPPRESSED,
      END // Marker
    };

    Frame()
      : mKind(Kind::STRING)
      , mString("")
    {}

    explicit Frame(const char* aString)
      : mKind(Kind::STRING)
      , mString(aString)
    {}

    explicit Frame(ModOffset aModOffset)
      : mKind(Kind::MODOFFSET)
      , mModOffset(aModOffset)
    {}

    explicit Frame(uintptr_t aPC)
      : mKind(Kind::PC)
      , mPC(aPC)
    {}

    Kind GetKind() const {
      return mKind;
    }

    const char*& AsString() {
      MOZ_ASSERT(mKind == Kind::STRING);
      return mString;
    }

    const char* const& AsString() const {
      MOZ_ASSERT(mKind == Kind::STRING);
      return mString;
    }

    const ModOffset& AsModOffset() const {
      MOZ_ASSERT(mKind == Kind::MODOFFSET);
      return mModOffset;
    }

    const uintptr_t& AsPC() const {
      MOZ_ASSERT(mKind == Kind::PC);
      return mPC;
    }

    // Public constant frames copies of each of the data-less frames.
    static Frame Content() {
      return Frame(Kind::CONTENT);
    }
    static Frame Jit() {
      return Frame(Kind::JIT);
    }
    static Frame Wasm() {
      return Frame(Kind::WASM);
    }
    static Frame Suppressed() {
      return Frame(Kind::SUPPRESSED);
    }

  private:
    explicit Frame(Kind aKind)
      : mKind(aKind)
    {
      MOZ_ASSERT(aKind == Kind::CONTENT ||
                 aKind == Kind::JIT ||
                 aKind == Kind::WASM ||
                 aKind == Kind::SUPPRESSED,
                 "Kind must only be one of CONTENT, JIT, WASM or SUPPRESSED "
                 "for the data-free constructor.");
    }

    Kind mKind;
    union {
      const char* mString;
      ModOffset mModOffset;
      uintptr_t mPC;
    };
  };

  struct Module {
    // The file name, /foo/bar/libxul.so for example.
    // It can contain unicode characters.
    nsString mName;
    nsCString mBreakpadId;

    bool operator==(const Module& aOther) const {
      return mName == aOther.mName && mBreakpadId == aOther.mBreakpadId;
    }
  };

private:
  typedef mozilla::Vector<Frame, sMaxInlineStorage> Impl;
  Impl mImpl;

  // Stack entries can either be a static const char*
  // or a pointer to within this buffer.
  mozilla::Vector<char, 0> mBuffer;
  nsTArray<Module> mModules;

public:
  HangStack() {}

  HangStack(const HangStack& aOther);
  HangStack(HangStack&& aOther)
    : mImpl(mozilla::Move(aOther.mImpl))
    , mBuffer(mozilla::Move(aOther.mBuffer))
    , mModules(mozilla::Move(aOther.mModules))
  {
  }

  HangStack& operator=(HangStack&& aOther) {
    mImpl = mozilla::Move(aOther.mImpl);
    mBuffer = mozilla::Move(aOther.mBuffer);
    mModules = mozilla::Move(aOther.mModules);
    return *this;
  }

  bool operator==(const HangStack& aOther) const {
    for (size_t i = 0; i < length(); i++) {
      if (!IsSameAsEntry(operator[](i), aOther[i])) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(const HangStack& aOther) const {
    return !operator==(aOther);
  }

  Frame& operator[](size_t aIndex) {
    return mImpl[aIndex];
  }

  Frame const& operator[](size_t aIndex) const {
    return mImpl[aIndex];
  }

  size_t capacity() const { return mImpl.capacity(); }
  size_t length() const { return mImpl.length(); }
  bool empty() const { return mImpl.empty(); }
  bool canAppendWithoutRealloc(size_t aNeeded) const {
    return mImpl.canAppendWithoutRealloc(aNeeded);
  }
  void infallibleAppend(Frame aEntry) { mImpl.infallibleAppend(aEntry); }
  MOZ_MUST_USE bool reserve(size_t aRequest) { return mImpl.reserve(aRequest); }
  Frame* begin() { return mImpl.begin(); }
  Frame const* begin() const { return mImpl.begin(); }
  Frame* end() { return mImpl.end(); }
  Frame const* end() const { return mImpl.end(); }
  Frame& back() { return mImpl.back(); }
  void erase(Frame* aEntry) { mImpl.erase(aEntry); }
  void erase(Frame* aBegin, Frame* aEnd) {
    mImpl.erase(aBegin, aEnd);
  }

  void clear() {
    mImpl.clear();
    mBuffer.clear();
    mModules.Clear();
  }

  bool IsInBuffer(const char* aEntry) const {
    return aEntry >= mBuffer.begin() && aEntry < mBuffer.end();
  }

  bool IsSameAsEntry(const Frame& aFrame, const Frame& aOther) const {
    if (aFrame.GetKind() != aOther.GetKind()) {
      return false;
    }

    switch (aFrame.GetKind()) {
      case Frame::Kind::STRING:
        // If the entry came from the buffer, we need to compare its content;
        // otherwise we only need to compare its pointer.
        return IsInBuffer(aFrame.AsString()) ?
          !strcmp(aFrame.AsString(), aOther.AsString()) :
          (aFrame.AsString() == aOther.AsString());
      case Frame::Kind::MODOFFSET:
        return aFrame.AsModOffset() == aOther.AsModOffset();
      case Frame::Kind::PC:
        return aFrame.AsPC() == aOther.AsPC();
      default:
        MOZ_CRASH();
    }
  }

  bool IsSameAsEntry(const char* aEntry, const char* aOther) const {
    // If the entry came from the buffer, we need to compare its content;
    // otherwise we only need to compare its pointer.
    return IsInBuffer(aEntry) ? !strcmp(aEntry, aOther) : (aEntry == aOther);
  }

  size_t AvailableBufferSize() const {
    return mBuffer.capacity() - mBuffer.length();
  }

  MOZ_MUST_USE bool EnsureBufferCapacity(size_t aCapacity) {
    // aCapacity is the minimal capacity and Vector may make the actual
    // capacity larger, in which case we want to use up all the space.
    return mBuffer.reserve(aCapacity) &&
           mBuffer.reserve(mBuffer.capacity());
  }

  void InfallibleAppendViaBuffer(const char* aText, size_t aLength);
  bool AppendViaBuffer(const char* aText, size_t aLength);

  const nsTArray<Module>& GetModules() const {
    return mModules;
  }
  nsTArray<Module>& GetModules() {
    return mModules;
  }

  /**
   * Get the current list of loaded modules, and use it to transform Kind::PC
   * stack frames from within these modules into Kind::MODOFFSET stack entries.
   *
   * This method also populates the mModules list, which should be empty when
   * this method is called.
   */
  void ReadModuleInformation();
};

} // namespace mozilla

namespace IPC {

template<>
class ParamTraits<mozilla::HangStack::ModOffset>
{
public:
  typedef mozilla::HangStack::ModOffset paramType;
  static void Write(Message* aMsg, const paramType& aParam);
  static bool Read(const Message* aMsg,
                   PickleIterator* aIter,
                   paramType* aResult);
};

template<>
struct ParamTraits<mozilla::HangStack::Frame::Kind>
  : public ContiguousEnumSerializer<
            mozilla::HangStack::Frame::Kind,
            mozilla::HangStack::Frame::Kind::STRING,
            mozilla::HangStack::Frame::Kind::END>
{};

template<>
struct ParamTraits<mozilla::HangStack::Module>
{
public:
  typedef mozilla::HangStack::Module paramType;
  static void Write(Message* aMsg, const paramType& aParam);
  static bool Read(const Message* aMsg,
                   PickleIterator* aIter,
                   paramType* aResult);
};

template<>
struct ParamTraits<mozilla::HangStack>
{
  typedef mozilla::HangStack paramType;
  static void Write(Message* aMsg, const paramType& aParam);
  static bool Read(const Message* aMsg,
                   PickleIterator* aIter,
                   paramType* aResult);
};

} // namespace IPC

#endif // mozilla_HangStack_h
