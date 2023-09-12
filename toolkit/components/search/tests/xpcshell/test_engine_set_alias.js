"use strict";

add_setup(async function () {
  useHttpServer();
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

add_task(async function test_engine_set_alias() {
  info("Set engine alias");
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name: "bacon",
      keyword: "b",
      search_url: "https://www.bacon.test/find",
    },
    { skipUnload: true }
  );
  let engine1 = await Services.search.getEngineByName("bacon");
  Assert.ok(engine1.aliases.includes("b"));
  engine1.alias = "a";
  Assert.equal(engine1.alias, "a");
  await extension.unload();
});

add_task(async function test_engine_set_alias_with_left_space() {
  info("Set engine alias with left space");
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name: "bacon",
      keyword: "   a",
      search_url: "https://www.bacon.test/find",
    },
    { skipUnload: true }
  );
  let engine2 = await Services.search.getEngineByName("bacon");
  Assert.ok(engine2.aliases.includes("a"));
  engine2.alias = "    c";
  Assert.equal(engine2.alias, "c");
  await extension.unload();
});

add_task(async function test_engine_set_alias_with_right_space() {
  info("Set engine alias with right space");
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name: "bacon",
      keyword: "c   ",
      search_url: "https://www.bacon.test/find",
    },
    { skipUnload: true }
  );
  let engine3 = await Services.search.getEngineByName("bacon");
  Assert.ok(engine3.aliases.includes("c"));
  engine3.alias = "o    ";
  Assert.equal(engine3.alias, "o");
  await extension.unload();
});

add_task(async function test_engine_set_alias_with_right_left_space() {
  info("Set engine alias with left and right space");
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name: "bacon",
      keyword: " o  ",
      search_url: "https://www.bacon.test/find",
    },
    { skipUnload: true }
  );
  let engine4 = await Services.search.getEngineByName("bacon");
  Assert.ok(engine4.aliases.includes("o"));
  engine4.alias = "  n ";
  Assert.equal(engine4.alias, "n");
  await extension.unload();
});

add_task(async function test_engine_set_alias_with_space() {
  info("Set engine alias with space");
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name: "bacon",
      keyword: " ",
      search_url: "https://www.bacon.test/find",
    },
    { skipUnload: true }
  );
  let engine5 = await Services.search.getEngineByName("bacon");
  Assert.equal(engine5.alias, "");
  engine5.alias = "b";
  Assert.equal(engine5.alias, "b");
  engine5.alias = "  ";
  Assert.equal(engine5.alias, "");
  await extension.unload();
});

add_task(async function test_engine_change_alias() {
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name: "bacon",
      keyword: " o  ",
      search_url: "https://www.bacon.test/find",
    },
    { skipUnload: true }
  );
  let engine6 = await Services.search.getEngineByName("bacon");

  let promise = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  engine6.alias = "ba";

  await promise;
  Assert.equal(
    engine6.alias,
    "ba",
    "Should have correctly notified and changed the alias."
  );

  let observed = false;
  Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
    observed = true;
  }, SearchUtils.TOPIC_ENGINE_MODIFIED);

  engine6.alias = "ba";

  Assert.equal(engine6.alias, "ba", "Should have not changed the alias");
  Assert.ok(!observed, "Should not have notified for no change in alias");

  await extension.unload();
});
