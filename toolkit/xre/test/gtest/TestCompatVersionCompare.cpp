/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "nsAppRunner.h"
#include "nsString.h"

void CheckCompatVersionCompare(const nsCString& aOldCompatVersion,
                               const nsCString& aNewCompatVersion,
                               bool aExpectedSame, bool aExpectedDowngrade) {
  printf("Comparing '%s' to '%s'.\n", aOldCompatVersion.get(), aNewCompatVersion.get());

  int32_t result = CompareCompatVersions(aOldCompatVersion, aNewCompatVersion);

  ASSERT_EQ(aExpectedSame, result == 0) << "Version sameness check should match.";
  ASSERT_EQ(aExpectedDowngrade, result > 0) << "Version downgrade check should match.";
}

void CheckExpectedResult(
  const char* aOldAppVersion, const char* aOldAppID, const char* aOldToolkitID,
  const char* aNewAppVersion, const char* aNewAppID, const char* aNewToolkitID,
  bool aExpectedSame, bool aExpectedDowngrade) {

  nsCString oldCompatVersion;
  BuildCompatVersion(aOldAppVersion, aOldAppID, aOldToolkitID, oldCompatVersion);

  nsCString newCompatVersion;
  BuildCompatVersion(aNewAppVersion, aNewAppID, aNewToolkitID, newCompatVersion);

  CheckCompatVersionCompare(oldCompatVersion, newCompatVersion,
                            aExpectedSame, aExpectedDowngrade);
}

// clang-format off
TEST(CompatVersionCompare, CompareVersionChange) {
  // Identical
  CheckExpectedResult(
    "67.0",   "20000000000000", "20000000000000",
    "67.0",   "20000000000000", "20000000000000",
    true, false);

  // Build ID changes
  CheckExpectedResult(
    "67.0",   "20000000000000", "20000000000001",
    "67.0",   "20000000000000", "20000000000000",
    false, true);
  CheckExpectedResult(
    "67.0",   "20000000000001", "20000000000000",
    "67.0",   "20000000000000", "20000000000000",
    false, true);
  CheckExpectedResult(
    "67.0",   "20000000000000", "20000000000000",
    "67.0",   "20000000000000", "20000000000001",
    false, false);
  CheckExpectedResult(
    "67.0",   "20000000000000", "20000000000000",
    "67.0",   "20000000000001", "20000000000000",
    false, false);

  // Version changes
  CheckExpectedResult(
    "67.0",   "20000000000000", "20000000000000",
    "68.0",   "20000000000000", "20000000000000",
    false, false);
  CheckExpectedResult(
    "68.0",   "20000000000000", "20000000000000",
    "67.0",   "20000000000000", "20000000000000",
    false, true);
  CheckExpectedResult(
    "67.0",   "20000000000000", "20000000000000",
    "67.0.1", "20000000000000", "20000000000000",
    false, false);
  CheckExpectedResult(
    "67.0.1", "20000000000000", "20000000000000",
    "67.0",   "20000000000000", "20000000000000",
    false, true);
  CheckExpectedResult(
    "67.0.1", "20000000000000", "20000000000000",
    "67.0.1", "20000000000000", "20000000000000",
    true, false);
  CheckExpectedResult(
    "67.0.1", "20000000000000", "20000000000000",
    "67.0.2", "20000000000000", "20000000000000",
    false, false);
  CheckExpectedResult(
    "67.0.2", "20000000000000", "20000000000000",
    "67.0.1", "20000000000000", "20000000000000",
    false, true);

  // Unexpected ID formats
  CheckExpectedResult(
    "67.0.1", "build1", "build1",
    "67.0.1", "build2", "build2",
    false, false);
  CheckExpectedResult(
    "67.0.1", "build10", "build10",
    "67.0.1", "build2", "build2",
    false, true);
  CheckExpectedResult(
    "67.0.1", "1", "1",
    "67.0.1", "10", "10",
    false, false);
  CheckExpectedResult(
    "67.0.1", "10", "10",
    "67.0.1", "1", "1",
    false, true);

  // These support an upgrade case from a normal-style build ID to the one
  // we're suggesting Ubuntu use.
  CheckExpectedResult(
    "67.0.1", "20000000000000", "20000000000000",
    "67.0.1", "1build1", "1build1",
    false, false);
  CheckExpectedResult(
    "67.0.1", "1build1", "1build1",
    "67.0.1", "20000000000000", "20000000000000",
    false, true);

  // The actual case from bug 1554029:
  CheckExpectedResult(
    "67.0",   "20190516215225", "20190516215225",
    "67.0.5", "20190523030228","20190523030228",
    false, false);
  CheckExpectedResult(
    "67.0.5", "20190523030228", "20190523030228",
    "67.0",   "20190516215225", "20190516215225",
    false, true);

  // A newer or equal version should not go on to test the build IDs (bug 1556612).
  CheckExpectedResult(
    "65.0",   "30000000000000", "20000000000000",
    "66.0",   "20000000000000", "20000000000000",
    false, false);
  CheckExpectedResult(
    "65.0",   "20000000000000", "30000000000000",
    "66.0",   "20000000000000", "20000000000000",
    false, false);
  CheckExpectedResult(
    "66.0",   "30000000000000", "20000000000000",
    "65.0",   "20000000000000", "20000000000000",
    false, true);
  CheckExpectedResult(
    "66.0",   "20000000000000", "30000000000000",
    "65.0",   "20000000000000", "20000000000000",
    false, true);

  // Check that if the last run was safe mode then we consider this an upgrade.
  CheckCompatVersionCompare(
    NS_LITERAL_CSTRING("Safe Mode"),
    NS_LITERAL_CSTRING("67.0.1_20000000000000/20000000000000"),
    false, false);
}
// clang-format on
