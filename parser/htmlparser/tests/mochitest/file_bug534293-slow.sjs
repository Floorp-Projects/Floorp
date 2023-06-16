var timer;

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/javascript", false);
  response.write("ok(true, 'Slow script ran.');");
  response.processAsync();
  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(
    function () {
      response.finish();
    },
    500,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
}
