/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// this extension requires a 'control server' to be running on port 8000
// (see raptor prototype framework). It will provide the test options, as
// well as receive test results

// note: currently the prototype assumes the test page(s) are
// already available somewhere independently; so for now locally
// inside the 'talos-pagesets' dir or 'heroes' dir (tarek's github
// repo) or 'webkit/PerformanceTests' dir (for benchmarks) first run:
// 'python -m SimpleHTTPServer 8081'
// to serve out the pages that we want to prototype with. Also
// update the manifest content 'matches' accordingly

var browserName;
var ext;
var settingsURL = null;
var testType;
var pageCycles = 0;
var pageCycle = 0;
var pageCycleDelay = 1000;
var testURL;
var testTabID = 0;
var results = {"page": "", "measurements": {}};
var getHero = false;
var getFNBPaint = false;
var getFCP = false;
var isHeroPending = false;
var pendingHeroes = [];
var settings = {};
var isFNBPaintPending = false;
var isFCPPending = false;
var isBenchmarkPending = false;
var pageTimeout = 5000; // default pageload timeout

function getTestSettings() {
  console.log("getting test settings from control server");
  return new Promise(resolve => {

    fetch(settingsURL).then(function(response) {
      response.text().then(function(text) {
        console.log(text);
        settings = JSON.parse(text)["raptor-options"];

        // parse the test settings
        testType = settings.type;
        pageCycles = settings.page_cycles;
        testURL = settings.test_url;
        results.page = testURL;
        results.type = testType;

        if (settings.page_timeout !== undefined) {
          pageTimeout = settings.page_timeout;
        }
        console.log("using page timeout (ms): " + pageTimeout);

        if (testType == "pageload") {
          getFNBPaint = settings.measure.fnbpaint;
          getFCP = settings.measure.fcp;
          if (settings.measure.hero.length !== 0) {
            getHero = true;
          }
        }

        // write options to storage that our content script needs to know
        if (browserName === "firefox") {
          ext.storage.local.clear().then(function() {
            ext.storage.local.set({settings}).then(function() {
              console.log("wrote settings to ext local storage");
              resolve();
            });
          });
        } else {
          ext.storage.local.clear(function() {
            ext.storage.local.set({settings}, function() {
              console.log("wrote settings to ext local storage");
              resolve();
            });
          });
        }
      });
    });
  });
}

function getBrowserInfo() {
  return new Promise(resolve => {
    if (browserName === "firefox") {
      ext = browser;
      var gettingInfo = browser.runtime.getBrowserInfo();
      gettingInfo.then(function(bi) {
        results.browser = bi.name + " " + bi.version + " " + bi.buildID;
        console.log("testing on " + results.browser);
        resolve();
      });
    } else {
      ext = chrome;
      var browserInfo = window.navigator.userAgent.split(" ");
      for (let x in browserInfo) {
        if (browserInfo[x].indexOf("Chrome") > -1) {
          results.browser = browserInfo[x];
          break;
        }
      }
      console.log("testing on " + results.browser);
      resolve();
    }
  });
}

function testTabCreated(tab) {
  testTabID = tab.id;
  console.log("opened new empty tab " + testTabID);
  nextCycle();
}

async function testTabUpdated(tab) {
  console.log("tab " + tab.id + " reloaded");
  // wait for pageload test result from content
  await waitForResult();
  // move on to next cycle (or test complete)
  nextCycle();
}

function waitForResult() {
  console.log("awaiting results...");
  return new Promise(resolve => {
    function checkForResult() {
      if (testType == "pageload") {
        if (!isHeroPending && !isFNBPaintPending && !isFCPPending) {
          cancelTimeoutAlarm("raptor-page-timeout");
          resolve();
        } else {
          setTimeout(checkForResult, 5);
        }
      } else if (testType == "benchmark") {
        if (!isBenchmarkPending) {
          cancelTimeoutAlarm("raptor-page-timeout");
          resolve();
        } else {
          setTimeout(checkForResult, 5);
        }
      }
    }
    checkForResult();
  });
}

function nextCycle() {
  pageCycle++;
  if (pageCycle == 1) {
    var text = "running " + pageCycles + " pagecycles of " + testURL;
    postToControlServer("status", text);
  }
  if (pageCycle <= pageCycles) {
    setTimeout(function() {
      var text = "begin pagecycle " + pageCycle;
      console.log("\n" + text);
      postToControlServer("status", text);

      // set page timeout alarm
      setTimeoutAlarm("raptor-page-timeout", pageTimeout);

      if (testType == "pageload") {
        if (getHero)
          isHeroPending = true;
          pendingHeroes = Array.from(settings.measure.hero);
        if (getFNBPaint)
          isFNBPaintPending = true;
        if (getFCP)
          isFCPPending = true;
      } else if (testType == "benchmark") {
        isBenchmarkPending = true;
      }
      // reload the test page
      ext.tabs.update(testTabID, {url: testURL}, testTabUpdated);
    }, pageCycleDelay);
  } else {
    verifyResults();
  }
}

function timeoutAlarmListener(alarm) {
  var text = alarm.name;
  console.error(text);
  postToControlServer("status", text);
  // call clean-up to shutdown gracefully
  cleanUp();
}

function setTimeoutAlarm(timeoutName, timeoutMS) {
  var timeout_when = window.performance.now() + timeoutMS;
  ext.alarms.create(timeoutName, { when: timeout_when });
  console.log("set " + timeoutName);
}

function cancelTimeoutAlarm(timeoutName) {
  if (browserName === "firefox") {
    var clearAlarm = ext.alarms.clear(timeoutName);
    clearAlarm.then(function(onCleared) {
      if (onCleared) {
        console.log("cancelled " + timeoutName);
      } else {
        console.error("failed to clear " + timeoutName);
      }
    });
  } else {
    chrome.alarms.clear(timeoutName, function(wasCleared) {
      if (wasCleared) {
        console.log("cancelled " + timeoutName);
      } else {
        console.error("failed to clear " + timeoutName);
      }
    });
  }
}

function resultListener(request, sender, sendResponse) {
  console.log("received message from " + sender.tab.url);
  if (request.type && request.value) {
    console.log("result: " + request.type + " " + request.value);
    sendResponse({text: "confirmed " + request.type});

    if (!(request.type in results.measurements))
      results.measurements[request.type] = [];

    if (testType == "pageload") {
      // a single tp7 pageload measurement was received
      if (request.type.indexOf("hero") > -1) {
        results.measurements[request.type].push(request.value);
        var _found = request.type.split("hero:")[1];
        var index = pendingHeroes.indexOf(_found);
        if (index > -1) {
          pendingHeroes.splice(index, 1);
          if (pendingHeroes.length == 0) {
            console.log("measured all expected hero elements");
            isHeroPending = false;
          }
        }
      } else if (request.type == "fnbpaint") {
        results.measurements.fnbpaint.push(request.value);
        isFNBPaintPending = false;
      } else if (request.type == "fcp") {
        results.measurements.fcp.push(request.value);
        isFCPPending = false;
      }
    } else if (testType == "benchmark") {
      // benchmark results received (all results for that complete benchmark run)
      console.log("received results from benchmark");
      results.measurements[request.type].push(request.value);
      isBenchmarkPending = false;
    }
  } else {
    console.log("unknown message received from content: " + request);
  }
}

function verifyResults() {
  console.log("\nVerifying results:");
  console.log(results);
  for (var x in results.measurements) {
    let count = results.measurements[x].length;
    if (count == pageCycles) {
      console.log("have " + count + " results for " + x + ", as expected");
    } else {
      console.log("ERROR: expected " + pageCycles + " results for "
                  + x + " but only have " + count);
    }
  }
  postToControlServer("results", results);
}

function postToControlServer(msgType, msgData) {
  // requires 'control server' running at port 8000 to receive results
  var url = "http://127.0.0.1:8000/";
  var client = new XMLHttpRequest();
  client.onreadystatechange = function() {
    if (client.readyState == XMLHttpRequest.DONE && client.status == 200) {
      console.log("post success");
    }
  };

  client.open("POST", url, true);

  client.setRequestHeader("Content-Type", "application/json");
  if (client.readyState == 1) {
    console.log("posting to control server");
    var data = { "type": "webext_" + msgType, "data": msgData};
    client.send(JSON.stringify(data));
  }
  if (msgType == "results") {
    // we're finished, move to cleanup
    cleanUp();
  }
}

function cleanUp() {
  // close tab
  ext.tabs.remove(testTabID);
  console.log("closed tab " + testTabID);
  if (testType == "pageload") {
    // remove listeners
    ext.runtime.onMessage.removeListener(resultListener);
    ext.tabs.onCreated.removeListener(testTabCreated);
    ext.alarms.onAlarm.removeListener(timeoutAlarmListener);
    console.log("pageloader test finished");
  } else if (testType == "benchmark") {
    console.log("benchmark complete");
  }
  window.onload = null;
  // done, dump to console to tell framework to shutdown browser; currently
  // this only works with Firefox as google chrome doesn't support dump()
  if (browserName === "firefox")
    window.dump("\n__raptor_shutdownBrowser\n");

}

function runner() {
  let config = getTestConfig();
  settingsURL = config.test_settings_url;
  browserName = config.browser;
  getBrowserInfo().then(function() {
    getTestSettings().then(function() {
      if (testType == "benchmark") {
        // webkit benchmark type of test
        console.log("benchmark test start");
      } else if (testType == "pageload") {
        // standard 'tp7' pageload test
        console.log("pageloader test start");
      }
      // results listener
      ext.runtime.onMessage.addListener(resultListener);
      // tab creation listener
      ext.tabs.onCreated.addListener(testTabCreated);
      // timeout alarm listener
      ext.alarms.onAlarm.addListener(timeoutAlarmListener);
      // create new empty tab, which starts the test
      ext.tabs.create({url: "about:blank"});
    });
  });
}

window.onload = runner();
