"use strict";

ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://normandy/lib/Addons.jsm", this);

// Initialize test utils
AddonTestUtils.initMochitest(this);

const testInstallId = "testInstallUpdate@example.com";
decorate_task(
  withInstalledWebExtension({ version: "1.0", id: testInstallId }),
  withWebExtension({ version: "2.0", id: testInstallId }),
  async function testInstallUpdate([id1, addonFile1], [id2, addonFile2]) {
    // Fail to install the 2.0 add-on without updating enabled
    const newAddonUrl = Services.io.newFileURI(addonFile2).spec;
    await Assert.rejects(
      Addons.install(newAddonUrl, { update: false }),
      /updating is disabled/,
      "install rejects when the study add-on is already installed and updating is disabled"
    );

    // Install the new add-on with updating enabled
    const startupPromise = AddonTestUtils.promiseWebExtensionStartup(
      testInstallId
    );
    await Addons.install(newAddonUrl, { update: true });

    const addon = await startupPromise;
    is(
      addon.version,
      "2.0",
      "install can successfully update an already-installed addon when updating is enabled."
    );
  }
);
