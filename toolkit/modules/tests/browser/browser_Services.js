/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/Services.jsm", this);

add_task(function* test_ppmm() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "about:blank"
  }, function*(browser) {
    info("Checking parent process");
    Assert.notEqual(Services.ppmm, null, "Services.ppmm is defined in the parent process");

    info("Checking content");
    let {hasPPMM, isContentProcess} = yield ContentTask.spawn(browser, null, function*() {
      const Cu = Components.utils;
      let Services = Cu.import("resource://gre/modules/Services.jsm", {}).Services;
      return {
        hasPPMM: !!Services.ppmm,
        isContentProcess: Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT
      };
    });

    Assert.equal(hasPPMM, !isContentProcess, "The content should have Services.ppmm iff it's not the content process");
  });
});
