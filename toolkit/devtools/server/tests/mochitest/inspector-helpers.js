var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

// Always log packets when running tests.
Services.prefs.setBoolPref("devtools.debugger.log", true);
SimpleTest.registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.debugger.log");
});

Cu.import("resource://gre/modules/devtools/Loader.jsm");
Cu.import("resource://gre/modules/devtools/dbg-client.jsm");
Cu.import("resource://gre/modules/devtools/dbg-server.jsm");

if (!DebuggerServer.initialized) {
  DebuggerServer.init(() => true);
  DebuggerServer.addBrowserActors();
  SimpleTest.registerCleanupFunction(function() {
    DebuggerServer.destroy();
  });
}

/**
 * Open a tab, load the url, wait for it to signal its readiness,
 * find the tab with the debugger server, and call the callback.
 */
function attachURL(url, callback) {
  var win = window.open(url, "_blank");

  SimpleTest.registerCleanupFunction(function() {
    win.close();
  });

  window.addEventListener("message", function loadListener(event) {
    if (event.data === "ready") {
      let client = new DebuggerClient(DebuggerServer.connectPipe());
      SimpleTest.registerCleanupFunction(function() {
        client.close();
      });

      client.connect((applicationType, traits) => {
        client.listTabs(response => {
          for (let tab of response.tabs) {
            if (tab.url === url) {
              window.removeEventListener("message", loadListener, false);
              try {
                callback(null, client, tab, win.document);
              } catch(ex) {
                Cu.reportError(ex);
                dump(ex);
              }
              break;
            }
          }
        });
      });
    }
  }, false);
}


// Verify that an actorID is inaccessible both from the client library and the server.
function checkMissing(client, actorID) {
  let deferred = Promise.defer();
  let front = client.getActor(actorID);
  ok(!front, "Front shouldn't be accessible from the client for actorID: " + actorID);

  let deferred = Promise.defer();
  client.request({
    to: actorID,
    type: "request",
  }, response => {
    is(response.error, "noSuchActor", "node list actor should no longer be contactable.");
    deferred.resolve(undefined);
  });
  return deferred.promise;
}

function promiseDone(promise) {
  promise.then(null, err => {
    ok(false, "Promise failed: " + err);
    if (err.stack) {
      dump(err.stack);
    }
    SimpleTest.finish();
  });
}

var _tests = [];
function addTest(test) {
  _tests.push(test);
}

function runNextTest() {
  if (_tests.length == 0) {
    SimpleTest.finish()
    return;
  }
  _tests.shift()();
}