var timer = null; // Declare outside to prevent premature GC

function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/javascript", false);
  response.write("var i = 0;");
  response.bodyOutputStream.flush();
  response.processAsync();
  timer = Components.classes["@mozilla.org/timer;1"]
    .createInstance(Components.interfaces.nsITimer);
  timer.initWithCallback(function() {
      response.finish();
    }, 500, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
}

