/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["reftest"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",

  AppInfo: "chrome://marionette/content/appinfo.js",
  assert: "chrome://marionette/content/assert.js",
  capture: "chrome://marionette/content/capture.js",
  error: "chrome://marionette/content/error.js",
  Log: "chrome://marionette/content/log.js",
  navigate: "chrome://marionette/content/navigate.js",
  print: "chrome://marionette/content/print.js",
  windowManager: "chrome://marionette/content/window-manager.js",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

const XHTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

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

const DEFAULT_REFTEST_WIDTH = 600;
const DEFAULT_REFTEST_HEIGHT = 600;

// reftest-print page dimensions in cm
const CM_PER_INCH = 2.54;
const DEFAULT_PAGE_WIDTH = 5 * CM_PER_INCH;
const DEFAULT_PAGE_HEIGHT = 3 * CM_PER_INCH;
const DEFAULT_PAGE_MARGIN = 0.5 * CM_PER_INCH;

// CSS 96 pixels per inch, compared to pdf.js default 72 pixels per inch
const DEFAULT_PDF_RESOLUTION = 96 / 72;

/**
 * Implements an fast runner for web-platform-tests format reftests
 * c.f. http://web-platform-tests.org/writing-tests/reftests.html.
 *
 * @namespace
 */
this.reftest = {};

/**
 * @memberof reftest
 * @class Runner
 */
reftest.Runner = class {
  constructor(driver) {
    this.driver = driver;
    this.canvasCache = new DefaultMap(undefined, () => new Map([[null, []]]));
    this.isPrint = null;
    this.windowUtils = null;
    this.lastURL = null;
    this.useRemoteTabs = AppInfo.browserTabsRemoteAutostart;
    this.useRemoteSubframes = AppInfo.fissionAutostart;
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
  setup(urlCount, screenshotMode, isPrint = false) {
    this.isPrint = isPrint;

    assert.open(this.driver.getBrowsingContext({ top: true }));
    this.parentWindow = this.driver.getCurrentWindow();

    this.screenshotMode =
      SCREENSHOT_MODE[screenshotMode] || SCREENSHOT_MODE.unexpected;

    this.urlCount = Object.keys(urlCount || {}).reduce(
      (map, key) => map.set(key, urlCount[key]),
      new Map()
    );

    if (isPrint) {
      this.loadPdfJs();
    }

    ChromeUtils.registerWindowActor("MarionetteReftest", {
      kind: "JSWindowActor",
      parent: {
        moduleURI:
          "chrome://marionette/content/actors/MarionetteReftestParent.jsm",
      },
      child: {
        moduleURI:
          "chrome://marionette/content/actors/MarionetteReftestChild.jsm",
        events: {
          load: { mozSystemGroup: true, capture: true },
        },
      },
      allFrames: true,
    });
  }

  /**
   * Cleanup the environment once the reftest is finished.
   */
  teardown() {
    // Abort the current test if any.
    this.abort();

    // Unregister the JSWindowActors.
    ChromeUtils.unregisterWindowActor("MarionetteReftest");
  }

  async ensureWindow(timeout, width, height) {
    logger.debug(`ensuring we have a window ${width}x${height}`);

    if (this.reftestWin && !this.reftestWin.closed) {
      let browserRect = this.reftestWin.gBrowser.getBoundingClientRect();
      if (browserRect.width === width && browserRect.height === height) {
        return this.reftestWin;
      }
      logger.debug(`current: ${browserRect.width}x${browserRect.height}`);
    }

    let reftestWin;
    if (AppInfo.isAndroid) {
      logger.debug("Using current window");
      reftestWin = this.parentWindow;
      await navigate.waitForNavigationCompleted(this.driver, () => {
        const browsingContext = this.driver.getBrowsingContext();
        navigate.navigateTo(browsingContext, "about:blank");
      });
    } else {
      logger.debug("Using separate window");
      if (this.reftestWin && !this.reftestWin.closed) {
        this.reftestWin.close();
      }
      reftestWin = await this.openWindow(width, height);
    }

    this.setupWindow(reftestWin, width, height);
    this.windowUtils = reftestWin.windowUtils;
    this.reftestWin = reftestWin;

    let windowHandle = windowManager.getWindowProperties(reftestWin);
    await this.driver.setWindowHandle(windowHandle, true);

    const url = await this.driver._getCurrentURL();
    this.lastURL = url.href;
    logger.debug(`loaded initial URL: ${this.lastURL}`);

    let browserRect = reftestWin.gBrowser.getBoundingClientRect();
    logger.debug(`new: ${browserRect.width}x${browserRect.height}`);

    return reftestWin;
  }

  async openWindow(width, height) {
    assert.positiveInteger(width);
    assert.positiveInteger(height);

    let reftestWin = this.parentWindow.open(
      "chrome://marionette/content/reftest.xhtml",
      "reftest",
      `chrome,height=${height},width=${width}`
    );

    await new Promise(resolve => {
      reftestWin.addEventListener("load", resolve, { once: true });
    });
    return reftestWin;
  }

  setupWindow(reftestWin, width, height) {
    let browser;
    if (AppInfo.isAndroid) {
      browser = reftestWin.document.getElementsByTagName("browser")[0];
      browser.setAttribute("remote", "false");
    } else {
      browser = reftestWin.document.createElementNS(XUL_NS, "xul:browser");
      browser.permanentKey = {};
      browser.setAttribute("id", "browser");
      browser.setAttribute("type", "content");
      browser.setAttribute("primary", "true");
      browser.setAttribute("remote", this.useRemoteTabs ? "true" : "false");
    }
    // Make sure the browser element is exactly the right size, no matter
    // what size our window is
    const windowStyle = `padding: 0px; margin: 0px; border:none;
min-width: ${width}px; min-height: ${height}px;
max-width: ${width}px; max-height: ${height}px`;
    browser.setAttribute("style", windowStyle);

    if (!AppInfo.isAndroid) {
      let doc = reftestWin.document.documentElement;
      while (doc.firstChild) {
        doc.firstChild.remove();
      }
      doc.appendChild(browser);
    }
    if (reftestWin.BrowserApp) {
      reftestWin.BrowserApp = browser;
    }
    reftestWin.gBrowser = browser;
    return reftestWin;
  }

  async abort() {
    if (this.reftestWin && this.reftestWin != this.parentWindow) {
      this.driver.closeChromeWindow();
      let parentHandle = windowManager.getWindowProperties(this.parentWindow);
      await this.driver.setWindowHandle(parentHandle);
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
   *     Array representing a tree of references to try.
   *
   *     Each item in the array represents a single reference node and
   *     has the form <code>[referenceUrl, references, relation]</code>,
   *     where <var>referenceUrl</var> is a string to the URL, relation
   *     is either <code>==</code> or <code>!=</code> depending on the
   *     type of reftest, and references is another array containing
   *     items of the same form, representing further comparisons treated
   *     as AND with the current item. Sibling entries are treated as OR.
   *
   *     For example with testUrl of T:
   *
   *     <pre><code>
   *       references = [[A, [[B, [], ==]], ==]]
   *       Must have T == A AND A == B to pass
   *
   *       references = [[A, [], ==], [B, [], !=]
   *       Must have T == A OR T != B
   *
   *       references = [[A, [[B, [], ==], [C, [], ==]], ==], [D, [], ]]
   *       Must have (T == A AND A == B) OR (T == A AND A == C) OR (T == D)
   *     </code></pre>
   *
   * @param {string} expected
   *     Expected test outcome (e.g. <tt>PASS</tt>, <tt>FAIL</tt>).
   * @param {number} timeout
   *     Test timeout in milliseconds.
   *
   * @return {Object}
   *     Result object with fields status, message and extra.
   */
  async run(
    testUrl,
    references,
    expected,
    timeout,
    pageRanges = {},
    width = DEFAULT_REFTEST_WIDTH,
    height = DEFAULT_REFTEST_HEIGHT
  ) {
    let timeoutHandle;

    let timeoutPromise = new Promise(resolve => {
      timeoutHandle = this.parentWindow.setTimeout(() => {
        resolve({ status: STATUS.TIMEOUT, message: null, extra: {} });
      }, timeout);
    });

    let testRunner = (async () => {
      let result;
      try {
        result = await this.runTest(
          testUrl,
          references,
          expected,
          timeout,
          pageRanges,
          width,
          height
        );
      } catch (e) {
        result = {
          status: STATUS.ERROR,
          message: String(e),
          stack: e.stack,
          extra: {},
        };
      }
      return result;
    })();

    let result = await Promise.race([testRunner, timeoutPromise]);
    this.parentWindow.clearTimeout(timeoutHandle);
    if (result.status === STATUS.TIMEOUT) {
      await this.abort();
    }

    return result;
  }

  async runTest(
    testUrl,
    references,
    expected,
    timeout,
    pageRanges,
    width,
    height
  ) {
    let win = await this.ensureWindow(timeout, width, height);

    function toBase64(screenshot) {
      let dataURL = screenshot.canvas.toDataURL();
      return dataURL.split(",")[1];
    }

    let result = {
      status: STATUS.FAIL,
      message: "",
      stack: null,
      extra: {},
    };

    let screenshotData = [];

    let stack = [];
    for (let i = references.length - 1; i >= 0; i--) {
      let item = references[i];
      stack.push([testUrl, ...item]);
    }

    let done = false;

    while (stack.length && !done) {
      let [lhsUrl, rhsUrl, references, relation, extras = {}] = stack.pop();
      result.message += `Testing ${lhsUrl} ${relation} ${rhsUrl}\n`;

      let comparison;
      try {
        comparison = await this.compareUrls(
          win,
          lhsUrl,
          rhsUrl,
          relation,
          timeout,
          pageRanges,
          extras
        );
      } catch (e) {
        comparison = {
          lhs: null,
          rhs: null,
          passed: false,
          error: e,
          msg: null,
        };
      }
      if (comparison.msg) {
        result.message += `${comparison.msg}\n`;
      }
      if (comparison.error !== null) {
        result.status = STATUS.ERROR;
        result.message += String(comparison.error);
        result.stack = comparison.error.stack;
      }

      function recordScreenshot() {
        let encodedLHS = comparison.lhs ? toBase64(comparison.lhs) : "";
        let encodedRHS = comparison.rhs ? toBase64(comparison.rhs) : "";
        screenshotData.push([
          { url: lhsUrl, screenshot: encodedLHS },
          relation,
          { url: rhsUrl, screenshot: encodedRHS },
        ]);
      }

      if (this.screenshotMode === SCREENSHOT_MODE.always) {
        recordScreenshot();
      }

      if (comparison.passed) {
        if (references.length) {
          for (let i = references.length - 1; i >= 0; i--) {
            let item = references[i];
            stack.push([rhsUrl, ...item]);
          }
        } else {
          // Reached a leaf node so all of one reference chain passed
          result.status = STATUS.PASS;
          if (
            this.screenshotMode <= SCREENSHOT_MODE.fail &&
            expected != result.status
          ) {
            recordScreenshot();
          }
          done = true;
        }
      } else if (!stack.length || result.status == STATUS.ERROR) {
        // If we don't have any alternatives to try then this will be
        // the last iteration, so save the failing screenshots if required.
        let isFail = this.screenshotMode === SCREENSHOT_MODE.fail;
        let isUnexpected = this.screenshotMode === SCREENSHOT_MODE.unexpected;
        if (isFail || (isUnexpected && expected != result.status)) {
          recordScreenshot();
        }
      }

      // Return any reusable canvases to the pool
      let cacheKey = width + "x" + height;
      let canvasPool = this.canvasCache.get(cacheKey).get(null);
      [comparison.lhs, comparison.rhs].map(screenshot => {
        if (screenshot !== null && screenshot.reuseCanvas) {
          canvasPool.push(screenshot.canvas);
        }
      });
      logger.debug(
        `Canvas pool (${cacheKey}) is of length ${canvasPool.length}`
      );
    }

    if (screenshotData.length) {
      // For now the tbpl formatter only accepts one screenshot, so just
      // return the last one we took.
      let lastScreenshot = screenshotData[screenshotData.length - 1];
      // eslint-disable-next-line camelcase
      result.extra.reftest_screenshots = lastScreenshot;
    }

    return result;
  }

  async compareUrls(
    win,
    lhsUrl,
    rhsUrl,
    relation,
    timeout,
    pageRanges,
    extras
  ) {
    logger.info(`Testing ${lhsUrl} ${relation} ${rhsUrl}`);

    if (relation !== "==" && relation != "!=") {
      throw new error.InvalidArgumentError(
        "Reftest operator should be '==' or '!='"
      );
    }

    let lhsIter, lhsCount, rhsIter, rhsCount;
    if (!this.isPrint) {
      // Take the reference screenshot first so that if we pause
      // we see the test rendering
      rhsIter = [await this.screenshot(win, rhsUrl, timeout)].values();
      lhsIter = [await this.screenshot(win, lhsUrl, timeout)].values();
      lhsCount = rhsCount = 1;
    } else {
      [rhsIter, rhsCount] = await this.screenshotPaginated(
        win,
        rhsUrl,
        timeout,
        pageRanges
      );
      [lhsIter, lhsCount] = await this.screenshotPaginated(
        win,
        lhsUrl,
        timeout,
        pageRanges
      );
    }

    let passed = null;
    let error = null;
    let pixelsDifferent = null;
    let maxDifferences = {};
    let msg = null;

    if (lhsCount != rhsCount) {
      passed = relation == "!=";
      if (!passed) {
        msg = `Got different numbers of pages; test has ${lhsCount}, ref has ${rhsCount}`;
      }
    }

    let lhs = null;
    let rhs = null;
    logger.debug(`Comparing ${lhsCount} pages`);
    if (passed === null) {
      for (let i = 0; i < lhsCount; i++) {
        lhs = (await lhsIter.next()).value;
        rhs = (await rhsIter.next()).value;
        logger.debug(
          `lhs canvas size ${lhs.canvas.width}x${lhs.canvas.height}`
        );
        logger.debug(
          `rhs canvas size ${rhs.canvas.width}x${rhs.canvas.height}`
        );
        try {
          pixelsDifferent = this.windowUtils.compareCanvases(
            lhs.canvas,
            rhs.canvas,
            maxDifferences
          );
        } catch (e) {
          error = e;
          passed = false;
          break;
        }

        let areEqual = this.isAcceptableDifference(
          maxDifferences.value,
          pixelsDifferent,
          extras.fuzzy
        );
        logger.debug(
          `Page ${i + 1} maxDifferences: ${maxDifferences.value} ` +
            `pixelsDifferent: ${pixelsDifferent}`
        );
        logger.debug(
          `Page ${i + 1} ${areEqual ? "compare equal" : "compare unequal"}`
        );
        if (!areEqual) {
          if (relation == "==") {
            passed = false;
            msg =
              `Found ${pixelsDifferent} pixels different, ` +
              `maximum difference per channel ${maxDifferences.value}`;
            if (this.isPrint) {
              msg += ` on page ${i + 1}`;
            }
          } else {
            passed = true;
          }
          break;
        }
      }
    }

    // If passed isn't set we got to the end without finding differences
    if (passed === null) {
      if (relation == "==") {
        passed = true;
      } else {
        msg = `mismatch reftest has no differences`;
        passed = false;
      }
    }
    return { lhs, rhs, passed, error, msg };
  }

  isAcceptableDifference(maxDifference, pixelsDifferent, allowed) {
    if (!allowed) {
      logger.info(`No differences allowed`);
      return pixelsDifferent === 0;
    }
    let [allowedDiff, allowedPixels] = allowed;
    logger.info(
      `Allowed ${allowedPixels.join("-")} pixels different, ` +
        `maximum difference per channel ${allowedDiff.join("-")}`
    );
    return (
      (pixelsDifferent === 0 && allowedPixels[0] == 0) ||
      (maxDifference === 0 && allowedDiff[0] == 0) ||
      (maxDifference >= allowedDiff[0] &&
        maxDifference <= allowedDiff[1] &&
        (pixelsDifferent >= allowedPixels[0] ||
          pixelsDifferent <= allowedPixels[1]))
    );
  }

  ensureFocus(win) {
    const focusManager = Services.focus;
    if (focusManager.activeWindow != win) {
      win.focus();
    }
    this.driver.curBrowser.contentBrowser.focus();
  }

  updateBrowserRemotenessByURL(browser, url) {
    // We don't use remote tabs on Android.
    if (AppInfo.isAndroid) {
      return;
    }
    let oa = E10SUtils.predictOriginAttributes({ browser });
    let remoteType = E10SUtils.getRemoteTypeForURI(
      url,
      this.useRemoteTabs,
      this.useRemoteSubframes,
      E10SUtils.DEFAULT_REMOTE_TYPE,
      null,
      oa
    );

    // Only re-construct the browser if its remote type needs to change.
    if (browser.remoteType !== remoteType) {
      if (remoteType === E10SUtils.NOT_REMOTE) {
        browser.removeAttribute("remote");
        browser.removeAttribute("remoteType");
      } else {
        browser.setAttribute("remote", "true");
        browser.setAttribute("remoteType", remoteType);
      }

      browser.changeRemoteness({ remoteType });
      browser.construct();
    }
  }

  async loadTestUrl(win, url, timeout) {
    const browsingContext = this.driver.getBrowsingContext({ top: true });

    logger.debug(`Starting load of ${url}`);
    if (this.lastURL === url) {
      logger.debug(`Refreshing page`);
      await navigate.waitForNavigationCompleted(this.driver, () => {
        navigate.refresh(browsingContext);
      });
    } else {
      // HACK: DocumentLoadListener currently doesn't know how to
      // process-switch loads in a non-tabbed <browser>. We need to manually
      // set the browser's remote type in order to ensure that the load
      // happens in the correct process.
      //
      // See bug 1636169.
      this.updateBrowserRemotenessByURL(win.gBrowser, url);
      navigate.navigateTo(browsingContext, url);

      this.lastURL = url;
    }

    this.ensureFocus(win);

    // TODO: Move all the wait logic into the parent process (bug 1669787)
    let isReftestReady = false;
    while (!isReftestReady) {
      // Note: We cannot compare the URL here. Before the navigation is complete
      // currentWindowGlobal.documentURI.spec will still point to the old URL.
      const actor = browsingContext.currentWindowGlobal.getActor(
        "MarionetteReftest"
      );
      isReftestReady = await actor.reftestWait(url, this.useRemoteTabs);
    }
  }

  async screenshot(win, url, timeout) {
    // On windows the above doesn't *actually* set the window to be the
    // reftest size; but *does* set the content area to be the right size;
    // the window is given some extra borders that aren't explicable from CSS
    let browserRect = win.gBrowser.getBoundingClientRect();
    let canvas = null;
    let remainingCount = this.urlCount.get(url) || 1;
    let cache = remainingCount > 1;
    let cacheKey = browserRect.width + "x" + browserRect.height;
    logger.debug(
      `screenshot ${url} remainingCount: ` +
        `${remainingCount} cache: ${cache} cacheKey: ${cacheKey}`
    );
    let reuseCanvas = false;
    let sizedCache = this.canvasCache.get(cacheKey);
    if (sizedCache.has(url)) {
      logger.debug(`screenshot ${url} taken from cache`);
      canvas = sizedCache.get(url);
      if (!cache) {
        sizedCache.delete(url);
      }
    } else {
      let canvasPool = sizedCache.get(null);
      if (canvasPool.length) {
        logger.debug("reusing canvas from canvas pool");
        canvas = canvasPool.pop();
      } else {
        logger.debug("using new canvas");
        canvas = null;
      }
      reuseCanvas = !cache;

      let ctxInterface = win.CanvasRenderingContext2D;
      let flags =
        ctxInterface.DRAWWINDOW_DRAW_CARET |
        ctxInterface.DRAWWINDOW_DRAW_VIEW |
        ctxInterface.DRAWWINDOW_USE_WIDGET_LAYERS;

      if (
        !(
          0 <= browserRect.left &&
          0 <= browserRect.top &&
          win.innerWidth >= browserRect.width &&
          win.innerHeight >= browserRect.height
        )
      ) {
        logger.error(`Invalid window dimensions:
browserRect.left: ${browserRect.left}
browserRect.top: ${browserRect.top}
win.innerWidth: ${win.innerWidth}
browserRect.width: ${browserRect.width}
win.innerHeight: ${win.innerHeight}
browserRect.height: ${browserRect.height}`);
        throw new Error("Window has incorrect dimensions");
      }

      url = new URL(url).href; // normalize the URL

      await this.loadTestUrl(win, url, timeout);

      canvas = await capture.canvas(
        win,
        win.docShell.browsingContext,
        0, // left
        0, // top
        browserRect.width,
        browserRect.height,
        { canvas, flags, readback: true }
      );
    }
    if (
      canvas.width !== browserRect.width ||
      canvas.height !== browserRect.height
    ) {
      logger.warn(
        `Canvas dimensions changed to ${canvas.width}x${canvas.height}`
      );
      reuseCanvas = false;
      cache = false;
    }
    if (cache) {
      sizedCache.set(url, canvas);
    }
    this.urlCount.set(url, remainingCount - 1);
    return { canvas, reuseCanvas };
  }

  async screenshotPaginated(win, url, timeout, pageRanges) {
    url = new URL(url).href; // normalize the URL
    await this.loadTestUrl(win, url, timeout);

    const [width, height] = [DEFAULT_PAGE_WIDTH, DEFAULT_PAGE_HEIGHT];
    const margin = DEFAULT_PAGE_MARGIN;
    const settings = print.addDefaultSettings({
      page: {
        width,
        height,
      },
      margin: {
        left: margin,
        right: margin,
        top: margin,
        bottom: margin,
      },
      shrinkToFit: false,
      printBackground: true,
    });

    const filePath = await print.printToFile(win.gBrowser, settings);

    const fp = await OS.File.open(filePath, { read: true });
    try {
      const pdf = await this.loadPdf(url, fp);
      let pages = this.getPages(pageRanges, url, pdf.numPages);
      return [this.renderPages(pdf, pages), pages.size];
    } finally {
      fp.close();
      await OS.File.remove(filePath);
    }
  }

  async loadPdfJs() {
    // Ensure pdf.js is loaded in the opener window
    await new Promise((resolve, reject) => {
      const doc = this.parentWindow.document;
      const script = doc.createElement("script");
      script.src = "resource://pdf.js/build/pdf.js";
      script.onload = resolve;
      script.onerror = () => reject(new Error("pdfjs load failed"));
      doc.documentElement.appendChild(script);
    });
    this.parentWindow.pdfjsLib.GlobalWorkerOptions.workerSrc =
      "resource://pdf.js/build/pdf.worker.js";
  }

  async loadPdf(url, fp) {
    const data = await fp.read();
    return this.parentWindow.pdfjsLib.getDocument({ data }).promise;
  }

  async *renderPages(pdf, pages) {
    let canvas = null;
    for (let pageNumber = 1; pageNumber <= pdf.numPages; pageNumber++) {
      if (!pages.has(pageNumber)) {
        logger.info(`Skipping page ${pageNumber}/${pdf.numPages}`);
        continue;
      }
      logger.info(`Rendering page ${pageNumber}/${pdf.numPages}`);
      let page = await pdf.getPage(pageNumber);
      let viewport = page.getViewport({ scale: DEFAULT_PDF_RESOLUTION });
      // Prepare canvas using PDF page dimensions
      if (canvas === null) {
        canvas = this.parentWindow.document.createElementNS(XHTML_NS, "canvas");
        canvas.height = viewport.height;
        canvas.width = viewport.width;
      }

      // Render PDF page into canvas context
      let context = canvas.getContext("2d");
      let renderContext = {
        canvasContext: context,
        viewport,
      };
      await page.render(renderContext).promise;
      yield { canvas, reuseCanvas: false };
    }
  }

  getPages(pageRanges, url, totalPages) {
    // Extract test id from URL without parsing
    let afterHost = url.slice(url.indexOf(":") + 3);
    afterHost = afterHost.slice(afterHost.indexOf("/"));
    const ranges = pageRanges[afterHost];
    let rv = new Set();

    if (!ranges) {
      for (let i = 1; i <= totalPages; i++) {
        rv.add(i);
      }
      return rv;
    }

    for (let rangePart of ranges) {
      if (rangePart.length === 1) {
        rv.add(rangePart[0]);
      } else {
        if (rangePart.length !== 2) {
          throw new Error(
            `Page ranges must be <int> or <int> '-' <int>, got ${rangePart}`
          );
        }
        let [lower, upper] = rangePart;
        if (lower === null) {
          lower = 1;
        }
        if (upper === null) {
          upper = totalPages;
        }
        for (let i = lower; i <= upper; i++) {
          rv.add(i);
        }
      }
    }
    return rv;
  }
};

class DefaultMap extends Map {
  constructor(iterable, defaultFactory) {
    super(iterable);
    this.defaultFactory = defaultFactory;
  }

  get(key) {
    if (this.has(key)) {
      return super.get(key);
    }

    let v = this.defaultFactory();
    this.set(key, v);
    return v;
  }
}
