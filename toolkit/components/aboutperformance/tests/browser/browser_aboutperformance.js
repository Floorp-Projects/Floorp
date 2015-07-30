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
  
  addMessageListener("aboutperformance-test:hasItems", ({data: {title, options}}) => {
    let observer = function(subject, topic, mode) {
      Services.obs.removeObserver(observer, "about:performance-update-complete");
      let hasTitleInWebpages = false;
      let hasTitleInAddons = false;

      try {
        let eltWeb = content.document.getElementById("webpages");
        let eltAddons = content.document.getElementById("addons");
        if (!eltWeb || !eltAddons) {
          return;
        }

        let addonTitles = [for (eltContent of eltAddons.querySelectorAll("span.title")) eltContent.textContent];
        let webTitles = [for (eltContent of eltWeb.querySelectorAll("span.title")) eltContent.textContent];

        hasTitleInAddons = addonTitles.includes(title);
        hasTitleInWebpages = webTitles.includes(title);

      } catch (ex) {
        Cu.reportError("Error in content: " + ex);
        Cu.reportError(ex.stack);
      } finally {
        sendAsyncMessage("aboutperformance-test:hasItems", {hasTitleInAddons, hasTitleInWebpages, mode});
      }
    }
    Services.obs.addObserver(observer, "about:performance-update-complete", false);
    Services.obs.notifyObservers(null, "test-about:performance-test-driver", JSON.stringify(options));
  });
}

let gTabAboutPerformance = null;
let gTabContent = null;

add_task(function* init() {
  info("Setting up about:performance");
  gTabAboutPerformance = gBrowser.selectedTab = gBrowser.addTab("about:performance");
  yield ContentTask.spawn(gTabAboutPerformance.linkedBrowser, null, frameScript);

  info(`Setting up ${URL}`);
  gTabContent = gBrowser.addTab(URL);
  yield ContentTask.spawn(gTabContent.linkedBrowser, null, frameScript);
});

let promiseExpectContent = Task.async(function*(options) {
  let title = "Testing about:performance " + Math.random();
  for (let i = 0; i < 30; ++i) {
    yield promiseContentResponse(gTabContent.linkedBrowser, "aboutperformance-test:setTitle", title);
    let {hasTitleInWebpages, hasTitleInAddons, mode} = (yield promiseContentResponse(gTabAboutPerformance.linkedBrowser, "aboutperformance-test:hasItems", {title, options}));
    if (hasTitleInWebpages && ((mode == "recent") == options.displayRecent)) {
      Assert.ok(!hasTitleInAddons, "The title appears in webpages, but not in addons");
      return true;
    }
    info(`Title not found, trying again ${i}/30`);
    yield new Promise(resolve => setTimeout(resolve, 100));
  }
  return false;
});

add_task(function* tests() {
    for (let autoRefresh of [100, -1]) {
      for (let displayRecent of [true, false]) {
        info(`Testing ${autoRefresh > 0?"with":"without"} autoRefresh, in ${displayRecent?"recent":"global"} mode`);
        let found = yield promiseExpectContent({autoRefresh, displayRecent});
        Assert.equal(found, autoRefresh > 0, "The page title appears iff about:performance is set to auto-refresh");
      }
    }
});

add_task(function* cleanup() {
  // Cleanup
  info("Cleaning up");
  yield promiseContentResponse(gTabAboutPerformance.linkedBrowser, "aboutperformance-test:done", null);

  info("Closing tabs");
  for (let tab of gBrowser.tabs) {
    yield BrowserTestUtils.removeTab(tab);
  }

  info("Done");
  gBrowser.selectedTab = null;
});
