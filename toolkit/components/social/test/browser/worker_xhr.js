// Used to test XHR in the worker.
onconnect = function(e) {
  let port = e.ports[0];
  let req;
  try {
    req = new XMLHttpRequest();
  } catch(e) {
    port.postMessage({topic: "done", result: "FAILED to create XHR object, " + e.toString() });
  }
  if (req === undefined) { // until bug 756173 is fixed...
    port.postMessage({topic: "done", result: "FAILED to create XHR object"});
    return;
  }
  // The test that uses this worker MUST use the same origin to load the worker.
  // We fetch the test app manifest so we can check the data is what we expect.
  let url = "https://example.com/browser/toolkit/components/social/test/browser/data.json";
  req.open("GET", url, true);
  req.onreadystatechange = function() {
    if (req.readyState === 4) {
      let ok = req.status == 200 && req.responseText.length > 0;
      if (ok) {
        // check we actually got something sane...
        try {
          let data = JSON.parse(req.responseText);
          ok = "response" in data;
        } catch(e) {
          ok = e.toString();
        }
      }
      port.postMessage({topic: "done", result: ok ? "ok" : "bad response"});
    }
  }
  req.send(null);
}
