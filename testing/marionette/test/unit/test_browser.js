const {Context} = ChromeUtils.import("chrome://marionette/content/browser.js", {});

add_test(function test_Context() {
  ok(Context.hasOwnProperty("Chrome"));
  ok(Context.hasOwnProperty("Content"));
  equal(typeof Context.Chrome, "string");
  equal(typeof Context.Content, "string");
  equal(Context.Chrome, "chrome");
  equal(Context.Content, "content");

  run_next_test();
});

add_test(function test_Context_fromString() {
  equal(Context.fromString("chrome"), Context.Chrome);
  equal(Context.fromString("content"), Context.Content);

  for (let typ of ["", "foo", true, 42, [], {}, null, undefined]) {
    Assert.throws(() => Context.fromString(typ), /TypeError/);
  }

  run_next_test();
});
