
Components.utils.import("resource:///modules/devtools/client/framework/gDevTools.jsm");
const {devtools} =
  Components.utils.import("resource://gre/modules/devtools/shared/Loader.jsm", {});
const { getActiveTab } = devtools.require("sdk/tabs/utils");
const { getMostRecentBrowserWindow } = devtools.require("sdk/window/utils");

const SIMPLE_URL = "chrome://damp/content/pages/simple.html";
const COMPLICATED_URL = "http://localhost/page_load_test/tp5n/bild.de/www.bild.de/index.html";

function Damp() {
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

  _startTest: function() {

    var self = this;
    var openToolbox = this.openToolbox.bind(this);
    var closeToolbox = this.closeToolbox.bind(this);
    var reloadPage = this.reloadPage.bind(this);
    var next = this._nextCommand.bind(this);
    var config = this._config;
    var rest = config.rest; // How long to wait in between opening the tab and starting the test.

    let tests = getTestsForURL(SIMPLE_URL, "simple");
    tests = tests.concat(getTestsForURL(COMPLICATED_URL, "complicated"));

    this._doSequence(tests, this._doneInternal);

    function getTestsForURL(url, label) {

      // This is called before each subtest
      let init = [
        () => { self.addTab(url).then(next); },
        () => { setTimeout(next, rest); },
      ];

      // This is called after each subtest
      let restore = [
        () => { self.closeCurrentTab(); next(); }
      ];

      var subtests = {

        webconsoleOpen: [
          () => { openToolbox(label + ".webconsole", "webconsole").then(next); },
          () => { reloadPage(label + ".webconsole").then(next); },
          () => { closeToolbox(label + ".webconsole").then(next); },
        ],

        inspectorOpen: [
          () => { openToolbox(label + ".inspector", "inspector").then(next); },
          () => { reloadPage(label + ".inspector").then(next); },
          () => { closeToolbox(label + ".inspector").then(next); },
        ],

        debuggerOpen: [
          () => { openToolbox(label + ".jsdebugger", "jsdebugger").then(next); },
          () => { reloadPage(label + ".jsdebugger").then(next); },
          () => { closeToolbox(label + ".jsdebugger").then(next); },
        ],

        styleEditorOpen: [
          () => { openToolbox(label + ".styleeditor", "styleeditor").then(next); },
          () => { reloadPage(label + ".styleeditor").then(next); },
          () => { closeToolbox(label + ".styleeditor").then(next); },
        ],

        performanceOpen: [
          () => { openToolbox(label + ".performance", "performance").then(next); },
          () => { reloadPage(label + ".performance").then(next); },
          () => { closeToolbox(label + ".performance").then(next); },
        ],

        netmonitorOpen: [
          () => { openToolbox(label + ".netmonitor", "netmonitor").then(next); },
          () => { reloadPage(label + ".netmonitor").then(next); },
          () => { closeToolbox(label + ".netmonitor").then(next); },
        ],

      };

      // Construct the sequence array: config.repeat times config.subtests,
      // where each subtest implicitly starts with init.
      sequenceArray = [];
      for (var i in config.subtests) {
        for (var r = 0; r < config.repeat; r++) {
          sequenceArray = sequenceArray.concat(init);
          sequenceArray = sequenceArray.concat(subtests[config.subtests[i]]);
          sequenceArray = sequenceArray.concat(restore);
        }
      }

      return sequenceArray;
    }
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
  _nextCommand: function() {
    if (this._nextCommandIx >= this._commands.length) {
      this._onSequenceComplete();
      return;
    }
    this._commands[this._nextCommandIx++]();
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

    return this._startTest();
  }
}
