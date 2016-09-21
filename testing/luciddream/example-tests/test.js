ok(true, "Assertion from a JS script!");
//ok(false, "test failure");
setTimeout(function() {
  ok(true, "Assertion from setTimeout!");
  finish();
}, 15);
