/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://testing-common/ContentTask.jsm", this);

const SALT = Math.random();
const URL = "http://example.com/browser/toolkit/components/aboutperformance/tests/browser/browser_compartments.html?test=" + SALT;

// This function is injected as source as a frameScript
function frameScript() {
  "use strict";

  addMessageListener("aboutperformance-test:hasItems", ({data: salt}) => {
    let hasPlatform = false;
    let hasSalt = false;

    try {
      let eltData = content.document.getElementById("liveData");
      if (!eltData) {
        return;
      }

      // Find if we have a row for "platform"
      hasPlatform = eltData.querySelector("tr.platform") != null;

      // Find if we have a row for our URL
      hasSalt = false;
      for (let eltContent of eltData.querySelectorAll("tr.content td.name")) {
        let name = eltContent.textContent;
        if (name.contains(salt) && !name.contains("http")) {
          hasSalt = true;
          break;
        }
      }

    } catch (ex) {
      Cu.reportError("Error in content: " + ex);
      Cu.reportError(ex.stack);
    } finally {
      sendAsyncMessage("aboutperformance-test:hasItems", {hasPlatform, hasSalt});
    }
  });
}


add_task(function* test() {
  let tabAboutPerformance = gBrowser.addTab("about:performance");
  let tabContent = gBrowser.addTab(URL);

  yield ContentTask.spawn(tabAboutPerformance.linkedBrowser, null, frameScript);

  while (true) {
    yield new Promise(resolve => setTimeout(resolve, 100));
    let {hasPlatform, hasSalt} = (yield promiseContentResponse(tabAboutPerformance.linkedBrowser, "aboutperformance-test:hasItems", SALT));
    info(`Platform: ${hasPlatform}, salt: ${hasSalt}`);
    if (hasPlatform && hasSalt) {
      Assert.ok(true, "Found a row for <platform> and a row for our page");
      break;
    }
  }

  // Cleanup
  gBrowser.removeTab(tabContent);
  gBrowser.removeTab(tabAboutPerformance);
});
