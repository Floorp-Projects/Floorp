/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  createAppInfo,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

SearchTestUtils.initXPCShellAddonManager(this);

function extInfo(id, name, version, keyword) {
  return {
    useAddonManager: "permanent",
    manifest: {
      version,
      applications: {
        gecko: { id: `${id}@tests.mozilla.org` },
      },
      chrome_settings_overrides: {
        search_provider: {
          name,
          keyword,
          search_url: `https://example.com/?q={searchTerms}&version=${version}`,
        },
      },
    },
  };
}

add_task(async function setup() {
  await promiseStartupManager();
  registerCleanupFunction(promiseShutdownManager);
});

add_task(async function basic_install_test() {
  await Services.search.init();
  await promiseAfterCache();

  let info = extInfo("example", "Example", "1.0", "foo");
  let extension = ExtensionTestUtils.loadExtension(info);
  await extension.startup();
  await AddonTestUtils.waitForSearchProviderStartup(extension);

  let engine = Services.search.getEngineByAlias("foo");
  Assert.ok(engine, "Can fetch engine with alias");
  engine.alias = "testing";

  engine = Services.search.getEngineByAlias("testing");
  Assert.ok(engine, "Can fetch engine by alias");
  let params = engine.getSubmission("test").uri.query.split("&");
  Assert.ok(params.includes("version=1.0"), "Correct version installed");

  let promiseChanged = TestUtils.topicObserved(
    "browser-search-engine-modified",
    (eng, verb) => verb == "engine-changed"
  );

  await extension.upgrade(extInfo("example", "Example", "2.0", "bar"));
  await AddonTestUtils.waitForSearchProviderStartup(extension);
  await promiseChanged;

  engine = Services.search.getEngineByAlias("testing");
  Assert.ok(engine, "Engine still has alias set");

  params = engine.getSubmission("test").uri.query.split("&");
  Assert.ok(params.includes("version=2.0"), "Correct version installed");

  await extension.unload();
  await promiseAfterCache();
});
