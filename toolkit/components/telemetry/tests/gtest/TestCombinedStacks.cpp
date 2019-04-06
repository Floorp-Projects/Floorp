/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

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
