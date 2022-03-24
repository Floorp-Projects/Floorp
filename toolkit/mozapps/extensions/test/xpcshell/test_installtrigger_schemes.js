/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

createHttpServer({ hosts: ["example.com"] });

AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "1.9.2"
);

async function assertInstallTriggetRejected(page, xpi_url, expectedError) {
  await Assert.rejects(
    page.spawn([xpi_url], async url => {
      this.content.eval(`InstallTrigger.install({extension: '${url}'});`);
    }),
    expectedError,
    `InstallTrigger.install expected to throw on xpi url "${xpi_url}"`
  );
}

add_task(
  {
    // Once InstallTrigger is removed, this test should be removed as well.
    pref_set: [
      ["extensions.InstallTrigger.enabled", true],
      ["extensions.InstallTriggerImpl.enabled", true],
      // Relax the user input requirements while running this test.
      ["xpinstall.userActivation.required", false],
    ],
  },
  async function test_InstallTriggerThrows_on_unsupported_xpi_schemes_blob() {
    const page = await ExtensionTestUtils.loadContentPage("http://example.com");
    const blob_url = await page.spawn([], () => {
      return this.content.eval(`(function () {
      const blob = new Blob(['fakexpicontent']);
      return URL.createObjectURL(blob);
    })()`);
    });
    await assertInstallTriggetRejected(page, blob_url, /Unsupported scheme/);
    await page.close();
  }
);

add_task(
  {
    // Once InstallTrigger is removed, this test should be removed as well.
    pref_set: [
      ["extensions.InstallTrigger.enabled", true],
      ["extensions.InstallTriggerImpl.enabled", true],
      // Relax the user input requirements while running this test.
      ["xpinstall.userActivation.required", false],
    ],
  },
  async function test_InstallTriggerThrows_on_unsupported_xpi_schemes_data() {
    const page = await ExtensionTestUtils.loadContentPage("http://example.com");
    const data_url = "data:;,fakexpicontent";
    // This is actually rejected by the checkLoadURIWithPrincipal, which fails with
    // NS_ERROR_DOM_BAD_URI triggered by CheckLoadURIWithPrincipal's call to
    //
    //   DenyAccessIfURIHasFlags(aTargetURI, nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT)
    //
    // and so it is not a site permission that the user can actually grant, unlike the error
    // raised may suggest.
    await assertInstallTriggetRejected(
      page,
      data_url,
      /Insufficient permissions to install/
    );
    await page.close();
  }
);
