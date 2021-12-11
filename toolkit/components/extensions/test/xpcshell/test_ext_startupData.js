"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "1"
);

// Tests that startupData is persisted and is available at startup
add_task(async function test_startupData() {
  await AddonTestUtils.promiseStartupManager();

  let wrapper = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
  });
  await wrapper.startup();

  let { extension } = wrapper;

  deepEqual(
    extension.startupData,
    {},
    "startupData for a new extension defaults to empty object"
  );

  const DATA = { test: "i am some startup data" };
  extension.startupData = DATA;
  extension.saveStartupData();

  await AddonTestUtils.promiseRestartManager();
  await wrapper.startupPromise;

  ({ extension } = wrapper);
  deepEqual(extension.startupData, DATA, "startupData is present on restart");

  const DATA2 = { other: "this is different data" };
  extension.startupData = DATA2;
  extension.saveStartupData();

  await AddonTestUtils.promiseRestartManager();
  await wrapper.startupPromise;

  ({ extension } = wrapper);
  deepEqual(
    extension.startupData,
    DATA2,
    "updated startupData is present on restart"
  );

  await wrapper.unload();
  await AddonTestUtils.promiseShutdownManager();
});
