/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests features made available to the frameworker via the sandbox.
// For other frameworker tests, see browser_frameworker.js

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
