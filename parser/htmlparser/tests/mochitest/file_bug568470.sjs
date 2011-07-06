var timer; // Place timer in global scope to avoid it getting GC'ed prematurely

function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);
  response.write("<script src='file_bug568470-script.sjs'></script>");
  response.write("<div id='flushable'>");
  for (var i = 0; i < 2000; i++) { 
    response.write("Lorem ipsum dolor sit amet. ");
  }
  response.write("</div>");
  response.bodyOutputStream.flush();
  response.processAsync();
  timer = Components.classes["@mozilla.org/timer;1"]
    .createInstance(Components.interfaces.nsITimer);
  timer.initWithCallback(function() {
      response.finish();
    }, 1200, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
}

