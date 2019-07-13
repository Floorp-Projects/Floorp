/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { SearchExtensionLoader } = ChromeUtils.import(
  "resource://gre/modules/SearchUtils.jsm"
);
const { ExtensionTestUtils } = ChromeUtils.import(
  "resource://testing-common/ExtensionXPCShellUtils.jsm"
);

ExtensionTestUtils.init(this);
AddonTestUtils.usePrivilegedSignatures = false;
AddonTestUtils.overrideCertDB();

add_task(async function test_install_manifest_failure() {
  // Force addon loading to reject on errors
  SearchExtensionLoader._strict = true;
  useTestEngines("invalid-extension");
  await AddonTestUtils.promiseStartupManager();

  await Services.search
    .init()
    .then(() => {
      ok(false, "search init did not throw");
    })
    .catch(e => {
      equal(Cr.NS_ERROR_FAILURE, e, "search init error");
    });
});
