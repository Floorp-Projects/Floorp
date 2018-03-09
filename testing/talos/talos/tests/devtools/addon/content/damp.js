const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});
const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", {});
const gMgr = Cc["@mozilla.org/memory-reporter-manager;1"].getService(Ci.nsIMemoryReporterManager);
const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);

XPCOMUtils.defineLazyGetter(this, "require", function() {
  let { require } =
    ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
  return require;
});
XPCOMUtils.defineLazyGetter(this, "gDevTools", function() {
  let { gDevTools } = require("devtools/client/framework/devtools");
  return gDevTools;
});
XPCOMUtils.defineLazyGetter(this, "EVENTS", function() {
  let { EVENTS } = require("devtools/client/netmonitor/src/constants");
  return EVENTS;
});
XPCOMUtils.defineLazyGetter(this, "TargetFactory", function() {
  let { TargetFactory } = require("devtools/client/framework/target");
  return TargetFactory;
});

const webserver = Services.prefs.getCharPref("addon.test.damp.webserver");

const PAGES_BASE_URL = webserver + "/tests/devtools/addon/content/pages/";
const SIMPLE_URL = PAGES_BASE_URL + "simple.html";
const COMPLICATED_URL = webserver + "/tests/tp5n/bild.de/www.bild.de/index.html";
const CUSTOM_URL = PAGES_BASE_URL + "custom/$TOOL/index.html";
const PANELS_IN_BACKGROUND = PAGES_BASE_URL +
  "custom/panels-in-background/panels-in-background.html";

// Record allocation count in new subtests if DEBUG_DEVTOOLS_ALLOCATIONS is set to
// "normal". Print allocation sites to stdout if DEBUG_DEVTOOLS_ALLOCATIONS is set to
// "verbose".
const DEBUG_ALLOCATIONS = env.get("DEBUG_DEVTOOLS_ALLOCATIONS");

function getMostRecentBrowserWindow() {
  return Services.wm.getMostRecentWindow("navigator:browser");
}

function getActiveTab(window) {
  return window.gBrowser.selectedTab;
}

let gmm = window.getGroupMessageManager("browsers");

const frameScript = "data:," + encodeURIComponent(`(${
  function() {
    addEventListener("load", function(event) {
      let subframe = event.target != content.document;
      sendAsyncMessage("browser-test-utils:loadEvent",
        {subframe, url: event.target.documentURI});
    }, true);
  }
})()`);

gmm.loadFrameScript(frameScript, true);

// This is duplicated from BrowserTestUtils.jsm
function awaitBrowserLoaded(browser, includeSubFrames = false, wantLoad = null) {
  // If browser belongs to tabbrowser-tab, ensure it has been
  // inserted into the document.
  let tabbrowser = browser.ownerGlobal.gBrowser;
  if (tabbrowser && tabbrowser.getTabForBrowser) {
    tabbrowser._insertBrowser(tabbrowser.getTabForBrowser(browser));
  }

  function isWanted(url) {
    if (!wantLoad) {
      return true;
    } else if (typeof(wantLoad) == "function") {
      return wantLoad(url);
    }
    // It's a string.
    return wantLoad == url;
  }

  return new Promise(resolve => {
    let mm = browser.ownerGlobal.messageManager;
    mm.addMessageListener("browser-test-utils:loadEvent", function onLoad(msg) {
      if (msg.target == browser && (!msg.data.subframe || includeSubFrames) &&
          isWanted(msg.data.url)) {
        mm.removeMessageListener("browser-test-utils:loadEvent", onLoad);
        resolve(msg.data.url);
      }
    });
  });
}

/* ************* Debugger Helper ***************/
/*
 * These methods are used for working with debugger state changes in order
 * to make it easier to manipulate the ui and test different behavior. These
 * methods roughly reflect those found in debugger/new/test/mochi/head.js with
 * a few exceptions. The `dbg` object is not exactly the same, and the methods
 * have been simplified. We may want to consider unifying them in the future
 */

const DEBUGGER_POLLING_INTERVAL = 50;

const debuggerHelper = {
  waitForState(dbg, predicate, msg) {
    return new Promise(resolve => {
      if (msg) {
        dump(`Waiting for state change: ${msg}\n`);
      }
      if (predicate(dbg.store.getState())) {
        if (msg) {
          dump(`Finished waiting for state change: ${msg}\n`);
        }
        return resolve();
      }

      const unsubscribe = dbg.store.subscribe(() => {
        if (predicate(dbg.store.getState())) {
          if (msg) {
            dump(`Finished waiting for state change: ${msg}\n`);
          }
          unsubscribe();
          resolve();
        }
      });
      return false;
    });
  },

  waitForDispatch(dbg, type) {
    return new Promise(resolve => {
      dbg.store.dispatch({
        type: "@@service/waitUntil",
        predicate: action => {
          if (action.type === type) {
            return action.status
              ? action.status === "done" || action.status === "error"
              : true;
          }
          return false;
        },
        run: (dispatch, getState, action) => {
          resolve(action);
        }
      });
    });
  },

  async waitUntil(predicate, msg) {
    if (msg) {
      dump(`Waiting until: ${msg}\n`);
    }
    return new Promise(resolve => {
      const timer = setInterval(() => {
        if (predicate()) {
          clearInterval(timer);
          if (msg) {
            dump(`Finished Waiting until: ${msg}\n`);
          }
          resolve();
        }
      }, DEBUGGER_POLLING_INTERVAL);
    });
  },

  findSource(dbg, url) {
    const sources = dbg.selectors.getSources(dbg.getState());
    return sources.find(s => (s.get("url") || "").includes(url));
  },

  getCM(dbg) {
    const el = dbg.win.document.querySelector(".CodeMirror");
    return el.CodeMirror;
  },

  waitForText(dbg, url, text) {
    return this.waitUntil(() => {
      // the welcome box is removed once text is displayed
      const welcomebox = dbg.win.document.querySelector(".welcomebox");
      if (welcomebox) {
        return false;
      }
      const cm = this.getCM(dbg);
      const editorText = cm.doc.getValue();
      return editorText.includes(text);
    }, "text is visible");
  },

  waitForMetaData(dbg) {
    return this.waitUntil(
      () => {
        const state = dbg.store.getState();
        const source = dbg.selectors.getSelectedSource(state);
        // wait for metadata -- this involves parsing the file to determine its type.
        // if the object is empty, the data has not yet loaded
        const metaData = dbg.selectors.getSourceMetaData(state, source.get("id"));
        return !!Object.keys(metaData).length;
      },
      "has file metadata"
    );
  },

  waitForSources(dbg, expectedSources) {
    const { selectors } = dbg;
    function countSources(state) {
      const sources = selectors.getSources(state);
      return sources.size >= expectedSources;
    }
    return this.waitForState(dbg, countSources, "count sources");
  },

  async createContext(panel) {
    const { store, selectors, actions } = panel.getVarsForTests();

    return {
      actions,
      selectors,
      getState: store.getState,
      win: panel.panelWin,
      store
    };
  },

  selectSource(dbg, url) {
    dump(`Selecting source: ${url}\n`);
    const line = 1;
    const source = this.findSource(dbg, url);
    dbg.actions.selectLocation({ sourceId: source.get("id"), line });
    return this.waitForState(
      dbg,
      state => {
        const source = dbg.selectors.getSelectedSource(state);
        const isLoaded = source && source.get("loadedState") === "loaded";
        if (!isLoaded) {
          return false;
        }

        // wait for symbols -- a flat map of all named variables in a file -- to be calculated.
        // this is a slow process and becomes slower the larger the file is
        return dbg.selectors.hasSymbols(state, source.toJS());
      },
      "selected source"
    );
  },

  async addBreakpoint(dbg, line, url) {
    dump(`add breakpoint\n`);
    const source = this.findSource(dbg, url);
    const location = {
      sourceId: source.get("id"),
      line,
      column: 0
    };
    const onDispatched = debuggerHelper.waitForDispatch(dbg, "ADD_BREAKPOINT");
    dbg.actions.addBreakpoint(location);
    return onDispatched;
  },

  async removeBreakpoints(dbg, line, url) {
    dump(`remove all breakpoints\n`);
    const breakpoints = dbg.selectors.getBreakpoints(dbg.getState());

    const onBreakpointsCleared =  this.waitForState(
      dbg,
      state => !dbg.selectors.getBreakpoints(state).length
    );
    await dbg.actions.removeBreakpoints(breakpoints);
    return onBreakpointsCleared;
  },

  async pauseDebugger(dbg, tab, testFunction, { line, file }) {
    await this.addBreakpoint(dbg, line, file);
    const onPaused = this.waitForPaused(dbg);
    await this.evalInContent(dbg, tab, testFunction);
    return onPaused;
  },

  async waitForPaused(dbg) {
    const onLoadedScope = this.waitForLoadedScopes(dbg);
    const onStateChange =  this.waitForState(
      dbg,
      state => {
        return dbg.selectors.getSelectedScope(state) && dbg.selectors.isPaused(state);
      },
    );
    return Promise.all([onLoadedScope, onStateChange]);
  },

  async resume(dbg) {
    const onResumed = this.waitForResumed(dbg);
    dbg.actions.resume();
    return onResumed;
  },

  async waitForResumed(dbg) {
    return this.waitForState(
      dbg,
      state => !dbg.selectors.isPaused(state)
    );
  },

  evalInContent(dbg, tab, testFunction) {
    dump(`Run function in content process: ${testFunction}\n`);
    // Load a frame script using a data URI so we can run a script
    // inside of the content process and trigger debugger functionality
    // as needed
    const messageManager = tab.linkedBrowser.messageManager;
    return messageManager.loadFrameScript("data:,(" + encodeURIComponent(
      `function () {
          content.window.eval("${testFunction}");
      }`
    ) + ")()", true);
  },

  async waitForElement(dbg, name) {
    await this.waitUntil(() => dbg.win.document.querySelector(name));
    return dbg.win.document.querySelector(name);
  },

  async waitForLoadedScopes(dbg) {
    const element = ".scopes-list .tree-node[aria-level=\"1\"]";
    return this.waitForElement(dbg, element);
  },

  async step(dbg, stepType) {
    const resumed = this.waitForResumed(dbg);
    dbg.actions[stepType]();
    await resumed;
    return this.waitForPaused(dbg);
  }
};

async function garbageCollect() {
  dump("Garbage collect\n");

  // Minimize memory usage
  // mimic miminizeMemoryUsage, by only flushing JS objects via GC.
  // We don't want to flush all the cache like minimizeMemoryUsage,
  // as it slow down next executions almost like a cold start.

  // See minimizeMemoryUsage code to justify the 3 iterations and the setTimeout:
  // https://searchfox.org/mozilla-central/source/xpcom/base/nsMemoryReporterManager.cpp#2574-2585
  for (let i = 0; i < 3; i++) {
    // See minimizeMemoryUsage code here to justify the GC+CC+GC:
    // https://searchfox.org/mozilla-central/rev/be78e6ea9b10b1f5b2b3b013f01d86e1062abb2b/dom/base/nsJSEnvironment.cpp#341-349
    Cu.forceGC();
    Cu.forceCC();
    Cu.forceGC();
    await new Promise(done => setTimeout(done, 0));
  }
}

/* globals res:true */

function Damp() {
  // Path to the temp file where the heap snapshot file is saved. Set by
  // saveHeapSnapshot and read by readHeapSnapshot.
  this._heapSnapshotFilePath = null;
  // HeapSnapshot instance. Set by readHeapSnapshot, used by takeCensus.
  this._snapshot = null;

  Services.prefs.setBoolPref("devtools.webconsole.new-frontend-enabled", true);
}

Damp.prototype = {

  /**
   * Helper to tell when a test start and when it is finished.
   * It helps recording its duration, but also put markers for perf-html when profiling
   * DAMP.
   *
   * When this method is called, the test is considered to be starting immediately
   * When the test is over, the returned object's `done` method should be called.
   *
   * @param label String
   *        Test title, displayed everywhere in PerfHerder, DevTools Perf Dashboard, ...
   *
   * @return object
   *         With a `done` method, to be called whenever the test is finished running
   *         and we should record its duration.
   */
  runTest(label) {
    if (DEBUG_ALLOCATIONS) {
      if (!this.allocationTracker) {
        this.allocationTracker = this.startAllocationTracker();
      }
      // Flush the current allocations before running the test
      this.allocationTracker.flushAllocations();
    }

    let startLabel = label + ".start";
    performance.mark(startLabel);
    let start = performance.now();

    return {
      done: () => {
        let end = performance.now();
        let duration = end - start;
        performance.measure(label, startLabel);
        this._results.push({
          name: label,
          value: duration
        });

        if (DEBUG_ALLOCATIONS == "normal") {
          this._results.push({
            name: label + ".allocations",
            value: this.allocationTracker.countAllocations()
          });
        } else if (DEBUG_ALLOCATIONS == "verbose") {
          this.allocationTracker.logAllocationSites();
        }
      }
    };
  },

  async addTab(url) {
    let tab = this._win.gBrowser.selectedTab = this._win.gBrowser.addTab(url);
    let browser = tab.linkedBrowser;
    await awaitBrowserLoaded(browser);
    return tab;
  },

  closeCurrentTab() {
    this._win.BrowserCloseTabOrWindow();
    return this._win.gBrowser.selectedTab;
  },

  reloadPage(onReload) {
    return new Promise(resolve => {
      let browser = gBrowser.selectedBrowser;
      if (typeof(onReload) == "function") {
        onReload().then(resolve);
      } else {
        resolve(awaitBrowserLoaded(browser));
      }
      browser.reload();
    });
  },

  async openToolbox(tool = "webconsole", onLoad) {
    let tab = getActiveTab(getMostRecentBrowserWindow());
    let target = TargetFactory.forTab(tab);
    let onToolboxCreated = gDevTools.once("toolbox-created");
    let showPromise = gDevTools.showToolbox(target, tool);
    let toolbox = await onToolboxCreated;

    if (typeof(onLoad) == "function") {
      let panel = await toolbox.getPanelWhenReady(tool);
      await onLoad(toolbox, panel);
    }
    await showPromise;

    return toolbox;
  },

  async closeToolbox() {
    let tab = getActiveTab(getMostRecentBrowserWindow());
    let target = TargetFactory.forTab(tab);
    await target.client.waitForRequestsToSettle();
    await gDevTools.closeToolbox(target);
  },

  async saveHeapSnapshot(label) {
    let tab = getActiveTab(getMostRecentBrowserWindow());
    let target = TargetFactory.forTab(tab);
    let toolbox = gDevTools.getToolbox(target);
    let panel = toolbox.getCurrentPanel();
    let memoryFront = panel.panelWin.gFront;

    let test = this.runTest(label + ".saveHeapSnapshot");
    this._heapSnapshotFilePath = await memoryFront.saveHeapSnapshot();
    test.done();
  },

  readHeapSnapshot(label) {
    let test = this.runTest(label + ".readHeapSnapshot");
    this._snapshot = ChromeUtils.readHeapSnapshot(this._heapSnapshotFilePath);
    test.done();
    return Promise.resolve();
  },

  async waitForNetworkRequests(label, toolbox, expectedRequests) {
    let test = this.runTest(label + ".requestsFinished.DAMP");
    await this.waitForAllRequestsFinished(expectedRequests);
    test.done();
  },

  async _consoleBulkLoggingTest() {
    let TOTAL_MESSAGES = 10;
    let tab = await this.testSetup(SIMPLE_URL);
    let messageManager = tab.linkedBrowser.messageManager;
    let toolbox = await this.openToolbox("webconsole");
    let webconsole = toolbox.getPanel("webconsole");

    // Resolve once the last message has been received.
    let allMessagesReceived = new Promise(resolve => {
      function receiveMessages(messages) {
        for (let m of messages) {
          if (m.node.textContent.includes("damp " + TOTAL_MESSAGES)) {
            webconsole.hud.ui.off("new-messages", receiveMessages);
            // Wait for the console to redraw
            requestAnimationFrame(resolve);
          }
        }
      }
      webconsole.hud.ui.on("new-messages", receiveMessages);
    });

    // Load a frame script using a data URI so we can do logs
    // from the page.  So this is running in content.
    messageManager.loadFrameScript("data:,(" + encodeURIComponent(
      `function () {
        addMessageListener("do-logs", function () {
          for (var i = 0; i < ${TOTAL_MESSAGES}; i++) {
            content.console.log('damp', i+1, content);
          }
        });
      }`
    ) + ")()", true);

    // Kick off the logging
    messageManager.sendAsyncMessage("do-logs");

    let test = this.runTest("console.bulklog");
    await allMessagesReceived;
    test.done();

    await this.closeToolbox();
    await this.testTeardown();
  },

  // Log a stream of console messages, 1 per rAF.  Then record the average
  // time per rAF.  The idea is that the console being slow can slow down
  // content (i.e. Bug 1237368).
  async _consoleStreamLoggingTest() {
    let TOTAL_MESSAGES = 100;
    let tab = await this.testSetup(SIMPLE_URL);
    let messageManager = tab.linkedBrowser.messageManager;
    await this.openToolbox("webconsole");

    // Load a frame script using a data URI so we can do logs
    // from the page.  So this is running in content.
    messageManager.loadFrameScript("data:,(" + encodeURIComponent(
      `function () {
        let count = 0;
        let startTime = content.performance.now();
        function log() {
          if (++count < ${TOTAL_MESSAGES}) {
            content.document.querySelector("h1").textContent += count + "\\n";
            content.console.log('damp', count,
                                content,
                                content.document,
                                content.document.body,
                                content.document.documentElement,
                                new Array(100).join(" DAMP? DAMP! "));
            content.requestAnimationFrame(log);
          } else {
            let avgTime = (content.performance.now() - startTime) / ${TOTAL_MESSAGES};
            sendSyncMessage("done", Math.round(avgTime));
          }
        }
        log();
      }`
    ) + ")()", true);

    let avgTime = await new Promise(resolve => {
      messageManager.addMessageListener("done", (e) => {
        resolve(e.data);
      });
    });

    this._results.push({
      name: "console.streamlog",
      value: avgTime
    });

    await this.closeToolbox();
    await this.testTeardown();
  },

  async _consoleObjectExpansionTest() {
    let tab = await this.testSetup(SIMPLE_URL);
    let messageManager = tab.linkedBrowser.messageManager;
    let toolbox = await this.openToolbox("webconsole");
    let webconsole = toolbox.getPanel("webconsole");

    // Resolve once the first message is received.
    let onMessageReceived = new Promise(resolve => {
      function receiveMessages(messages) {
        for (let m of messages) {
          resolve(m);
        }
      }
      webconsole.hud.ui.once("new-messages", receiveMessages);
    });

    // Load a frame script using a data URI so we can do logs
    // from the page.
    messageManager.loadFrameScript("data:,(" + encodeURIComponent(
      `function () {
        addMessageListener("do-dir", function () {
          content.console.dir(Array.from({length:1000}).reduce((res, _, i)=> {
            res["item_" + i] = i;
            return res;
          }, {}));
        });
      }`
    ) + ")()", true);

    // Kick off the logging
    messageManager.sendAsyncMessage("do-dir");

    let test = this.runTest("console.objectexpand");
    await onMessageReceived;
    const tree = webconsole.hud.ui.outputNode.querySelector(".dir.message .tree");
    // The tree can be collapsed since the properties are fetched asynchronously.
    if (tree.querySelectorAll(".node").length === 1) {
      // If this is the case, we wait for the properties to be fetched and displayed.
      await new Promise(resolve => {
        const observer = new MutationObserver(mutations => {
          resolve(mutations);
          observer.disconnect();
        });
        observer.observe(tree, {
          childList: true
        });
      });
    }

    test.done();

    await this.closeToolboxAndLog("console.objectexpanded", toolbox);
    await this.testTeardown();
  },

async _consoleOpenWithCachedMessagesTest() {
  let TOTAL_MESSAGES = 100;
  let tab = await this.testSetup(SIMPLE_URL);

  // Load a frame script using a data URI so we can do logs
  // from the page.  So this is running in content.
  tab.linkedBrowser.messageManager.loadFrameScript("data:,(" + encodeURIComponent(`
    function () {
      for (var i = 0; i < ${TOTAL_MESSAGES}; i++) {
        content.console.log('damp', i+1, content);
      }
    }`
  ) + ")()", true);

  await this.openToolboxAndLog("console.openwithcache", "webconsole");

  await this.closeToolbox();
  await this.testTeardown();
},

  /**
   * Measure the time necessary to perform successive childList mutations in the content
   * page and update the markup-view accordingly.
   */
  async _inspectorMutationsTest() {
    let tab = await this.testSetup(SIMPLE_URL);
    let messageManager = tab.linkedBrowser.messageManager;
    let toolbox = await this.openToolbox("inspector");
    let inspector = toolbox.getPanel("inspector");

    // Test with n=LIMIT mutations, with t=DELAY ms between each one.
    const LIMIT = 100;
    const DELAY = 5;

    messageManager.loadFrameScript("data:,(" + encodeURIComponent(
      `function () {
        const LIMIT = ${LIMIT};
        addMessageListener("start-mutations-test", function () {
          let addElement = function(index) {
            if (index == LIMIT) {
              // LIMIT was reached, stop adding elements.
              return;
            }
            let div = content.document.createElement("div");
            content.document.body.appendChild(div);
            content.setTimeout(() => addElement(index + 1), ${DELAY});
          };
          addElement(0);
        });
      }`
    ) + ")()", false);

    let test = this.runTest("inspector.mutations");

    await new Promise(resolve => {
      let childListMutationsCounter = 0;
      inspector.on("markupmutation", (evt, mutations) => {
        let childListMutations = mutations.filter(m => m.type === "childList");
        childListMutationsCounter += childListMutations.length;
        if (childListMutationsCounter === LIMIT) {
          // Wait until we received exactly n=LIMIT mutations in the markup view.
          resolve();
        }
      });

      messageManager.sendAsyncMessage("start-mutations-test");
    });

    test.done();

    await this.closeToolbox();
    await this.testTeardown();
  },

  /**
   * Measure the time to open toolbox on the inspector with the layout tab selected.
   */
  async _inspectorLayoutTest() {
    let tab = await this.testSetup(SIMPLE_URL);
    let messageManager = tab.linkedBrowser.messageManager;

    // Backup current sidebar tab preference
    let sidebarTab = Services.prefs.getCharPref("devtools.inspector.activeSidebar");

    // Set layoutview as the current inspector sidebar tab.
    Services.prefs.setCharPref("devtools.inspector.activeSidebar", "layoutview");

    // Setup test page. It is a simple page containing 5000 regular nodes and 10 grid
    // containers.
    await new Promise(resolve => {
      messageManager.addMessageListener("setup-test-done", resolve);

      const NODES = 5000;
      const GRID_NODES = 10;
      messageManager.loadFrameScript("data:,(" + encodeURIComponent(
        `function () {
          let div = content.document.createElement("div");
          div.innerHTML =
            new Array(${NODES}).join("<div></div>") +
            new Array(${GRID_NODES}).join("<div style='display:grid'></div>");
          content.document.body.appendChild(div);
          sendSyncMessage("setup-test-done");
        }`
      ) + ")()", false);
    });

    // Record the time needed to open the toolbox.
    let test = this.runTest("inspector.layout.open");
    await this.openToolbox("inspector");
    test.done();

    await this.closeToolbox();

    // Restore sidebar tab preference.
    Services.prefs.setCharPref("devtools.inspector.activeSidebar", sidebarTab);

    await this.testTeardown();
  },

  takeCensus(label) {
    let test = this.runTest(label + ".takeCensus");

    this._snapshot.takeCensus({
      breakdown: {
        by: "coarseType",
        objects: {
          by: "objectClass",
          then: { by: "count", bytes: true, count: true },
          other: { by: "count", bytes: true, count: true }
        },
        strings: {
          by: "internalType",
          then: { by: "count", bytes: true, count: true }
        },
        scripts: {
          by: "internalType",
          then: { by: "count", bytes: true, count: true }
        },
        other: {
          by: "internalType",
          then: { by: "count", bytes: true, count: true }
        }
      }
    });

    test.done();

    return Promise.resolve();
  },

  /**
   * Wait for any pending paint.
   * The tool may have touched the DOM elements at the very end of the current test.
   * We should ensure waiting for the reflow related to these changes.
   */
  async waitForPendingPaints(toolbox) {
    let panel = toolbox.getCurrentPanel();
    // All panels have its own way of exposing their window object...
    let window = panel.panelWin || panel._frameWindow || panel.panelWindow;

    let utils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDOMWindowUtils);
    window.performance.mark("pending paints.start");
    while (utils.isMozAfterPaintPending) {
      await new Promise(done => {
        window.addEventListener("MozAfterPaint", function listener() {
          window.performance.mark("pending paint");
          done();
        }, { once: true });
      });
    }
    window.performance.measure("pending paints", "pending paints.start");
  },

  async openToolboxAndLog(name, tool, onLoad) {
    dump("Open toolbox on '" + name + "'\n");
    let test = this.runTest(name + ".open.DAMP");
    let toolbox = await this.openToolbox(tool, onLoad);
    test.done();

    test = this.runTest(name + ".open.settle.DAMP");
    await this.waitForPendingPaints(toolbox);
    test.done();

    // Force freeing memory after toolbox open as it creates a lot of objects
    // and for complex documents, it introduces a GC that runs during 'reload' test.
    await garbageCollect();

    return toolbox;
  },

  async closeToolboxAndLog(name, toolbox) {
    let { target } = toolbox;
    dump("Close toolbox on '" + name + "'\n");
    await target.client.waitForRequestsToSettle();

    let test = this.runTest(name + ".close.DAMP");
    await gDevTools.closeToolbox(target);
    test.done();
  },

  async reloadPageAndLog(name, toolbox, onReload) {
    dump("Reload page on '" + name + "'\n");
    let test = this.runTest(name + ".reload.DAMP");
    await this.reloadPage(onReload);
    test.done();

    test = this.runTest(name + ".reload.settle.DAMP");
    await this.waitForPendingPaints(toolbox);
    test.done();
  },

  async exportHar(label, toolbox) {
    let test = this.runTest(label + ".exportHar");

    // Export HAR from the Network panel.
    await toolbox.getHARFromNetMonitor();

    test.done();
  },

  async _coldInspectorOpen() {
    await this.testSetup(SIMPLE_URL);
    await this.openToolboxAndLog("cold.inspector", "inspector");
    await this.closeToolbox();
    await this.testTeardown();
  },

  async _panelsInBackgroundReload() {
    await this.testSetup(PANELS_IN_BACKGROUND);

    // Make sure the Console and Network panels are initialized
    let toolbox = await this.openToolbox("webconsole");
    let monitor = await toolbox.selectTool("netmonitor");

    // Select the options panel to make both the Console and Network
    // panel be in background.
    // Options panel should not do anything on page reload.
    await toolbox.selectTool("options");

    // Reload the page and wait for all HTTP requests
    // to finish (1 doc + 600 XHRs).
    let payloadReady = this.waitForPayload(601, monitor.panelWin);
    await this.reloadPageAndLog("panelsInBackground", toolbox);
    await payloadReady;

    // Clean up
    await this.closeToolbox();
    await this.testTeardown();
  },

  waitForPayload(count, panelWin) {
    return new Promise(resolve => {
      let payloadReady = 0;

      function onPayloadReady(_, id) {
        payloadReady++;
        maybeResolve();
      }

      function maybeResolve() {
        if (payloadReady >= count) {
          panelWin.off(EVENTS.PAYLOAD_READY, onPayloadReady);
          resolve();
        }
      }

      panelWin.on(EVENTS.PAYLOAD_READY, onPayloadReady);
    });
  },

  async reloadInspectorAndLog(label, toolbox) {
    let onReload = async function() {
      let inspector = toolbox.getPanel("inspector");
      // First wait for markup view to be loaded against the new root node
      await inspector.once("new-root");
      // Then wait for inspector to be updated
      await inspector.once("inspector-updated");
    };
    await this.reloadPageAndLog(label + ".inspector", toolbox, onReload);
  },

  async customInspector() {
    let url = CUSTOM_URL.replace(/\$TOOL/, "inspector");
    await this.testSetup(url);
    let toolbox = await this.openToolboxAndLog("custom.inspector", "inspector");

    await this.reloadInspectorAndLog("custom", toolbox);
    await this.selectNodeWithManyRulesAndLog(toolbox);
    await this.closeToolboxAndLog("custom.inspector", toolbox);
    await this.testTeardown();
  },

  /**
   * Measure the time necessary to select a node and display the rule view when many rules
   * match the element.
   */
  async selectNodeWithManyRulesAndLog(toolbox) {
    let inspector = toolbox.getPanel("inspector");

    // Local helper to select a node front and wait for the ruleview to be refreshed.
    let selectNodeFront = (nodeFront) => {
      let onRuleViewRefreshed = inspector.once("rule-view-refreshed");
      inspector.selection.setNodeFront(nodeFront);
      return onRuleViewRefreshed;
    };

    let initialNodeFront = inspector.selection.nodeFront;

    // Retrieve the node front for the test node.
    let root = await inspector.walker.getRootNode();
    let referenceNodeFront = await inspector.walker.querySelector(root, ".no-css-rules");
    let testNodeFront = await inspector.walker.querySelector(root, ".many-css-rules");

    // Select test node and measure the time to display the rule view with many rules.
    dump("Selecting .many-css-rules test node front\n");
    let test = this.runTest("custom.inspector.manyrules.selectnode");
    await selectNodeFront(testNodeFront);
    test.done();

    // Select reference node and measure the time to empty the rule view.
    dump("Move the selection to a node with no rules\n");
    test = this.runTest("custom.inspector.manyrules.deselectnode");
    await selectNodeFront(referenceNodeFront);
    test.done();

    await selectNodeFront(initialNodeFront);
  },

  async openDebuggerAndLog(label, { expectedSources, selectedFile, expectedText }) {
   const onLoad = async (toolbox, panel) => {
    const dbg = await debuggerHelper.createContext(panel);
    await debuggerHelper.waitForSources(dbg, expectedSources);
    await debuggerHelper.selectSource(dbg, selectedFile);
    await debuggerHelper.waitForText(dbg, selectedFile, expectedText);
    await debuggerHelper.waitForMetaData(dbg);
   };
   const toolbox = await this.openToolboxAndLog(label + ".jsdebugger", "jsdebugger", onLoad);
   return toolbox;
  },

  async pauseDebuggerAndLog(tab, toolbox, { testFunction }) {
    const panel = await toolbox.getPanelWhenReady("jsdebugger");
    const dbg = await debuggerHelper.createContext(panel);
    const pauseLocation = { line: 22, file: "App.js" };

    dump("Pausing debugger\n");
    let test = this.runTest("custom.jsdebugger.pause.DAMP");
    await debuggerHelper.pauseDebugger(dbg, tab, testFunction, pauseLocation);
    test.done();

    await debuggerHelper.removeBreakpoints(dbg);
    await debuggerHelper.resume(dbg);
    await garbageCollect();
  },

  async stepDebuggerAndLog(tab, toolbox, { testFunction }) {
    const panel = await toolbox.getPanelWhenReady("jsdebugger");
    const dbg = await debuggerHelper.createContext(panel);
    const stepCount = 2;

    /*
     * Each Step test has a max step count of at least 200;
     * see https://github.com/codehag/debugger-talos-example/blob/master/src/ and the specific test
     * file for more information
     */

    const stepTests = [
      {
        location: { line: 10194, file: "step-in-test.js" },
        key: "stepIn"
      },
      {
        location: { line: 16, file: "step-over-test.js" },
        key: "stepOver"
      },
      {
        location: { line: 998, file: "step-out-test.js" },
        key: "stepOut"
      }
    ];

    for (const stepTest of stepTests) {
      await debuggerHelper.pauseDebugger(dbg, tab, testFunction, stepTest.location);
      const test = this.runTest(`custom.jsdebugger.${stepTest.key}.DAMP`);
      for (let i = 0; i < stepCount; i++) {
        await debuggerHelper.step(dbg, stepTest.key);
      }
      test.done();
      await debuggerHelper.removeBreakpoints(dbg);
      await debuggerHelper.resume(dbg);
      await garbageCollect();
    }
  },

  async reloadDebuggerAndLog(label, toolbox, { expectedSources, selectedFile, expectedText }) {
    const onReload = async () => {
      const panel = await toolbox.getPanelWhenReady("jsdebugger");
      const dbg = await debuggerHelper.createContext(panel);
      await debuggerHelper.waitForDispatch(dbg, "NAVIGATE");
      await debuggerHelper.waitForSources(dbg, expectedSources);
      await debuggerHelper.waitForText(dbg, selectedFile, expectedText);
      await debuggerHelper.waitForMetaData(dbg);
    };
    await this.reloadPageAndLog(`${label}.jsdebugger`, toolbox, onReload);
  },

  async customDebugger() {
    const label = "custom";
    let url = CUSTOM_URL.replace(/\$TOOL/, "debugger");

    const tab = await this.testSetup(url);
    const debuggerTestData = {
      expectedSources: 7,
      testFunction: "window.hitBreakpoint()",
      selectedFile: "App.js",
      expectedText: "import React, { Component } from 'react';"
    };
    const toolbox = await this.openDebuggerAndLog(label, debuggerTestData);
    await this.reloadDebuggerAndLog(label, toolbox, debuggerTestData);

    // these tests are only run on custom.jsdebugger
    await this.pauseDebuggerAndLog(tab, toolbox, debuggerTestData);
    await this.stepDebuggerAndLog(tab, toolbox, debuggerTestData);

    await this.closeToolboxAndLog("custom.jsdebugger", toolbox);
    await this.testTeardown();
  },

  async reloadConsoleAndLog(label, toolbox, expectedMessages) {
    let onReload = async function() {
      let webconsole = toolbox.getPanel("webconsole");
      await new Promise(done => {
        let messages = 0;
        let receiveMessages = () => {
          if (++messages == expectedMessages) {
            webconsole.hud.ui.off("new-messages", receiveMessages);
            done();
          }
        };
        webconsole.hud.ui.on("new-messages", receiveMessages);
      });
    };
    await this.reloadPageAndLog(label + ".webconsole", toolbox, onReload);
  },

  async customConsole() {
    // These numbers controls the number of console api calls we do in the test
    let sync = 250, stream = 250, async = 250;
    let params = `?sync=${sync}&stream=${stream}&async=${async}`;
    let url = CUSTOM_URL.replace(/\$TOOL/, "console") + params;
    await this.testSetup(url);
    let toolbox = await this.openToolboxAndLog("custom.webconsole", "webconsole");
    await this.reloadConsoleAndLog("custom", toolbox, sync + stream + async);
    await this.closeToolboxAndLog("custom.webconsole", toolbox);
    await this.testTeardown();
  },

  _getToolLoadingTests(url, label, {
    expectedMessages,
    expectedRequests,
    debuggerTestData
  }) {
    let tests = {
      async inspector() {
        await this.testSetup(url);
        let toolbox = await this.openToolboxAndLog(label + ".inspector", "inspector");
        await this.reloadInspectorAndLog(label, toolbox);
        await this.closeToolboxAndLog(label + ".inspector", toolbox);
        await this.testTeardown();
      },

      async webconsole() {
        await this.testSetup(url);
        let toolbox = await this.openToolboxAndLog(label + ".webconsole", "webconsole");
        await this.reloadConsoleAndLog(label, toolbox, expectedMessages);
        await this.closeToolboxAndLog(label + ".webconsole", toolbox);
        await this.testTeardown();
      },

      async debugger() {
        await this.testSetup(url);
        let toolbox = await this.openDebuggerAndLog(label, debuggerTestData);
        await this.reloadDebuggerAndLog(label, toolbox, debuggerTestData);
        await this.closeToolboxAndLog(label + ".jsdebugger", toolbox);
        await this.testTeardown();
      },

      async styleeditor() {
        await this.testSetup(url);
        const toolbox = await this.openToolboxAndLog(label + ".styleeditor", "styleeditor");
        await this.reloadPageAndLog(label + ".styleeditor", toolbox);
        await this.closeToolboxAndLog(label + ".styleeditor", toolbox);
        await this.testTeardown();
      },

      async performance() {
        await this.testSetup(url);
        const toolbox = await this.openToolboxAndLog(label + ".performance", "performance");
        await this.reloadPageAndLog(label + ".performance", toolbox);
        await this.closeToolboxAndLog(label + ".performance", toolbox);
        await this.testTeardown();
      },

      async netmonitor() {
        await this.testSetup(url);
        const toolbox = await this.openToolboxAndLog(label + ".netmonitor", "netmonitor");
        const requestsDone = this.waitForNetworkRequests(label + ".netmonitor", toolbox, expectedRequests);
        await this.reloadPageAndLog(label + ".netmonitor", toolbox);
        await requestsDone;
        await this.exportHar(label + ".netmonitor", toolbox);
        await this.closeToolboxAndLog(label + ".netmonitor", toolbox);
        await this.testTeardown();
      },

      async saveAndReadHeapSnapshot() {
        await this.testSetup(url);
        const toolbox = await this.openToolboxAndLog(label + ".memory", "memory");
        await this.reloadPageAndLog(label + ".memory", toolbox);
        await this.saveHeapSnapshot(label);
        await this.readHeapSnapshot(label);
        await this.takeCensus(label);
        await this.closeToolboxAndLog(label + ".memory", toolbox);
        await this.testTeardown();
      },
    };
    // Prefix all tests with the page type (simple or complicated)
    for (let name in tests) {
      tests[label + "." + name] = tests[name];
      delete tests[name];
    }
    return tests;
  },

  async testSetup(url) {
    let tab = await this.addTab(url);
    await new Promise(resolve => {
      setTimeout(resolve, this._config.rest);
    });
    return tab;
  },

  async testTeardown(url) {
    this.closeCurrentTab();

    // Force freeing memory now so that it doesn't happen during the next test
    await garbageCollect();

    this._nextCommand();
  },

  // Everything below here are common pieces needed for the test runner to function,
  // just copy and pasted from Tart with /s/TART/DAMP

  _win: undefined,
  _dampTab: undefined,
  _results: [],
  _config: {subtests: [], repeat: 1, rest: 100},
  _nextCommandIx: 0,
  _commands: [],
  _onSequenceComplete: 0,
  _nextCommand() {
    if (this._nextCommandIx >= this._commands.length) {
      this._onSequenceComplete();
      return;
    }
    this._commands[this._nextCommandIx++].call(this);
  },
  // Each command at the array a function which must call nextCommand once it's done
  _doSequence(commands, onComplete) {
    this._commands = commands;
    this._onSequenceComplete = onComplete;
    this._results = [];
    this._nextCommandIx = 0;

    this._nextCommand();
  },

  _log(str) {
    if (window.MozillaFileLogger && window.MozillaFileLogger.log)
      window.MozillaFileLogger.log(str);

    window.dump(str);
  },

  _logLine(str) {
    return this._log(str + "\n");
  },

  _reportAllResults() {
    var testNames = [];
    var testResults = [];

    var out = "";
    for (var i in this._results) {
      res = this._results[i];
      var disp = [].concat(res.value).map(function(a) { return (isNaN(a) ? -1 : a.toFixed(1)); }).join(" ");
      out += res.name + ": " + disp + "\n";

      if (!Array.isArray(res.value)) { // Waw intervals array is not reported to talos
        testNames.push(res.name);
        testResults.push(res.value);
      }
    }
    this._log("\n" + out);

    if (content && content.tpRecordTime) {
      content.tpRecordTime(testResults.join(","), 0, testNames.join(","));
    } else {
      // alert(out);
    }
  },

  _onTestComplete: null,

  _doneInternal() {
    if (this.allocationTracker) {
      this.allocationTracker.stop();
      this.allocationTracker = null;
    }
    this._logLine("DAMP_RESULTS_JSON=" + JSON.stringify(this._results));
    this._reportAllResults();
    this._win.gBrowser.selectedTab = this._dampTab;

    if (this._onTestComplete) {
      this._onTestComplete(JSON.parse(JSON.stringify(this._results))); // Clone results
    }
  },

  /**
   * Start monitoring all incoming update events about network requests and wait until
   * a complete info about all requests is received. (We wait for the timings info
   * explicitly, because that's always the last piece of information that is received.)
   *
   * This method is designed to wait for network requests that are issued during a page
   * load, when retrieving page resources (scripts, styles, images). It has certain
   * assumptions that can make it unsuitable for other types of network communication:
   * - it waits for at least one network request to start and finish before returning
   * - it waits only for request that were issued after it was called. Requests that are
   *   already in mid-flight will be ignored.
   * - the request start and end times are overlapping. If a new request starts a moment
   *   after the previous one was finished, the wait will be ended in the "interim"
   *   period.
   * @returns a promise that resolves when the wait is done.
   */
  waitForAllRequestsFinished(expectedRequests) {
    let tab = getActiveTab(getMostRecentBrowserWindow());
    let target = TargetFactory.forTab(tab);
    let toolbox = gDevTools.getToolbox(target);
    let window = toolbox.getCurrentPanel().panelWin;

    return new Promise(resolve => {
      // Explicitly waiting for specific number of requests arrived
      let payloadReady = 0;
      let timingsUpdated = 0;

      function onPayloadReady(_, id) {
        payloadReady++;
        maybeResolve();
      }

      function onTimingsUpdated(_, id) {
        timingsUpdated++;
        maybeResolve();
      }

      function maybeResolve() {
        // Have all the requests finished yet?
        if (payloadReady >= expectedRequests && timingsUpdated >= expectedRequests) {
          // All requests are done - unsubscribe from events and resolve!
          window.off(EVENTS.PAYLOAD_READY, onPayloadReady);
          window.off(EVENTS.RECEIVED_EVENT_TIMINGS, onTimingsUpdated);
          resolve();
        }
      }

      window.on(EVENTS.PAYLOAD_READY, onPayloadReady);
      window.on(EVENTS.RECEIVED_EVENT_TIMINGS, onTimingsUpdated);
    });
  },

  startAllocationTracker() {
    const { allocationTracker } = require("devtools/shared/test-helpers/allocation-tracker");
    return allocationTracker();
  },

  startTest(doneCallback, config) {
    this._onTestComplete = function(results) {
      TalosParentProfiler.pause("DAMP - end");
      doneCallback(results);
    };
    this._config = config;

    this._win = Services.wm.getMostRecentWindow("navigator:browser");
    this._dampTab = this._win.gBrowser.selectedTab;
    this._win.gBrowser.selectedBrowser.focus(); // Unfocus the URL bar to avoid caret blink

    TalosParentProfiler.resume("DAMP - start");

    let tests = {};

    // Run cold test only once
    let topWindow = getMostRecentBrowserWindow();
    if (!topWindow.coldRunDAMP) {
      topWindow.coldRunDAMP = true;
      tests["cold.inspector"] = this._coldInspectorOpen;
    }

    tests["panelsInBackground.reload"] = this._panelsInBackgroundReload;

    // Run all tests against "simple" document
    Object.assign(tests, this._getToolLoadingTests(SIMPLE_URL, "simple", {
      expectedMessages: 1,
      expectedRequests: 1,
      debuggerTestData: {
        expectedSources: 1,
        selectedFile: "simple.html",
        expectedText: "This is a simple page"
      }
    }));

    // Run all tests against "complicated" document
    Object.assign(tests, this._getToolLoadingTests(COMPLICATED_URL, "complicated", {
      expectedMessages: 7,
      expectedRequests: 280,
      debuggerTestData: {
        expectedSources: 14,
        selectedFile: "ga.js",
        expectedText: "Math;function ga(a,b){return a.name=b}"
      }
    }));

    // Run all tests against a document specific to each tool
    tests["custom.inspector"] = this.customInspector;
    tests["custom.debugger"] = this.customDebugger;
    tests["custom.webconsole"] = this.customConsole;

    // Run individual tests covering a very precise tool feature.
    tests["console.bulklog"] = this._consoleBulkLoggingTest;
    tests["console.streamlog"] = this._consoleStreamLoggingTest;
    tests["console.objectexpand"] = this._consoleObjectExpansionTest;
    tests["console.openwithcache"] = this._consoleOpenWithCachedMessagesTest;
    tests["inspector.mutations"] = this._inspectorMutationsTest;
    tests["inspector.layout"] = this._inspectorLayoutTest;
    // ⚠  Adding new individual tests slows down DAMP execution ⚠
    // ⚠  Consider contributing to custom.${tool} rather than adding isolated tests ⚠
    // ⚠  See http://docs.firefox-dev.tools/tests/writing-perf-tests.html ⚠

    // Filter tests via `./mach --subtests filter` command line argument
    let filter = Services.prefs.getCharPref("talos.subtests", "");
    if (filter) {
      for (let name in tests) {
        if (!name.includes(filter)) {
          delete tests[name];
        }
      }
      if (Object.keys(tests).length == 0) {
        dump("ERROR: Unable to find any test matching '" + filter + "'\n");
        this._doneInternal();
        return;
      }
    }

    // Construct the sequence array while filtering tests
    let sequenceArray = [];
    for (var i in config.subtests) {
      for (var r = 0; r < config.repeat; r++) {
        if (!config.subtests[i] || !tests[config.subtests[i]]) {
          continue;
        }
        sequenceArray.push(tests[config.subtests[i]]);
      }
    }

    // Free memory before running the first test, otherwise we may have a GC
    // related to Firefox startup or DAMP setup during the first test.
    garbageCollect().then(() => {
      this._doSequence(sequenceArray, this._doneInternal);
    }).catch(e => {
      dump("Exception while running DAMP tests: " + e + "\n" + e.stack + "\n");
    });
  }
};
