/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/osfile.jsm");

function run_test() {
  initTestLogging();
  run_next_test();
}

add_test(function test_writeJSON_readJSON() {
  _("Round-trip some JSON through the promise-based JSON writer.");

  let contents = {
    "a": 12345.67,
    "b": {
      "c": "héllö",
    },
    "d": undefined,
    "e": null,
  };

  function checkJSON(json) {
    Assert.equal(contents.a, json.a);
    Assert.equal(contents.b.c, json.b.c);
    Assert.equal(contents.d, json.d);
    Assert.equal(contents.e, json.e);
    run_next_test();
  }

  function doRead() {
    CommonUtils.readJSON(path)
               .then(checkJSON, do_throw);
  }

  let path = OS.Path.join(OS.Constants.Path.profileDir, "bar.json");
  CommonUtils.writeJSON(contents, path)
             .then(doRead, do_throw);
});
