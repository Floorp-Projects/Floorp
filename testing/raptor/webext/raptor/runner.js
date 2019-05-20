
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

// Supported test types
const TEST_BENCHMARK = "benchmark";
const TEST_PAGE_LOAD = "pageload";
const TEST_SCENARIO = "scenario";

// when the browser starts this webext runner will start automatically; we
// want to give the browser some time (ms) to settle before starting tests
var postStartupDelay;

// delay (ms) between pageload cycles
var pageCycleDelay = 1000;

var newTabDelay = 1000;
var reuseTab = false;

var browserName;
var ext;
var testName = null;
var settingsURL = null;
var csPort = null;
var host = null;
var benchmarkPort = null;
var testType;
var browserCycle = 0;
var pageCycles = 0;
var pageCycle = 0;
var testURL;
var testTabID = 0;
var scenarioTestTime = 60000;
var getHero = false;
var getFNBPaint = false;
var getFCP = false;
var getDCF = false;
var getTTFI = false;
var getLoadTime = false;
var isHeroPending = false;
var pendingHeroes = [];
var settings = {};
var isFNBPaintPending = false;
var isFCPPending = false;
var isDCFPending = false;
var isTTFIPending = false;
var isLoadTimePending = false;
var isScenarioPending = false;
var isBenchmarkPending = false;
var pageTimeout = 10000; // default pageload timeout
var geckoProfiling = false;
var geckoInterval = 1;
var geckoEntries = 1000000;
var geckoThreads = [];
var debugMode = 0;
var screenCapture = false;

var results = {
  "name": "",
  "page": "",
  "type": "",
  "browser_cycle": 0,
  "expected_browser_cycles": 0,
  "cold": false,
  "lower_is_better": true,
  "alert_change_type": "relative",
  "alert_threshold": 2.0,
  "measurements": {},
};

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
        scenarioTestTime = settings.scenario_time;

        // for pageload type tests, the testURL is fine as is - we don't have
        // to add a port as it's accessed via proxy and the playback tool
        // however for benchmark tests, their source is served out on a local
        // webserver, so we need to swap in the webserver port into the testURL
        if (testType == TEST_BENCHMARK) {
          // just replace the '<port>' keyword in the URL with actual benchmarkPort
          testURL = testURL.replace("<port>", benchmarkPort);
        }

        if (host) {
          // just replace the '<host>' keyword in the URL with actual host
          testURL = testURL.replace("<host>", host);
        }

        console.log(`testURL: ${testURL}`);

        results.alert_change_type = settings.alert_change_type;
        results.alert_threshold = settings.alert_threshold;
        results.browser_cycle = browserCycle;
        results.cold = settings.cold;
        results.expected_browser_cycles = settings.expected_browser_cycles;
        results.lower_is_better = settings.lower_is_better === true;
        results.name = testName;
        results.page = testURL;
        results.type = testType;
        results.unit = settings.unit;
        results.subtest_unit = settings.subtest_unit;
        results.subtest_lower_is_better = settings.subtest_lower_is_better === true;

        if (settings.gecko_profile === true) {
          results.extra_options = ["gecko_profile"];

          geckoProfiling = true;
          geckoEntries = settings.gecko_profile_entries;
          geckoInterval = settings.gecko_profile_interval;
          geckoThreads = settings.gecko_profile_threads;
        }

        if (settings.screen_capture !== undefined) {
          screenCapture = settings.screen_capture;
        }

        if (settings.newtab_per_cycle !== undefined) {
          reuseTab = settings.newtab_per_cycle;
        }

        if (settings.page_timeout !== undefined) {
          pageTimeout = settings.page_timeout;
        }
        console.log(`using page timeout: ${pageTimeout}ms`);

        switch (testType) {
          case TEST_PAGE_LOAD:
            if (settings.measure !== undefined) {
              if (settings.measure.fnbpaint !== undefined) {
                getFNBPaint = settings.measure.fnbpaint;
              }
              if (settings.measure.dcf !== undefined) {
                getDCF = settings.measure.dcf;
              }
              if (settings.measure.fcp !== undefined) {
                getFCP = settings.measure.fcp;
              }
              if (settings.measure.hero !== undefined) {
                if (settings.measure.hero.length !== 0) {
                  getHero = true;
                }
              }
              if (settings.measure.ttfi !== undefined) {
                getTTFI = settings.measure.ttfi;
              }
              if (settings.measure.loadtime !== undefined) {
                getLoadTime = settings.measure.loadtime;
              }
            } else {
              console.log("abort: 'measure' key not found in test settings");
              cleanUp();
            }
            break;
        }

        // write options to storage that our content script needs to know
        if (["firefox", "geckoview", "refbrow", "fenix"].includes(browserName)) {
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
    if (["firefox", "geckoview", "refbrow", "fenix"].includes(browserName)) {
      ext = browser;
      var gettingInfo = browser.runtime.getBrowserInfo();
      gettingInfo.then(function(bi) {
        results.browser = bi.name + " " + bi.version + " " + bi.buildID;
        console.log(`testing on ${results.browser}`);
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
      console.log(`testing on ${results.browser}`);
      resolve();
    }
  });
}

function scenarioTimer() {
  postToControlServer("status", `started scenario test timer`);
    setTimeout(function() {
      isScenarioPending = false;
      results.measurements.scenario = [1];
  }, scenarioTestTime);
}

function testTabCreated(tab) {
  testTabID = tab.id;
  postToControlServer("status", `opened new empty tab: ${testTabID}`);
  // update raptor browser toolbar icon text, for a visual indicator when debugging
  ext.browserAction.setTitle({title: "Raptor RUNNING"});
}

function testTabRemoved(tab) {
  postToControlServer("status", `Removed tab: ${testTabID}`);
  testTabID = 0;
}

async function testTabUpdated(tab) {
  postToControlServer("status", `test tab updated: ${testTabID}`);
  // wait for pageload test result from content
  await waitForResult();
  // move on to next cycle (or test complete)
  nextCycle();
}

async function waitForResult() {
  let results = await new Promise(resolve => {
    function checkForResult() {
      console.log("checking results...");
      switch (testType) {
        case TEST_BENCHMARK:
          if (!isBenchmarkPending) {
            resolve();
          } else {
            setTimeout(checkForResult, 250);
          }
          break;

        case TEST_PAGE_LOAD:
          if (!isHeroPending &&
              !isFNBPaintPending &&
              !isFCPPending &&
              !isDCFPending &&
              !isTTFIPending &&
              !isLoadTimePending) {
            resolve();
          } else {
            setTimeout(checkForResult, 250);
          }
          break;

        case TEST_SCENARIO:
          if (!isScenarioPending) {
            cancelTimeoutAlarm("raptor-page-timeout");
            postToControlServer("status", `scenario test ended`);
            resolve();
          } else {
            setTimeout(checkForResult, 5);
          }
          break;
      }
    }

    checkForResult();
  });

  cancelTimeoutAlarm("raptor-page-timeout");

  postToControlServer("status", "results received");

  if (geckoProfiling) {
    await getGeckoProfile();
  }

  if (screenCapture) {
    await getScreenCapture();
  }

  return results;
}

async function getScreenCapture() {
  console.log("capturing screenshot");

  try {
    let screenshotUri;

    if (["firefox", "geckoview", "refbrow", "fenix"].includes(browserName)) {
      screenshotUri = await ext.tabs.captureVisibleTab();
    } else {
      screenshotUri = await new Promise(resolve =>
          ext.tabs.captureVisibleTab(resolve));
    }
    postToControlServer("screenshot", [screenshotUri, testName, pageCycle]);
  } catch (e) {
    console.log(`failed to capture screenshot: ${e}`);
  }
}

async function startGeckoProfiling() {
  postToControlServer("status", `starting Gecko profiling for threads: ${geckoThreads}`);
  await browser.geckoProfiler.start({
    bufferSize: geckoEntries,
    interval: geckoInterval,
    features: ["js", "leaf", "stackwalk", "threads", "responsiveness"],
    threads: geckoThreads.split(","),
  });
}

async function stopGeckoProfiling() {
  postToControlServer("status", "stopping gecko profiling");
  await browser.geckoProfiler.stop();
}

async function getGeckoProfile() {
  // get the profile and send to control server
  postToControlServer("status", "retrieving gecko profile");
  let arrayBuffer = await browser.geckoProfiler.getProfileAsArrayBuffer();
  let textDecoder = new TextDecoder();
  let profile = JSON.parse(textDecoder.decode(arrayBuffer));
  console.log(profile);
  postToControlServer("gecko_profile", [testName, pageCycle, profile]);
  // stop the profiler; must stop so it clears before next cycle
  await stopGeckoProfiling();
  // resume if we have more pagecycles left
  if (pageCycle + 1 <= pageCycles) {
    await startGeckoProfiling();
  }
}

async function nextCycle() {
  pageCycle++;
  if (pageCycle == 1) {
    let text = `running ${pageCycles} pagecycles of ${testURL}`;
    postToControlServer("status", text);
    // start the profiler if enabled
    if (geckoProfiling) {
      await startGeckoProfiling();
    }
  }
  if (pageCycle <= pageCycles) {
    setTimeout(function() {
      let text = "begin pagecycle " + pageCycle;
      postToControlServer("status", text);

      // set page timeout alarm
      setTimeoutAlarm("raptor-page-timeout", pageTimeout);

      switch (testType) {
        case TEST_BENCHMARK:
          isBenchmarkPending = true;
          break;

        case TEST_PAGE_LOAD:
          if (getHero) {
            isHeroPending = true;
            pendingHeroes = Array.from(settings.measure.hero);
          }
          if (getFNBPaint)
            isFNBPaintPending = true;
          if (getFCP)
            isFCPPending = true;
          if (getDCF)
            isDCFPending = true;
          if (getTTFI)
            isTTFIPending = true;
          if (getLoadTime)
            isLoadTimePending = true;
          break;

        case TEST_SCENARIO:
          isScenarioPending = true;
          break;
      }

      if (reuseTab && testTabID != 0) {
        // close previous test tab
        ext.tabs.remove(testTabID);
        postToControlServer("status", `closing Tab: ${testTabID}`);

        // open new tab
        ext.tabs.create({url: "about:blank"});
        postToControlServer("status", "Open new tab");
      }
      setTimeout(function() {
        postToControlServer("status", `update tab: ${testTabID}`);
        // update the test page - browse to our test URL
        ext.tabs.update(testTabID, {url: testURL}, testTabUpdated);
        if (testType == TEST_SCENARIO) {
          scenarioTimer();
        }
        }, newTabDelay);
      }, pageCycleDelay);
    } else {
      verifyResults();
    }
}

async function timeoutAlarmListener() {
  console.error(`raptor-page-timeout on ${testURL}`);

  var pendingMetrics = {
    "hero": isHeroPending,
    "fnb paint": isFNBPaintPending,
    "fcp": isFCPPending,
    "dcf": isDCFPending,
    "ttfi": isTTFIPending,
    "load time": isLoadTimePending,
  };

  var msgData = [testName, testURL];
  if (testType == TEST_PAGE_LOAD) {
    msgData.push(pendingMetrics);
  }
  postToControlServer("raptor-page-timeout", msgData);

  await getScreenCapture();

  // call clean-up to shutdown gracefully
  cleanUp();
}

function setTimeoutAlarm(timeoutName, timeoutMS) {
  // webext alarms require date.now NOT performance.now
  var now = Date.now(); // eslint-disable-line mozilla/avoid-Date-timing
  var timeout_when = now + timeoutMS;
  ext.alarms.create(timeoutName, { when: timeout_when });
  console.log(`now is ${now}, set raptor alarm ${timeoutName} to expire ` +
    `at ${timeout_when}`);
}

function cancelTimeoutAlarm(timeoutName) {
  if (browserName === "firefox" || browserName === "geckoview" ||
      browserName === "refbrow" || browserName === "fenix") {
    var clearAlarm = ext.alarms.clear(timeoutName);
    clearAlarm.then(function(onCleared) {
      if (onCleared) {
        console.log(`cancelled raptor alarm ${timeoutName}`);
      } else {
        console.error(`failed to clear raptor alarm ${timeoutName}`);
      }
    });
  } else {
    chrome.alarms.clear(timeoutName, function(wasCleared) {
      if (wasCleared) {
        console.log(`cancelled raptor alarm ${timeoutName}`);
      } else {
        console.error(`failed to clear raptor alarm ${timeoutName}`);
      }
    });
  }
}

function resultListener(request, sender, sendResponse) {
  console.log(`received message from ${sender.tab.url}`);
  if (request.type && request.value) {
    console.log(`result: ${request.type} ${request.value}`);
    sendResponse({text: `confirmed ${request.type}`});

    if (!(request.type in results.measurements))
      results.measurements[request.type] = [];

    switch (testType) {
      case TEST_BENCHMARK:
        // benchmark results received (all results for that complete benchmark run)
        console.log("received results from benchmark");
        results.measurements[request.type].push(request.value);
        isBenchmarkPending = false;
        break;

      case TEST_PAGE_LOAD:
        // a single pageload measurement was received
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
        } else if (request.type == "dcf") {
          results.measurements.dcf.push(request.value);
          isDCFPending = false;
        } else if (request.type == "ttfi") {
          results.measurements.ttfi.push(request.value);
          isTTFIPending = false;
        } else if (request.type == "fcp") {
          results.measurements.fcp.push(request.value);
          isFCPPending = false;
        } else if (request.type == "loadtime") {
          results.measurements.loadtime.push(request.value);
          isLoadTimePending = false;
        }
        break;
    }
  } else {
    console.log(`unknown message received from content: ${request}`);
  }
}

function verifyResults() {
  console.log("\nVerifying results:");
  console.log(results);
  for (var x in results.measurements) {
    let count = results.measurements[x].length;
    if (count == pageCycles) {
      console.log(`have ${count} results for ${x}, as expected`);
    } else {
      console.log(`ERROR: expected ${pageCycles} results for ${x} ` +
                  `but only have ${count}`);
    }
  }
  postToControlServer("results", results);
}

function postToControlServer(msgType, msgData) {
  // if posting a status message, log it to console also
  if (msgType == "status") {
    console.log(`\n${msgData}`);
  }
  // requires 'control server' running at port 8000 to receive results
  var url = `http://${host}:${csPort}/`;
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
    console.log(msgData);
    var data = { "type": `webext_${msgType}`, "data": msgData};
    client.send(JSON.stringify(data));
  }
  if (msgType == "results") {
    // we're finished, move to cleanup
    cleanUp();
  }
}

function cleanUp() {
  // close tab unless raptor debug-mode is enabled
  if (debugMode != 1) {
    ext.tabs.remove(testTabID);
    console.log(`closed tab ${testTabID}`);
  } else {
    console.log("raptor debug-mode enabled, leaving tab open");
  }

  if (testType == TEST_PAGE_LOAD) {
    // remove listeners
    ext.alarms.onAlarm.removeListener(timeoutAlarmListener);
    ext.runtime.onMessage.removeListener(resultListener);
    ext.tabs.onCreated.removeListener(testTabCreated);
  }
  console.log(`${testType} test finished`);

  // if profiling was enabled, stop the profiler - may have already
  // been stopped but stop again here in cleanup in case of timeout
  if (geckoProfiling) {
    stopGeckoProfiling();
  }

  // tell the control server we are done and the browser can be shutdown
  postToControlServer("status", "__raptor_shutdownBrowser");
}

function raptorRunner() {
  let config = getTestConfig();
  console.log(`test name is: ${config.test_name}`);
  console.log(`test settings url is: ${config.test_settings_url}`);
  testName = config.test_name;
  settingsURL = config.test_settings_url;
  csPort = config.cs_port;
  browserName = config.browser;
  benchmarkPort = config.benchmark_port;
  postStartupDelay = config.post_startup_delay;
  host = config.host;
  debugMode = config.debug_mode;
  browserCycle = config.browser_cycle;

  postToControlServer("status", "raptor runner.js is loaded!");

  getBrowserInfo().then(function() {
    getTestSettings().then(function() {
      console.log(`${testType} test start`);

      // timeout alarm listener
      ext.alarms.onAlarm.addListener(timeoutAlarmListener);

      // results listener
      ext.runtime.onMessage.addListener(resultListener);

      // tab creation listener
      ext.tabs.onCreated.addListener(testTabCreated);

      // tab remove listener
      ext.tabs.onRemoved.addListener(testTabRemoved);

      // create new empty tab, which starts the test; we want to
      // wait some time for the browser to settle before beginning
      let text = `* pausing ${postStartupDelay / 1000} seconds to let browser settle... *`;
      postToControlServer("status", text);

      // setTimeout(function() { nextCycle(); }, postStartupDelay);
      // on geckoview you can't create a new tab; only using existing tab - set it blank first
      if (config.browser == "geckoview" || config.browser == "refbrow" ||
          config.browser == "fenix") {
        setTimeout(function() { nextCycle(); }, postStartupDelay);
      } else {
        setTimeout(function() {
          ext.tabs.create({url: "about:blank"});
          nextCycle();
        }, postStartupDelay);
      }
    });
  });
}

if (window.addEventListener) {
  window.addEventListener("load", raptorRunner);
  postToControlServer("status", "Attaching event listener successful!");
} else {
  postToControlServer("status", "Attaching event listener failed!");
}
