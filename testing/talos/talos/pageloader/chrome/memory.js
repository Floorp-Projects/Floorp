
var gChildProcess = true;
var gMemCallback = null;


/*
 * Initialize memory collector.  Determine if we have a child process.
 */
function initializeMemoryCollector(callback, args) {
    gMemCallback = function() { return callback(args); };

    var os = Components.classes["@mozilla.org/observer-service;1"].
        getService(Components.interfaces.nsIObserverService);

    os.addObserver(function () {
        var os = Components.classes["@mozilla.org/observer-service;1"].
            getService(Components.interfaces.nsIObserverService);

        memTimer.cancel();
        memTimer = null;

        os.removeObserver(arguments.callee, "child-memory-reporter-update");
        os.addObserver(collectAndReport, "child-memory-reporter-update", false);
        gMemCallback();
    }, "child-memory-reporter-update", false);

   /*
    * Assume we have a child process, but if timer fires before we call the observer
    * we will assume there is no child process.
    */
    var event = {
      notify: function(timer) {
        memTimer = null;
        gChildProcess = false;
        gMemCallback();
      }
    }
 
    var memTimer = Components.classes["@mozilla.org/timer;1"].createInstance(Components.interfaces.nsITimer);
    memTimer.initWithCallback(event, 10000, Components.interfaces.nsITimer.TYPE_ONE_SHOT);

    os.notifyObservers(null, "child-memory-reporter-request", "generation=1 anonymize=0 minimize=0 DMDident=0");
}

/*
 * Collect memory from all processes and callback when done collecting.
 */
function collectMemory(callback, args) {
  gMemCallback = function() { return callback(args); };

  if (gChildProcess) {
    var os = Components.classes["@mozilla.org/observer-service;1"].
        getService(Components.interfaces.nsIObserverService);

    os.notifyObservers(null, "child-memory-reporter-request", null);
  } else {
    collectAndReport(null, null, null);
  }
}

function collectAndReport(aSubject, aTopic, aData) {
  dumpLine(collectRSS());
  gMemCallback();
}

function collectRSS() {
  var mgr = Components.classes["@mozilla.org/memory-reporter-manager;1"].
      getService(Components.interfaces.nsIMemoryReporterManager);
  return "RSS: Main: " + mgr.resident + "\n";
}

/*
 * Cleanup and stop memory collector.
 */
function stopMemCollector() {
  if (gChildProcess) {
    var os = Cc["@mozilla.org/observer-service;1"].
        getService(Ci.nsIObserverService);
    os.removeObserver(collectAndReport, "child-memory-reporter-update");
  }
}
