/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Sets the worker message and error event handlers to respond to SimpleTest
 * style messages.
 */
function listenForTests(worker, opts = { verbose: true }) {
  worker.onerror = function (error) {
    error.preventDefault();
    ok(false, "Worker error " + error.message);
  };
  worker.onmessage = function (msg) {
    if (opts && opts.verbose) {
      ok(true, "MAIN: onmessage " + JSON.stringify(msg.data));
    }
    switch (msg.data.kind) {
      case "is":
        SimpleTest.ok(
          msg.data.outcome,
          msg.data.description + "( " + msg.data.a + " ==? " + msg.data.b + ")"
        );
        return;
      case "isnot":
        SimpleTest.ok(
          msg.data.outcome,
          msg.data.description + "( " + msg.data.a + " !=? " + msg.data.b + ")"
        );
        return;
      case "ok":
        SimpleTest.ok(msg.data.condition, msg.data.description);
        return;
      case "info":
        SimpleTest.info(msg.data.description);
        return;
      case "finish":
        SimpleTest.finish();
        return;
      default:
        SimpleTest.ok(
          false,
          "test_osfile.xul: wrong message " + JSON.stringify(msg.data)
        );
    }
  };
}
