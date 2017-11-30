const { Services } = Components.utils.import("resource://gre/modules/Services.jsm", {});
const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});
const gMgr = Cc["@mozilla.org/memory-reporter-manager;1"].getService(Ci.nsIMemoryReporterManager);

XPCOMUtils.defineLazyGetter(this, "require", function() {
  let { require } =
    Components.utils.import("resource://devtools/shared/Loader.jsm", {});
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
XPCOMUtils.defineLazyGetter(this, "ChromeUtils", function() {
  return require("ChromeUtils");
});

const webserver = Services.prefs.getCharPref("addon.test.damp.webserver");

const SIMPLE_URL = webserver + "/tests/devtools/addon/content/pages/simple.html";
const COMPLICATED_URL = webserver + "/tests/tp5n/bild.de/www.bild.de/index.html";
const CUSTOM_URL = webserver + "/tests/devtools/addon/content/pages/custom/$TOOL.html";

function getMostRecentBrowserWindow() {
  return Services.wm.getMostRecentWindow("navigator:browser");
}

function getActiveTab(window) {
  return window.gBrowser.selectedTab;
}

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
      }
    };
  },

  addTab(url) {
    return new Promise((resolve, reject) => {
      let tab = this._win.gBrowser.selectedTab = this._win.gBrowser.addTab(url);
      let browser = tab.linkedBrowser;
      browser.addEventListener("load", function onload() {
        resolve(tab);
      }, {capture: true, once: true});
    });
  },

  closeCurrentTab() {
    this._win.BrowserCloseTabOrWindow();
    return this._win.gBrowser.selectedTab;
  },

  reloadPage(onReload) {
    return new Promise(resolve => {
      let browser = gBrowser.selectedBrowser;
      if (typeof (onReload) == "function") {
        onReload().then(resolve);
      } else {
        browser.addEventListener("load", resolve, {capture: true, once: true});
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

  async waitForNetworkRequests(label, toolbox) {
    let test = this.runTest(label + ".requestsFinished.DAMP");
    await this.waitForAllRequestsFinished();
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
      function receiveMessages(e, messages) {
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
      function receiveMessages(e, messages) {
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

    await this.closeToolboxAndLog("console.objectexpanded");
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

    // Open the toolbox and record the time.
    let start = performance.now();
    await this.openToolbox("inspector");
    this._results.push({
      name: "inspector.layout.open",
      value: performance.now() - start
    });

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

  async openToolboxAndLog(name, tool, onLoad) {
    dump("Open toolbox on '" + name + "'\n");
    let test = this.runTest(name + ".open.DAMP");
    let toolbox = await this.openToolbox(tool, onLoad);
    test.done();

    // Force freeing memory after toolbox open as it creates a lot of objects
    // and for complex documents, it introduces a GC that runs during 'reload' test.
    await garbageCollect();

    return toolbox;
  },

  async closeToolboxAndLog(name) {
    dump("Close toolbox on '" + name + "'\n");
    let tab = getActiveTab(getMostRecentBrowserWindow());
    let target = TargetFactory.forTab(tab);
    await target.client.waitForRequestsToSettle();

    let test = this.runTest(name + ".close.DAMP");
    await gDevTools.closeToolbox(target);
    test.done();
  },

  async reloadPageAndLog(name, onReload) {
    dump("Reload page on '" + name + "'\n");
    let test = this.runTest(name + ".reload.DAMP");
    await this.reloadPage(onReload);
    test.done();
  },

  async _coldInspectorOpen() {
    await this.testSetup(SIMPLE_URL);
    await this.openToolboxAndLog("cold.inspector", "inspector");
    await this.closeToolbox();
    await this.testTeardown();
  },

  async _panelsInBackgroundReload() {
    let url = "data:text/html;charset=UTF-8," + encodeURIComponent(`
      <script>
      // Log a significant amount of messages
      for(let i = 0; i < 2000; i++) {
        console.log("log in background", i);
      }
      </script>
    `);
    await this.testSetup(url);
    let toolbox = await this.openToolbox("webconsole");

    // Select the options panel to make the console be in background.
    // Options panel should not do anything on page reload.
    await toolbox.selectTool("options");

    await this.reloadPageAndLog("panelsInBackground");

    await this.closeToolbox();
    await this.testTeardown();
  },

  async reloadInspectorAndLog(label, toolbox) {
    let onReload = async function() {
      let inspector = toolbox.getPanel("inspector");
      // First wait for markup view to be loaded against the new root node
      await inspector.once("new-root");
      // Then wait for inspector to be updated
      await inspector.once("inspector-updated");
    };
    await this.reloadPageAndLog(label + ".inspector", onReload);
  },

  async customInspector() {
    let url = CUSTOM_URL.replace(/\$TOOL/, "inspector");
    await this.testSetup(url);
    let toolbox = await this.openToolboxAndLog("custom.inspector", "inspector");
    await this.reloadInspectorAndLog("custom", toolbox);
    await this.closeToolboxAndLog("custom.inspector");
    await this.testTeardown();
  },

  _getToolLoadingTests(url, label, { expectedMessages, expectedSources }) {
    let tests = {
      async inspector() {
        await this.testSetup(url);
        let toolbox = await this.openToolboxAndLog(label + ".inspector", "inspector");
        await this.reloadInspectorAndLog(label, toolbox);
        await this.closeToolboxAndLog(label + ".inspector");
        await this.testTeardown();
      },

      async webconsole() {
        await this.testSetup(url);
        let toolbox = await this.openToolboxAndLog(label + ".webconsole", "webconsole");
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
        await this.reloadPageAndLog(label + ".webconsole", onReload);
        await this.closeToolboxAndLog(label + ".webconsole");
        await this.testTeardown();
      },

      async debugger() {
        await this.testSetup(url);
        let onLoad = async function(toolbox, dbg) {
          await new Promise(done => {
            let { selectors, store } = dbg.panelWin.getGlobalsForTesting();
            let unsubscribe;
            function countSources() {
              const sources = selectors.getSources(store.getState());
              if (sources.size >= expectedSources) {
                unsubscribe();
                done();
              }
            }
            unsubscribe = store.subscribe(countSources);
            countSources();
          });
        };
        let toolbox = await this.openToolboxAndLog(label + ".jsdebugger", "jsdebugger", onLoad);
        let onReload = async function() {
          await new Promise(done => {
            let count = 0;
            let { client } = toolbox.target;
            let onSource = async (_, actor) => {
              if (++count >= expectedSources) {
                client.removeListener("newSource", onSource);
                done();
              }
            };
            client.addListener("newSource", onSource);
          });
        };
        await this.reloadPageAndLog(label + ".jsdebugger", onReload);
        await this.closeToolboxAndLog(label + ".jsdebugger");
        await this.testTeardown();
      },

      async styleeditor() {
        await this.testSetup(url);
        await this.openToolboxAndLog(label + ".styleeditor", "styleeditor");
        await this.reloadPageAndLog(label + ".styleeditor");
        await this.closeToolboxAndLog(label + ".styleeditor");
        await this.testTeardown();
      },

      async performance() {
        await this.testSetup(url);
        await this.openToolboxAndLog(label + ".performance", "performance");
        await this.reloadPageAndLog(label + ".performance");
        await this.closeToolboxAndLog(label + ".performance");
        await this.testTeardown();
      },

      async netmonitor() {
        await this.testSetup(url);
        const toolbox = await this.openToolboxAndLog(label + ".netmonitor", "netmonitor");
        const requestsDone = this.waitForNetworkRequests(label + ".netmonitor", toolbox);
        await this.reloadPageAndLog(label + ".netmonitor");
        await requestsDone;
        await this.closeToolboxAndLog(label + ".netmonitor");
        await this.testTeardown();
      },

      async saveAndReadHeapSnapshot() {
        await this.testSetup(url);
        await this.openToolboxAndLog(label + ".memory", "memory");
        await this.reloadPageAndLog(label + ".memory");
        await this.saveHeapSnapshot(label);
        await this.readHeapSnapshot(label);
        await this.takeCensus(label);
        await this.closeToolboxAndLog(label + ".memory");
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
  waitForAllRequestsFinished() {
    let tab = getActiveTab(getMostRecentBrowserWindow());
    let target = TargetFactory.forTab(tab);
    let toolbox = gDevTools.getToolbox(target);
    let window = toolbox.getCurrentPanel().panelWin;

    return new Promise(resolve => {
      // Key is the request id, value is a boolean - is request finished or not?
      let requests = new Map();

      function onRequest(_, id) {
        requests.set(id, false);
      }

      function onPayloadReady(_, id) {
        requests.set(id, true);
        maybeResolve();
      }

      function maybeResolve() {
        // Have all the requests in the map finished yet?
        if (![...requests.values()].every(finished => finished)) {
          return;
        }

        // All requests are done - unsubscribe from events and resolve!
        window.off(EVENTS.NETWORK_EVENT, onRequest);
        window.off(EVENTS.PAYLOAD_READY, onPayloadReady);
        resolve();
      }

      window.on(EVENTS.NETWORK_EVENT, onRequest);
      window.on(EVENTS.PAYLOAD_READY, onPayloadReady);
    });
  },

  startTest(doneCallback, config) {
    this._onTestComplete = function(results) {
      TalosParentProfiler.pause("DAMP - end");
      doneCallback(results);
    };
    this._config = config;

    const Ci = Components.interfaces;
    var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator);
    this._win = wm.getMostRecentWindow("navigator:browser");
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
      expectedSources: 1,
    }));

    // Run all tests against "complicated" document
    Object.assign(tests, this._getToolLoadingTests(COMPLICATED_URL, "complicated", {
      expectedMessages: 7,
      expectedSources: 14,
    }));

    // Run all tests against a document specific to each tool
    tests["custom.inspector"] = this.customInspector;

    // Run individual tests covering a very precise tool feature
    tests["console.bulklog"] = this._consoleBulkLoggingTest;
    tests["console.streamlog"] = this._consoleStreamLoggingTest;
    tests["console.objectexpand"] = this._consoleObjectExpansionTest;
    tests["console.openwithcache"] = this._consoleOpenWithCachedMessagesTest;
    tests["inspector.mutations"] = this._inspectorMutationsTest;
    tests["inspector.layout"] = this._inspectorLayoutTest;

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
    });
  }
};
