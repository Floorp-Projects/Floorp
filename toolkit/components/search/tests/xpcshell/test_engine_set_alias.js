"use strict";

add_task(async function setup() {
  useHttpServer();
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

add_task(async function test_engine_set_alias() {
  info("Set engine alias");
  let [engine1] = await addTestEngines([
    {
      name: "bacon",
      details: {
        alias: "b",
        description: "Search Bacon",
        method: "GET",
        template: "http://www.bacon.test/find",
      },
    },
  ]);
  Assert.equal(engine1.alias, "b");
  engine1.alias = "a";
  Assert.equal(engine1.alias, "a");
  await Services.search.removeEngine(engine1);
});

add_task(async function test_engine_set_alias_with_left_space() {
  info("Set engine alias with left space");
  let [engine2] = await addTestEngines([
    {
      name: "bacon",
      details: {
        alias: "   a",
        description: "Search Bacon",
        method: "GET",
        template: "http://www.bacon.test/find",
      },
    },
  ]);
  Assert.equal(engine2.alias, "a");
  engine2.alias = "    c";
  Assert.equal(engine2.alias, "c");
  await Services.search.removeEngine(engine2);
});

add_task(async function test_engine_set_alias_with_right_space() {
  info("Set engine alias with right space");
  let [engine3] = await addTestEngines([
    {
      name: "bacon",
      details: {
        alias: "c   ",
        description: "Search Bacon",
        method: "GET",
        template: "http://www.bacon.test/find",
      },
    },
  ]);
  Assert.equal(engine3.alias, "c");
  engine3.alias = "o    ";
  Assert.equal(engine3.alias, "o");
  await Services.search.removeEngine(engine3);
});

add_task(async function test_engine_set_alias_with_right_left_space() {
  info("Set engine alias with left and right space");
  let [engine4] = await addTestEngines([
    {
      name: "bacon",
      details: {
        alias: " o  ",
        description: "Search Bacon",
        method: "GET",
        template: "http://www.bacon.test/find",
      },
    },
  ]);
  Assert.equal(engine4.alias, "o");
  engine4.alias = "  n ";
  Assert.equal(engine4.alias, "n");
  await Services.search.removeEngine(engine4);
});

add_task(async function test_engine_set_alias_with_space() {
  info("Set engine alias with space");
  let [engine5] = await addTestEngines([
    {
      name: "bacon",
      details: {
        alias: " ",
        description: "Search Bacon",
        method: "GET",
        template: "http://www.bacon.test/find",
      },
    },
  ]);
  Assert.equal(engine5.alias, null);
  engine5.alias = "b";
  Assert.equal(engine5.alias, "b");
  engine5.alias = "  ";
  Assert.equal(engine5.alias, null);
  await Services.search.removeEngine(engine5);
});
