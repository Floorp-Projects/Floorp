/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

SearchTestUtils.initXPCShellAddonManager(this);

const NAME = "Test Alias Engine";

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

add_task(async function upgrade_with_configuration_change_test() {
  let cacheFileWritten = promiseAfterCache();
  let extension = await SearchTestUtils.installSearchExtension({
    name: NAME,
    keyword: [" test", "alias "],
  });
  await extension.awaitStartup();
  await cacheFileWritten;

  let engine = Services.search.getEngineByAlias("test");
  Assert.equal(engine?.name, NAME, "Can be fetched by either alias");
  engine = Services.search.getEngineByAlias("alias");
  Assert.equal(engine?.name, NAME, "Can be fetched by either alias");

  await extension.unload();
});
