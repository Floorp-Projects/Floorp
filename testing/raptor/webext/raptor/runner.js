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

const GECKOVIEW_BROWSERS = ["fenix", "geckoview", "refbrow"];
const GECKO_BROWSERS = GECKOVIEW_BROWSERS + ["firefox"];

// when the browser starts this webext runner will start automatically; we
// want to give the browser some time (ms) to settle before starting tests
var postStartupDelay;

// delay (ms) between pageload cycles
var pageCycleDelay = 1000;

var newTabPerCycle = false;

// delay (ms) for foregrounding app
var foregroundDelay = 5000;

var browserName;
var isGecko;
var isGeckoView;
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
var isBackgroundTest = false;
var pageTimeout = 10000; // default pageload timeout
var geckoProfiling = false;
var geckoInterval = 1;
var geckoEntries = 1000000;
var geckoThreads = [];
var debugMode = 0;
var screenCapture = false;

var results = {
  name: "",
  page: "",
  type: "",
  browser_cycle: 0,
  expected_browser_cycles: 0,
  cold: false,
  lower_is_better: true,
  alert_change_type: "relative",
  alert_threshold: 2.0,
  measurements: {},
};

async function getTestSettings() {
  raptorLog("getting test settings from control server");
  const response = await fetch(settingsURL);
  const data = await response.text();
  raptorLog(`test settings received: ${data}`);

  // parse the test settings
  settings = JSON.parse(data)["raptor-options"];
  testType = settings.type;
  pageCycles = settings.page_cycles;
  testURL = settings.test_url;
  scenarioTestTime = settings.scenario_time;
  isBackgroundTest = settings.background_test;

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

  raptorLog(`test URL: ${testURL}`);

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
    newTabPerCycle = settings.newtab_per_cycle;
  }

  if (settings.page_timeout !== undefined) {
    pageTimeout = settings.page_timeout;
  }
  raptorLog(`using page timeout: ${pageTimeout}ms`);

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
        raptorLog("abort: 'measure' key not found in test settings");
        await cleanUp();
      }
      break;
  }

  // write options to storage that our content script needs to know
  if (isGecko) {
    await ext.storage.local.clear();
    await ext.storage.local.set({ settings });
  } else {
    await new Promise(resolve => {
      ext.storage.local.clear(() => {
        ext.storage.local.set({ settings }, resolve);
      });
    });
  }
  raptorLog("wrote settings to ext local storage");
}

async function sleep(delay) {
  return new Promise(resolve => setTimeout(resolve, delay));
}

async function getBrowserInfo() {
  if (isGecko) {
    const info = await ext.runtime.getBrowserInfo();
    results.browser = `${info.name} ${info.version} ${info.buildID}`;
  } else {
    const info = window.navigator.userAgent.split(" ");
    for (const entry in info) {
      if (info[entry].indexOf("Chrome") > -1) {
        results.browser = info[entry];
        break;
      }
    }
  }

  raptorLog(`testing on ${results.browser}`);
}

async function startScenarioTimer() {
  setTimeout(function() {
    isScenarioPending = false;
    results.measurements.scenario = [1];
  }, scenarioTestTime);

  await postToControlServer("status", `started scenario test timer`);
}

async function closeTab(tabId) {
  if (tabId == 0) {
    return;
  }

  await postToControlServer("status", `closing Tab: ${tabId}`);

  if (isGecko) {
    await ext.tabs.remove(tabId);
  } else {
    await new Promise(resolve => {
      ext.tabs.remove(tabId, resolve);
    });
  }

  await postToControlServer("status", `closed tab: ${tabId}`);
}

async function openTab() {
  await postToControlServer("status", "openinig new tab");

  let tab;
  if (isGecko) {
    tab = await ext.tabs.create({ url: "about:blank" });
  } else {
    tab = await new Promise(resolve => {
      ext.tabs.create({ url: "about:blank" }, resolve);
    });
  }

  await postToControlServer("status", `opened new empty tab: ${tab.id}`);

  return tab.id;
}

/**
 * Update the given tab by navigating to the test URL
 */
async function updateTab(tabId, url) {
  await postToControlServer("status", `update tab ${tabId} for ${url}`);

  // "null" = active tab
  if (isGecko) {
    await ext.tabs.update(tabId || null, { url });
  } else {
    await new Promise(resolve => {
      ext.tabs.update(tabId || null, { url }, resolve);
    });
  }

  await postToControlServer("status", `tab ${tabId} updated`);
}

async function collectResults() {
  // now we can set the page timeout timer and wait for pageload test result from content
  raptorLog("ready to poll for results; turning on page-timeout timer");
  setTimeoutAlarm("raptor-page-timeout", pageTimeout);

  // wait for pageload test result from content
  await waitForResults();

  // move on to next cycle (or test complete)
  await nextCycle();
}

function checkForTestFinished() {
  let finished = false;

  switch (testType) {
    case TEST_BENCHMARK:
      finished = !isBenchmarkPending;
      break;
    case TEST_PAGE_LOAD:
      if (
        !isHeroPending &&
        !isFNBPaintPending &&
        !isFCPPending &&
        !isDCFPending &&
        !isTTFIPending &&
        !isLoadTimePending
      ) {
        finished = true;
      }
      break;

    case TEST_SCENARIO:
      finished = !isScenarioPending;
      break;
  }

  return finished;
}

async function waitForResults() {
  raptorLog("waiting for results...");

  while (!checkForTestFinished()) {
    raptorLog("results pending...");
    await sleep(250);
  }

  await cancelTimeoutAlarm("raptor-page-timeout");

  await postToControlServer("status", "results received");

  if (geckoProfiling) {
    await getGeckoProfile();
  }

  if (screenCapture) {
    await getScreenCapture();
  }
}

async function getScreenCapture() {
  raptorLog("capturing screenshot");

  try {
    let screenshotUri;

    if (isGecko) {
      screenshotUri = await ext.tabs.captureVisibleTab();
    } else {
      screenshotUri = await new Promise(resolve =>
        ext.tabs.captureVisibleTab(resolve)
      );
    }

    await postToControlServer("screenshot", [
      screenshotUri,
      testName,
      pageCycle,
    ]);
  } catch (e) {
    raptorLog(`failed to capture screenshot: ${e}`);
  }
}

async function startGeckoProfiling() {
  await postToControlServer(
    "status",
    `starting Gecko profiling for threads: ${geckoThreads}`
  );
  await ext.geckoProfiler.start({
    bufferSize: geckoEntries,
    interval: geckoInterval,
    features: ["js", "leaf", "stackwalk", "threads", "responsiveness"],
    threads: geckoThreads.split(","),
  });
}

async function stopGeckoProfiling() {
  await postToControlServer("status", "stopping gecko profiling");
  await ext.geckoProfiler.stop();
}

async function getGeckoProfile() {
  // trigger saving the gecko profile, and send the file name to the control server
  const fileName = `${testName}_pagecycle_${pageCycle}.profile`;

  await postToControlServer("status", `saving gecko profile ${fileName}`);
  await ext.geckoProfiler.dumpProfileToFile(fileName);
  await postToControlServer("gecko_profile", fileName);

  // must stop the profiler so it clears the buffer before next cycle
  await stopGeckoProfiling();

  // resume if we have more pagecycles left
  if (pageCycle + 1 <= pageCycles) {
    await startGeckoProfiling();
  }
}

async function nextCycle() {
  pageCycle++;
  if (isBackgroundTest) {
    await postToControlServer(
      "end_background",
      `bringing app to foreground, pausing for ${foregroundDelay /
        1000} seconds`
    );
    // wait a bit to be sure the app is in foreground before starting
    // new test, or finishing test
    await sleep(foregroundDelay);
  }
  if (pageCycle == 1) {
    const text = `running ${pageCycles} pagecycles of ${testURL}`;
    await postToControlServer("status", text);
    // start the profiler if enabled
    if (geckoProfiling) {
      await startGeckoProfiling();
    }
  }
  if (pageCycle <= pageCycles) {
    if (isBackgroundTest) {
      await postToControlServer(
        "start_background",
        `bringing app to background`
      );
    }

    await sleep(pageCycleDelay);

    await postToControlServer("status", `begin page cycle ${pageCycle}`);

    switch (testType) {
      case TEST_BENCHMARK:
        isBenchmarkPending = true;
        break;

      case TEST_PAGE_LOAD:
        if (getHero) {
          isHeroPending = true;
          pendingHeroes = Array.from(settings.measure.hero);
        }
        if (getFNBPaint) {
          isFNBPaintPending = true;
        }
        if (getFCP) {
          isFCPPending = true;
        }
        if (getDCF) {
          isDCFPending = true;
        }
        if (getTTFI) {
          isTTFIPending = true;
        }
        if (getLoadTime) {
          isLoadTimePending = true;
        }
        break;

      case TEST_SCENARIO:
        isScenarioPending = true;
        break;
    }

    if (newTabPerCycle) {
      // close previous test tab and open a new one
      await closeTab(testTabID);
      testTabID = await openTab();
    }

    await updateTab(testTabID, testURL);

    if (testType == TEST_SCENARIO) {
      await startScenarioTimer();
    }

    // For benchmark or scenario type tests we can proceed directly to
    // waitForResults. However for page-load tests we must first wait until
    // we hear back from pageloaderjs that it has been successfully loaded
    // in the test page and has been invoked; and only then start looking
    // for measurements.
    if (testType != TEST_PAGE_LOAD) {
      await collectResults();
    }

    await postToControlServer("status", `ended page cycle ${pageCycle}`);
  } else {
    await verifyResults();
  }
}

async function timeoutAlarmListener() {
  raptorLog(`raptor-page-timeout on ${testURL}`, "error");

  const pendingMetrics = {
    hero: isHeroPending,
    "fnb paint": isFNBPaintPending,
    fcp: isFCPPending,
    dcf: isDCFPending,
    ttfi: isTTFIPending,
    "load time": isLoadTimePending,
  };

  let msgData = [testName, testURL];
  if (testType == TEST_PAGE_LOAD) {
    msgData.push(pendingMetrics);
  }

  await postToControlServer("raptor-page-timeout", msgData);
  await getScreenCapture();

  // call clean-up to shutdown gracefully
  await cleanUp();
}

function setTimeoutAlarm(timeoutName, timeoutMS) {
  // webext alarms require date.now NOT performance.now
  const now = Date.now(); // eslint-disable-line mozilla/avoid-Date-timing
  const timeout_when = now + timeoutMS;
  ext.alarms.create(timeoutName, { when: timeout_when });

  raptorLog(
    `now is ${now}, set raptor alarm ${timeoutName} to expire ` +
      `at ${timeout_when}`
  );
}

async function cancelTimeoutAlarm(timeoutName) {
  let cleared = false;

  if (isGecko) {
    cleared = await ext.alarms.clear(timeoutName);
  } else {
    cleared = await new Promise(resolve => {
      chrome.alarms.clear(timeoutName, resolve);
    });
  }

  if (cleared) {
    raptorLog(`cancelled raptor alarm ${timeoutName}`);
  } else {
    raptorLog(`failed to clear raptor alarm ${timeoutName}`, "error");
  }
}

function resultListener(request, sender, sendResponse) {
  raptorLog(`received message from ${sender.tab.url}`);

  // check if this is a message from pageloaderjs indicating it is ready to start
  if (request.type == "pageloadjs-ready") {
    raptorLog("received pageloadjs-ready!");

    sendResponse({ text: "pageloadjs-ready-response" });
    collectResults();
    return;
  }

  if (request.type && request.value) {
    raptorLog(`result: ${request.type} ${request.value}`);
    sendResponse({ text: `confirmed ${request.type}` });

    if (!(request.type in results.measurements)) {
      results.measurements[request.type] = [];
    }

    switch (testType) {
      case TEST_BENCHMARK:
        // benchmark results received (all results for that complete benchmark run)
        raptorLog("received results from benchmark");
        results.measurements[request.type].push(request.value);
        isBenchmarkPending = false;
        break;

      case TEST_PAGE_LOAD:
        // a single pageload measurement was received
        if (request.type.indexOf("hero") > -1) {
          results.measurements[request.type].push(request.value);
          const _found = request.type.split("hero:")[1];
          const index = pendingHeroes.indexOf(_found);
          if (index > -1) {
            pendingHeroes.splice(index, 1);
            if (pendingHeroes.length == 0) {
              raptorLog("measured all expected hero elements");
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
    raptorLog(`unknown message received from content: ${request}`);
  }
}

async function verifyResults() {
  raptorLog("Verifying results:");
  raptorLog(results);

  for (var x in results.measurements) {
    const count = results.measurements[x].length;
    if (count == pageCycles) {
      raptorLog(`have ${count} results for ${x}, as expected`);
    } else {
      raptorLog(
        `expected ${pageCycles} results for ${x} but only have ${count}`,
        "error"
      );
    }
  }

  await postToControlServer("results", results);

  // we're finished, move to cleanup
  await cleanUp();
}

async function postToControlServer(msgType, msgData = "") {
  await new Promise(resolve => {
    // requires 'control server' running at port 8000 to receive results
    const xhr = new XMLHttpRequest();
    xhr.open("POST", `http://${host}:${csPort}/`, true);
    xhr.setRequestHeader("Content-Type", "application/json");

    xhr.onreadystatechange = () => {
      if (xhr.readyState == XMLHttpRequest.DONE) {
        if (xhr.status != 200) {
          // Failed to send the message. At least add a console error.
          let msg = msgType;
          if (msgType != "screenshot") {
            msg += ` with '${msgData}'`;
          }
          raptorLog(`failed to post ${msg} to control server`, "error");
        }

        resolve();
      }
    };

    xhr.send(
      JSON.stringify({
        type: `webext_${msgType}`,
        data: msgData,
      })
    );
  });
}

async function cleanUp() {
  // close tab unless raptor debug-mode is enabled
  if (debugMode == 1) {
    raptorLog("debug-mode enabled, leaving tab open");
  } else {
    await closeTab();
  }

  if (testType == TEST_PAGE_LOAD) {
    // remove listeners
    ext.alarms.onAlarm.removeListener(timeoutAlarmListener);
    ext.runtime.onMessage.removeListener(resultListener);
  }
  raptorLog(`${testType} test finished`);

  // if profiling was enabled, stop the profiler - may have already
  // been stopped but stop again here in cleanup in case of timeout
  if (geckoProfiling) {
    await stopGeckoProfiling();
  }

  // tell the control server we are done and the browser can be shutdown
  await postToControlServer("shutdownBrowser");
}

async function raptorRunner() {
  await postToControlServer("status", "starting raptorRunner");

  if (isBackgroundTest) {
    await postToControlServer(
      "status",
      "raptor test will be backgrounding the app"
    );
  }

  await getBrowserInfo();
  await getTestSettings();

  raptorLog(`${testType} test start`);

  ext.alarms.onAlarm.addListener(timeoutAlarmListener);
  ext.runtime.onMessage.addListener(resultListener);

  // create new empty tab, which starts the test; we want to
  // wait some time for the browser to settle before beginning
  const text = `* pausing ${postStartupDelay /
    1000} seconds to let browser settle... *`;
  await postToControlServer("status", text);
  await sleep(postStartupDelay);

  // GeckoView doesn't support tabs
  if (!isGeckoView) {
    testTabID = await openTab();
  }

  await nextCycle();
}

function raptorLog(text, level = "info") {
  let prefix = "";

  if (level == "error") {
    prefix = "ERROR: ";
  }

  console[level](`${prefix}[raptor-runnerjs] ${text}`);
}

async function init() {
  const config = getTestConfig();
  testName = config.test_name;
  settingsURL = config.test_settings_url;
  csPort = config.cs_port;
  browserName = config.browser;
  benchmarkPort = config.benchmark_port;
  postStartupDelay = config.post_startup_delay;
  host = config.host;
  debugMode = config.debug_mode;
  browserCycle = config.browser_cycle;

  isGecko = GECKO_BROWSERS.includes(browserName);
  isGeckoView = GECKOVIEW_BROWSERS.includes(browserName);

  ext = isGecko ? browser : chrome;

  await postToControlServer("status", "raptor runner.js is loaded!");
  await postToControlServer("status", `test name is: ${testName}`);
  await postToControlServer("status", `test settings url is: ${settingsURL}`);

  try {
    if (window.document.readyState != "complete") {
      await new Promise(resolve => {
        window.addEventListener("load", resolve);
        raptorLog("Waiting for load event...");
      });
    }

    await raptorRunner();
  } catch (e) {
    await postToControlServer("error", [e.message, e.stack]);
    await postToControlServer("shutdownBrowser");
  }
}

init();
