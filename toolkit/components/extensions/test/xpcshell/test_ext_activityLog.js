"use strict";

add_task(async function test_api_restricted() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: { id: "activityLog-permission@tests.mozilla.org" },
      },
      permissions: ["activityLog"],
    },
    async background() {
      browser.test.assertEq(
        undefined,
        browser.activityLog,
        "activityLog is privileged"
      );
    },
  });
  await extension.startup();
  await extension.unload();
});
