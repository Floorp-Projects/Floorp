// Used to test XHR in the worker.
onconnect = function(e) {
  let port = e.ports[0];
  try {
    localStorage.setItem("foo", "1");
  } catch(e) {
    port.postMessage({topic: "done", result: "FAILED to set localStorage, " + e.toString() });
  }

  var ok;
  try {
    ok = localStorage["foo"] == 1;
  } catch (e) {
    ok = false;
  }
  port.postMessage({topic: "done", result: ok ? "ok" : "FAILED to read localStorage"});
}
