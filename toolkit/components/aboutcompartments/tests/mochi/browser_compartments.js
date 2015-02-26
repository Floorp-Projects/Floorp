/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


"use strict";


const ROOT = getRootDirectory(gTestPath);
const FRAME_SCRIPTS = [
  ROOT + "content.js",
];
const URL = "chrome://mochitests/content/browser/toolkit/components/aboutcompartments/tests/mochi/browser_compartments.html";

let mm = Cc["@mozilla.org/globalmessagemanager;1"]
           .getService(Ci.nsIMessageListenerManager);

for (let script of FRAME_SCRIPTS) {
  mm.loadFrameScript(script, true);
}

registerCleanupFunction(() => {
  for (let script of FRAME_SCRIPTS) {
    mm.removeDelayedFrameScript(script, true);
  }
});

function promiseContentResponse(browser, name, message) {
  let mm = browser.messageManager;
  let promise = new Promise(resolve => {
    function removeListener() {
      mm.removeMessageListener(name, listener);
    }

    function listener(msg) {
      removeListener();
      resolve(msg.data);
    }

    mm.addMessageListener(name, listener);
    registerCleanupFunction(removeListener);
  });
  mm.sendAsyncMessage(name, message);
  return promise;
}

function getStatistics() {
  let compartmentInfo = Cc["@mozilla.org/compartment-info;1"]
          .getService(Ci.nsICompartmentInfo);
  let data = compartmentInfo.getCompartments();
  let result = [];
  for (let i = 0; i < data.length; ++i) {
    result.push(data.queryElementAt(i, Ci.nsICompartment));
  }
  return result;
}

function* promiseTabLoadEvent(tab, url)
{
  return new Promise(function (resolve, reject) {
    function handleLoadEvent(event) {
      if (event.originalTarget != tab.linkedBrowser.contentDocument ||
          event.target.location.href == "about:blank" ||
          (url && event.target.location.href != url)) {
        return;
      }

      tab.linkedBrowser.removeEventListener("load", handleLoadEvent, true);
      resolve(event);
    }

    tab.linkedBrowser.addEventListener("load", handleLoadEvent, true, true);
    if (url)
      tab.linkedBrowser.loadURI(url);
  });
}

add_task(function* init() {
  let monitoring = Cu.stopwatchMonitoring;
  Cu.stopwatchMonitoring = true;
  registerCleanupFunction(() => {
    Cu.stopwatchMonitoring = monitoring;
  });
});

add_task(function* test() {
  info("Extracting initial state");
  let stats0 = getStatistics();
  Assert.notEqual(stats0.length, 0, "There is more than one compartment");
  Assert.ok(!stats0.find(stat => stat.compartmentName.indexOf(URL) != -1),
    "The url doesn't appear yet");
  stats0.forEach(stat => {
    Assert.ok(stat.totalUserTime >= stat.ownUserTime, "Total >= own user time: " + stat.compartmentName);
    Assert.ok(stat.totalSystemTime >= stat.ownSystemTime, "Total >= own system time: " + stat.compartmentName);
  });

  let newTab = gBrowser.addTab();
  let browser = newTab.linkedBrowser;
  // Setup monitoring in the tab
  info("Setting up monitoring in the tab");
  let childMonitoring = yield promiseContentResponse(browser, "compartments-test:setMonitoring", true);

  info("Waiting for load to be complete");
  yield promiseTabLoadEvent(newTab, URL);

  if (!gMultiProcessBrowser) {
    // In non-e10s mode, the stats are counted in the single process
    let stats1 = getStatistics();
    let pageStats = stats1.find(stat => stat.compartmentName.indexOf(URL) != -1);
    Assert.notEqual(pageStats, null, "The new page appears in the single-process statistics");
    Assert.notEqual(pageStats.totalUserTime, 0, "Total single-process user time is > 0");
  }

  // Regardless of e10s, the stats should be accessible in the content process (or pseudo-process)
  let stats2 = yield promiseContentResponse(browser, "compartments-test:getCompartments", null);
  let pageStats = stats2.find(stat => stat.compartmentName.indexOf(URL) != -1);
  Assert.notEqual(pageStats, null, "The new page appears in the content statistics");
  Assert.notEqual(pageStats.totalUserTime, 0, "Total content user time is > 0");
  Assert.ok(pageStats.totalUserTime >= 1000, "Total content user time is at least 1ms");

  // Cleanup
  yield promiseContentResponse(browser, "compartments-test:setMonitoring", childMonitoring);
  gBrowser.removeTab(newTab);
});
