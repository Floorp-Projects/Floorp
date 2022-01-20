var timer; // Place timer in global scope to avoid it getting GC'ed prematurely

function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "application/xml", false);
  response.write("<?");
  response.bodyOutputStream.flush();
  response.processAsync();
  timer = Components.classes["@mozilla.org/timer;1"]
    .createInstance(Components.interfaces.nsITimer);
  timer.initWithCallback(function() {
    response.write("xml");
    response.bodyOutputStream.flush();
    timer.initWithCallback(function() {
      response.write(" version='1.0' encoding='windows-1251'?><root>\u00E6</root>\n");
      response.finish();
    }, 10, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
  }, 10, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
}

