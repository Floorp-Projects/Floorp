/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Task.jsm");

Cu.import("chrome://marionette/content/assert.js");
Cu.import("chrome://marionette/content/capture.js");
const {InvalidArgumentError} =
    Cu.import("chrome://marionette/content/error.js", {});

this.EXPORTED_SYMBOLS = ["reftest"];

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const PREF_E10S = "browser.tabs.remote.autostart";

const logger = Log.repository.getLogger("Marionette");

const SCREENSHOT_MODE = {
  unexpected: 0,
  fail: 1,
  always: 2,
};

const STATUS = {
  PASS: "PASS",
  FAIL: "FAIL",
  ERROR: "ERROR",
  TIMEOUT: "TIMEOUT",
};

/**
 * Implements an fast runner for web-platform-tests format reftests
 * c.f. http://web-platform-tests.org/writing-tests/reftests.html
 */
let reftest = {};

reftest.Runner = class {
  constructor(driver) {
    this.driver = driver;
    this.canvasCache = new Map([[null, []]]);
    this.windowUtils = null;
    this.lastURL = null;
    this.remote = Preferences.get(PREF_E10S);
  }

  /**
   * Setup the required environment for running reftests.
   *
   * This will open a non-browser window in which the tests will
   * be loaded, and set up various caches for the reftest run.
   *
   * @param {Object.<Number>} urlCount
   *     Object holding a map of URL: number of times the URL
   *     will be opened during the reftest run, where that's
   *     greater than 1.
   * @param {string} screenshotMode
   *     String enum representing when screenshots should be taken
   */
  *setup(urlCount, screenshotMode) {
    this.parentWindow =  assert.window(this.driver.getCurrentWindow());

    this.screenshotMode = SCREENSHOT_MODE[screenshotMode] ||
        SCREENSHOT_MODE["unexpected"];

    this.urlCount = Object.keys(urlCount || {})
        .reduce((map, key) => map.set(key, urlCount[key]), new Map());

    yield this.ensureWindow();
  }

  *ensureWindow() {
    if (this.reftestWin && !this.reftestWin.closed) {
      return this.reftestWin;
    }

    let reftestWin = yield this.openWindow();

    let found = this.driver.findWindow([reftestWin], () => true);
    yield this.driver.setWindowHandle(found, true);

    this.windowUtils = reftestWin.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils);
    this.reftestWin = reftestWin;
    return reftestWin;
  }

  *openWindow() {
    let reftestWin;
    yield new Promise(resolve => {
      reftestWin = this.parentWindow.openDialog("chrome://marionette/content/reftest.xul",
                                                "reftest",
                                                "chrome,dialog,height=600,width=600,all",
                                                () => resolve());
    });

    let browser = reftestWin.document.createElementNS(XUL_NS, "xul:browser");
    browser.permanentKey = {};
    browser.setAttribute("id", "browser");
    browser.setAttribute("anonid", "initialBrowser");
    browser.setAttribute("type", "content");
    browser.setAttribute("primary", "true");

    if (this.remote) {
      browser.setAttribute("remote", "true");
      browser.setAttribute("remoteType", "web");
    }
    // Make sure the browser element is exactly 600x600, no matter
    // what size our window is
    const window_style = `padding: 0px; margin: 0px; border:none;
min-width: 600px; min-height: 600px; max-width: 600px; max-height: 600px`;
    browser.setAttribute("style", window_style);

    let doc = reftestWin.document.documentElement;
    while (doc.firstChild) {
      doc.firstChild.remove();
    }
    doc.appendChild(browser);
    reftestWin.gBrowser = browser;

    return reftestWin;
  }

  abort() {
    if (this.reftestWin) {
      this.driver.close();
    }
    this.reftestWin = null;
  }

  /**
   * Run a specific reftest.
   *
   * The assumed semantics are those of web-platform-tests where
   * references form a tree and each test must meet all the conditions
   * to reach one leaf node of the tree in order for the overall test
   * to pass.
   *
   * @param {string} testUrl
   *     URL of the test itself.
   * @param {Array.<Array>} references
   *     Array representing a tree of references to try. Each item in
   *     the array represents a single reference node and has the form
   *     [referenceUrl, references, relation], where referenceUrl is a
   *     string to the url, relation is either "==" or "!=" depending on
   *     the type of reftest, and references is another array containing
   *     items of the same form, representing further comparisons treated
   *     as AND with the current item. Sibling entries are treated as OR.
   *     For example with testUrl of T:
   *       references = [[A, [[B, [], ==]], ==]]
   *       Must have T == A AND A == B to pass
   *
   *       references = [[A, [], ==], [B, [], !=]
   *       Must have T == A OR T != B
   *
   *       references = [[A, [[B, [], ==], [C, [], ==]], ==], [D, [], ]]
   *       Must have (T == A AND A == B) OR (T == A AND A == C) OR (T == D)
   * @param {string} expected
   *     Expected test outcome (e.g. PASS, FAIL).
   * @param {number} timeout
   *     Test timeout in ms
   *
   * @return {Object}
   *     Result object with fields status, message and extra.
   */
  *run(testUrl, references, expected, timeout) {

    let timeoutHandle;

    let timeoutPromise = new Promise(resolve => {
      timeoutHandle = this.parentWindow.setTimeout(() => {
        resolve({status: STATUS.TIMEOUT, message: null, extra: {}});
      }, timeout);
    });

    let testRunner = Task.spawn(function*() {
      let result;
      try {
        result = yield this.runTest(testUrl, references, expected, timeout);
      } catch (e) {
        result = {status: STATUS.ERROR, message: e.stack, extra: {}};
      }
      return result;
    }.bind(this));

    let result = yield Promise.race([testRunner, timeoutPromise]);
    this.parentWindow.clearTimeout(timeoutHandle);
    if (result.status === STATUS.TIMEOUT) {
      this.abort();
    }

    return result;
  }

  *runTest(testUrl, references, expected, timeout) {
    let win = yield this.ensureWindow();

    function toBase64(screenshot) {
      let dataURL = screenshot.canvas.toDataURL();
      return dataURL.split(",")[1];
    }

    win.innerWidth = 600;
    win.innerHeight = 600;

    let message = "";

    let screenshotData = [];

    let stack = [];
    for (let i = references.length - 1; i >= 0; i--) {
      let item = references[i];
      stack.push([testUrl, item[0], item[1], item[2]]);
    }

    let status = STATUS.FAIL;

    while (stack.length) {
      let [lhsUrl, rhsUrl, references, relation] = stack.pop();
      message += `Testing ${lhsUrl} ${relation} ${rhsUrl}\n`;

      let comparison = yield this.compareUrls(
          win, lhsUrl, rhsUrl, relation, timeout);

      function recordScreenshot() {
        let encodedLHS = toBase64(comparison.lhs);
        let encodedRHS = toBase64(comparison.rhs);
        screenshotData.push([{url: lhsUrl, screenshot: encodedLHS},
          relation,
          {url: rhsUrl, screenshot: encodedRHS}]);
      }

      if (this.screenshotMode === SCREENSHOT_MODE.always) {
        recordScreenshot();
      }

      if (comparison.passed) {
        if (references.length) {
          for (let i = references.length - 1; i >= 0; i--) {
            let item = references[i];
            stack.push([testUrl, item[0], item[1], item[2]]);
          }
        } else {
          // Reached a leaf node so all of one reference chain passed
          status = STATUS.PASS;
          if (this.screenshotMode <= SCREENSHOT_MODE.fail &&
              expected != status) {
            recordScreenshot();
          }
          break;
        }
      } else if (!stack.length) {
        // If we don't have any alternatives to try then this will be
        // the last iteration, so save the failing screenshots if required.
        let isFail = this.screenshotMode === SCREENSHOT_MODE.fail;
        let isUnexpected = this.screenshotMode === SCREENSHOT_MODE.unexpected;
        if (isFail || (isUnexpected && expected != status)) {
          recordScreenshot();
        }
      }

      // Return any reusable canvases to the pool
      let canvasPool = this.canvasCache.get(null);
      [comparison.lhs, comparison.rhs].map(screenshot => {
        if (screenshot.reuseCanvas) {
          canvasPool.push(screenshot.canvas);
        }
      });
      logger.debug(`Canvas pool is of length ${canvasPool.length}`);
    }

    let result = {status, message, extra: {}};
    if (screenshotData.length) {
      // For now the tbpl formatter only accepts one screenshot, so just
      // return the last one we took.
      let lastScreenshot = screenshotData[screenshotData.length - 1];
      result.extra.reftest_screenshots = lastScreenshot;
    }

    return result;
  }

  *compareUrls(win, lhsUrl, rhsUrl, relation, timeout) {
    logger.info(`Testing ${lhsUrl} ${relation} ${rhsUrl}`);

    // Take the reference screenshot first so that if we pause
    // we see the test rendering
    let rhs = yield this.screenshot(win, rhsUrl, timeout);
    let lhs = yield this.screenshot(win, lhsUrl, timeout);

    let maxDifferences = {};

    let differences = this.windowUtils.compareCanvases(
        lhs.canvas, rhs.canvas, maxDifferences);

    let passed;
    switch (relation) {
      case "==":
        passed = differences === 0;
        if (!passed) {
          logger.info(`Found ${differences} pixels different, ` +
              `maximum difference per channel ${maxDifferences.value}`);
        }
        break;

      case "!=":
        passed = differences !== 0;
        break;

      default:
        throw new InvalidArgumentError("Reftest operator should be '==' or '!='");
    }

    return {lhs, rhs, passed};
  }

  *screenshot(win, url, timeout) {
    let canvas = null;
    let remainingCount = this.urlCount.get(url) || 1;
    let cache = remainingCount > 1;
    logger.debug(`screenshot ${url} remainingCount: ` +
        `${remainingCount} cache: ${cache}`);
    let reuseCanvas = false;
    if (this.canvasCache.has(url)) {
      logger.debug(`screenshot ${url} taken from cache`);
      canvas = this.canvasCache.get(url);
      if (!cache) {
        this.canvasCache.delete(url);
      }
    } else {
      let canvases = this.canvasCache.get(null);
      if (canvases.length) {
        canvas = canvases.pop();
      } else {
        canvas = null;
      }
      reuseCanvas = !cache;

      let ctxInterface = win.CanvasRenderingContext2D;
      let flags = ctxInterface.DRAWWINDOW_DRAW_CARET |
          ctxInterface.DRAWWINDOW_USE_WIDGET_LAYERS |
          ctxInterface.DRAWWINDOW_DRAW_VIEW;

      logger.debug(`Starting load of ${url}`);
      let navigateOpts = {
        commandId: this.driver.listener.activeMessageId,
        pageTimeout: timeout,
      };
      if (this.lastURL === url) {
        logger.debug(`Refreshing page`);
        yield this.driver.listener.refresh(navigateOpts);
      } else {
        navigateOpts.url = url;
        navigateOpts.loadEventExpected = false;
        yield this.driver.listener.get(navigateOpts);
        this.lastURL = url;
      }

      this.driver.curBrowser.contentBrowser.focus();
      yield this.driver.listener.reftestWait(url, this.remote);

      canvas = capture.canvas(
          win,
          0,  // left
          0,  // top
          win.innerWidth,
          win.innerHeight,
          {canvas, flags});
    }
    if (cache) {
      this.canvasCache.set(url, canvas);
    }
    this.urlCount.set(url, remainingCount - 1);
    return {canvas, reuseCanvas};
  }
};
