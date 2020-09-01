/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the search service manages to initialize correctly when the
 * distribution directory can't be accessed due to access denied.
 */

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
  await useTestEngines("simple-engines");
});

add_task(async function test_distribution_unreadable() {
  const distributionDir = installDistributionEngine();
  const oldPermissions = distributionDir.permissions;
  distributionDir.permissions = 0o000;

  registerCleanupFunction(() => {
    distributionDir.permissions = oldPermissions;
  });

  Assert.ok(!Services.search.isInitialized);

  await Services.search.init().then(aStatus => {
    Assert.ok(
      Components.isSuccessCode(aStatus),
      "Should have passed initialization"
    );
    Assert.ok(
      Services.search.isInitialized,
      "Should have flagged the search service as initialized"
    );
  });
});
