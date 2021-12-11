/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// The delay time will not impact test running time, as the test does
// not wait for the "load" event. We just need to ensure the pwmgr code
// e.g. autofill happens before the delay time elapses.
const DELAY_MS = "5000";

let timer;

function handleRequest(req, resp) {
  resp.processAsync();
  resp.setHeader("Cache-Control", "no-cache", false);
  resp.setHeader("Content-Type", "image/png", false);

  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.init(
    () => {
      resp.write("");
      resp.finish();
    },
    DELAY_MS,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
}
