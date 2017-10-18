const { Services } = Components.utils.import("resource://gre/modules/Services.jsm", {});
const { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});

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
XPCOMUtils.defineLazyGetter(this, "ThreadSafeChromeUtils", function() {
  return require("ThreadSafeChromeUtils");
});

const webserver = Services.prefs.getCharPref("addon.test.damp.webserver");

const SIMPLE_URL = webserver + "/tests/devtools/addon/content/pages/simple.html";
const COMPLICATED_URL = webserver + "/tests/tp5n/bild.de/www.bild.de/index.html";

function getMostRecentBrowserWindow() {
  return Services.wm.getMostRecentWindow("navigator:browser");
}

function getActiveTab(window) {
  return window.gBrowser.selectedTab;
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
    let startReloadTimestamp = performance.now();
    return new Promise((resolve, reject) => {
      let browser = gBrowser.selectedBrowser;
      if (typeof (onReload) == "function") {
        onReload().then(function() {
          let stopReloadTimestamp = performance.now();
          resolve({
            time: stopReloadTimestamp - startReloadTimestamp
          });
        });
      } else {
        browser.addEventListener("load", function onload() {
          let stopReloadTimestamp = performance.now();
          resolve({
            time: stopReloadTimestamp - startReloadTimestamp
          });
        }, {capture: true, once: true});
      }
      browser.reload();

    });
  },

  async openToolbox(tool = "webconsole", onLoad) {
    let tab = getActiveTab(getMostRecentBrowserWindow());
    let target = TargetFactory.forTab(tab);
    let startRecordTimestamp = performance.now();
    let onToolboxCreated = gDevTools.once("toolbox-created");
    let showPromise = gDevTools.showToolbox(target, tool);
    let toolbox = await onToolboxCreated;

    if (typeof(onLoad) == "function") {
      let panel = await toolbox.getPanelWhenReady(tool);
      await onLoad(toolbox, panel);
    }
    await showPromise;

    let stopRecordTimestamp = performance.now();
    return {
      toolbox,
      time: stopRecordTimestamp - startRecordTimestamp
    };
  },

  closeToolbox: Task.async(function* () {
    let tab = getActiveTab(getMostRecentBrowserWindow());
    let target = TargetFactory.forTab(tab);
    yield target.client.waitForRequestsToSettle();
    let startRecordTimestamp = performance.now();
    yield gDevTools.closeToolbox(target);
    let stopRecordTimestamp = performance.now();
    return {
      time: stopRecordTimestamp - startRecordTimestamp
    };
  }),

  saveHeapSnapshot(label) {
    let tab = getActiveTab(getMostRecentBrowserWindow());
    let target = TargetFactory.forTab(tab);
    let toolbox = gDevTools.getToolbox(target);
    let panel = toolbox.getCurrentPanel();
    let memoryFront = panel.panelWin.gFront;

    let start = performance.now();
    return memoryFront.saveHeapSnapshot().then(filePath => {
      this._heapSnapshotFilePath = filePath;
      let end = performance.now();
      this._results.push({
        name: label + ".saveHeapSnapshot",
        value: end - start
      });
    });
  },

  readHeapSnapshot(label) {
    let start = performance.now();
    this._snapshot = ThreadSafeChromeUtils.readHeapSnapshot(this._heapSnapshotFilePath);
    let end = performance.now();
    this._results.push({
      name: label + ".readHeapSnapshot",
      value: end - start
    });
    return Promise.resolve();
  },

  waitForNetworkRequests: Task.async(function* (label, toolbox) {
    const start = performance.now();
    yield this.waitForAllRequestsFinished();
    const end = performance.now();
    this._results.push({
      name: label + ".requestsFinished.DAMP",
      value: end - start
    });
  }),

  _consoleBulkLoggingTest: Task.async(function* () {
    let TOTAL_MESSAGES = 10;
    let tab = yield this.testSetup(SIMPLE_URL);
    let messageManager = tab.linkedBrowser.messageManager;
    let {toolbox} = yield this.openToolbox("webconsole");
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

    let start = performance.now();
    yield allMessagesReceived;
    let end = performance.now();

    this._results.push({
      name: "console.bulklog",
      value: end - start
    });

    yield this.closeToolbox(null);
    yield this.testTeardown();
  }),

  // Log a stream of console messages, 1 per rAF.  Then record the average
  // time per rAF.  The idea is that the console being slow can slow down
  // content (i.e. Bug 1237368).
  _consoleStreamLoggingTest: Task.async(function* () {
    let TOTAL_MESSAGES = 100;
    let tab = yield this.testSetup(SIMPLE_URL);
    let messageManager = tab.linkedBrowser.messageManager;
    yield this.openToolbox("webconsole");

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

    let avgTime = yield new Promise(resolve => {
      messageManager.addMessageListener("done", (e) => {
        resolve(e.data);
      });
    });

    this._results.push({
      name: "console.streamlog",
      value: avgTime
    });

    yield this.closeToolbox(null);
    yield this.testTeardown();
  }),

  _consoleObjectExpansionTest: Task.async(function* () {
    let tab = yield this.testSetup(SIMPLE_URL);
    let messageManager = tab.linkedBrowser.messageManager;
    let {toolbox} = yield this.openToolbox("webconsole");
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

    let start = performance.now();
    yield onMessageReceived;
    const tree = webconsole.hud.ui.outputNode.querySelector(".dir.message .tree");
    // The tree can be collapsed since the properties are fetched asynchronously.
    if (tree.querySelectorAll(".node").length === 1) {
      // If this is the case, we wait for the properties to be fetched and displayed.
      yield new Promise(resolve => {
        const observer = new MutationObserver(mutations => {
          resolve(mutations);
          observer.disconnect();
        });
        observer.observe(tree, {
          childList: true
        });
      });
    }

    this._results.push({
      name: "console.objectexpand",
      value: performance.now() - start,
    });

    yield this.closeToolboxAndLog("console.objectexpanded");
    yield this.testTeardown();
  }),

  takeCensus(label) {
    let start = performance.now();

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

    let end = performance.now();

    this._results.push({
      name: label + ".takeCensus",
      value: end - start
    });

    return Promise.resolve();
  },

  async openToolboxAndLog(name, tool, onLoad) {
    dump("Open toolbox on '" + name + "'\n");
    let {time, toolbox} = await this.openToolbox(tool, onLoad);
    this._results.push({name: name + ".open.DAMP", value: time });
    return toolbox;
  },

  async closeToolboxAndLog(name) {
    dump("Close toolbox on '" + name + "'\n");
    let {time} = await this.closeToolbox();
    this._results.push({name: name + ".close.DAMP", value: time });
  },

  async reloadPageAndLog(name, onReload) {
    dump("Reload page on '" + name + "'\n");
    let {time} = await this.reloadPage(onReload);
    this._results.push({name: name + ".reload.DAMP", value: time });
  },

  async _coldInspectorOpen() {
    await this.testSetup(SIMPLE_URL);
    await this.openToolboxAndLog("cold.inspector", "inspector");
    await this.closeToolbox();
    await this.testTeardown();
  },

  _getToolLoadingTests(url, label, { expectedMessages, expectedSources }) {
    let subtests = {
      inspectorOpen: Task.async(function* () {
        yield this.testSetup(url);
        let toolbox = yield this.openToolboxAndLog(label + ".inspector", "inspector");
        let onReload = async function() {
          let inspector = toolbox.getPanel("inspector");
          // First wait for markup view to be loaded against the new root node
          await inspector.once("new-root");
          // Then wait for inspector to be updated
          await inspector.once("inspector-updated");
        };
        yield this.reloadPageAndLog(label + ".inspector", onReload);
        yield this.closeToolboxAndLog(label + ".inspector");
        yield this.testTeardown();
      }),

      webconsoleOpen: Task.async(function* () {
        yield this.testSetup(url);
        let toolbox = yield this.openToolboxAndLog(label + ".webconsole", "webconsole");
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
        yield this.reloadPageAndLog(label + ".webconsole", onReload);
        yield this.closeToolboxAndLog(label + ".webconsole");
        yield this.testTeardown();
      }),

      debuggerOpen: Task.async(function* () {
        yield this.testSetup(url);
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
        let toolbox = yield this.openToolboxAndLog(label + ".jsdebugger", "jsdebugger", onLoad);
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
        yield this.reloadPageAndLog(label + ".jsdebugger", onReload);
        yield this.closeToolboxAndLog(label + ".jsdebugger");
        yield this.testTeardown();
      }),

      styleEditorOpen: Task.async(function* () {
        yield this.testSetup(url);
        yield this.openToolboxAndLog(label + ".styleeditor", "styleeditor");
        yield this.reloadPageAndLog(label + ".styleeditor");
        yield this.closeToolboxAndLog(label + ".styleeditor");
        yield this.testTeardown();
      }),

      performanceOpen: Task.async(function* () {
        yield this.testSetup(url);
        yield this.openToolboxAndLog(label + ".performance", "performance");
        yield this.reloadPageAndLog(label + ".performance");
        yield this.closeToolboxAndLog(label + ".performance");
        yield this.testTeardown();
      }),

      netmonitorOpen: Task.async(function* () {
        yield this.testSetup(url);
        const toolbox = yield this.openToolboxAndLog(label + ".netmonitor", "netmonitor");
        const requestsDone = this.waitForNetworkRequests(label + ".netmonitor", toolbox);
        yield this.reloadPageAndLog(label + ".netmonitor");
        yield requestsDone;
        yield this.closeToolboxAndLog(label + ".netmonitor");
        yield this.testTeardown();
      }),

      saveAndReadHeapSnapshot: Task.async(function* () {
        yield this.testSetup(url);
        yield this.openToolboxAndLog(label + ".memory", "memory");
        yield this.reloadPageAndLog(label + ".memory");
        yield this.saveHeapSnapshot(label);
        yield this.readHeapSnapshot(label);
        yield this.takeCensus(label);
        yield this.closeToolboxAndLog(label + ".memory");
        yield this.testTeardown();
      }),
    };

    // Construct the sequence array: config.repeat times config.subtests
    let config = this._config;
    let sequenceArray = [];
    for (var i in config.subtests) {
      for (var r = 0; r < config.repeat; r++) {
        if (!config.subtests[i] || !subtests[config.subtests[i]]) {
          continue;
        }

        sequenceArray.push(subtests[config.subtests[i]]);
      }
    }

    return sequenceArray;
  },

  testSetup: Task.async(function* (url) {
    let tab = yield this.addTab(url);
    yield new Promise(resolve => {
      setTimeout(resolve, this._config.rest);
    });
    return tab;
  }),

  testTeardown: Task.async(function* (url) {
    this.closeCurrentTab();
    this._nextCommand();
  }),

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

      function onTimings(_, id) {
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
        window.off(EVENTS.RECEIVED_EVENT_TIMINGS, onTimings);
        resolve();
      }

      window.on(EVENTS.NETWORK_EVENT, onRequest);
      window.on(EVENTS.RECEIVED_EVENT_TIMINGS, onTimings);
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

    let tests = [];
    if (config.subtests.indexOf("inspectorOpen") > -1) {
      // Run cold test only once
      let topWindow = getMostRecentBrowserWindow();
      if (!topWindow.coldRunDAMP) {
        topWindow.coldRunDAMP = true;
        tests = tests.concat(this._coldInspectorOpen);
      }
    }

    tests = tests.concat(this._getToolLoadingTests(SIMPLE_URL, "simple", {
      expectedMessages: 1,
      expectedSources: 1,
    }));

    tests = tests.concat(this._getToolLoadingTests(COMPLICATED_URL, "complicated", {
      expectedMessages: 7,
      expectedSources: 14,
    }));

    if (config.subtests.indexOf("consoleBulkLogging") > -1) {
      tests = tests.concat(this._consoleBulkLoggingTest);
    }
    if (config.subtests.indexOf("consoleStreamLogging") > -1) {
      tests = tests.concat(this._consoleStreamLoggingTest);
    }
    if (config.subtests.indexOf("consoleObjectExpansion") > -1) {
      tests = tests.concat(this._consoleObjectExpansionTest);
    }

    this._doSequence(tests, this._doneInternal);
  }
}
