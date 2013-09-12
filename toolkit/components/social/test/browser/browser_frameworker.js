/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests message ports and semantics of the frameworker which aren't
// directly related to the sandbox.  See also browser_frameworker_sandbox.js.

function makeWorkerUrl(runner) {
  let prefix =  "http://example.com/browser/toolkit/components/social/test/browser/echo.sjs?";
  if (typeof runner == "function") {
    runner = "var run=" + runner.toSource() + ";run();";
  }
  return prefix + encodeURI(runner);
}

var getFrameWorkerHandle;
function test() {
  waitForExplicitFinish();

  let scope = {};
  Cu.import("resource://gre/modules/FrameWorker.jsm", scope);
  getFrameWorkerHandle = scope.getFrameWorkerHandle;

  runTests(tests);
}

let tests = {
  testSimple: function(cbnext) {
    let run = function() {
      onconnect = function(e) {
        let port = e.ports[0];
        port.onmessage = function(e) {
          if (e.data.topic == "ping") {
            port.postMessage({topic: "pong"});
          }
        }
      }
    }

    let worker = getFrameWorkerHandle(makeWorkerUrl(run), undefined, "testSimple");

    worker.port.onmessage = function(e) {
      if (e.data.topic == "pong") {
        worker.terminate();
        cbnext();
      }
    }
    worker.port.postMessage({topic: "ping"})
  },

  // when the client closes early but the worker tries to send anyway...
  testEarlyClose: function(cbnext) {
    let run = function() {
      onconnect = function(e) {
        let port = e.ports[0];
        port.postMessage({topic: "oh hai"});
      }
    }

    let worker = getFrameWorkerHandle(makeWorkerUrl(run), undefined, "testEarlyClose");
    worker.port.close();
    worker.terminate();
    cbnext();
  },

  // Check we do get a social.port-closing message as the port is closed.
  testPortClosingMessage: function(cbnext) {
    // We use 2 ports - we close the first and report success via the second.
    let run = function() {
      let firstPort, secondPort;
      onconnect = function(e) {
        let port = e.ports[0];
        if (firstPort === undefined) {
          firstPort = port;
          port.onmessage = function(e) {
            if (e.data.topic == "social.port-closing") {
              secondPort.postMessage({topic: "got-closing"});
            }
          }
        } else {
          secondPort = port;
          // now both ports are connected we can trigger the client side
          // closing the first.
          secondPort.postMessage({topic: "connected"});
        }
      }
    }
    let workerurl = makeWorkerUrl(run);
    let worker1 = getFrameWorkerHandle(workerurl, undefined, "testPortClosingMessage worker1");
    let worker2 = getFrameWorkerHandle(workerurl, undefined, "testPortClosingMessage worker2");
    worker2.port.onmessage = function(e) {
      if (e.data.topic == "connected") {
        // both ports connected, so close the first.
        worker1.port.close();
      } else if (e.data.topic == "got-closing") {
        worker2.terminate();
        cbnext();
      }
    }
  },

  // Tests that prototypes added to core objects work with data sent over
  // the message ports.
  testPrototypes: function(cbnext) {
    let run = function() {
      // Modify the Array prototype...
      Array.prototype.customfunction = function() {};
      onconnect = function(e) {
        let port = e.ports[0];
        port.onmessage = function(e) {
          // Check the data we get via the port has the prototype modification
          if (e.data.topic == "hello" && e.data.data.customfunction) {
            port.postMessage({topic: "hello", data: [1,2,3]});
          }
        }
      }
    }
    // hrmph - this kinda sucks as it is really just testing the actual
    // implementation rather than the end result, but is OK for now.
    // Really we are just testing that JSON.parse in the client window
    // is called.
    let fakeWindow = {
      JSON: {
        parse: function(s) {
          let data = JSON.parse(s);
          data.data.somextrafunction = function() {};
          return data;
        }
      }
    }
    let worker = getFrameWorkerHandle(makeWorkerUrl(run), fakeWindow, "testPrototypes");
    worker.port.onmessage = function(e) {
      if (e.data.topic == "hello") {
        ok(e.data.data.somextrafunction, "have someextrafunction")
        worker.terminate();
        cbnext();
      }
    }
    worker.port.postMessage({topic: "hello", data: [1,2,3]});
  },

  testSameOriginImport: function(cbnext) {
    let run = function() {
      onconnect = function(e) {
        let port = e.ports[0];
        port.onmessage = function(e) {
          if (e.data.topic == "ping") {
            try {
              importScripts("http://foo.bar/error");
            } catch(ex) {
              port.postMessage({topic: "pong", data: ex});
              return;
            }
            port.postMessage({topic: "pong", data: null});
          }
        }
      }
    }

    let worker = getFrameWorkerHandle(makeWorkerUrl(run), undefined, "testSameOriginImport");
    worker.port.onmessage = function(e) {
      if (e.data.topic == "pong") {
        isnot(e.data.data, null, "check same-origin applied to importScripts");
        worker.terminate();
        cbnext();
      }
    }
    worker.port.postMessage({topic: "ping"})
  },

  testRelativeImport: function(cbnext) {
    let url = "https://example.com/browser/toolkit/components/social/test/browser/worker_relative.js";
    let worker = getFrameWorkerHandle(url, undefined, "testSameOriginImport");
    worker.port.onmessage = function(e) {
      if (e.data.topic == "done") {
        is(e.data.result, "ok", "check relative url in importScripts");
        worker.terminate();
        cbnext();
      }
    }
  },

  testNavigator: function(cbnext) {
    let run = function() {
      let port;
      ononline = function() {
        port.postMessage({topic: "ononline", data: navigator.onLine});
      }
      onoffline = function() {
        port.postMessage({topic: "onoffline", data: navigator.onLine});
      }
      onconnect = function(e) {
        port = e.ports[0];
        port.postMessage({topic: "ready",
                          data: {
                            appName: navigator.appName,
                            appVersion: navigator.appVersion,
                            platform: navigator.platform,
                            userAgent: navigator.userAgent,
                          }
                         });
      }
    }
    let ioService = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService2);
    let oldManage = ioService.manageOfflineStatus;
    let oldOffline = ioService.offline;

    ioService.manageOfflineStatus = false;
    let worker = getFrameWorkerHandle(makeWorkerUrl(run), undefined, "testNavigator");
    let expected_topic = "onoffline";
    let expected_data = false;
    worker.port.onmessage = function(e) {
      is(e.data.topic, "ready");
      for each (let attr in ['appName', 'appVersion', 'platform', 'userAgent']) {
        // each attribute must be a string with length > 0.
        is(typeof e.data.data[attr], "string");
        ok(e.data.data[attr].length > 0);
      }

      worker.port.onmessage = function(e) {
        // a handler specifically for the offline notification.
        is(e.data.topic, "onoffline");
        is(e.data.data, false);

        // add another handler specifically for the 'online' case.
        worker.port.onmessage = function(e) {
          is(e.data.topic, "ononline");
          is(e.data.data, true);
          // all good!
          ioService.manageOfflineStatus = oldManage;
          ioService.offline = oldOffline;
          worker.terminate();
          cbnext();
        }
        ioService.offline = false;
      }
      ioService.offline = true;
    }
  },

  testMissingWorker: function(cbnext) {
    // don't ever create this file!  We want a 404.
    let url = "https://example.com/browser/toolkit/components/social/test/browser/worker_is_missing.js";
    let worker = getFrameWorkerHandle(url, undefined, "testMissingWorker");
    Services.obs.addObserver(function handleError(subj, topic, data) {
      Services.obs.removeObserver(handleError, "social:frameworker-error");
      is(data, worker._worker.origin, "social:frameworker-error was handled");
      worker.terminate();
      cbnext();
    }, 'social:frameworker-error', false);
    worker.port.onmessage = function(e) {
      ok(false, "social:frameworker-error was handled");
      cbnext();
    }
  },

  testNoConnectWorker: function(cbnext) {
    let worker = getFrameWorkerHandle(makeWorkerUrl(function () {}),
                                      undefined, "testNoConnectWorker");
    Services.obs.addObserver(function handleError(subj, topic, data) {
      Services.obs.removeObserver(handleError, "social:frameworker-error");
      is(data, worker._worker.origin, "social:frameworker-error was handled");
      worker.terminate();
      cbnext();
    }, 'social:frameworker-error', false);
    worker.port.onmessage = function(e) {
      ok(false, "social:frameworker-error was handled");
      cbnext();
    }
  },

  testEmptyWorker: function(cbnext) {
    let worker = getFrameWorkerHandle(makeWorkerUrl(''),
                                      undefined, "testEmptyWorker");
    Services.obs.addObserver(function handleError(subj, topic, data) {
      Services.obs.removeObserver(handleError, "social:frameworker-error");
      is(data, worker._worker.origin, "social:frameworker-error was handled");
      worker.terminate();
      cbnext();
    }, 'social:frameworker-error', false);
    worker.port.onmessage = function(e) {
      ok(false, "social:frameworker-error was handled");
      cbnext();
    }
  },

  testWorkerConnectError: function(cbnext) {
    let run = function () {
      onconnect = function(e) {
        throw new Error("worker failure");
      }
    }
    let worker = getFrameWorkerHandle(makeWorkerUrl(run),
                                      undefined, "testWorkerConnectError");
    Services.obs.addObserver(function handleError(subj, topic, data) {
      Services.obs.removeObserver(handleError, "social:frameworker-error");
      is(data, worker._worker.origin, "social:frameworker-error was handled");
      worker.terminate();
      cbnext();
    }, 'social:frameworker-error', false);
    worker.port.onmessage = function(e) {
      ok(false, "social:frameworker-error was handled");
      cbnext();
    }
  },

  // This will create the worker, then send a message to the port, then close
  // the port - all before the worker has actually initialized.
  testCloseFirstSend: function(cbnext) {
    let run = function() {
      let numPings = 0, numCloses = 0;
      onconnect = function(e) {
        let port = e.ports[0];
        port.onmessage = function(e) {
          if (e.data.topic == "ping") {
            numPings += 1;
          } else if (e.data.topic == "social.port-closing") {
            numCloses += 1;
          } else if (e.data.topic == "get-counts") {
            port.postMessage({topic: "result",
                             result: {ping: numPings, close: numCloses}});
          }
        }
      }
    }

    let worker = getFrameWorkerHandle(makeWorkerUrl(run), undefined, "testSendAndClose");
    worker.port.postMessage({topic: "ping"});
    worker.port.close();
    let newPort = getFrameWorkerHandle(makeWorkerUrl(run), undefined, "testSendAndClose").port;
    newPort.onmessage = function(e) {
      if (e.data.topic == "result") {
        is(e.data.result.ping, 1, "the worker got the ping");
        is(e.data.result.close, 1, "the worker got 1 close message");
        worker.terminate();
        cbnext();
      }
    }
    newPort.postMessage({topic: "get-counts"});
  },

  // Like testCloseFirstSend, although in this test the worker has already
  // initialized (so the "connect pending ports" part of the worker isn't
  // what needs to handle this case.)
  testCloseAfterInit: function(cbnext) {
    let run = function() {
      let numPings = 0, numCloses = 0;
      onconnect = function(e) {
        let port = e.ports[0];
        port.onmessage = function(e) {
          if (e.data.topic == "ping") {
            numPings += 1;
          } else if (e.data.topic == "social.port-closing") {
            numCloses += 1;
          } else if (e.data.topic == "get-counts") {
            port.postMessage({topic: "result",
                             result: {ping: numPings, close: numCloses}});
          } else if (e.data.topic == "get-ready") {
            port.postMessage({topic: "ready"});
          }
        }
      }
    }

    let worker = getFrameWorkerHandle(makeWorkerUrl(run), undefined, "testSendAndClose");
    worker.port.onmessage = function(e) {
      if (e.data.topic == "ready") {
        let newPort = getFrameWorkerHandle(makeWorkerUrl(run), undefined, "testSendAndClose").port;
        newPort.postMessage({topic: "ping"});
        newPort.close();
        worker.port.postMessage({topic: "get-counts"});
      } else if (e.data.topic == "result") {
        is(e.data.result.ping, 1, "the worker got the ping");
        is(e.data.result.close, 1, "the worker got 1 close message");
        worker.terminate();
        cbnext();
      }
    }
    worker.port.postMessage({topic: "get-ready"});
  },
}
