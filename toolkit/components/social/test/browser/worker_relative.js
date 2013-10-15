// Used to test XHR in the worker.
onconnect = function(e) {
  let port = e.ports[0];
  let req;
  try {
    importScripts("relative_import.js");
    // the import should have exposed "testVar" and "testFunc" from the module.
    if (testVar != "oh hai" || testFunc() != "oh hai") {
      port.postMessage({topic: "done", result: "import worked but global is not available"});
      return;
    }

    // causeError will cause a script error, so that we can check the
    // error location for importScripts'ed files is correct.
    try {
      causeError();
    } catch(e) {
      let fileName = e.fileName;
      if (fileName.startsWith("http") &&
          fileName.endsWith("/relative_import.js") &&
          e.lineNumber == 4)
        port.postMessage({topic: "done", result: "ok"});
      else
        port.postMessage({topic: "done", result: "invalid error location: " + fileName + ":" + e.lineNumber});
      return;
    }
  } catch(e) {
    port.postMessage({topic: "done", result: "FAILED to importScripts, " + e.toString() });
    return;
  }
  port.postMessage({topic: "done", result: "FAILED to importScripts, no exception" });
}
