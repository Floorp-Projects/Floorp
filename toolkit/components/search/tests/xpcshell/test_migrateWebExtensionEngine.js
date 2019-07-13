/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExtensionTestUtils } = ChromeUtils.import(
  "resource://testing-common/ExtensionXPCShellUtils.jsm"
);
ExtensionTestUtils.init(this);

AddonTestUtils.usePrivilegedSignatures = false;
AddonTestUtils.overrideCertDB();

const kSearchEngineID = "addEngineWithDetails_test_engine";
const kExtensionID = "test@example.com";

const kSearchEngineDetails = {
  template: "http://example.com/?search={searchTerms}",
  description: "Test Description",
  iconURL:
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg==",
  suggestURL: "http://example.com/?suggest={searchTerms}",
  alias: "alias_foo",
};

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

add_task(async function test_migrateLegacyEngine() {
  await Services.search.addEngineWithDetails(
    kSearchEngineID,
    kSearchEngineDetails
  );

  // Modify the loadpath so it looks like an legacy plugin loadpath
  let engine = Services.search.getEngineByName(kSearchEngineID);
  Assert.ok(!!engine, "opensearch engine installed");
  engine.wrappedJSObject._loadPath = `jar:[profile]/extensions/${kExtensionID}.xpi!/engine.xml`;
  await Services.search.setDefault(engine);
  Assert.equal(
    engine.name,
    Services.search.defaultEngine.name,
    "set engine to default"
  );

  // We assume the default engines are installed, so our position will be after the default engine.
  // This sets up the test to later test the engine position after updates.
  let allEngines = await Services.search.getEngines();
  Assert.ok(
    allEngines.length > 2,
    "default engines available " + allEngines.length
  );
  let origIndex = allEngines.map(e => e.name).indexOf(kSearchEngineID);
  Assert.ok(
    origIndex > 1,
    "opensearch engine installed at position " + origIndex
  );
  await Services.search.moveEngine(engine, origIndex - 1);
  let index = (await Services.search.getEngines())
    .map(e => e.name)
    .indexOf(kSearchEngineID);
  Assert.equal(
    origIndex - 1,
    index,
    "opensearch engine moved to position " + index
  );

  // Replace the opensearch extension with a webextension
  let extensionInfo = {
    useAddonManager: "permanent",
    manifest: {
      version: "1.0",
      applications: {
        gecko: {
          id: kExtensionID,
        },
      },
      chrome_settings_overrides: {
        search_provider: {
          name: kSearchEngineID,
          search_url: "https://example.com/?q={searchTerms}",
        },
      },
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionInfo);
  await extension.startup();

  engine = Services.search.getEngineByName(kSearchEngineID);
  Assert.equal(
    engine.wrappedJSObject._loadPath,
    "[other]addEngineWithDetails:" + kExtensionID
  );
  Assert.equal(engine.wrappedJSObject._extensionID, kExtensionID);
  Assert.equal(engine.wrappedJSObject._version, "1.0");
  index = (await Services.search.getEngines())
    .map(e => e.name)
    .indexOf(kSearchEngineID);
  Assert.equal(origIndex - 1, index, "webext position " + index);
  Assert.equal(
    engine.name,
    Services.search.defaultEngine.name,
    "engine stil default"
  );

  extensionInfo.manifest.version = "2.0";
  await extension.upgrade(extensionInfo);
  await AddonTestUtils.waitForSearchProviderStartup(extension);

  engine = Services.search.getEngineByName(kSearchEngineID);
  Assert.equal(
    engine.wrappedJSObject._loadPath,
    "[other]addEngineWithDetails:" + kExtensionID
  );
  Assert.equal(engine.wrappedJSObject._extensionID, kExtensionID);
  Assert.equal(engine.wrappedJSObject._version, "2.0");
  index = (await Services.search.getEngines())
    .map(e => e.name)
    .indexOf(kSearchEngineID);
  Assert.equal(origIndex - 1, index, "webext position " + index);
  Assert.equal(
    engine.name,
    Services.search.defaultEngine.name,
    "engine stil default"
  );

  // A different extension cannot use the same name
  extensionInfo.manifest.applications.gecko.id = "takeover@search.foo";
  let otherExt = ExtensionTestUtils.loadExtension(extensionInfo);
  await otherExt.startup();
  // Verify correct owner
  engine = Services.search.getEngineByName(kSearchEngineID);
  Assert.equal(
    engine.wrappedJSObject._extensionID,
    kExtensionID,
    "prior search engine could not be overwritten"
  );
  // Verify no engine installed
  let engines = await Services.search.getEnginesByExtensionID(
    "takeover@search.foo"
  );
  Assert.equal(engines.length, 0, "no search engines installed");
  await otherExt.unload();

  // An opensearch engine cannot replace a webextension.
  try {
    await Services.search.addEngineWithDetails(
      kSearchEngineID,
      kSearchEngineDetails
    );
    Assert.ok(false, "unable to install opensearch over webextension");
  } catch (e) {
    Assert.ok(true, "unable to install opensearch over webextension");
  }

  await extension.unload();
});
