/* -*- js-indent-level: 2; tab-width: 2; indent-tabs-mode: nil -*- */

/* eslint-env mozilla/browser-window */
/* import-globals-from chrome-harness.js */
/* import-globals-from mochitest-e10s-utils.js */

// Test timeout (seconds)
var gTimeoutSeconds = 45;
var gConfig;
var gSaveInstrumentationData = null;

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "ContentSearch",
  "resource:///modules/ContentSearch.jsm");

const SIMPLETEST_OVERRIDES =
  ["ok", "is", "isnot", "todo", "todo_is", "todo_isnot", "info", "expectAssertions", "requestCompleteLog"];

// non-android is bootstrapped by marionette
if (Services.appinfo.OS == "Android") {
  window.addEventListener("load", function() {
    window.addEventListener("MozAfterPaint", function() {
      setTimeout(testInit, 0);
    }, {once: true});
  }, {once: true});
} else {
  setTimeout(testInit, 0);
}

var TabDestroyObserver = {
  outstanding: new Set(),
  promiseResolver: null,

  init() {
    Services.obs.addObserver(this, "message-manager-close");
    Services.obs.addObserver(this, "message-manager-disconnect");
  },

  destroy() {
    Services.obs.removeObserver(this, "message-manager-close");
    Services.obs.removeObserver(this, "message-manager-disconnect");
  },

  observe(subject, topic, data) {
    if (topic == "message-manager-close") {
      this.outstanding.add(subject);
    } else if (topic == "message-manager-disconnect") {
      this.outstanding.delete(subject);
      if (!this.outstanding.size && this.promiseResolver) {
        this.promiseResolver();
      }
    }
  },

  wait() {
    if (!this.outstanding.size) {
      return Promise.resolve();
    }

    return new Promise((resolve) => {
      this.promiseResolver = resolve;
    });
  },
};

function testInit() {
  gConfig = readConfig();
  if (gConfig.testRoot == "browser") {
    // Make sure to launch the test harness for the first opened window only
    var prefs = Services.prefs;
    if (prefs.prefHasUserValue("testing.browserTestHarness.running"))
      return;

    prefs.setBoolPref("testing.browserTestHarness.running", true);

    if (prefs.prefHasUserValue("testing.browserTestHarness.timeout"))
      gTimeoutSeconds = prefs.getIntPref("testing.browserTestHarness.timeout");

    var sstring = Cc["@mozilla.org/supports-string;1"].
                  createInstance(Ci.nsISupportsString);
    sstring.data = location.search;

    Services.ww.openWindow(window, "chrome://mochikit/content/browser-harness.xul", "browserTest",
                           "chrome,centerscreen,dialog=no,resizable,titlebar,toolbar=no,width=800,height=600", sstring);
  } else {
    // This code allows us to redirect without requiring specialpowers for chrome and a11y tests.
    let messageHandler = function(m) {
      // eslint-disable-next-line no-undef
      messageManager.removeMessageListener("chromeEvent", messageHandler);
      var url = m.json.data;

      // Window is the [ChromeWindow] for messageManager, so we need content.window
      // Currently chrome tests are run in a content window instead of a ChromeWindow
      // eslint-disable-next-line no-undef
      var webNav = content.window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation);
      webNav.loadURI(url, null, null, null, null);
    };

    var listener = 'data:,function doLoad(e) { var data=e.detail&&e.detail.data;removeEventListener("contentEvent", function (e) { doLoad(e); }, false, true);sendAsyncMessage("chromeEvent", {"data":data}); };addEventListener("contentEvent", function (e) { doLoad(e); }, false, true);';
    // eslint-disable-next-line no-undef
    messageManager.addMessageListener("chromeEvent", messageHandler);
    // eslint-disable-next-line no-undef
    messageManager.loadFrameScript(listener, true);
  }
  if (gConfig.e10s) {
    e10s_init();

    let processCount = prefs.getIntPref("dom.ipc.processCount", 1);
    if (processCount > 1) {
      // Currently starting a content process is slow, to aviod timeouts, let's
      // keep alive content processes.
      prefs.setIntPref("dom.ipc.keepProcessesAlive.web", processCount);
    }

    Services.mm.loadFrameScript("chrome://mochikit/content/shutdown-leaks-collector.js", true);
  } else {
    // In non-e10s, only run the ShutdownLeaksCollector in the parent process.
    ChromeUtils.import("chrome://mochikit/content/ShutdownLeaksCollector.jsm");
  }

  Services.mm.loadFrameScript("chrome://mochikit/content/tests/SimpleTest/AsyncUtilsContent.js", true);

  var testSuite = Cc["@mozilla.org/process/environment;1"].
                    getService(Ci.nsIEnvironment).
                    get("TEST_SUITE");
  if (testSuite == "browser-chrome-instrumentation") {
    takeInstrumentation();
  }
}

function takeInstrumentation() {

  let instrumentData = {
    elements: {}
  };

  function pad(str, length) {
    if (str.length >= length)
      return str;

    return str + " ".repeat(length - str.length);
  }

  function byCount(a, b) {
    return b[1] - a[1];
  }

  function getSummaryText() {
    let summary = [];
    let allData = {};
    for (let selector of Object.keys(instrumentData.elements)) {
      allData[selector] = instrumentData.elements[selector];
    }

    let selectors = Object.keys(allData);
    let elements = selectors.map(s => allData[s]);

    let namespaceMap = new Map();
    let bindingMap = new Map();

    for (let element of elements) {
      if (!bindingMap.has(element.binding)) {
        bindingMap.set(element.binding, 1);
      } else {
        bindingMap.set(element.binding, bindingMap.get(element.binding) + 1);
      }

      if (!namespaceMap.has(element.namespaceURI)) {
        namespaceMap.set(element.namespaceURI, new Map());
      }

      let localNameMap = namespaceMap.get(element.namespaceURI);
      if (!localNameMap.has(element.localName)) {
        localNameMap.set(element.localName, 1);
      } else {
        localNameMap.set(element.localName, localNameMap.get(element.localName) + 1);
      }
    }

    for (let [namespace, localNameMap] of namespaceMap) {
      summary.push(`Elements in namespace ${namespace}`);

      let entries = Array.from(localNameMap);
      entries.sort(byCount);
      for (let entry of entries) {
        summary.push(`  ${pad(entry[1] + "", 5)} ${entry[0]}`);
      }
    }

    summary.push("XBL bindings");
    let bindings = Array.from(bindingMap);
    bindings.sort(byCount);
    let bindingsJSON = {};
    for (let binding of bindings) {
      summary.push(`  ${pad(binding[1] + "", 5)} ${binding[0]}`);
      if (binding[0]) {
        bindingsJSON[binding[0].split("#")[1].split('"')[0]] = binding[1];
      }
    }

    summary.push("XBL bindings as JSON");
    summary.push(JSON.stringify(bindingsJSON, null, 2));

    return summary.join("\n");
  }

  // Saves instrumantation data
  function saveData() {
    let path = Cc["@mozilla.org/process/environment;1"].
               getService(Ci.nsIEnvironment).
               get("MOZ_UPLOAD_DIR");
    let encoder = new TextEncoder();

    let instrumentPath = OS.Path.join(path, "xulinstrument.txt");
    OS.File.writeAtomic(instrumentPath, encoder.encode(JSON.stringify(instrumentData, null, 2)));

    let summaryPath = OS.Path.join(path, "xulsummary.txt");
    OS.File.writeAtomic(summaryPath, encoder.encode(getSummaryText()));
  }

  // An iterator over an element and its ancestors
  function* elementPath(element) {
    yield element;
    while ((element = element.parentNode) && (element instanceof Element)) {
      yield element;
    }
  }

  // Returns the information we care about for an element
  function getElementInfo(element) {
    let style = element.ownerGlobal.getComputedStyle(element);
    let binding = style && style.getPropertyValue("-moz-binding");

    return {
      namespaceURI: element.namespaceURI,
      localName: element.localName,
      binding: (binding && binding != "none") ? binding : null,
    };
  }

  // The selector for just this element
  function immediateSelector(element) {
    if (element.localName == "notificationbox" && element.parentNode &&
        element.parentNode.classList.contains("tabbrowser-tabpanels")) {
      // Don't do a full selector for a tabpanel's notificationbox
      return element.localName;
    }

    if (element.localName == "tab" && element.classList.contains("tabbrowser-tab")) {
      // Don't do a full selector for a tab
      return element.localName;
    }

    if (element.id) {
      return `#${element.id}`;
    }

    let selector = element.localName;

    if (element.classList.length) {
      selector += `.${Array.from(element.classList).join(".")}`;
    }

    for (let attr of ["src", "label"]) {
      if (element.hasAttribute(attr)) {
        selector += `[${attr}=${JSON.stringify(element.getAttribute(attr))}]`;
      }
    }

    return selector;
  }

  // The selector chain for the element
  function elementSelector(element) {
    return Array.from(elementPath(element)).reverse().map(immediateSelector).join(" > ");
  }

  // An iterator over all elements in the window
  function* windowElements(win) {
    yield* elementDescendants(win.document.documentElement);
  }

  // An iterator over an element and all of its descendants
  function* elementDescendants(element) {
    let walker = Cc["@mozilla.org/inspector/deep-tree-walker;1"].
                 createInstance(Ci.inIDeepTreeWalker);
    walker.showAnonymousContent = true;
    walker.showSubDocuments = false;
    walker.showDocumentsAsNodes = false;
    walker.init(element, NodeFilter.SHOW_ELEMENT);

    yield element;
    while (walker.nextNode()) {
      if (walker.currentNode instanceof Element) {
        yield walker.currentNode;
      }
    }
  }

  // Checks if we've seen an element and if not adds it to the instrumentation data
  function instrumentElement(element) {
    if (element.__instrumentSeen) {
      return;
    }

    let selector = elementSelector(element);
    element.__instrumentSeen = true;

    if (selector in instrumentData.elements) {
      return;
    }

    instrumentData.elements[selector] = getElementInfo(element);
  }

  // Instruments every element in a window
  function scanWindow(win) {
    Array.from(windowElements(win)).forEach(instrumentElement);
  }

  // Instruments every element in an element's descendants
  function scanElement(element) {
    Array.from(elementDescendants(element)).forEach(instrumentElement);
  }

  function handleMutation(mutation) {
    if (mutation.type != "childList") {
      return;
    }

    for (let node of mutation.addedNodes) {
      if (node instanceof Element) {
        scanElement(node);
      }
    }
  }
  // Watches a window for new elements to instrument
  function observeWindow(win) {
    let observer = new MutationObserver((mutations) => {
      mutations.forEach(handleMutation);
    });

    observer.observe(win.document, {
      childList: true,
      subtree: true,
    });

    win.addEventListener("unload", () => {
      observer.takeRecords().forEach(handleMutation);
    }, { once: true });
  }

  scanWindow(window);
  observeWindow(window);
  gSaveInstrumentationData = saveData;

  Services.ww.registerNotification((win, topic, data) => {
    if (topic != "domwindowopened") {
      return;
    }

    win.addEventListener("load", () => {
      if (win.location.href != "chrome://browser/content/browser.xul") {
        return;
      }

      scanWindow(win);
      observeWindow(win);
    }, { once: true });
  });
}

function isGenerator(value) {
  return value && typeof value === "object" && typeof value.next === "function";
}

function Tester(aTests, structuredLogger, aCallback) {
  this.structuredLogger = structuredLogger;
  this.tests = aTests;
  this.callback = aCallback;

  this._scriptLoader = Services.scriptloader;
  this.EventUtils = {};
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", this.EventUtils);

  // In order to allow existing tests to continue using unsafe CPOWs
  // with EventUtils, we need to load a separate copy into a sandbox
  // which has unsafe CPOW usage whitelisted.
  this.cpowSandbox = Cu.Sandbox(window, {sandboxPrototype: window});
  Cu.permitCPOWsInScope(this.cpowSandbox);

  this.cpowEventUtils = new this.cpowSandbox.Object();
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", this.cpowEventUtils);

  var simpleTestScope = {};
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/specialpowersAPI.js", simpleTestScope);
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/SpecialPowersObserverAPI.js", simpleTestScope);
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/ChromePowers.js", simpleTestScope);
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/SimpleTest.js", simpleTestScope);
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/MemoryStats.js", simpleTestScope);
  this._scriptLoader.loadSubScript("chrome://mochikit/content/chrome-harness.js", simpleTestScope);
  this.SimpleTest = simpleTestScope.SimpleTest;

  var extensionUtilsScope = {
    registerCleanupFunction: (fn) => {
      this.currentTest.scope.registerCleanupFunction(fn);
    },
  };
  extensionUtilsScope.SimpleTest = this.SimpleTest;
  this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/ExtensionTestUtils.js", extensionUtilsScope);
  this.ExtensionTestUtils = extensionUtilsScope.ExtensionTestUtils;

  this.SimpleTest.harnessParameters = gConfig;

  this.MemoryStats = simpleTestScope.MemoryStats;
  this.ContentTask = ChromeUtils.import("resource://testing-common/ContentTask.jsm", null).ContentTask;
  this.BrowserTestUtils = ChromeUtils.import("resource://testing-common/BrowserTestUtils.jsm", null).BrowserTestUtils;
  this.TestUtils = ChromeUtils.import("resource://testing-common/TestUtils.jsm", null).TestUtils;
  this.Promise = ChromeUtils.import("resource://gre/modules/Promise.jsm", null).Promise;
  this.PromiseTestUtils = ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm", null).PromiseTestUtils;
  this.Assert = ChromeUtils.import("resource://testing-common/Assert.jsm", null).Assert;

  this.PromiseTestUtils.init();

  this.SimpleTestOriginal = {};
  SIMPLETEST_OVERRIDES.forEach(m => {
    this.SimpleTestOriginal[m] = this.SimpleTest[m];
  });

  this._coverageCollector = null;

  const XPCOMUtilsMod = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", {});

  // Avoid failing tests when XPCOMUtils.defineLazyScriptGetter is used.
  XPCOMUtilsMod.Services = Object.create(Services, {
    scriptloader: {
      configurable: true,
      writable: true,
      value: {
        loadSubScript: (url, obj, charset) => {
          let before = Object.keys(window);
          try {
            return this._scriptLoader.loadSubScript(url, obj, charset);
          } finally {
            for (let property of Object.keys(window)) {
              if (!before.includes(property) && !this._globalProperties.includes(property)) {
                this._globalProperties.push(property);
                this.SimpleTest.info("Global property added while loading " + url + ": " + property);
              }
            }
          }
        },
        loadSubScriptWithOptions: this._scriptLoader.loadSubScriptWithOptions.bind(this._scriptLoader),
      },
    },
  });
}
Tester.prototype = {
  EventUtils: {},
  SimpleTest: {},
  ContentTask: null,
  ExtensionTestUtils: null,
  Assert: null,

  repeat: 0,
  runUntilFailure: false,
  checker: null,
  currentTestIndex: -1,
  lastStartTime: null,
  lastAssertionCount: 0,
  failuresFromInitialWindowState: 0,

  get currentTest() {
    return this.tests[this.currentTestIndex];
  },
  get done() {
    return (this.currentTestIndex == this.tests.length - 1) && (this.repeat <= 0);
  },

  start: function Tester_start() {
    TabDestroyObserver.init();

    // if testOnLoad was not called, then gConfig is not defined
    if (!gConfig)
      gConfig = readConfig();

    if (gConfig.runUntilFailure)
      this.runUntilFailure = true;

    if (gConfig.repeat)
      this.repeat = gConfig.repeat;

    if (gConfig.jscovDirPrefix) {
      let coveragePath = gConfig.jscovDirPrefix;
      let {CoverageCollector} = ChromeUtils.import("resource://testing-common/CoverageUtils.jsm",
                                                   {});
      this._coverageCollector = new CoverageCollector(coveragePath);
    }

    this.structuredLogger.info("*** Start BrowserChrome Test Results ***");
    Services.console.registerListener(this);
    this._globalProperties = Object.keys(window);
    this._globalPropertyWhitelist = [
      "navigator", "constructor", "top",
      "Application",
      "__SS_tabsToRestore", "__SSi",
      "webConsoleCommandController",
    ];

    if (this.tests.length)
      this.waitForGraphicsTestWindowToBeGone(this.nextTest.bind(this));
    else
      this.finish();
  },

  waitForGraphicsTestWindowToBeGone(aCallback) {
    let windowsEnum = Services.wm.getEnumerator(null);
    while (windowsEnum.hasMoreElements()) {
      let win = windowsEnum.getNext();
      if (win != window && !win.closed &&
          win.document.documentURI == "chrome://gfxsanity/content/sanityparent.html") {
        this.BrowserTestUtils.domWindowClosed(win).then(aCallback);
        return;
      }
    }
    // graphics test window is already gone, just call callback immediately
    aCallback();
  },

  waitForWindowsState: function Tester_waitForWindowsState(aCallback) {
    let timedOut = this.currentTest && this.currentTest.timedOut;
    // eslint-disable-next-line no-nested-ternary
    let baseMsg = timedOut ? "Found a {elt} after previous test timed out"
                           : this.currentTest ? "Found an unexpected {elt} at the end of test run"
                                              : "Found an unexpected {elt}";

    // Remove stale tabs
    if (this.currentTest && window.gBrowser && gBrowser.tabs.length > 1) {
      while (gBrowser.tabs.length > 1) {
        let lastTab = gBrowser.tabContainer.lastChild;
        if (!lastTab.closing) {
          // Report the stale tab as an error only when they're not closing.
          // Tests can finish without waiting for the closing tabs.
          this.currentTest.addResult(new testResult({
            name: baseMsg.replace("{elt}", "tab") + ": " +
              lastTab.linkedBrowser.currentURI.spec,
            allowFailure: this.currentTest.allowFailure,
          }));
        }
        gBrowser.removeTab(lastTab);
      }
    }

    // Replace the last tab with a fresh one
    if (window.gBrowser) {
      gBrowser.addTab("about:blank", { skipAnimation: true });
      gBrowser.removeTab(gBrowser.selectedTab, { skipPermitUnload: true });
      gBrowser.stop();
    }

    // Remove stale windows
    this.structuredLogger.info("checking window state");
    let windowsEnum = Services.wm.getEnumerator(null);
    while (windowsEnum.hasMoreElements()) {
      let win = windowsEnum.getNext();
      if (win != window && !win.closed &&
          win.document.documentElement.getAttribute("id") != "browserTestHarness") {
        let type = win.document.documentElement.getAttribute("windowtype");
        switch (type) {
        case "navigator:browser":
          type = "browser window";
          break;
        case null:
          type = "unknown window with document URI: " + win.document.documentURI +
                 " and title: " + win.document.title;
          break;
        }
        let msg = baseMsg.replace("{elt}", type);
        if (this.currentTest) {
          this.currentTest.addResult(new testResult({
            name: msg,
            allowFailure: this.currentTest.allowFailure,
          }));
        } else {
          this.failuresFromInitialWindowState++;
          this.structuredLogger.error("browser-test.js | " + msg);
        }

        win.close();
      }
    }

    // Make sure the window is raised before each test.
    this.SimpleTest.waitForFocus(aCallback);
  },

  finish: function Tester_finish(aSkipSummary) {
    var passCount = this.tests.reduce((a, f) => a + f.passCount, 0);
    var failCount = this.tests.reduce((a, f) => a + f.failCount, 0);
    var todoCount = this.tests.reduce((a, f) => a + f.todoCount, 0);

    // Include failures from window state checking prior to running the first test
    failCount += this.failuresFromInitialWindowState;

    TabDestroyObserver.destroy();
    Services.console.unregisterListener(this);

    // It's important to terminate the module to avoid crashes on shutdown.
    this.PromiseTestUtils.uninit();

    // In the main process, we print the ShutdownLeaksCollector message here.
    let pid = Services.appinfo.processID;
    dump("Completed ShutdownLeaks collections in process " + pid + "\n");

    this.structuredLogger.info("TEST-START | Shutdown");

    if (this.tests.length) {
      let e10sMode = gMultiProcessBrowser ? "e10s" : "non-e10s";
      this.structuredLogger.info("Browser Chrome Test Summary");
      this.structuredLogger.info("Passed:  " + passCount);
      this.structuredLogger.info("Failed:  " + failCount);
      this.structuredLogger.info("Todo:    " + todoCount);
      this.structuredLogger.info("Mode:    " + e10sMode);
    } else {
      this.structuredLogger.error("browser-test.js | No tests to run. Did you pass invalid test_paths?");
    }
    this.structuredLogger.info("*** End BrowserChrome Test Results ***");

    // Tests complete, notify the callback and return
    this.callback(this.tests);
    this.callback = null;
    this.tests = null;

    if (gSaveInstrumentationData) {
      gSaveInstrumentationData();
    }
  },

  haltTests: function Tester_haltTests() {
    // Do not run any further tests
    this.currentTestIndex = this.tests.length - 1;
    this.repeat = 0;
  },

  observe: function Tester_observe(aSubject, aTopic, aData) {
    if (!aTopic) {
      this.onConsoleMessage(aSubject);
    }
  },

  onConsoleMessage: function Tester_onConsoleMessage(aConsoleMessage) {
    // Ignore empty messages.
    if (!aConsoleMessage.message)
      return;

    try {
      var msg = "Console message: " + aConsoleMessage.message;
      if (this.currentTest)
        this.currentTest.addResult(new testMessage(msg));
      else
        this.structuredLogger.info("TEST-INFO | (browser-test.js) | " + msg.replace(/\n$/, "") + "\n");
    } catch (ex) {
      // Swallow exception so we don't lead to another error being reported,
      // throwing us into an infinite loop
    }
  },

  async nextTest() {
    if (this.currentTest) {
      if (this._coverageCollector) {
        this._coverageCollector.recordTestCoverage(this.currentTest.path);
      }

      // Run cleanup functions for the current test before moving on to the
      // next one.
      let testScope = this.currentTest.scope;
      while (testScope.__cleanupFunctions.length > 0) {
        let func = testScope.__cleanupFunctions.shift();
        try {
          let result = await func.apply(testScope);
          if (isGenerator(result)) {
            this.SimpleTest.ok(false, "Cleanup function returned a generator");
          }
        } catch (ex) {
          this.currentTest.addResult(new testResult({
            name: "Cleanup function threw an exception",
            ex,
            allowFailure: this.currentTest.allowFailure,
          }));
        }
      }

      if (this.currentTest.passCount === 0 &&
          this.currentTest.failCount === 0 &&
          this.currentTest.todoCount === 0) {
        this.currentTest.addResult(new testResult({
          name: "This test contains no passes, no fails and no todos. Maybe" +
                " it threw a silent exception? Make sure you use" +
                " waitForExplicitFinish() if you need it.",
        }));
      }

      let winUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
      if (winUtils.isTestControllingRefreshes) {
        this.currentTest.addResult(new testResult({
          name: "test left refresh driver under test control",
        }));
        winUtils.restoreNormalRefresh();
      }

      if (this.SimpleTest.isExpectingUncaughtException()) {
        this.currentTest.addResult(new testResult({
          name: "expectUncaughtException was called but no uncaught" +
                " exception was detected!",
          allowFailure: this.currentTest.allowFailure,
        }));
      }

      this.PromiseTestUtils.ensureDOMPromiseRejectionsProcessed();
      this.PromiseTestUtils.assertNoUncaughtRejections();
      this.PromiseTestUtils.assertNoMoreExpectedRejections();

      Object.keys(window).forEach(function(prop) {
        if (parseInt(prop) == prop) {
          // This is a string which when parsed as an integer and then
          // stringified gives the original string.  As in, this is in fact a
          // string representation of an integer, so an index into
          // window.frames.  Skip those.
          return;
        }
        if (!this._globalProperties.includes(prop)) {
          this._globalProperties.push(prop);
          if (!this._globalPropertyWhitelist.includes(prop)) {
            this.currentTest.addResult(new testResult({
              name: "test left unexpected property on window: " + prop,
              allowFailure: this.currentTest.allowFailure,
            }));
          }
        }
      }, this);

      // Clear document.popupNode.  The test could have set it to a custom value
      // for its own purposes, nulling it out it will go back to the default
      // behavior of returning the last opened popup.
      document.popupNode = null;

      // eslint-disable-next-line no-undef
      await new Promise(resolve => SpecialPowers.flushPrefEnv(resolve));

      if (gConfig.cleanupCrashes) {
        let gdir = Services.dirsvc.get("UAppData", Ci.nsIFile);
        gdir.append("Crash Reports");
        gdir.append("pending");
        if (gdir.exists()) {
          let entries = gdir.directoryEntries;
          while (entries.hasMoreElements()) {
            let entry = entries.nextFile;
            if (entry.isFile()) {
              let msg = "this test left a pending crash report; ";
              try {
                entry.remove(false);
                msg += "deleted " + entry.path;
              } catch (e) {
                msg += "could not delete " + entry.path;
              }
              this.structuredLogger.info(msg);
            }
          }
        }
      }

      // Notify a long running test problem if it didn't end up in a timeout.
      if (this.currentTest.unexpectedTimeouts && !this.currentTest.timedOut) {
        this.currentTest.addResult(new testResult({
          name: "This test exceeded the timeout threshold. It should be" +
                " rewritten or split up. If that's not possible, use" +
                " requestLongerTimeout(N), but only as a last resort.",
        }));
      }

      // If we're in a debug build, check assertion counts.  This code
      // is similar to the code in TestRunner.testUnloaded in
      // TestRunner.js used for all other types of mochitests.
      let debugsvc = Cc["@mozilla.org/xpcom/debug;1"].getService(Ci.nsIDebug2);
      if (debugsvc.isDebugBuild) {
        let newAssertionCount = debugsvc.assertionCount;
        let numAsserts = newAssertionCount - this.lastAssertionCount;
        this.lastAssertionCount = newAssertionCount;

        let max = testScope.__expectedMaxAsserts;
        let min = testScope.__expectedMinAsserts;
        if (numAsserts > max) {
          // TEST-UNEXPECTED-FAIL
          this.currentTest.addResult(new testResult({
            name: "Assertion count " + numAsserts +
                  " is greater than expected range " +
                  min + "-" + max + " assertions.",
            pass: true, // TEMPORARILY TEST-KNOWN-FAIL
            todo: true,
            allowFailure: this.currentTest.allowFailure,
          }));
        } else if (numAsserts < min) {
          // TEST-UNEXPECTED-PASS
          this.currentTest.addResult(new testResult({
            name: "Assertion count " + numAsserts +
                  " is less than expected range " +
                  min + "-" + max + " assertions.",
            todo: true,
            allowFailure: this.currentTest.allowFailure,
          }));
        } else if (numAsserts > 0) {
          // TEST-KNOWN-FAIL
          this.currentTest.addResult(new testResult({
            name: "Assertion count " + numAsserts +
                  " is within expected range " +
                  min + "-" + max + " assertions.",
            pass: true,
            todo: true,
            allowFailure: this.currentTest.allowFailure,
          }));
        }
      }

      if (this.currentTest.allowFailure) {
        if (this.currentTest.expectedAllowedFailureCount) {
          this.currentTest.addResult(new testResult({
            name: "Expected " +
                  this.currentTest.expectedAllowedFailureCount +
                  " failures in this file, got " +
                  this.currentTest.allowedFailureCount + ".",
            pass: this.currentTest.expectedAllowedFailureCount ==
                  this.currentTest.allowedFailureCount,
          }));
        } else if (this.currentTest.allowedFailureCount == 0) {
          this.currentTest.addResult(new testResult({
            name: "We expect at least one assertion to fail because this" +
                  " test file is marked as fail-if in the manifest.",
            todo: true,
          }));
        }
      }

      // Dump memory stats for main thread.
      if (Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
        this.MemoryStats.dump(this.currentTestIndex,
                              this.currentTest.path,
                              gConfig.dumpOutputDirectory,
                              gConfig.dumpAboutMemoryAfterTest,
                              gConfig.dumpDMDAfterTest);
      }

      // Note the test run time
      let time = Date.now() - this.lastStartTime;
      this.structuredLogger.testEnd(this.currentTest.path,
                                           "OK",
                                           undefined,
                                           "finished in " + time + "ms");
      this.currentTest.setDuration(time);

      if (this.runUntilFailure && this.currentTest.failCount > 0) {
        this.haltTests();
      }

      // Restore original SimpleTest methods to avoid leaks.
      SIMPLETEST_OVERRIDES.forEach(m => {
        this.SimpleTest[m] = this.SimpleTestOriginal[m];
      });

      this.ContentTask.setTestScope(null);
      testScope.destroy();
      this.currentTest.scope = null;
    }

    // Check the window state for the current test before moving to the next one.
    // This also causes us to check before starting any tests, since nextTest()
    // is invoked to start the tests.
    this.waitForWindowsState(() => {
      if (this.done) {
        if (this._coverageCollector) {
          this._coverageCollector.finalize();
        }

        // Uninitialize a few things explicitly so that they can clean up
        // frames and browser intentionally kept alive until shutdown to
        // eliminate false positives.
        if (gConfig.testRoot == "browser") {
          // Skip if SeaMonkey
          if (AppConstants.MOZ_APP_NAME != "seamonkey") {
            // Replace the document currently loaded in the browser's sidebar.
            // This will prevent false positives for tests that were the last
            // to touch the sidebar. They will thus not be blamed for leaking
            // a document.
            let sidebar = document.getElementById("sidebar");
            sidebar.setAttribute("src", "data:text/html;charset=utf-8,");
            sidebar.docShell.createAboutBlankContentViewer(null);
            sidebar.setAttribute("src", "about:blank");
          }

          // Destroy BackgroundPageThumbs resources.
          let {BackgroundPageThumbs} =
            ChromeUtils.import("resource://gre/modules/BackgroundPageThumbs.jsm", {});
          BackgroundPageThumbs._destroy();

          // Destroy preloaded browsers.
          if (gBrowser._preloadedBrowser) {
            let browser = gBrowser._preloadedBrowser;
            gBrowser._preloadedBrowser = null;
            gBrowser.getNotificationBox(browser).remove();
          }
        }

        // Schedule GC and CC runs before finishing in order to detect
        // DOM windows leaked by our tests or the tested code. Note that we
        // use a shrinking GC so that the JS engine will discard JIT code and
        // JIT caches more aggressively.

        let shutdownCleanup = aCallback => {
          Cu.schedulePreciseShrinkingGC(() => {
            // Run the GC and CC a few times to make sure that as much
            // as possible is freed.
            let numCycles = 3;
            for (let i = 0; i < numCycles; i++) {
              Cu.forceGC();
              Cu.forceCC();
            }
            aCallback();
          });
        };


        let {AsyncShutdown} =
          ChromeUtils.import("resource://gre/modules/AsyncShutdown.jsm", {});

        let barrier = new AsyncShutdown.Barrier(
          "ShutdownLeaks: Wait for cleanup to be finished before checking for leaks");
        Services.obs.notifyObservers({wrappedJSObject: barrier},
          "shutdown-leaks-before-check");

        barrier.client.addBlocker("ShutdownLeaks: Wait for tabs to finish closing",
                                  TabDestroyObserver.wait());

        barrier.wait().then(() => {
          // Simulate memory pressure so that we're forced to free more resources
          // and thus get rid of more false leaks like already terminated workers.
          Services.obs.notifyObservers(null, "memory-pressure", "heap-minimize");

          Services.ppmm.broadcastAsyncMessage("browser-test:collect-request");

          shutdownCleanup(() => {
            setTimeout(() => {
              shutdownCleanup(() => {
                this.finish();
              });
            }, 1000);
          });
        });

        return;
      }

      if (this.repeat > 0) {
        --this.repeat;
        if (this.currentTestIndex < 0) {
          this.currentTestIndex = 0;
        }
        this.execTest();
      } else {
        this.currentTestIndex++;
        if (gConfig.repeat)
          this.repeat = gConfig.repeat;
        this.execTest();
      }
    });
  },

  execTest: function Tester_execTest() {
    this.structuredLogger.testStart(this.currentTest.path);

    this.SimpleTest.reset();

    // Load the tests into a testscope
    let currentScope = this.currentTest.scope = new testScope(this, this.currentTest, this.currentTest.expected);
    let currentTest = this.currentTest;

    // Import utils in the test scope.
    let {scope} = this.currentTest;
    scope.EventUtils = this.currentTest.usesUnsafeCPOWs ? this.cpowEventUtils : this.EventUtils;
    scope.SimpleTest = this.SimpleTest;
    scope.gTestPath = this.currentTest.path;
    scope.ContentTask = this.ContentTask;
    scope.BrowserTestUtils = this.BrowserTestUtils;
    scope.TestUtils = this.TestUtils;
    scope.ExtensionTestUtils = this.ExtensionTestUtils;
    // Pass a custom report function for mochitest style reporting.
    scope.Assert = new this.Assert(function(err, message, stack) {
      currentTest.addResult(new testResult(err ? {
        name: err.message,
        ex: err.stack,
        stack: err.stack,
        allowFailure: currentTest.allowFailure,
      } : {
        name: message,
        pass: true,
        stack,
        allowFailure: currentTest.allowFailure,
      }));
    }, true);

    this.ContentTask.setTestScope(currentScope);

    // Allow Assert.jsm methods to be tacked to the current scope.
    scope.export_assertions = function() {
      for (let func in this.Assert) {
        this[func] = this.Assert[func].bind(this.Assert);
      }
    };

    // Override SimpleTest methods with ours.
    SIMPLETEST_OVERRIDES.forEach(function(m) {
      this.SimpleTest[m] = this[m];
    }, scope);

    // load the tools to work with chrome .jar and remote
    try {
      this._scriptLoader.loadSubScript("chrome://mochikit/content/chrome-harness.js", scope);
    } catch (ex) { /* no chrome-harness tools */ }

    // Import head.js script if it exists.
    var currentTestDirPath =
      this.currentTest.path.substr(0, this.currentTest.path.lastIndexOf("/"));
    var headPath = currentTestDirPath + "/head.js";
    try {
      this._scriptLoader.loadSubScript(headPath, scope);
    } catch (ex) {
      // Ignore if no head.js exists, but report all other errors.  Note this
      // will also ignore an existing head.js attempting to import a missing
      // module - see bug 755558 for why this strategy is preferred anyway.
      if (!/^Error opening input stream/.test(ex.toString())) {
       this.currentTest.addResult(new testResult({
         name: "head.js import threw an exception",
         ex,
       }));
      }
    }

    // Import the test script.
    try {
      this._scriptLoader.loadSubScript(this.currentTest.path, scope);
      // Run the test
      this.lastStartTime = Date.now();
      if (this.currentTest.scope.__tasks) {
        // This test consists of tasks, added via the `add_task()` API.
        if ("test" in this.currentTest.scope) {
          throw "Cannot run both a add_task test and a normal test at the same time.";
        }
        let PromiseTestUtils = this.PromiseTestUtils;

        // Allow for a task to be skipped; we need only use the structured logger
        // for this, whilst deactivating log buffering to ensure that messages
        // are always printed to stdout.
        let skipTask = (task) => {
          let logger = this.structuredLogger;
          logger.deactivateBuffering();
          logger.testStatus(this.currentTest.path, task.name, "SKIP");
          logger.warning("Skipping test " + task.name);
          logger.activateBuffering();
        };

        (async function() {
          let task;
          while ((task = this.__tasks.shift())) {
            if (task.__skipMe || (this.__runOnlyThisTask && task != this.__runOnlyThisTask)) {
              skipTask(task);
              continue;
            }
            this.SimpleTest.info("Entering test " + task.name);
            try {
              let result = await task();
              if (isGenerator(result)) {
                this.SimpleTest.ok(false, "Task returned a generator");
              }
            } catch (ex) {
              if (currentTest.timedOut) {
                currentTest.addResult(new testResult({
                  name: "Uncaught exception received from previously timed out test",
                  pass: false,
                  ex,
                  stack: (typeof ex == "object" && "stack" in ex) ? ex.stack : null,
                  allowFailure: currentTest.allowFailure,
                }));
                // We timed out, so we've already cleaned up for this test, just get outta here.
                return;
              }
                currentTest.addResult(new testResult({
                  name: "Uncaught exception",
                  pass: this.SimpleTest.isExpectingUncaughtException(),
                  ex,
                  stack: (typeof ex == "object" && "stack" in ex) ? ex.stack : null,
                  allowFailure: currentTest.allowFailure,
                }));

            }
            PromiseTestUtils.assertNoUncaughtRejections();
            this.SimpleTest.info("Leaving test " + task.name);
          }
          this.finish();
        }).call(currentScope);
      } else if (typeof scope.test == "function") {
        scope.test();
      } else {
        throw "This test didn't call add_task, nor did it define a generatorTest() function, nor did it define a test() function, so we don't know how to run it.";
      }
    } catch (ex) {
      if (!this.SimpleTest.isIgnoringAllUncaughtExceptions()) {
        this.currentTest.addResult(new testResult({
          name: "Exception thrown",
          pass: this.SimpleTest.isExpectingUncaughtException(),
          ex,
          allowFailure: this.currentTest.allowFailure,
        }));
        this.SimpleTest.expectUncaughtException(false);
      } else {
        this.currentTest.addResult(new testMessage("Exception thrown: " + ex));
      }
      this.currentTest.scope.finish();
    }

    // If the test ran synchronously, move to the next test, otherwise the test
    // will trigger the next test when it is done.
    if (this.currentTest.scope.__done) {
      this.nextTest();
    } else {
      var self = this;
      var timeoutExpires = Date.now() + gTimeoutSeconds * 1000;
      var waitUntilAtLeast = timeoutExpires - 1000;
      this.currentTest.scope.__waitTimer =
        this.SimpleTest._originalSetTimeout.apply(window, [function timeoutFn() {
        // We sometimes get woken up long before the gTimeoutSeconds
        // have elapsed (when running in chaos mode for example). This
        // code ensures that we don't wrongly time out in that case.
        if (Date.now() < waitUntilAtLeast) {
          self.currentTest.scope.__waitTimer =
            setTimeout(timeoutFn, timeoutExpires - Date.now());
          return;
        }

        if (--self.currentTest.scope.__timeoutFactor > 0) {
          // We were asked to wait a bit longer.
          self.currentTest.scope.info(
            "Longer timeout required, waiting longer...  Remaining timeouts: " +
            self.currentTest.scope.__timeoutFactor);
          self.currentTest.scope.__waitTimer =
            setTimeout(timeoutFn, gTimeoutSeconds * 1000);
          return;
        }

        // If the test is taking longer than expected, but it's not hanging,
        // mark the fact, but let the test continue.  At the end of the test,
        // if it didn't timeout, we will notify the problem through an error.
        // To figure whether it's an actual hang, compare the time of the last
        // result or message to half of the timeout time.
        // Though, to protect against infinite loops, limit the number of times
        // we allow the test to proceed.
        const MAX_UNEXPECTED_TIMEOUTS = 10;
        if (Date.now() - self.currentTest.lastOutputTime < (gTimeoutSeconds / 2) * 1000 &&
            ++self.currentTest.unexpectedTimeouts <= MAX_UNEXPECTED_TIMEOUTS) {
            self.currentTest.scope.__waitTimer =
              setTimeout(timeoutFn, gTimeoutSeconds * 1000);
          return;
        }

        self.currentTest.addResult(new testResult({ name: "Test timed out" }));
        self.currentTest.timedOut = true;
        self.currentTest.scope.__waitTimer = null;
        self.nextTest();
      }, gTimeoutSeconds * 1000]);
    }
  },

  QueryInterface: ChromeUtils.generateQI(["nsIConsoleListener"])
};

/**
 * Represents the result of one test assertion. This is described with a string
 * in traditional logging, and has a "status" and "expected" property used in
 * structured logging. Normally, results are mapped as follows:
 *
 *   pass:    todo:    Added to:    Described as:           Status:  Expected:
 *     true     false    passCount    TEST-PASS               PASS     PASS
 *     true     true     todoCount    TEST-KNOWN-FAIL         FAIL     FAIL
 *     false    false    failCount    TEST-UNEXPECTED-FAIL    FAIL     PASS
 *     false    true     failCount    TEST-UNEXPECTED-PASS    PASS     FAIL
 *
 * The "allowFailure" argument indicates that this is one of the assertions that
 * should be allowed to fail, for example because "fail-if" is true for the
 * current test file in the manifest. In this case, results are mapped this way:
 *
 *   pass:    todo:    Added to:    Described as:           Status:  Expected:
 *     true     false    passCount    TEST-PASS               PASS     PASS
 *     true     true     todoCount    TEST-KNOWN-FAIL         FAIL     FAIL
 *     false    false    todoCount    TEST-KNOWN-FAIL         FAIL     FAIL
 *     false    true     todoCount    TEST-KNOWN-FAIL         FAIL     FAIL
 */
function testResult({ name, pass, todo, ex, stack, allowFailure }) {
  this.info = false;
  this.name = name;
  this.msg = "";

  if (allowFailure && !pass) {
    this.allowedFailure = true;
    this.pass = true;
    this.todo = true;
  } else {
    this.pass = !!pass;
    this.todo = todo;
  }

  this.expected = this.todo ? "FAIL" : "PASS";

  if (this.pass) {
    this.status = this.expected;
    return;
  }

  this.status = this.todo ? "PASS" : "FAIL";

  if (ex) {
    if (typeof ex == "object" && "fileName" in ex) {
      // we have an exception - print filename and linenumber information
      this.msg += "at " + ex.fileName + ":" + ex.lineNumber + " - ";
    }
    this.msg += String(ex);
  }

  if (stack) {
    this.msg += "\nStack trace:\n";
    let normalized;
    if (stack instanceof Ci.nsIStackFrame) {
      let frames = [];
      for (let frame = stack; frame; frame = frame.asyncCaller || frame.caller) {
        let msg = `${frame.filename}:${frame.name}:${frame.lineNumber}`;
        frames.push(frame.asyncCause ? `${frame.asyncCause}*${msg}` : msg);
      }
      normalized = frames.join("\n");
    } else {
      normalized = "" + stack;
    }
    this.msg += normalized;
  }

  if (gConfig.debugOnFailure) {
    // You've hit this line because you requested to break into the
    // debugger upon a testcase failure on your test run.
    // eslint-disable-next-line no-debugger
    debugger;
  }
}

function testMessage(msg) {
  this.msg = msg || "";
  this.info = true;
}

// Need to be careful adding properties to this object, since its properties
// cannot conflict with global variables used in tests.
function testScope(aTester, aTest, expected) {
  this.__tester = aTester;

  aTest.allowFailure = expected == "fail";

  var self = this;
  this.ok = function test_ok(condition, name, ex, stack) {
    aTest.addResult(new testResult({
      name, pass: condition, ex,
      stack: stack || Components.stack.caller,
      allowFailure: aTest.allowFailure,
    }));
  };
  this.is = function test_is(a, b, name) {
    self.ok(a == b, name, "Got " + a + ", expected " + b, false,
            Components.stack.caller);
  };
  this.isnot = function test_isnot(a, b, name) {
    self.ok(a != b, name, "Didn't expect " + a + ", but got it", false,
            Components.stack.caller);
  };
  this.todo = function test_todo(condition, name, ex, stack) {
    aTest.addResult(new testResult({
      name, pass: !condition, todo: true, ex,
      stack: stack || Components.stack.caller,
      allowFailure: aTest.allowFailure,
    }));
  };
  this.todo_is = function test_todo_is(a, b, name) {
    self.todo(a == b, name, "Got " + a + ", expected " + b,
              Components.stack.caller);
  };
  this.todo_isnot = function test_todo_isnot(a, b, name) {
    self.todo(a != b, name, "Didn't expect " + a + ", but got it",
              Components.stack.caller);
  };
  this.info = function test_info(name) {
    aTest.addResult(new testMessage(name));
  };

  this.executeSoon = function test_executeSoon(func) {
    Services.tm.dispatchToMainThread({
      run() {
        func();
      }
    });
  };

  this.waitForExplicitFinish = function test_waitForExplicitFinish() {
    self.__done = false;
  };

  this.waitForFocus = function test_waitForFocus(callback, targetWindow, expectBlankPage) {
    self.SimpleTest.waitForFocus(callback, targetWindow, expectBlankPage);
  };

  this.waitForClipboard = function test_waitForClipboard(expected, setup, success, failure, flavor) {
    self.SimpleTest.waitForClipboard(expected, setup, success, failure, flavor);
  };

  this.registerCleanupFunction = function test_registerCleanupFunction(aFunction) {
    self.__cleanupFunctions.push(aFunction);
  };

  this.requestLongerTimeout = function test_requestLongerTimeout(aFactor) {
    self.__timeoutFactor = aFactor;
  };

  this.copyToProfile = function test_copyToProfile(filename) {
    self.SimpleTest.copyToProfile(filename);
  };

  this.expectUncaughtException = function test_expectUncaughtException(aExpecting) {
    self.SimpleTest.expectUncaughtException(aExpecting);
  };

  this.ignoreAllUncaughtExceptions = function test_ignoreAllUncaughtExceptions(aIgnoring) {
    self.SimpleTest.ignoreAllUncaughtExceptions(aIgnoring);
  };

  this.expectAssertions = function test_expectAssertions(aMin, aMax) {
    let min = aMin;
    let max = aMax;
    if (typeof(max) == "undefined") {
      max = min;
    }
    if (typeof(min) != "number" || typeof(max) != "number" ||
        min < 0 || max < min) {
      throw "bad parameter to expectAssertions";
    }
    self.__expectedMinAsserts = min;
    self.__expectedMaxAsserts = max;
  };

  this.setExpectedFailuresForSelfTest = function test_setExpectedFailuresForSelfTest(expectedAllowedFailureCount) {
    aTest.allowFailure = true;
    aTest.expectedAllowedFailureCount = expectedAllowedFailureCount;
  };

  this.finish = function test_finish() {
    self.__done = true;
    if (self.__waitTimer) {
      self.executeSoon(function() {
        if (self.__done && self.__waitTimer) {
          clearTimeout(self.__waitTimer);
          self.__waitTimer = null;
          self.__tester.nextTest();
        }
      });
    }
  };

  this.requestCompleteLog = function test_requestCompleteLog() {
    self.__tester.structuredLogger.deactivateBuffering();
    self.registerCleanupFunction(function() {
      self.__tester.structuredLogger.activateBuffering();
    });
  };

  // If we're running a test that requires unsafe CPOWs, create a
  // separate sandbox scope, with CPOWS whitelisted, for that test, and
  // mirror all of our properties onto it. Test files will be loaded
  // into this sandbox.
  //
  // Otherwise, load test files directly into the testScope instance.
  if (aTest.usesUnsafeCPOWs) {
    let sandbox = this._createSandbox();
    Cu.permitCPOWsInScope(sandbox);
    return sandbox;
  }
  return this;
}

function decorateTaskFn(fn) {
  fn = fn.bind(this);
  fn.skip = () => fn.__skipMe = true;
  fn.only = () => this.__runOnlyThisTask = fn;
  return fn;
}

testScope.prototype = {
  __done: true,
  __tasks: null,
  __runOnlyThisTask: null,
  __waitTimer: null,
  __cleanupFunctions: [],
  __timeoutFactor: 1,
  __expectedMinAsserts: 0,
  __expectedMaxAsserts: 0,

  EventUtils: {},
  SimpleTest: {},
  ContentTask: null,
  BrowserTestUtils: null,
  TestUtils: null,
  ExtensionTestUtils: null,
  Assert: null,

  _createSandbox() {
    let sandbox = Cu.Sandbox(window, {sandboxPrototype: window});

    for (let prop in this) {
      if (typeof this[prop] == "function") {
        sandbox[prop] = this[prop].bind(this);
      } else {
        Object.defineProperty(sandbox, prop, {
          configurable: true,
          enumerable: true,
          get: () => {
            return this[prop];
          },
          set: (value) => {
            this[prop] = value;
          }
        });
      }
    }

    return sandbox;
  },

  /**
   * Add a function which returns a promise (usually an async function)
   * as a test task.
   *
   * The task ends when the promise returned by the function resolves or
   * rejects. If the test function throws, or the promise it returns
   * rejects, the test is reported as a failure. Execution continues
   * with the next test function.
   *
   * Example usage:
   *
   * add_task(async function test() {
   *   let result = yield Promise.resolve(true);
   *
   *   ok(result);
   *
   *   let secondary = await someFunctionThatReturnsAPromise(result);
   *   is(secondary, "expected value");
   * });
   *
   * add_task(async function test_early_return() {
   *   let result = await somethingThatReturnsAPromise();
   *
   *   if (!result) {
   *     // Test is ended immediately, with success.
   *     return;
   *   }
   *
   *   is(result, "foo");
   * });
   */
  add_task(aFunction) {
    if (!this.__tasks) {
      this.waitForExplicitFinish();
      this.__tasks = [];
    }
    let bound = decorateTaskFn.call(this, aFunction);
    this.__tasks.push(bound);
    return bound;
  },

  destroy: function test_destroy() {
    for (let prop in this)
      delete this[prop];
  }
};
