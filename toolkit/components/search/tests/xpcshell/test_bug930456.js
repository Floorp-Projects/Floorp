/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  if (XULRuntime.processType == XULRuntime.PROCESS_TYPE_CONTENT) {
    Assert.equal(false, "@mozilla.org/browser/search-service;1" in Cc);
  } else {
    Assert.ok("@mozilla.org/browser/search-service;1" in Cc);
  }
}
