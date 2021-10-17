var timer;

function armTimer(response) {
  timer = Components.classes["@mozilla.org/timer;1"]
    .createInstance(Components.interfaces.nsITimer);
  timer.initWithCallback(function() {
      if (getState("docwritepreloadssecond") == "second" && getState("docwritepreloadsthird") == "third") {
        response.write("ok(true, 'Second and third scripts should have started loading while the first one is loading');");
        response.finish();
      } else {
        armTimer(response);
      }
    }, 20, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
}

function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/javascript", false);
  if (request.queryString.indexOf("first") != -1) {
    response.write("// first\n");
    response.processAsync();
    armTimer(response);
  } else if (request.queryString.indexOf("second") != -1) {
    response.write("// second\n");
    setState("docwritepreloadssecond", "second");
  } else {
    response.write("// third\n");
    setState("docwritepreloadsthird", "third");
  }
}

