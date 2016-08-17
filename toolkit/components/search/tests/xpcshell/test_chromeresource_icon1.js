/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Test that resource URLs can be used in default engines */

"use strict";

function run_test() {
  updateAppInfo();

  // The test engines used in this test need to be recognized as 'default'
  // engines or the resource URL won't be used
  let url = "resource://test/data/";
  let resProt = Services.io.getProtocolHandler("resource")
                        .QueryInterface(Ci.nsIResProtocolHandler);
  resProt.setSubstitution("search-plugins",
                          Services.io.newURI(url, null, null));

  run_next_test();
}

add_task(function* test_defaultresourceicon() {
  yield asyncInit();

  let engine1 = Services.search.getEngineByName("engine-resourceicon");
  do_check_eq(engine1.iconURI.spec, "resource://search-plugins/icon16.png");
  do_check_eq(engine1.getIconURLBySize(32, 32), "resource://search-plugins/icon32.png");
  let engine2 = Services.search.getEngineByName("engine-chromeicon");
  do_check_eq(engine2.iconURI.spec, "chrome://branding/content/icon16.png");
  do_check_eq(engine2.getIconURLBySize(32, 32), "chrome://branding/content/icon32.png");
});
