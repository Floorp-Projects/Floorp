/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://testing-common/ContentTask.jsm", this);

const URL = "http://example.com/browser/toolkit/components/aboutperformance/tests/browser/browser_compartments.html?test=" + Math.random();

// This function is injected as source as a frameScript
function frameScript() {
  "use strict";

  addMessageListener("aboutperformance-test:done", () => {
    content.postMessage("stop", "*");
    sendAsyncMessage("aboutperformance-test:done", null);
  });
  addMessageListener("aboutperformance-test:setTitle", ({data: title}) => {
    content.document.title = title;
    sendAsyncMessage("aboutperformance-test:setTitle", null);
  });
  
  addMessageListener("aboutperformance-test:hasItems", ({data: title}) => {
    let observer = function() {
      Services.obs.removeObserver(observer, "about:performance-update-complete");
      let hasPlatform = false;
      let hasTitle = false;

      try {
        let eltData = content.document.getElementById("liveData");
        if (!eltData) {
          return;
        }

        // Find if we have a row for "platform"
        hasPlatform = eltData.querySelector("tr.platform") != null;

        // Find if we have a row for our content page
        let titles = [for (eltContent of eltData.querySelectorAll("td.contents.name")) eltContent.textContent];

        hasTitle = titles.includes(title);
      } catch (ex) {
        Cu.reportError("Error in content: " + ex);
        Cu.reportError(ex.stack);
      } finally {
        sendAsyncMessage("aboutperformance-test:hasItems", {hasPlatform, hasTitle});
      }
    }
    Services.obs.addObserver(observer, "about:performance-update-complete", false);
    Services.obs.notifyObservers(null, "about:performance-update-immediately", "");
  });
}


add_task(function* go() {
  info("Setting up about:performance");
  let tabAboutPerformance = gBrowser.selectedTab = gBrowser.addTab("about:performance");
  yield ContentTask.spawn(tabAboutPerformance.linkedBrowser, null, frameScript);

  info(`Setting up ${URL}`);
  let tabContent = gBrowser.addTab(URL);
  yield ContentTask.spawn(tabContent.linkedBrowser, null, frameScript);

  let title = "Testing about:performance " + Math.random();
  info(`Setting up title ${title}`);
  while (true) {
    yield promiseContentResponse(tabContent.linkedBrowser, "aboutperformance-test:setTitle", title);
    yield new Promise(resolve => setTimeout(resolve, 100));
    let {hasPlatform, hasTitle} = (yield promiseContentResponse(tabAboutPerformance.linkedBrowser, "aboutperformance-test:hasItems", title));
    info(`Platform: ${hasPlatform}, title: ${hasTitle}`);
    if (hasPlatform && hasTitle) {
      Assert.ok(true, "Found a row for <platform> and a row for our page");
      break;
    }
  }

  // Cleanup
  info("Cleaning up");
  yield promiseContentResponse(tabAboutPerformance.linkedBrowser, "aboutperformance-test:done", null);

  info("Closing tabs");
  for (let tab of gBrowser.tabs) {
    yield BrowserTestUtils.removeTab(tab);
  }

  info("Done");
  gBrowser.selectedTab = null;
});
