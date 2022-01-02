/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "TelemetryFixture.h"
#include "TelemetryTestHelpers.h"
#include "other/CombinedStacks.h"
#include "other/ProcessedStack.h"
#include "nsPrintfCString.h"

using namespace mozilla::Telemetry;
using namespace TelemetryTestHelpers;

TEST_F(TelemetryTestFixture, CombinedStacks) {
  const size_t kMaxStacksKept = 10;
  CombinedStacks stacks(kMaxStacksKept);

  size_t iterations = kMaxStacksKept * 2;
  for (size_t i = 0; i < iterations; ++i) {
    ProcessedStack stack;
    ProcessedStack::Frame frame = {static_cast<uint16_t>(i)};
    const nsAutoString& name =
        NS_ConvertUTF8toUTF16(nsPrintfCString("test%zu", i));
    ProcessedStack::Module module = {name};

    stack.AddFrame(frame);
    stack.AddModule(module);
    stacks.AddStack(stack);
  }

  ASSERT_EQ(stacks.GetStackCount(), kMaxStacksKept) << "Wrong number of stacks";
  ASSERT_EQ(stacks.GetModuleCount(), kMaxStacksKept * 2)
      << "Wrong number of modules";

  for (size_t i = 0; i < kMaxStacksKept; ++i) {
    ProcessedStack::Frame frame = stacks.GetStack(i)[0];
    ASSERT_EQ(frame.mOffset, kMaxStacksKept + i)
        << "Frame is not returning expected value";

    ProcessedStack::Module module = stacks.GetModule(frame.mModIndex);
    nsPrintfCString moduleName("test%hu", frame.mModIndex);
    ASSERT_TRUE(module.mName.Equals(NS_ConvertUTF8toUTF16(moduleName)))
    << "Module should have expected name";
  }

  for (size_t i = 0; i < kMaxStacksKept; ++i) {
    stacks.RemoveStack(kMaxStacksKept - i - 1);
    ASSERT_EQ(stacks.GetStackCount(), kMaxStacksKept - i - 1)
        << "Stack should be removed";
  }
}

template <int N>
ProcessedStack MakeStack(const nsLiteralString (&aModules)[N],
                         const uintptr_t (&aOffsets)[N]) {
  ProcessedStack stack;
  for (int i = 0; i < N; ++i) {
    ProcessedStack::Frame frame = {aOffsets[i]};
    if (aModules[i].IsEmpty()) {
      frame.mModIndex = std::numeric_limits<uint16_t>::max();
    } else {
      frame.mModIndex = stack.GetNumModules();
      stack.AddModule(ProcessedStack::Module{aModules[i]});
    }
    stack.AddFrame(frame);
  }
  return stack;
}

TEST(CombinedStacks, Combine)
{
  const nsLiteralString moduleSet1[] = {u"mod1"_ns, u"mod2"_ns, u"base"_ns};
  const nsLiteralString moduleSet2[] = {u"modX"_ns, u""_ns, u"modZ"_ns,
                                        u"base"_ns};
  // [0] 00 mod1+100
  //     01 mod2+200
  //     02 base+300
  // [1] 00 mod1+1000
  //     01 mod2+2000
  //     02 base+3000
  // [2] 00 modX+100
  //     01 <no module>+200
  //     02 modZ+300
  //     03 base+400
  // [3] 00 modX+1000
  //     01 <no module>+3000
  //     02 modZ+2000
  //     03 base+4000
  const ProcessedStack testStacks[] = {
      MakeStack(moduleSet1, {100ul, 200ul, 300ul}),
      MakeStack(moduleSet1, {1000ul, 2000ul, 3000ul}),
      MakeStack(moduleSet2, {100ul, 200ul, 300ul, 400ul}),
      MakeStack(moduleSet2, {1000ul, 2000ul, 3000ul, 4000ul}),
  };

  // combined1 <-- testStacks[0] + testStacks[1]
  // combined2 <-- testStacks[2] + testStacks[3]
  CombinedStacks combined1, combined2;
  combined1.AddStack(testStacks[0]);
  combined1.AddStack(testStacks[1]);
  combined2.AddStack(testStacks[2]);
  combined2.AddStack(testStacks[3]);

  EXPECT_EQ(combined1.GetModuleCount(), mozilla::ArrayLength(moduleSet1));
  EXPECT_EQ(combined1.GetStackCount(), 2u);
  EXPECT_EQ(combined2.GetModuleCount(), mozilla::ArrayLength(moduleSet2) - 1);
  EXPECT_EQ(combined2.GetStackCount(), 2u);

  // combined1 <-- combined1 + combined2
  combined1.AddStacks(combined2);

  EXPECT_EQ(combined1.GetModuleCount(), 5u);  // {mod1, mod2, modX, modZ, base}
  EXPECT_EQ(combined1.GetStackCount(), mozilla::ArrayLength(testStacks));

  for (size_t i = 0; i < combined1.GetStackCount(); ++i) {
    const auto& expectedStack = testStacks[i];
    const auto& actualStack = combined1.GetStack(i);
    EXPECT_EQ(actualStack.size(), expectedStack.GetStackSize());
    if (actualStack.size() != expectedStack.GetStackSize()) {
      continue;
    }

    for (size_t j = 0; j < actualStack.size(); ++j) {
      const auto& expectedFrame = expectedStack.GetFrame(j);
      const auto& actualFrame = actualStack[j];

      EXPECT_EQ(actualFrame.mOffset, expectedFrame.mOffset);

      if (expectedFrame.mModIndex == std::numeric_limits<uint16_t>::max()) {
        EXPECT_EQ(actualFrame.mModIndex, std::numeric_limits<uint16_t>::max());
      } else {
        EXPECT_EQ(combined1.GetModule(actualFrame.mModIndex),
                  expectedStack.GetModule(expectedFrame.mModIndex));
      }
    }
  }

  // Only testStacks[3] will be stored into oneStack
  CombinedStacks oneStack(1);
  oneStack.AddStacks(combined1);

  EXPECT_EQ(oneStack.GetStackCount(), 1u);
  EXPECT_EQ(oneStack.GetStack(0).size(), testStacks[3].GetStackSize());

  for (size_t i = 0; i < oneStack.GetStack(0).size(); ++i) {
    const auto& expectedFrame = testStacks[3].GetFrame(i);
    const auto& actualFrame = oneStack.GetStack(0)[i];

    EXPECT_EQ(actualFrame.mOffset, expectedFrame.mOffset);

    if (expectedFrame.mModIndex == std::numeric_limits<uint16_t>::max()) {
      EXPECT_EQ(actualFrame.mModIndex, std::numeric_limits<uint16_t>::max());
    } else {
      EXPECT_EQ(oneStack.GetModule(actualFrame.mModIndex),
                testStacks[3].GetModule(expectedFrame.mModIndex));
    }
  }
}
