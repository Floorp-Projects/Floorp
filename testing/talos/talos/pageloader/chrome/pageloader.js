/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

try {
  if (Cc === undefined) {
    var Cc = Components.classes;
    var Ci = Components.interfaces;
  }
} catch (ex) {}

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/Task.jsm");
Components.utils.import("resource:///modules/E10SUtils.jsm");

var NUM_CYCLES = 5;
var numPageCycles = 1;

var numRetries = 0;
var maxRetries = 3;

var pageFilterRegexp = null;
var useBrowser = true;
var winWidth = 1024;
var winHeight = 768;

var doRenderTest = false;

var pages;
var pageIndex;
var start_time;
var cycle;
var pageCycle;
var report;
var noisy = false;
var timeout = -1;
var delay = 250;
var running = false;
var forceCC = true;
var reportRSS = true;

var useMozAfterPaint = false;
var gPaintWindow = window;
var gPaintListener = false;
var loadNoCache = false;
var scrollTest = false;
var gDisableE10S = false;
var gUseE10S = false;
var profilingInfo = false;

//when TEST_DOES_OWN_TIMING, we need to store the time from the page as MozAfterPaint can be slower than pageload
var gTime = -1;
var gStartTime = -1;
var gReference = -1;

var content;

// These are binary flags. Use 1/2/4/8/...
var TEST_DOES_OWN_TIMING = 1;
var EXECUTE_SCROLL_TEST  = 2;

var browserWindow = null;

var recordedName = null;
var pageUrls;

// the io service
var gIOS = null;

/**
 * SingleTimeout class. Allow to register one and only one callback using
 * setTimeout at a time.
 */
var SingleTimeout = function() {
  this.timeoutEvent = undefined;
};

/**
 * Register a callback with the given timeout.
 *
 * If timeout is < 0, this is a no-op.
 *
 * If a callback was previously registered and has not been called yet, it is
 * first cleared with clear().
 */
SingleTimeout.prototype.register = function(callback, timeout) {
  if (timeout >= 0) {
    if (this.timeoutEvent !== undefined) {
      this.clear();
    }
    var that = this;
    this.timeoutEvent = setTimeout(function() {
      that.timeoutEvent = undefined;
      callback();
    }, timeout);
  }
};

/**
 * Clear a registered callback.
 */
SingleTimeout.prototype.clear = function() {
  if (this.timeoutEvent !== undefined) {
    clearTimeout(this.timeoutEvent);
    this.timeoutEvent = undefined;
  }
};

var failTimeout = new SingleTimeout();

function plInit() {
  if (running) {
    return;
  }
  running = true;

  cycle = 0;
  pageCycle = 1;

  try {
    var args;
    
    /*
     * Desktop firefox:
     * non-chrome talos runs - tp-cmdline will create and load pageloader
     * into the main window of the app which displays and tests content.
     * chrome talos runs - tp-cmdline does the same however pageloader
     * creates a new chromed browser window below for content.
     */

    // cmdline arguments are on window
    args = window.arguments[0].wrappedJSObject;

    var manifestURI = args.manifest;
    var startIndex = 0;
    var endIndex = -1;
    if (args.startIndex) startIndex = parseInt(args.startIndex);
    if (args.endIndex) endIndex = parseInt(args.endIndex);
    if (args.numCycles) NUM_CYCLES = parseInt(args.numCycles);
    if (args.numPageCycles) numPageCycles = parseInt(args.numPageCycles);
    if (args.width) winWidth = parseInt(args.width);
    if (args.height) winHeight = parseInt(args.height);
    if (args.filter) pageFilterRegexp = new RegExp(args.filter);
    if (args.noisy) noisy = true;
    if (args.timeout) timeout = parseInt(args.timeout);
    if (args.delay) delay = parseInt(args.delay);
    if (args.mozafterpaint) useMozAfterPaint = true;
    if (args.rss) reportRSS = true;
    if (args.loadnocache) loadNoCache = true;
    if (args.scrolltest) scrollTest = true;
    if (args.disableE10S) gDisableE10S = true;
    if (args.profilinginfo) profilingInfo = JSON.parse(args.profilinginfo)

    if (profilingInfo) {
      Profiler.initFromObject(profilingInfo);
    }

    forceCC = !args.noForceCC;
    doRenderTest = args.doRender;

    if (forceCC &&
        !window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
               .getInterface(Components.interfaces.nsIDOMWindowUtils)
               .garbageCollect) {
      forceCC = false;
    }

    gIOS = Cc["@mozilla.org/network/io-service;1"]
      .getService(Ci.nsIIOService);
    if (args.offline)
      gIOS.offline = true;
    var fileURI = gIOS.newURI(manifestURI, null, null);
    pages = plLoadURLsFromURI(fileURI);

    if (!pages) {
      dumpLine('tp: could not load URLs, quitting');
      plStop(true);
    }

    if (pages.length == 0) {
      dumpLine('tp: no pages to test, quitting');
      plStop(true);
    }

    if (startIndex < 0)
      startIndex = 0;
    if (endIndex == -1 || endIndex >= pages.length)
      endIndex = pages.length-1;
    if (startIndex > endIndex) {
      dumpLine("tp: error: startIndex >= endIndex");
      plStop(true);
    }

    pages = pages.slice(startIndex,endIndex+1);
    pageUrls = pages.map(function(p) { return p.url.spec.toString(); });
    report = new Report();

    if (doRenderTest)
      renderReport = new Report();

    pageIndex = 0;
    if (profilingInfo) {
      Profiler.beginTest(getCurrentPageShortName());
    }
  
    if (args.useBrowserChrome) {
      // Create a new chromed browser window for content
      var wwatch = Cc["@mozilla.org/embedcomp/window-watcher;1"]
        .getService(Ci.nsIWindowWatcher);
      var blank = Cc["@mozilla.org/supports-string;1"]
        .createInstance(Ci.nsISupportsString);
      blank.data = "about:blank";
      browserWindow = wwatch.openWindow
        (null, "chrome://browser/content/", "_blank",
         "chrome,all,dialog=no,width=" + winWidth + ",height=" + winHeight, blank);

      gPaintWindow = browserWindow;
      // get our window out of the way
      window.resizeTo(10,10);

      var browserLoadFunc = function (ev) {
        browserWindow.removeEventListener('load', browserLoadFunc, true);

        function firstPageCanLoadAsRemote() {
          return E10SUtils.canLoadURIInProcess(pageUrls[0], Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT);
        }

        // do this half a second after load, because we need to be
        // able to resize the window and not have it get clobbered
        // by the persisted values
        setTimeout(function () {
                     // For e10s windows, the initial browser is not remote until it attempts to
                     // browse to a URI that should be remote (landed at bug 1047603).
                     // However, when it loads such URI and reinitialize as remote, we lose the
                     // load listener and the injected tpRecordTime.
                     // The same thing happens if the initial browser starts as remote but the
                     // first page is not-remote (such as with TART/CART which load a chrome URI).
                     //
                     // The preferred pageloader behaviour in e10s is to run the pages as as remote,
                     // so if the page can load as remote, we will load it as remote.
                     //
                     // It also probably means that per test (or, in fact, per pageloader browser
                     // instance which adds the load listener and injects tpRecordTime), all the
                     // pages should be able to load in the same mode as the initial page - due
                     // to this reinitialization on the switch.
                     if (browserWindow.gMultiProcessBrowser) {
                       if (firstPageCanLoadAsRemote())
                         browserWindow.XULBrowserWindow.forceInitialBrowserRemote();
                       // Implicit else: initial browser in e10s is non-remote by default.
                     }

                     browserWindow.resizeTo(winWidth, winHeight);
                     browserWindow.moveTo(0, 0);
                     browserWindow.focus();

                     content = browserWindow.getBrowser();
                     if (content.selectedBrowser) {
                       content.selectedBrowser.focus();
                     } else {
                       dump("WARNING: cannot focus content area\n");
                     }

                     gUseE10S = !gDisableE10S || (plPageFlags() & EXECUTE_SCROLL_TEST) ||
                                 (content.selectedBrowser &&
                                 content.selectedBrowser.getAttribute("remote") == "true")

                     // Load the frame script for e10s / IPC message support
                     if (gUseE10S) {
                       let contentScript = "data:,function _contentLoadHandler(e) { " +
                         "  if (e.originalTarget.defaultView == content) { " +
                         "    content.wrappedJSObject.tpRecordTime = function(t, s, n) { sendAsyncMessage('PageLoader:RecordTime', { time: t, startTime: s, testName: n }); }; ";
                       if (useMozAfterPaint) {
                         contentScript += "" +
                         "function _contentPaintHandler() { " +
                         "  var utils = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor).getInterface(Components.interfaces.nsIDOMWindowUtils); " +
                         "  if (utils.isMozAfterPaintPending) { " +
                         "    addEventListener('MozAfterPaint', function(e) { " +
                         "      removeEventListener('MozAfterPaint', arguments.callee, true); " +
                         "      sendAsyncMessage('PageLoader:LoadEvent', {}); " +
                         "    }, true); " +
                         "  } else { " +
                         "    sendAsyncMessage('PageLoader:LoadEvent', {}); " +
                         "  } " +
                         "}; " +
                         "content.setTimeout(_contentPaintHandler, 0); ";
                       } else {
                         contentScript += "    sendAsyncMessage('PageLoader:LoadEvent', {}); ";
                       }
                       contentScript += "" +
                         "  }" +
                         "} " +
                         "addEventListener('load', _contentLoadHandler, true); ";
                       content.selectedBrowser.messageManager.loadFrameScript(contentScript, false, true);
                       content.selectedBrowser.messageManager.loadFrameScript("chrome://pageloader/content/talos-content.js", false);
                       content.selectedBrowser.messageManager.loadFrameScript("chrome://pageloader/content/tscroll.js", false, true);
                       content.selectedBrowser.messageManager.loadFrameScript("chrome://pageloader/content/Profiler.js", false, true);
                     }

                     if (reportRSS) {
                       initializeMemoryCollector(plLoadPage, 100);
                     } else {
                       setTimeout(plLoadPage, 100);
                     }
                   }, 500);
      };

      browserWindow.addEventListener('load', browserLoadFunc, true);
    } else {
      // Loading content into the initial window we create
      gPaintWindow = window;
      window.resizeTo(winWidth, winHeight);

      content = document.getElementById('contentPageloader');

      if (reportRSS) {
        initializeMemoryCollector(plLoadPage, delay);
      } else {
        setTimeout(plLoadPage, delay);
      }
    }
  } catch(e) {
    dumpLine("pageloader exception: " + e);
    plStop(true);
  }
}

function plPageFlags() {
  return pages[pageIndex].flags;
}

var ContentListener = {
  receiveMessage: function(message) {
    switch (message.name) {
      case 'PageLoader:LoadEvent': return plLoadHandlerMessage(message);
      case 'PageLoader:RecordTime': return plRecordTimeMessage(message);
    }
  },
};

// load the current page, start timing
var removeLastAddedListener = null;
var removeLastAddedMsgListener = null;
function plLoadPage() {
  var pageName = pages[pageIndex].url.spec;

  if (removeLastAddedListener) {
    removeLastAddedListener();
    removeLastAddedListener = null;
  }

  if (removeLastAddedMsgListener) {
    removeLastAddedMsgListener();
    removeLastAddedMsgListener = null;
  }

  if ((plPageFlags() & TEST_DOES_OWN_TIMING) && !gUseE10S) {
    // if the page does its own timing, use a capturing handler
    // to make sure that we can set up the function for content to call

    content.addEventListener('load', plLoadHandlerCapturing, true);
    removeLastAddedListener = function() {
      content.removeEventListener('load', plLoadHandlerCapturing, true);
      if (useMozAfterPaint) {
        content.removeEventListener("MozAfterPaint", plPaintedCapturing, true);
        gPaintListener = false;
      }
    };
  } else if (!gUseE10S) {
    // if the page doesn't do its own timing, use a bubbling handler
    // to make sure that we're called after the page's own onload() handling

    // XXX we use a capturing event here too -- load events don't bubble up
    // to the <browser> element.  See bug 390263.
    content.addEventListener('load', plLoadHandler, true);
    removeLastAddedListener = function() {
      content.removeEventListener('load', plLoadHandler, true);
      if (useMozAfterPaint) {
        gPaintWindow.removeEventListener("MozAfterPaint", plPainted, true);
        gPaintListener = false;
      }
    };
  }

  // If the test browser is remote (e10s / IPC) we need to use messages to watch for page load
  if (gUseE10S) {
    let mm = content.selectedBrowser.messageManager;
    mm.addMessageListener('PageLoader:LoadEvent', ContentListener);
    mm.addMessageListener('PageLoader:RecordTime', ContentListener);
    removeLastAddedMsgListener = function() {
      mm.removeMessageListener('PageLoader:LoadEvent', ContentListener);
      mm.removeMessageListener('PageLoader:RecordTime', ContentListener);
    };
  }

  failTimeout.register(loadFail, timeout);

  // record which page we are about to open
  Profiler.mark("Opening " + pages[pageIndex].url.path);

  if (reportRSS) {
    collectMemory(startAndLoadURI, pageName);
  } else {
    startAndLoadURI(pageName);
  }
}

function startAndLoadURI(pageName) {
  if (!(plPageFlags() & TEST_DOES_OWN_TIMING)) {
    // Resume the profiler because we're really measuring page load time.
    // If the test is doing its own timing, it'll also need to do its own
    // profiler pausing / resuming.
    Profiler.resume("Starting to load URI " + pageName, true);
  }
  start_time = Date.now();
  if (loadNoCache) {
    content.loadURIWithFlags(pageName, Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE);
  } else {
    content.loadURI(pageName);
  }
}

function getTestName() { // returns tp5n
  var pageName = pages[pageIndex].url.spec;
  let parts = pageName.split('/');
  if (parts.length > 4) {
    return parts[4];
  }
  return "pageloader";
}

function getCurrentPageShortName() {
  var pageName = pages[pageIndex].url.spec;
  let parts = pageName.split('/');
  if (parts.length > 5) {
    return parts[5];
  }
  return "page_" + pageIndex;
}

function loadFail() {
  var pageName = pages[pageIndex].url.spec;
  numRetries++;

  if (numRetries >= maxRetries) {
    dumpLine('__FAILTimeout in ' + getTestName() + '__FAIL');
    dumpLine('__FAILTimeout (' + numRetries + '/' + maxRetries + ') exceeded on ' + pageName + '__FAIL');
    Profiler.finishTest();
    plStop(true);
  } else {
    dumpLine('__WARNTimeout (' + numRetries + '/' + maxRetries + ') exceeded on ' + pageName + '__WARN');
    // TODO: make this a cleaner cleanup
    pageCycle--;
    content.removeEventListener('load', plLoadHandler, true);
    content.removeEventListener('load', plLoadHandlerCapturing, true);
    content.removeEventListener("MozAfterPaint", plPaintedCapturing, true);
    content.removeEventListener("MozAfterPaint", plPainted, true);
    gPaintWindow.removeEventListener("MozAfterPaint", plPaintedCapturing, true);
    gPaintWindow.removeEventListener("MozAfterPaint", plPainted, true);
    removeLastAddedListener = null;
    removeLastAddedMsgListener = null;
    gPaintListener = false;

    //TODO: consider adding a tab and removing the old tab?!?
    setTimeout(plLoadPage, delay);
  }
}

var plNextPage = Task.async(function*() {
  var doNextPage = false;
  if (pageCycle < numPageCycles) {
    pageCycle++;
    doNextPage = true;
  } else {
    if (profilingInfo) {
      yield Profiler.finishTestAsync();
    }

    if (pageIndex < pages.length-1) {
      pageIndex++;
      if (profilingInfo) {
        Profiler.beginTest(getCurrentPageShortName());
      }
      recordedName = null;
      pageCycle = 1;
      doNextPage = true;
    }
  }

  if (doNextPage == true) {
    if (forceCC) {
      var tccstart = new Date();
      window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
            .getInterface(Components.interfaces.nsIDOMWindowUtils)
            .garbageCollect();
      var tccend = new Date();
      report.recordCCTime(tccend - tccstart);

      // Now asynchronously trigger GC / CC in the content process if we're
      // in an e10s window.
      if (browserWindow.gMultiProcessBrowser) {
        yield forceContentGC();
      }
    }

    setTimeout(plLoadPage, delay);
  } else {
    plStop(false);
  }
});

function forceContentGC() {
  return new Promise((resolve) => {
    let mm = browserWindow.getBrowser().selectedBrowser.messageManager;
    mm.addMessageListener("Talos:ForceGC:OK", function onTalosContentForceGC(msg) {
      mm.removeMessageListener("Talos:ForceGC:OK", onTalosContentForceGC);
      resolve();
    });
    mm.sendAsyncMessage("Talos:ForceGC");
  });
}

function plRecordTime(time) {
  var pageName = pages[pageIndex].url.spec;
  var i = pageIndex
  if (i < pages.length-1) {
    i++;
  } else {
    i = 0;
  }
  var nextName = pages[i].url.spec;
  if (!recordedName) {
    recordedName = pageUrls[pageIndex];
  }
  if (typeof(time) == "string") {
    var times = time.split(',');
    var names = recordedName.split(',');
    for (var t = 0; t < times.length; t++) {
      if (names.length == 1) {
        report.recordTime(names, times[t]);
      } else {
        report.recordTime(names[t], times[t]);
      }
    }
  } else {
    report.recordTime(recordedName, time);
  }
  if (noisy) {
    dumpLine("Cycle " + (cycle+1) + "(" + pageCycle + ")" + ": loaded " + pageName + " (next: " + nextName + ")");
  }
}

function plLoadHandlerCapturing(evt) {
  // make sure we pick up the right load event
  if (evt.type != 'load' ||
       evt.originalTarget.defaultView.frameElement)
      return;

  //set the tpRecordTime function (called from test pages we load) to store a global time.
  content.contentWindow.wrappedJSObject.tpRecordTime = function (time, startTime, testName) {
    gTime = time;
    gStartTime = startTime;
    recordedName = testName;
    setTimeout(plWaitForPaintingCapturing, 0);
  }

  content.contentWindow.wrappedJSObject.plGarbageCollect = function () {
    window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
          .getInterface(Components.interfaces.nsIDOMWindowUtils)
          .garbageCollect();
  }

  content.removeEventListener('load', plLoadHandlerCapturing, true);
  removeLastAddedListener = null;

  setTimeout(plWaitForPaintingCapturing, 0);
}

// Shim function this is really defined in tscroll.js
function sendScroll() {
  const SCROLL_TEST_STEP_PX = 10;
  const SCROLL_TEST_NUM_STEPS = 100;
  // The page doesn't really use tpRecordTime. Instead, we trigger the scroll test,
  // and the scroll test will call tpRecordTime which will take us to the next page
  let details = {target: 'content', stepSize: SCROLL_TEST_STEP_PX, opt_numSteps: SCROLL_TEST_NUM_STEPS};
  let mm = content.selectedBrowser.messageManager;
  mm.sendAsyncMessage("PageLoader:ScrollTest", { details: details });
}

function plWaitForPaintingCapturing() {
  if (gPaintListener)
    return;

  var utils = gPaintWindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                   .getInterface(Components.interfaces.nsIDOMWindowUtils);

  if (utils.isMozAfterPaintPending && useMozAfterPaint) {
    if (gPaintListener == false)
      gPaintWindow.addEventListener("MozAfterPaint", plPaintedCapturing, true);
    gPaintListener = true;
    return;
  }

  _loadHandlerCapturing();
}

function plPaintedCapturing() {
  gPaintWindow.removeEventListener("MozAfterPaint", plPaintedCapturing, true);
  gPaintListener = false;
  _loadHandlerCapturing();
}

function _loadHandlerCapturing() {
  failTimeout.clear();

  if (!(plPageFlags() & TEST_DOES_OWN_TIMING)) {
    dumpLine("tp: Capturing onload handler used with page that doesn't do its own timing?");
    plStop(true);
  }

  if (useMozAfterPaint) {
    if (gStartTime != null && gStartTime >= 0) {
      gTime = (new Date()) - gStartTime;
      gStartTime = -1;
    }
  }

  if (gTime !== -1) {
    plRecordTime(gTime);
    Profiler.pause("capturing load handler fired", true);
    gTime = -1;
    recordedName = null;
    setTimeout(plNextPage, delay);
  }
}

// the onload handler
function plLoadHandler(evt) {
  // make sure we pick up the right load event
  if (evt.type != 'load' ||
       evt.originalTarget.defaultView.frameElement)
      return;

  content.removeEventListener('load', plLoadHandler, true);
  setTimeout(waitForPainted, 0);
}

// This is called after we have received a load event, now we wait for painted
function waitForPainted() {

  var utils = gPaintWindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                   .getInterface(Components.interfaces.nsIDOMWindowUtils);

  if (!utils.isMozAfterPaintPending || !useMozAfterPaint) {
    _loadHandler();
    return;
  }

  if (gPaintListener == false)
    gPaintWindow.addEventListener("MozAfterPaint", plPainted, true);
  gPaintListener = true;
}

function plPainted() {
  gPaintWindow.removeEventListener("MozAfterPaint", plPainted, true);
  gPaintListener = false;
  _loadHandler();
}

function _loadHandler() {
  failTimeout.clear();

  var end_time = Date.now();
  var time = (end_time - start_time);

  Profiler.pause("Bubbling load handler fired.", true);

  // does this page want to do its own timing?
  // if so, we shouldn't be here
  if (plPageFlags() & TEST_DOES_OWN_TIMING) {
    dumpLine("tp: Bubbling onload handler used with page that does its own timing?");
    plStop(true);
  }

  plRecordTime(time);

  if (doRenderTest)
    runRenderTest();

  plNextPage();
}

// the core handler for remote (e10s) browser
function plLoadHandlerMessage() {
  failTimeout.clear();

  if ((plPageFlags() & EXECUTE_SCROLL_TEST)) {
    // Let the page settle down after its load event, then execute the scroll test.
    setTimeout(sendScroll, 500);
  } else if ((plPageFlags() & TEST_DOES_OWN_TIMING)) {
    var time;

    if (typeof(gStartTime) != "number")
      gStartTime = Date.parse(gStartTime);
    if (gTime !== -1) {
      if (useMozAfterPaint && gStartTime >= 0) {
        time = Date.now() - gStartTime;
        gStartTime = -1;
      } else if (!useMozAfterPaint) {
        time = gTime;
      }
      gTime = -1;
    }

    if (time !== undefined) {
      plRecordTime(time);
      if (doRenderTest)
        runRenderTest();
      plNextPage();
    }
  } else {
    _loadHandler();
  }
}

// the record time handler used for remote (e10s) browser
function plRecordTimeMessage(message) {
  gTime = message.json.time;
  gStartTime = message.json.startTime;
  recordedName = message.json.testName;

  if (useMozAfterPaint) {
    gStartTime = message.json.startTime;
  }
  _loadHandlerCapturing();
}

function runRenderTest() {
  const redrawsPerSample = 500;

  if (!Ci.nsIDOMWindowUtils)
    return;

  var win;

  if (browserWindow)
    win = content.contentWindow;
  else
    win = window;
  var wu = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);

  var start = Date.now();
  for (var j = 0; j < redrawsPerSample; j++)
    wu.redraw();
  var end = Date.now();

  renderReport.recordTime(pageIndex, end - start);
}

function plStop(force) {
  if (reportRSS) {
    collectMemory(plStopAll, force);
  } else {
    plStopAll(force);
  }
}

function plStopAll(force) {
  try {
    if (force == false) {
      pageIndex = 0;
      pageCycle = 1;
      if (cycle < NUM_CYCLES-1) {
        cycle++;
        recordedName = null;
        setTimeout(plLoadPage, delay);
        return;
      }

      /* output report */
      dumpLine(report.getReport());
      dumpLine(report.getReportSummary());
    }
  } catch (e) {
    dumpLine(e);
  }

  if (reportRSS) {
    stopMemCollector();
  }

  if (content) {
    content.removeEventListener('load', plLoadHandlerCapturing, true);
    content.removeEventListener('load', plLoadHandler, true);
    if (useMozAfterPaint)
      content.removeEventListener("MozAfterPaint", plPaintedCapturing, true);
      content.removeEventListener("MozAfterPaint", plPainted, true);

    if (gUseE10S) {
      let mm = content.selectedBrowser.messageManager;
      mm.removeMessageListener('PageLoader:LoadEvent', ContentListener);
      mm.removeMessageListener('PageLoader:RecordTime', ContentListener);

      mm.loadFrameScript("data:,removeEventListener('load', _contentLoadHandler, true);", false, true);
    }
  }

  if (MozillaFileLogger && MozillaFileLogger._foStream)
    MozillaFileLogger.close();

  goQuitApplication();
}

/* Returns array */
function plLoadURLsFromURI(manifestUri) {
  var fstream = Cc["@mozilla.org/network/file-input-stream;1"]
    .createInstance(Ci.nsIFileInputStream);
  var uriFile = manifestUri.QueryInterface(Ci.nsIFileURL);

  fstream.init(uriFile.file, -1, 0, 0);
  var lstream = fstream.QueryInterface(Ci.nsILineInputStream);

  var d = [];

  var lineNo = 0;
  var line = {value:null};
  var more;
  do {
    lineNo++;
    more = lstream.readLine(line);
    var s = line.value;

    // strip comments (only leading ones)
    s = s.replace(/^#.*/, '');

    // strip leading and trailing whitespace
    s = s.replace(/^\s*/, '').replace(/\s*$/, '');

    if (!s)
      continue;

    var flags = 0;
    var urlspec = s;

    // split on whitespace, and figure out if we have any flags
    var items = s.split(/\s+/);
    if (items[0] == "include") {
      if (items.length != 2) {
        dumpLine("tp: Error on line " + lineNo + " in " + manifestUri.spec + ": include must be followed by the manifest to include!");
        return null;
      }

      var subManifest = gIOS.newURI(items[1], null, manifestUri);
      if (subManifest == null) {
        dumpLine("tp: invalid URI on line " + manifestUri.spec + ":" + lineNo + " : '" + line.value + "'");
        return null;
      }

      var subItems = plLoadURLsFromURI(subManifest);
      if (subItems == null)
        return null;
      d = d.concat(subItems);
    } else {
      // For scrollTest flag, we accept "normal" pages but treat them as TEST_DOES_OWN_TIMING
      // together with EXECUTE_SCROLL_TEST which makes us run the scroll test on load.
      // We do this by artificially "injecting" the TEST_DOES_OWN_TIMING flag ("%") to the item
      // and then let the default flow for this flag run without further modifications
      // (other than calling the scroll test once the page is loaded).
      // Note that if we have the scrollTest flag but the item already has "%", then we do
      // nothing (the scroll test will not execute, and the page will report with its
      // own tpRecordTime and not the one from the scroll test).
      if (scrollTest && items[0].indexOf("%") < 0) {
        items.unshift("%");
        flags |= EXECUTE_SCROLL_TEST;
      }

      if (items.length == 2) {
        if (items[0].indexOf("%") != -1)
          flags |= TEST_DOES_OWN_TIMING;

        urlspec = items[1];
      } else if (items.length != 1) {
        dumpLine("tp: Error on line " + lineNo + " in " + manifestUri.spec + ": whitespace must be %-escaped!");
        return null;
      }

      var url = gIOS.newURI(urlspec, null, manifestUri);

      if (pageFilterRegexp && !pageFilterRegexp.test(url.spec))
        continue;

      d.push({   url: url,
               flags: flags });
    }
  } while (more);

  return d;
}

function dumpLine(str) {
  if (MozillaFileLogger && MozillaFileLogger._foStream)
    MozillaFileLogger.log(str + "\n");
  dump(str);
  dump("\n");
}

