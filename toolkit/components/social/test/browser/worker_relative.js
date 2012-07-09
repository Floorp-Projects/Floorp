// Used to test XHR in the worker.
onconnect = function(e) {
  let port = e.ports[0];
  let req;
  try {
    importScripts("relative_import.js");
    port.postMessage({topic: "done", result: "ok"});
  } catch(e) {
    port.postMessage({topic: "done", result: "FAILED to importScripts, " + e.toString() });
    return;
  }
  port.postMessage({topic: "done", result: "FAILED to importScripts, no exception" });
}
