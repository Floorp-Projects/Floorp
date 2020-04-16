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
  printf("Comparing '%s' to '%s'.\n", aOldCompatVersion.get(),
         aNewCompatVersion.get());

  int32_t result = CompareCompatVersions(aOldCompatVersion, aNewCompatVersion);

  ASSERT_EQ(aExpectedSame, result == 0)
      << "Version sameness check should match.";
  ASSERT_EQ(aExpectedDowngrade, result > 0)
      << "Version downgrade check should match.";
}

void CheckExpectedResult(const char* aOldAppVersion, const char* aNewAppVersion,
                         bool aExpectedSame, bool aExpectedDowngrade) {
  nsCString oldCompatVersion;
  BuildCompatVersion(aOldAppVersion, "", "", oldCompatVersion);

  nsCString newCompatVersion;
  BuildCompatVersion(aNewAppVersion, "", "", newCompatVersion);

  CheckCompatVersionCompare(oldCompatVersion, newCompatVersion, aExpectedSame,
                            aExpectedDowngrade);
}

TEST(CompatVersionCompare, CompareVersionChange)
{
  // Identical
  CheckExpectedResult("67.0", "67.0", true, false);

  // Version changes
  CheckExpectedResult("67.0", "68.0", false, false);
  CheckExpectedResult("68.0", "67.0", false, true);
  CheckExpectedResult("67.0", "67.0.1", true, false);
  CheckExpectedResult("67.0.1", "67.0", true, false);
  CheckExpectedResult("67.0.1", "67.0.1", true, false);
  CheckExpectedResult("67.0.1", "67.0.2", true, false);
  CheckExpectedResult("67.0.2", "67.0.1", true, false);

  // Check that if the last run was safe mode then we consider this an upgrade.
  CheckCompatVersionCompare(
      NS_LITERAL_CSTRING("Safe Mode"),
      NS_LITERAL_CSTRING("67.0.1_20000000000000/20000000000000"), false, false);
}
