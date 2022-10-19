"use strict";

let timer;

const DELAY_MS = 5000;
function handleRequest(request, response) {
  response.processAsync();
  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.init(
    () => {
      response.setHeader("Content-Type", "text/css ", false);
      response.write("body { background-color: red; }");
      response.finish();
    },
    DELAY_MS,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
}
