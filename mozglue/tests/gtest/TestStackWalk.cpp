/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The header under test.
#include "mozilla/StackWalk.h"

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

#include "gtest/gtest.h"

MOZ_EXPORT bool gStackWalkTesterDummy = true;

struct StackWalkTester;

// Descriptor of the recursive function calls wanted, and for each of them
// whether to perform tail call optimization or not.
struct CallInfo {
  int (*mFunc)(int aDepth, int aLastSkipped, int aIgnored,
               StackWalkTester& aTester);
  bool mTailCall;

  bool TailCall() {
#if defined(__i386__) || defined(MOZ_CODE_COVERAGE)
    // We can't make tail calls happen on i386 because all arguments to
    // functions are on the stack, so the stack pointer needs to be updated
    // before the call and restored after the call, so tail call optimization
    // never happens.
    // Similarly, code-coverage flags don't guarantee that tail call
    // optimization will happen.
    return false;
#else
    return mTailCall;
#endif
  }
};

struct PCRange {
  void* mStart;
  void* mEnd;
};

// PCRange pretty printer for gtest assertions.
std::ostream& operator<<(std::ostream& aStream, const PCRange& aRange) {
  aStream << aRange.mStart;
  aStream << "-";
  aStream << aRange.mEnd;
  return aStream;
}

// Allow to use EXPECT_EQ with a vector of PCRanges and a vector of plain
// addresses, allowing a more useful output when the test fails, showing
// both lists.
bool operator==(const std::vector<PCRange>& aRanges,
                const std::vector<void*>& aPtrs) {
  if (aRanges.size() != aPtrs.size()) {
    return false;
  }
  for (size_t i = 0; i < aRanges.size(); i++) {
    auto range = aRanges[i];
    auto ptr = reinterpret_cast<uintptr_t>(aPtrs[i]);
    if (ptr <= reinterpret_cast<uintptr_t>(range.mStart) ||
        ptr >= reinterpret_cast<uintptr_t>(range.mEnd)) {
      return false;
    }
  }
  return true;
}

struct StackWalkTester {
  // Description of the recursion of functions to perform for the testcase.
  std::vector<CallInfo> mFuncCalls;
  // Collection of PCs reported by MozStackWalk.
  std::vector<void*> mFramePCs;
  // Collection of PCs expected per what was observed while recursing.
  std::vector<PCRange> mExpectedFramePCs;
  // The aFirstFramePC value that will be passed to MozStackWalk.
  void* mFirstFramePC = nullptr;

  // Callback to be given to the stack walker.
  // aClosure should point at an instance of this class.
  static void StackWalkCallback(uint32_t aFrameNumber, void* aPC, void* aSP,
                                void* aClosure) {
    ASSERT_NE(aClosure, nullptr);
    StackWalkTester& tester = *reinterpret_cast<StackWalkTester*>(aClosure);
    tester.mFramePCs.push_back(aPC);
    EXPECT_EQ(tester.mFramePCs.size(), size_t(aFrameNumber))
        << "Frame number doesn't match";
  }

  // Callers of this function get a range of addresses with:
  // ```
  // label:
  //   recursion();
  //   AddExpectedPC(&&label);
  // ```
  // This intends to record the range from label to the return of AddExpectedPC.
  // The ideal code would be:
  // ```
  //   recursion();
  // label:
  //   AddExpectedPC(&&label);
  // ```
  // and we wouldn't need to keep ranges. But while this works fine with Clang,
  // GCC actually sometimes reorders code such the address received by
  // AddExpectedPC is the address *before* the recursion.
  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=99784 Using a label before the
  // recursion and CallerPC() from a function call after the recursion makes it
  // less likely for things to go wrong.
  MOZ_NEVER_INLINE void AddExpectedPC(void* aPC) {
    mExpectedFramePCs.push_back({aPC, CallerPC()});
  }

  // Function intended to be called in sequence for recursion.
  // CallInfo lists are meant to contain a sequence of IntermediateCallback<1>,
  // IntermediateCallback<2>, etc.
  // aDepth is a counter of how deep the recursion has gone so far;
  // aLastSkipped is the depth of the last frame we want skipped in the
  // testcase; aIgnored is there to avoid the compiler merging both recursive
  // function calls, which would prevent tail call optimization happening on one
  // of them. aTester is the instance of this class for the testcase.
  template <int Id>
  MOZ_NEVER_INLINE MOZ_EXPORT static int IntermediateCallback(
      int aDepth, int aLastSkipped, int aIgnored, StackWalkTester& aTester) {
    auto& callInfo = aTester.mFuncCalls.at(aDepth + 1);
    if (aDepth == aLastSkipped) {
      aTester.mFirstFramePC = CallerPC();
    }
    if (aTester.mFuncCalls.at(aDepth).TailCall()) {
      return callInfo.mFunc(aDepth + 1, aLastSkipped, Id, aTester);
      // Since we're doing a tail call, we're not expecting this frame appearing
      // in the trace.
    }
  here:
    callInfo.mFunc(aDepth + 1, aLastSkipped, Id + 1, aTester);
    aTester.AddExpectedPC(&&here);
    return 0;
  }

  MOZ_NEVER_INLINE MOZ_EXPORT static void LeafCallback(
      int aDepth, int aLastSkipped, int aIgnored, StackWalkTester& aTester) {
    if (aDepth == aLastSkipped) {
      aTester.mFirstFramePC = CallerPC();
    }
    if (aTester.mFuncCalls.at(aDepth).TailCall()) {
      // For the same reason that we have the aIgnored argument on these
      // callbacks, we need to avoid both MozStackWalk calls to be merged by the
      // compiler, so we use different values of aMaxFrames for that.
      return MozStackWalk(StackWalkTester::StackWalkCallback,
                          aTester.mFirstFramePC,
                          /*aMaxFrames*/ 19, &aTester);
      // Since we're doing a tail call, we're not expecting this frame appearing
      // in the trace.
    }
  here:
    MozStackWalk(StackWalkTester::StackWalkCallback, aTester.mFirstFramePC,
                 /*aMaxFrames*/ 20, &aTester);
    aTester.AddExpectedPC(&&here);
    // Because we return nothing from this function, simply returning here would
    // produce a tail-call optimization, which we explicitly don't want to
    // happen. So we add a branch that depends on an extern value to prevent
    // that from happening.
    MOZ_RELEASE_ASSERT(gStackWalkTesterDummy);
  }

  explicit StackWalkTester(std::initializer_list<CallInfo> aFuncCalls)
      : mFuncCalls(aFuncCalls) {}

  // Dump a vector of PCRanges as WalkTheStack would, for test failure output.
  // Only the end of the range is shown. Not ideal, but
  // MozFormatCodeAddressDetails only knows to deal with one address at a time.
  // The full ranges would be printed by EXPECT_EQ anyways.
  static std::string DumpFrames(std::vector<PCRange>& aFramePCRanges) {
    std::vector<void*> framePCs;
    framePCs.reserve(aFramePCRanges.size());
    for (auto range : aFramePCRanges) {
      framePCs.push_back(range.mEnd);
    }
    return DumpFrames(framePCs);
  }

  // Dump a vector of addresses as WalkTheStack would, for test failure output.
  static std::string DumpFrames(std::vector<void*>& aFramePCs) {
    size_t n = 0;
    std::string result;
    for (auto* framePC : aFramePCs) {
      char buf[1024];
      MozCodeAddressDetails details;
      result.append("  ");
      n++;
      if (MozDescribeCodeAddress(framePC, &details)) {
        int length =
            MozFormatCodeAddressDetails(buf, sizeof(buf), n, framePC, &details);
        result.append(buf, std::min(length, (int)sizeof(buf) - 1));
      } else {
        result.append("MozDescribeCodeAddress failed");
      }
      result.append("\n");
    }
    return result;
  }

  // Dump a description of the given test case.
  static std::string DumpFuncCalls(std::vector<CallInfo>& aFuncCalls) {
    std::string result;
    for (auto funcCall : aFuncCalls) {
      MozCodeAddressDetails details;
      result.append("  ");
      if (MozDescribeCodeAddress(reinterpret_cast<void*>(funcCall.mFunc),
                                 &details)) {
        result.append(details.function);
        if (funcCall.TailCall()) {
          result.append(" tail call");
        }
      } else {
        result.append("MozDescribeCodeAddress failed");
      }
      result.append("\n");
    }
    return result;
  }

  MOZ_EXPORT MOZ_NEVER_INLINE void RunTest(int aLastSkipped) {
    ASSERT_TRUE(aLastSkipped < (int)mFuncCalls.size());
    mFramePCs.clear();
    mExpectedFramePCs.clear();
    mFirstFramePC = nullptr;
    auto& callInfo = mFuncCalls.at(0);
  here:
    callInfo.mFunc(0, aLastSkipped, 0, *this);
    AddExpectedPC(&&here);
    if (aLastSkipped < 0) {
      aLastSkipped = mFuncCalls.size();
    }
    for (int i = (int)mFuncCalls.size() - 1; i >= aLastSkipped; i--) {
      if (!mFuncCalls.at(i).TailCall()) {
        mExpectedFramePCs.erase(mExpectedFramePCs.begin());
      }
    }
    mFramePCs.resize(std::min(mExpectedFramePCs.size(), mFramePCs.size()));
    EXPECT_EQ(mExpectedFramePCs, mFramePCs)
        << "Expected frames:\n"
        << DumpFrames(mExpectedFramePCs) << "Found frames:\n"
        << DumpFrames(mFramePCs)
        << "Function calls data (last skipped: " << aLastSkipped << "):\n"
        << DumpFuncCalls(mFuncCalls);
  }
};

TEST(TestStackWalk, StackWalk)
{
  const auto foo = StackWalkTester::IntermediateCallback<1>;
  const auto bar = StackWalkTester::IntermediateCallback<2>;
  const auto qux = StackWalkTester::IntermediateCallback<3>;
  const auto leaf = reinterpret_cast<int (*)(int, int, int, StackWalkTester&)>(
      StackWalkTester::LeafCallback);

  const std::initializer_list<CallInfo> tests[] = {
      {{foo, false}, {bar, false}, {qux, false}, {leaf, false}},
      {{foo, false}, {bar, true}, {qux, false}, {leaf, false}},
      {{foo, false}, {bar, false}, {qux, false}, {leaf, true}},
      {{foo, true}, {bar, false}, {qux, true}, {leaf, true}},
  };
  for (auto test : tests) {
    StackWalkTester tester(test);
    for (int i = -1; i < (int)test.size(); i++) {
      tester.RunTest(i);
    }
  }
}
