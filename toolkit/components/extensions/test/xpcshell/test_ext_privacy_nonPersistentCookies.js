"use strict";

const { promiseShutdownManager, promiseStartupManager } = AddonTestUtils;

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

add_task(async function test_nonPersistentCookies_is_deprecated() {
  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["privacy"],
    },
    useAddonManager: "temporary",
    async background() {
      for (const nonPersistentCookies of [true, false]) {
        await browser.privacy.websites.cookieConfig.set({
          value: {
            behavior: "reject_third_party",
            nonPersistentCookies,
          },
        });
      }

      browser.test.sendMessage("background-done");
    },
  });

  let { messages } = await promiseConsoleOutput(async () => {
    await extension.startup();
    await extension.awaitMessage("background-done");
    await extension.unload();
  });

  const expectedMessage =
    /"'nonPersistentCookies' has been deprecated and it has no effect anymore."/;

  AddonTestUtils.checkMessages(
    messages,
    {
      expected: [{ message: expectedMessage }, { message: expectedMessage }],
    },
    true
  );

  await promiseShutdownManager();
});
