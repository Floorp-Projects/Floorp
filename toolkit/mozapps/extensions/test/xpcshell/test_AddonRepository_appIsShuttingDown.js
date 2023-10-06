/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests AddonRepository.jsm when backgroundUpdateChecks are hit while the application
// shutdown has been already initiated (See Bug 1841444).

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

add_setup(async () => {
  AddonTestUtils.createAppInfo(
    "xpcshell@tests.mozilla.org",
    "XPCShell",
    "1",
    "1"
  );
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_backgroundUpdateCheck_after_shutdown_initiated() {
  const sandbox = sinon.createSandbox();
  ok(
    !AddonRepository.appIsShuttingDown,
    "Expect real appIsShuttingDown getter to be returning false"
  );
  sandbox.stub(AddonRepository, "appIsShuttingDown").get(() => true);
  ok(
    AddonRepository.appIsShuttingDown,
    "Expect mocked appIsShuttingDown getter to be returning true"
  );
  sandbox.spy(AddonRepository, "_getAllInstalledAddons");
  equal(
    AddonRepository._getAllInstalledAddons.callCount,
    0,
    "Expect _getAllInstalledAddons callCount to be initially 0"
  );

  await AddonRepository.backgroundUpdateCheck();

  // We expect backgroundUpdateCheck to be returning earlier and not be calling _getAllInstalledAddons method at all.
  equal(
    AddonRepository._getAllInstalledAddons.callCount,
    0,
    "Expect _getAllInstalledAddons to not have been called"
  );
  sandbox.restore();
});

add_task(async function test_fetchPaged_after_shutdown_initiated() {
  const sandbox = sinon.createSandbox();
  ok(
    !AddonRepository.appIsShuttingDown,
    "Expect real appIsShuttingDown getter to be returning false"
  );
  sandbox.stub(AddonRepository, "appIsShuttingDown").get(() => true);
  ok(
    AddonRepository.appIsShuttingDown,
    "Expect mocked appIsShuttingDown getter to be returning true"
  );
  sandbox.spy(AddonRepository, "_createServiceRequest");
  equal(
    AddonRepository._createServiceRequest.callCount,
    0,
    "Expect _createServiceRequest callCount to be initially 0"
  );

  await Assert.rejects(
    AddonRepository.getAddonsByIDs(["ext01@testext", "ext02@testext"]),
    /Reject ServiceRequest for ".*", shutdown already in progress/,
    "Expect getAddonsByIds to reject when called after shutdown was already initiated"
  );

  // We expect backgroundUpdateCheck to be returning earlier and not be calling _getAllInstalledAddons method at all.
  equal(
    AddonRepository._createServiceRequest.callCount,
    0,
    "Expect _createServiceRequest to not have been called"
  );
  sandbox.restore();
});
