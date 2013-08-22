/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    isnot(worker._worker.frame.contentWindow.toString(), "[object ChromeWindow]", "worker window isn't a chrome window");

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

  testArrayUsingBuffer: function(cbnext) {
    let run = function() {
      onconnect = function(e) {
        let port = e.ports[0];
        port.onmessage = function(e) {
          if (e.data.topic == "go") {
            let buffer = new ArrayBuffer(10);
            // this one has always worked in the past, but worth checking anyway...
            if (new Uint8Array(buffer).length != 10) {
              port.postMessage({topic: "result", reason: "first length was not 10"});
              return;
            }
            let reader = new FileReader();
            reader.onload = function(event) {
              if (new Uint8Array(buffer).length != 10) {
                port.postMessage({topic: "result", reason: "length in onload handler was not 10"});
                return;
              }
              // all seems good!
              port.postMessage({topic: "result", reason: "ok"});
            }
            let blob = new Blob([buffer], {type: "binary"});
            reader.readAsArrayBuffer(blob);
          }
        }
      }
    }
    let worker = getFrameWorkerHandle(makeWorkerUrl(run), undefined, "testArray");
    worker.port.onmessage = function(e) {
      if (e.data.topic == "result") {
        is(e.data.reason, "ok", "check the array worked");
        worker.terminate();
        cbnext();
      }
    }
    worker.port.postMessage({topic: "go"});
  },

  testArrayUsingReader: function(cbnext) {
    let run = function() {
      onconnect = function(e) {
        let port = e.ports[0];
        port.onmessage = function(e) {
          if (e.data.topic == "go") {
            let buffer = new ArrayBuffer(10);
            let reader = new FileReader();
            reader.onload = function(event) {
              try {
                if (new Uint8Array(reader.result).length != 10) {
                  port.postMessage({topic: "result", reason: "length in onload handler was not 10"});
                  return;
                }
                // all seems good!
                port.postMessage({topic: "result", reason: "ok"});
              } catch (ex) {
                port.postMessage({topic: "result", reason: ex.toString()});
              }
            }
            let blob = new Blob([buffer], {type: "binary"});
            reader.readAsArrayBuffer(blob);
          }
        }
      }
    }
    let worker = getFrameWorkerHandle(makeWorkerUrl(run), undefined, "testArray");
    worker.port.onmessage = function(e) {
      if (e.data.topic == "result") {
        is(e.data.reason, "ok", "check the array worked");
        worker.terminate();
        cbnext();
      }
    }
    worker.port.postMessage({topic: "go"});
  },

  testXHR: function(cbnext) {
    // NOTE: this url MUST be in the same origin as worker_xhr.js fetches from!
    let url = "https://example.com/browser/toolkit/components/social/test/browser/worker_xhr.js";
    let worker = getFrameWorkerHandle(url, undefined, "testXHR");
    worker.port.onmessage = function(e) {
      if (e.data.topic == "done") {
        is(e.data.result, "ok", "check the xhr test worked");
        worker.terminate();
        cbnext();
      }
    }
  },

  testLocalStorage: function(cbnext) {
    let run = function() {
      onconnect = function(e) {
        let port = e.ports[0];
        try {
          localStorage.setItem("foo", "1");
        } catch(e) {
          port.postMessage({topic: "done", result: "FAILED to set localStorage, " + e.toString() });
          return;
        }

        var ok;
        try {
          ok = localStorage["foo"] == 1;
        } catch (e) {
          port.postMessage({topic: "done", result: "FAILED to read localStorage, " + e.toString() });
          return;
        }
        port.postMessage({topic: "done", result: "ok"});
      }
    }
    let worker = getFrameWorkerHandle(makeWorkerUrl(run), undefined, "testLocalStorage", null, true);
    worker.port.onmessage = function(e) {
      if (e.data.topic == "done") {
        is(e.data.result, "ok", "check the localStorage test worked");
        worker.terminate();
        cbnext();
      }
    }
  },

  testNoLocalStorage: function(cbnext) {
    let run = function() {
      onconnect = function(e) {
        let port = e.ports[0];
        try {
          localStorage.setItem("foo", "1");
        } catch(e) {
          port.postMessage({topic: "done", result: "ok"});
          return;
        }

        port.postMessage({topic: "done", result: "FAILED because localStorage was exposed" });
      }
    }
    let worker = getFrameWorkerHandle(makeWorkerUrl(run), undefined, "testNoLocalStorage");
    worker.port.onmessage = function(e) {
      if (e.data.topic == "done") {
        is(e.data.result, "ok", "check that retrieving localStorage fails by default");
        worker.terminate();
        cbnext();
      }
    }
  },

  testBase64: function (cbnext) {
    let run = function() {
      onconnect = function(e) {
        let port = e.ports[0];
        var ok = false;
        try {
          ok = btoa("1234") == "MTIzNA==";
        } catch(e) {
          port.postMessage({topic: "done", result: "FAILED to call btoa, " + e.toString() });
          return;
        }
        if (!ok) {
          port.postMessage({topic: "done", result: "FAILED calling btoa"});
          return;
        }

        try {
          ok = atob("NDMyMQ==") == "4321";
        } catch (e) {
          port.postMessage({topic: "done", result: "FAILED to call atob, " + e.toString() });
          return;
        }
        if (!ok) {
          port.postMessage({topic: "done", result: "FAILED calling atob"});
          return;
        }

        port.postMessage({topic: "done", result: "ok"});
      }
    }
    let worker = getFrameWorkerHandle(makeWorkerUrl(run), undefined, "testBase64");
    worker.port.onmessage = function(e) {
      if (e.data.topic == "done") {
        is(e.data.result, "ok", "check the atob/btoa test worked");
        worker.terminate();
        cbnext();
      }
    }
  },

  testTimeouts: function (cbnext) {
    let run = function() {
      onconnect = function(e) {
        let port = e.ports[0];

        var timeout;
        try {
          timeout = setTimeout(function () {
            port.postMessage({topic: "done", result: "FAILED cancelled timeout was called"});
          }, 100);
        } catch (ex) {
          port.postMessage({topic: "done", result: "FAILED calling setTimeout: " + ex});
          return;
        }

        try {
          clearTimeout(timeout);
        } catch (ex) {
          port.postMessage({topic: "done", result: "FAILED calling clearTimeout: " + ex});
          return;
        }

        var counter = 0;
        try {
          timeout = setInterval(function () {
            if (++counter == 2) {
              clearInterval(timeout);
              setTimeout(function () {
                port.postMessage({topic: "done", result: "ok"});
                return;
              }, 0);
            }
          }, 100);
        } catch (ex) {
          port.postMessage({topic: "done", result: "FAILED calling setInterval: " + ex});
          return;
        }
      }
    }
    let worker = getFrameWorkerHandle(makeWorkerUrl(run), undefined, "testTimeouts");
    worker.port.onmessage = function(e) {
      if (e.data.topic == "done") {
        is(e.data.result, "ok", "check that timeouts worked");
        worker.terminate();
        cbnext();
      }
    }
  },

  testWebSocket: function (cbnext) {
    let run = function() {
      onconnect = function(e) {
        let port = e.ports[0];

        try {
          var exampleSocket = new WebSocket("ws://mochi.test:8888/socketserver");
        } catch (e) {
          port.postMessage({topic: "done", result: "FAILED calling WebSocket constructor: " + e});
          return;
        }

        port.postMessage({topic: "done", result: "ok"});
      }
    }
    let worker = getFrameWorkerHandle(makeWorkerUrl(run), undefined, "testWebSocket");
    worker.port.onmessage = function(e) {
      if (e.data.topic == "done") {
        is(e.data.result, "ok", "check that websockets worked");
        worker.terminate();
        cbnext();
      }
    }
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

  testReloadAndNewPort: function(cbnext) {
    let run = function () {
      onconnect = function(e) {
        e.ports[0].postMessage({topic: "ready"});
      }
    }
    let doneReload = false;
    let worker = getFrameWorkerHandle(makeWorkerUrl(run),
                                      undefined, "testReloadAndNewPort");
    worker.port.onmessage = function(e) {
      if (e.data.topic == "ready" && !doneReload) {
        // do the "reload"
        doneReload = true;
        worker._worker.reload();
        let worker2 = getFrameWorkerHandle(makeWorkerUrl(run),
                                           undefined, "testReloadAndNewPort");
        worker2.port.onmessage = function(e) {
          if (e.data.topic == "ready") {
            // "worker" and "worker2" are handles to the same worker
            worker2.terminate();
            cbnext();
          }
        }
      }
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

  testEventSource: function(cbnext) {
    let worker = getFrameWorkerHandle("https://example.com/browser/toolkit/components/social/test/browser/worker_eventsource.js", undefined, "testEventSource");
    worker.port.onmessage = function(e) {
      let m = e.data;
      if (m.topic == "eventSourceTest") {
        if (m.result.ok != undefined)
          ok(m.result.ok, e.data.result.msg);
        if (m.result.is != undefined)
          is(m.result.is, m.result.match, m.result.msg);
        if (m.result.info != undefined)
          info(m.result.info);
      } else if (e.data.topic == "pong") {
        worker.terminate();
        cbnext();
      }
    }
    worker.port.postMessage({topic: "ping"})
  },


  testIndexedDB: function(cbnext) {
    let worker = getFrameWorkerHandle("https://example.com/browser/toolkit/components/social/test/browser/worker_social.js", undefined, "testIndexedDB");
    worker.port.onmessage = function(e) {
      let m = e.data;
      if (m.topic == "social.indexeddb-result") {
        is(m.data.result, "ok", "created indexeddb");
        worker.terminate();
        cbnext();
      }
    }
    worker.port.postMessage({topic: "test-indexeddb-create"})
  },

  testSubworker: function(cbnext) {
    // the main "frameworker"...
    let mainworker = function() {
      onconnect = function(e) {
        let port = e.ports[0];
        port.onmessage = function(e) {
          if (e.data.topic == "go") {
            let suburl = e.data.data;
            let worker = new Worker(suburl);
            worker.onmessage = function(sube) {
              port.postMessage({topic: "sub-message", data: sube.data});
            }
          }
        }
      }
    }

    // The "subworker" that is actually a real, bona-fide worker.
    let subworker = function() {
      postMessage("hello");
    }
    let worker = getFrameWorkerHandle(makeWorkerUrl(mainworker), undefined, "testSubWorker");
    worker.port.onmessage = function(e) {
      if (e.data.topic == "sub-message" && e.data.data == "hello") {
        worker.terminate();
        cbnext();
      }
    }
    worker.port.postMessage({topic: "go", data: makeWorkerUrl(subworker)});
  }
}
