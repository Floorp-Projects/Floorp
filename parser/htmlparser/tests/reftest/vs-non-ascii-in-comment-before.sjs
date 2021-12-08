var timer; // Place timer in global scope to avoid it getting GC'ed prematurely

function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);
  response.write('<!DOCTYPE html>\n<head>\n<link rel="match" href="references/non-ascii-in-comment-before-ref.html">\n<!-- \u00E6 -->\n');
  response.bodyOutputStream.flush();
  response.processAsync();
  timer = Components.classes["@mozilla.org/timer;1"]
    .createInstance(Components.interfaces.nsITimer);
  timer.initWithCallback(function() {
    response.write('<meta charset="windows-1251">\n</head>\n<body>\n<p>Normal meta. Non-ASCII in comment before.</p>\n<p>Test: \u00E6</p>\n<p>If &#x0436;, meta takes effect</p>\n</body>\n');
    response.finish();
  }, 10, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
}
