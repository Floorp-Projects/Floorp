"use strict";

function run_test() {
  useHttpServer();

  run_next_test();
}

add_task(function* test_engine_set_alias() {
  yield asyncInit();
  do_print("Set engine alias");
  let [engine1] = yield addTestEngines([
    {
      name: "bacon",
      details: ["", "b", "Search Bacon", "GET", "http://www.bacon.test/find"]
    }
  ]);
  Assert.equal(engine1.alias, "b");
  engine1.alias = "a";
  Assert.equal(engine1.alias, "a");
  Services.search.removeEngine(engine1);
});

add_task(function* test_engine_set_alias_with_left_space() {
  do_print("Set engine alias with left space");
  let [engine2] = yield addTestEngines([
    {
      name: "bacon",
      details: ["", "   a", "Search Bacon", "GET", "http://www.bacon.test/find"]
    }
  ]);
  Assert.equal(engine2.alias, "a");
  engine2.alias = "    c";
  Assert.equal(engine2.alias, "c");
  Services.search.removeEngine(engine2);
});

add_task(function* test_engine_set_alias_with_right_space() {
  do_print("Set engine alias with right space");
  let [engine3] = yield addTestEngines([
    {
      name: "bacon",
      details: ["", "c   ", "Search Bacon", "GET", "http://www.bacon.test/find"]
    }
  ]);
  Assert.equal(engine3.alias, "c");
  engine3.alias = "o    ";
  Assert.equal(engine3.alias, "o");
  Services.search.removeEngine(engine3);
});

add_task(function* test_engine_set_alias_with_right_left_space() {
  do_print("Set engine alias with left and right space");
  let [engine4] = yield addTestEngines([
    {
      name: "bacon",
      details: ["", " o  ", "Search Bacon", "GET", "http://www.bacon.test/find"]
    }
  ]);
  Assert.equal(engine4.alias, "o");
  engine4.alias = "  n ";
  Assert.equal(engine4.alias, "n");
  Services.search.removeEngine(engine4);
});

add_task(function* test_engine_set_alias_with_space() {
  do_print("Set engine alias with space");
  let [engine5] = yield addTestEngines([
    {
      name: "bacon",
      details: ["", " ", "Search Bacon", "GET", "http://www.bacon.test/find"]
    }
  ]);
  Assert.equal(engine5.alias, null);
  engine5.alias = "b";
  Assert.equal(engine5.alias, "b");
  engine5.alias = "  ";
  Assert.equal(engine5.alias, null);
  Services.search.removeEngine(engine5);
});
