/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Test that an installed engine can't use a resource URL for an icon */

"use strict";

function run_test() {
  removeMetadata();
  updateAppInfo();
  useHttpServer();

  run_next_test();
}

add_task(function* test_installedresourceicon() {
  let [engine1, engine2] = yield addTestEngines([
    { name: "engine-resourceicon", xmlFileName: "engine-resourceicon.xml" },
    { name: "engine-chromeicon", xmlFileName: "engine-chromeicon.xml" },
  ]);
  do_check_null(engine1.iconURI);
  do_check_null(engine2.iconURI);
});
