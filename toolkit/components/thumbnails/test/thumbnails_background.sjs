/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// The timer never fires if it's not declared and set to this variable outside
// handleRequest, as if it's getting GC'ed when handleRequest's scope goes away.
// Shouldn't the timer thread hold a strong reference to it?
var timer;

function handleRequest(req, resp) {
  resp.processAsync();
  resp.setHeader("Cache-Control", "no-cache", false);
  resp.setHeader("Content-Type", "text/html;charset=utf-8", false);

  let opts = {};
  try {
    opts = JSON.parse(decodeURIComponent(req.queryString));
  }
  catch (err) {}

  if (opts.setGreenCookie)
    resp.setHeader("Set-Cookie", "green", false);

  if (req.hasHeader("Cookie") &&
      req.getHeader("Cookie").split(";").indexOf("green") >= 0) {
    resp.write('<html style="background: #0f0;"></html>');
    resp.finish();
    return;
  }

  if (opts.redirect) {
    resp.setHeader("Location", opts.redirect);
    resp.setStatusLine(null, 303, null);
    resp.finish();
    return;
  }

  if (opts.wait) {
    resp.write("Waiting " + opts.wait + " ms... ");
    timer = Components.classes["@mozilla.org/timer;1"].
            createInstance(Components.interfaces.nsITimer);
    timer.init(function ding() {
      resp.write("OK!");
      resp.finish();
    }, opts.wait, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
    return;
  }

  resp.write("<pre>" + JSON.stringify(opts, undefined, 2) + "</pre>");
  resp.finish();
}
