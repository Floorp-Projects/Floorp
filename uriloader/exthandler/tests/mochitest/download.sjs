"use strict";

function actuallyHandleRequest(req, res) {
  res.setHeader("Content-Type", "application/octet-stream", false);
  res.write("abc123");
  res.finish();
}

function handleRequest(req, res) {
  if (req.queryString.includes("finish")) {
    res.write("OK");
    let downloadReq = null;
    getObjectState("downloadReq", o => {
      downloadReq = o;
    });
    // Two possibilities: either the download request has already reached us, or not.
    if (downloadReq) {
      downloadReq.wrappedJSObject.callback();
    } else {
      // Set a variable to allow the request to complete immediately:
      setState("finishReq", "true");
    }
  } else if (req.queryString.includes("reset")) {
    res.write("OK");
    setObjectState("downloadReq", null);
    setState("finishReq", "false");
  } else {
    res.processAsync();
    if (getState("finishReq") === "true") {
      actuallyHandleRequest(req, res);
    } else {
      let o = {
        callback() {
          actuallyHandleRequest(req, res);
        },
      };
      o.wrappedJSObject = o;
      o.QueryInterface = () => o;
      setObjectState("downloadReq", o);
    }
  }
}
