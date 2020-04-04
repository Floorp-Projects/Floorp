/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Supported test types
const TEST_BENCHMARK = "benchmark";
const TEST_PAGE_LOAD = "pageload";

// content script for use with pageload tests
var perfData = window.performance;
var gRetryCounter = 0;

// measure hero element; must exist inside test page;
// supported on: Firefox, Chromium, Geckoview
// default only; this is set via control server settings json
var getHero = false;
var heroesToCapture = [];

// measure time-to-first-non-blank-paint
// supported on: Firefox, Geckoview
// note: this browser pref must be enabled:
// dom.performance.time_to_non_blank_paint.enabled = True
// default only; this is set via control server settings json
var getFNBPaint = false;

// measure time-to-first-contentful-paint
// supported on: Firefox, Chromium, Geckoview
// note: this browser pref must be enabled:
// dom.performance.time_to_contentful_paint.enabled = True
// default only; this is set via control server settings json
var getFCP = false;

// measure domContentFlushed
// supported on: Firefox, Geckoview
// note: this browser pref must be enabled:
// dom.performance.time_to_dom_content_flushed.enabled = True
// default only; this is set via control server settings json
var getDCF = false;

// measure TTFI
// supported on: Firefox, Geckoview
// note: this browser pref must be enabled:
// dom.performance.time_to_first_interactive.enabled = True
// default only; this is set via control server settings json
var getTTFI = false;

// supported on: Firefox, Chromium, Geckoview
// default only; this is set via control server settings json
var getLoadTime = false;

// performance.timing measurement used as 'starttime'
var startMeasure = "fetchStart";

async function raptorContentHandler() {
  raptorLog("pageloadjs raptorContentHandler!");
  // let the main raptor runner know that we (pageloadjs) is ready!
  await sendPageloadReady();

  // retrieve test settings from local ext storage
  let settings;
  if (typeof browser !== "undefined") {
    ({ settings } = await browser.storage.local.get("settings"));
  } else {
    ({ settings } = await new Promise(resolve => {
      chrome.storage.local.get("settings", resolve);
    }));
  }
  setup(settings);
}

function setup(settings) {
  if (settings.type != TEST_PAGE_LOAD) {
    return;
  }

  if (settings.measure == undefined) {
    raptorLog("abort: 'measure' key not found in test settings");
    return;
  }

  if (settings.measure.fnbpaint !== undefined) {
    getFNBPaint = settings.measure.fnbpaint;
    if (getFNBPaint) {
      raptorLog("will be measuring fnbpaint");
      measureFNBPaint();
    }
  }

  if (settings.measure.dcf !== undefined) {
    getDCF = settings.measure.dcf;
    if (getDCF) {
      raptorLog("will be measuring dcf");
      measureDCF();
    }
  }

  if (settings.measure.fcp !== undefined) {
    getFCP = settings.measure.fcp;
    if (getFCP) {
      raptorLog("will be measuring first-contentful-paint");
      measureFCP();
    }
  }

  if (settings.measure.hero !== undefined) {
    if (settings.measure.hero.length !== 0) {
      getHero = true;
      heroesToCapture = settings.measure.hero;
      raptorLog(`hero elements to measure: ${heroesToCapture}`);
      measureHero();
    }
  }

  if (settings.measure.ttfi !== undefined) {
    getTTFI = settings.measure.ttfi;
    if (getTTFI) {
      raptorLog("will be measuring ttfi");
      measureTTFI();
    }
  }

  if (settings.measure.loadtime !== undefined) {
    getLoadTime = settings.measure.loadtime;
    if (getLoadTime) {
      raptorLog("will be measuring loadtime");
      measureLoadTime();
    }
  }
}

function measureHero() {
  let obs;

  const heroElementsFound = window.document.querySelectorAll("[elementtiming]");
  raptorLog(`found ${heroElementsFound.length} hero elements in the page`);

  if (heroElementsFound) {
    async function callbackHero(entries, observer) {
      for (const entry in entries) {
        const heroFound = entry.target.getAttribute("elementtiming");
        // mark the time now as when hero element received
        perfData.mark(heroFound);
        const resultType = `hero:${heroFound}`;
        raptorLog(`found ${resultType}`);
        // calculcate result: performance.timing.fetchStart - time when we got hero element
        perfData.measure(
          (name = resultType),
          (startMark = startMeasure),
          (endMark = heroFound)
        );
        const perfResult = perfData.getEntriesByName(resultType);
        const _result = Math.round(perfResult[0].duration);
        await sendResult(resultType, _result);
        perfData.clearMarks();
        perfData.clearMeasures();
        obs.disconnect();
      }
    }
    // we want the element 100% visible on the viewport
    const options = { root: null, rootMargin: "0px", threshold: [1] };
    try {
      obs = new window.IntersectionObserver(callbackHero, options);
      heroElementsFound.forEach(function(el) {
        // if hero element is one we want to measure, add it to the observer
        if (heroesToCapture.indexOf(el.getAttribute("elementtiming")) > -1) {
          obs.observe(el);
        }
      });
    } catch (err) {
      raptorLog(err);
    }
  } else {
    raptorLog("couldn't find hero element");
  }
}

async function measureFNBPaint() {
  const x = window.performance.timing.timeToNonBlankPaint;

  if (typeof x == "undefined") {
    raptorLog(
      "timeToNonBlankPaint is undefined; ensure the pref is enabled",
      "error"
    );
    return;
  }
  if (x > 0) {
    raptorLog("got fnbpaint");
    gRetryCounter = 0;
    const startTime = perfData.timing.fetchStart;
    await sendResult("fnbpaint", x - startTime);
  } else {
    gRetryCounter += 1;
    if (gRetryCounter <= 10) {
      raptorLog(
        `fnbpaint is not yet available, retry number ${gRetryCounter}...`
      );
      window.setTimeout(measureFNBPaint, 100);
    } else {
      raptorLog(
        `unable to get a value for fnbpaint after ${gRetryCounter} retries`
      );
    }
  }
}

async function measureDCF() {
  const x = window.performance.timing.timeToDOMContentFlushed;

  if (typeof x == "undefined") {
    raptorLog(
      "domContentFlushed is undefined; ensure the pref is enabled",
      "error"
    );
    return;
  }
  if (x > 0) {
    raptorLog(`got domContentFlushed: ${x}`);
    gRetryCounter = 0;
    const startTime = perfData.timing.fetchStart;
    await sendResult("dcf", x - startTime);
  } else {
    gRetryCounter += 1;
    if (gRetryCounter <= 10) {
      raptorLog(
        `dcf is not yet available (0), retry number ${gRetryCounter}...`
      );
      window.setTimeout(measureDCF, 100);
    } else {
      raptorLog(`unable to get a value for dcf after ${gRetryCounter} retries`);
    }
  }
}

async function measureTTFI() {
  const x = window.performance.timing.timeToFirstInteractive;

  if (typeof x == "undefined") {
    raptorLog(
      "timeToFirstInteractive is undefined; ensure the pref is enabled",
      "error"
    );
    return;
  }
  if (x > 0) {
    raptorLog(`got timeToFirstInteractive: ${x}`);
    gRetryCounter = 0;
    const startTime = perfData.timing.fetchStart;
    await sendResult("ttfi", x - startTime);
  } else {
    gRetryCounter += 1;
    // NOTE: currently the gecko implementation doesn't look at network
    // requests, so this is closer to TimeToFirstInteractive than
    // TimeToInteractive.  TTFI/TTI requires running at least 5 seconds
    // past last "busy" point, give 25 seconds here (overall the harness
    // times out at 30 seconds).  Some pages will never get 5 seconds
    // without a busy period!
    if (gRetryCounter <= 25 * (1000 / 200)) {
      raptorLog(
        `TTFI is not yet available (0), retry number ${gRetryCounter}...`
      );
      window.setTimeout(measureTTFI, 200);
    } else {
      // unable to get a value for TTFI - negative value will be filtered out later
      raptorLog("TTFI was not available for this pageload");
      await sendResult("ttfi", -1);
    }
  }
}

async function measureFCP() {
  // see https://developer.mozilla.org/en-US/docs/Web/API/PerformancePaintTiming
  let result = window.performance.timing.timeToContentfulPaint;

  // Firefox implementation of FCP is not yet spec-compliant (see Bug 1519410)
  if (typeof result == "undefined") {
    // we're on chromium
    result = 0;

    const perfEntries = perfData.getEntriesByType("paint");
    if (perfEntries.length >= 2) {
      if (
        perfEntries[1].name == "first-contentful-paint" &&
        perfEntries[1].startTime != undefined
      ) {
        // this value is actually the final measurement / time to get the FCP event in MS
        result = perfEntries[1].startTime;
      }
    }
  }

  if (result > 0) {
    raptorLog("got time to first-contentful-paint");
    if (typeof browser !== "undefined") {
      // Firefox returns a timestamp, not the actual measurement in MS; need to calculate result
      const startTime = perfData.timing.fetchStart;
      result = result - startTime;
    }
    await sendResult("fcp", result);
    perfData.clearMarks();
    perfData.clearMeasures();
  } else {
    gRetryCounter += 1;
    if (gRetryCounter <= 10) {
      raptorLog(
        `time to first-contentful-paint is not yet available (0), retry number ${gRetryCounter}...`
      );
      window.setTimeout(measureFCP, 100);
    } else {
      raptorLog(
        `unable to get a value for time-to-fcp after ${gRetryCounter} retries`
      );
    }
  }
}

async function measureLoadTime() {
  const x = window.performance.timing.loadEventStart;

  if (typeof x == "undefined") {
    raptorLog("loadEventStart is undefined", "error");
    return;
  }
  if (x > 0) {
    raptorLog(`got loadEventStart: ${x}`);
    gRetryCounter = 0;
    const startTime = perfData.timing.fetchStart;
    await sendResult("loadtime", x - startTime);
  } else {
    gRetryCounter += 1;
    if (gRetryCounter <= 40 * (1000 / 200)) {
      raptorLog(
        `loadEventStart is not yet available (0), retry number ${gRetryCounter}...`
      );
      window.setTimeout(measureLoadTime, 100);
    } else {
      raptorLog(
        `unable to get a value for loadEventStart after ${gRetryCounter} retries`
      );
    }
  }
}

/**
 * Send message to runnerjs indicating pageloadjs is ready to start getting measures
 */
async function sendPageloadReady() {
  raptorLog("sending pageloadjs-ready message to runnerjs");

  let response;
  if (typeof browser !== "undefined") {
    response = await browser.runtime.sendMessage({ type: "pageloadjs-ready" });
  } else {
    response = await new Promise(resolve => {
      chrome.runtime.sendMessage({ type: "pageloadjs-ready" }, resolve);
    });
  }

  if (response) {
    raptorLog(`Response: ${response.text}`);
  }
}

/**
 * Send result back to background runner script
 */
async function sendResult(type, value) {
  raptorLog(`sending result back to runner: ${type} ${value}`);

  let response;
  if (typeof browser !== "undefined") {
    response = await browser.runtime.sendMessage({ type, value });
  } else {
    response = await new Promise(resolve => {
      chrome.runtime.sendMessage({ type, value }, resolve);
    });
  }

  if (response) {
    raptorLog(`Response: ${response.text}`);
  }
}

function raptorLog(text, level = "info") {
  let prefix = "";

  if (level == "error") {
    prefix = "ERROR: ";
  }

  console[level](`${prefix}[raptor-pageloadjs] ${text}`);
}

if (window.addEventListener) {
  window.addEventListener("load", raptorContentHandler);
}
