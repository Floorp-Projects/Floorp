Components.utils.import("resource://devtools/client/framework/gDevTools.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const {devtools} =
  Components.utils.import("resource://devtools/shared/Loader.jsm", {});
const { getActiveTab } = devtools.require("sdk/tabs/utils");
const { getMostRecentBrowserWindow } = devtools.require("sdk/window/utils");
const ThreadSafeChromeUtils = devtools.require("ThreadSafeChromeUtils");
const {Task} = Cu.import("resource://gre/modules/Task.jsm", {});

const webserver = Services.prefs.getCharPref("addon.test.damp.webserver");

const SIMPLE_URL = "chrome://damp/content/pages/simple.html";
const COMPLICATED_URL = webserver + "/tests/tp5n/bild.de/www.bild.de/index.html";

function Damp() {
  // Path to the temp file where the heap snapshot file is saved. Set by
  // saveHeapSnapshot and read by readHeapSnapshot.
  this._heapSnapshotFilePath = null;
  // HeapSnapshot instance. Set by readHeapSnapshot, used by takeCensus.
  this._snapshot = null;
}

Damp.prototype = {

  addTab: function(url) {
    return new Promise((resolve, reject) => {
      let tab = this._win.gBrowser.selectedTab = this._win.gBrowser.addTab(url);
      let browser = tab.linkedBrowser;
      browser.addEventListener("load", function onload() {
        browser.removeEventListener("load", onload, true);
        resolve(tab);
      }, true);
    });
  },

  closeCurrentTab: function() {
    this._win.BrowserCloseTabOrWindow();
    return this._win.gBrowser.selectedTab;
  },

  reloadPage: function(name) {
    let startReloadTimestamp = performance.now();
    return new Promise((resolve, reject) => {
      let browser = gBrowser.selectedBrowser;
      let self = this;
      browser.addEventListener("load", function onload() {
        browser.removeEventListener("load", onload, true);
        let stopReloadTimestamp = performance.now();
        self._results.push({name: name + ".reload.DAMP", value: stopReloadTimestamp - startReloadTimestamp});
        resolve();
      }, true);
      browser.reload();
    });
  },

  openToolbox: function (name, tool = "webconsole") {
    let tab = getActiveTab(getMostRecentBrowserWindow());
    let target = devtools.TargetFactory.forTab(tab);
    let startRecordTimestamp = performance.now();
    let showPromise = gDevTools.showToolbox(target, tool);

    return showPromise.then(toolbox => {
      let stopRecordTimestamp = performance.now();
      this._results.push({name: name + ".open.DAMP", value: stopRecordTimestamp - startRecordTimestamp});

      return toolbox;
    });
  },

  closeToolbox: function(name) {
    let tab = getActiveTab(getMostRecentBrowserWindow());
    let target = devtools.TargetFactory.forTab(tab);
    let startRecordTimestamp = performance.now();
    let closePromise = gDevTools.closeToolbox(target);
    return closePromise.then(() => {
      let stopRecordTimestamp = performance.now();
      this._results.push({name: name + ".close.DAMP", value: stopRecordTimestamp - startRecordTimestamp});
    });
  },

  saveHeapSnapshot: function(label) {
    let tab = getActiveTab(getMostRecentBrowserWindow());
    let target = devtools.TargetFactory.forTab(tab);
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

  readHeapSnapshot: function(label) {
    let start = performance.now();
    this._snapshot = ThreadSafeChromeUtils.readHeapSnapshot(this._heapSnapshotFilePath);
    let end = performance.now();
    this._results.push({
      name: label + ".readHeapSnapshot",
      value: end - start
    });
    return Promise.resolve();
  },

  takeCensus: function(label) {
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

  _getToolLoadingTests: function(url, label) {
    let subtests = {
      webconsoleOpen: Task.async(function*() {
        yield this.testSetup(url);
        yield this.openToolbox(label + ".webconsole", "webconsole");
        yield this.reloadPage(label + ".webconsole");
        yield this.closeToolbox(label + ".webconsole");
        yield this.testTeardown();
      }),

      inspectorOpen: Task.async(function*() {
        yield this.testSetup(url);
        yield this.openToolbox(label + ".inspector", "inspector");
        yield this.reloadPage(label + ".inspector");
        yield this.closeToolbox(label + ".inspector");
        yield this.testTeardown();
      }),

      debuggerOpen: Task.async(function*() {
        yield this.testSetup(url);
        yield this.openToolbox(label + ".jsdebugger", "jsdebugger");
        yield this.reloadPage(label + ".jsdebugger");
        yield this.closeToolbox(label + ".jsdebugger");
        yield this.testTeardown();
      }),

      styleEditorOpen: Task.async(function*() {
        yield this.testSetup(url);
        yield this.openToolbox(label + ".styleeditor", "styleeditor");
        yield this.reloadPage(label + ".styleeditor");
        yield this.closeToolbox(label + ".styleeditor");
        yield this.testTeardown();
      }),

      performanceOpen: Task.async(function*() {
        yield this.testSetup(url);
        yield this.openToolbox(label + ".performance", "performance");
        yield this.reloadPage(label + ".performance");
        yield this.closeToolbox(label + ".performance");
        yield this.testTeardown();
      }),

      netmonitorOpen: Task.async(function*() {
        yield this.testSetup(url);
        yield this.openToolbox(label + ".netmonitor", "netmonitor");
        yield this.reloadPage(label + ".netmonitor");
        yield this.closeToolbox(label + ".netmonitor");
        yield this.testTeardown();
      }),

      saveAndReadHeapSnapshot: Task.async(function*() {
        yield this.testSetup(url);
        yield this.openToolbox(label + ".memory", "memory");
        yield this.reloadPage(label + ".memory");
        yield this.saveHeapSnapshot(label);
        yield this.readHeapSnapshot(label);
        yield this.takeCensus(label);
        yield this.closeToolbox(label + ".memory");
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

  testSetup: Task.async(function*(url) {
    yield this.addTab(url);
    yield new Promise(resolve => {
      setTimeout(resolve, this._config.rest);
    });
  }),

  testTeardown: Task.async(function*(url) {
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
  _nextCommand: function() {
    if (this._nextCommandIx >= this._commands.length) {
      this._onSequenceComplete();
      return;
    }
    this._commands[this._nextCommandIx++].call(this);
  },
  // Each command at the array a function which must call nextCommand once it's done
  _doSequence: function(commands, onComplete) {
    this._commands = commands;
    this._onSequenceComplete = onComplete;
    this._results = [];
    this._nextCommandIx = 0;

    this._nextCommand();
  },

  _log: function(str) {
    if (window.MozillaFileLogger && window.MozillaFileLogger.log)
      window.MozillaFileLogger.log(str);

    window.dump(str);
  },

  _logLine: function(str) {
    return this._log(str + "\n");
  },

  _reportAllResults: function() {
    var testNames = [];
    var testResults = [];

    var out = "";
    for (var i in this._results) {
      res = this._results[i];
      var disp = [].concat(res.value).map(function(a){return (isNaN(a) ? -1 : a.toFixed(1));}).join(" ");
      out += res.name + ": " + disp + "\n";

      if (!Array.isArray(res.value)) { // Waw intervals array is not reported to talos
        testNames.push(res.name);
        testResults.push(res.value);
      }
    }
    this._log("\n" + out);

    if (content && content.tpRecordTime) {
      content.tpRecordTime(testResults.join(','), 0, testNames.join(','));
    } else {
      //alert(out);
    }
  },

  _onTestComplete: null,

  _doneInternal: function() {
    this._logLine("DAMP_RESULTS_JSON=" + JSON.stringify(this._results));
    this._reportAllResults();
    this._win.gBrowser.selectedTab = this._dampTab;

    if (this._onTestComplete) {
      this._onTestComplete(JSON.parse(JSON.stringify(this._results))); // Clone results
    }
  },

  startTest: function(doneCallback, config) {
    this._onTestComplete = function (results) {
      Profiler.mark("DAMP - end", true);
      doneCallback(results);
    };
    this._config = config;

    const Ci = Components.interfaces;
    var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator);
    this._win = wm.getMostRecentWindow("navigator:browser");
    this._dampTab = this._win.gBrowser.selectedTab;
    this._win.gBrowser.selectedBrowser.focus(); // Unfocus the URL bar to avoid caret blink

    Profiler.mark("DAMP - start", true);

    let tests = [];
    tests = tests.concat(this._getToolLoadingTests(SIMPLE_URL, "simple"));
    tests = tests.concat(this._getToolLoadingTests(COMPLICATED_URL, "complicated"));
    this._doSequence(tests, this._doneInternal);
  }
}
