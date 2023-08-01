/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Tests for correct behavior of the server start_ipv6() and stop() methods.
 */

ChromeUtils.defineLazyGetter(this, "PORT", function () {
  return srv.identity.primaryPort;
});

ChromeUtils.defineLazyGetter(this, "PREPATH", function () {
  return "http://localhost:" + PORT;
});

var srv, srv2;

function run_test() {
  if (mozinfo.os == "win") {
    dumpn(
      "*** not running test_start_stop.js on Windows for now, because " +
        "Windows is dumb"
    );
    return;
  }

  dumpn("*** run_test");

  srv = createServer();
  srv.start_ipv6(-1);

  try {
    srv.start_ipv6(PORT);
    do_throw("starting a started server");
  } catch (e) {
    isException(e, Cr.NS_ERROR_ALREADY_INITIALIZED);
  }

  do_test_pending();
  srv.stop(function () {
    try {
      do_test_pending();
      run_test_2();
    } finally {
      do_test_finished();
    }
  });
}

function run_test_2() {
  dumpn("*** run_test_2");

  do_test_finished();

  srv.start_ipv6(PORT);
  srv2 = createServer();

  try {
    srv2.start_ipv6(PORT);
    do_throw("two servers on one port?");
  } catch (e) {
    isException(e, Cr.NS_ERROR_NOT_AVAILABLE);
  }

  do_test_pending();
  try {
    srv.stop({
      onStopped() {
        try {
          do_test_pending();
          run_test_3();
        } finally {
          do_test_finished();
        }
      },
    });
  } catch (e) {
    do_throw("error stopping with an object: " + e);
  }
}

function run_test_3() {
  dumpn("*** run_test_3");

  do_test_finished();

  srv.start_ipv6(PORT);

  do_test_pending();
  try {
    srv.stop().then(function () {
      try {
        do_test_pending();
        run_test_4();
      } finally {
        do_test_finished();
      }
    });
  } catch (e) {
    do_throw("error stopping with an object: " + e);
  }
}

function run_test_4() {
  dumpn("*** run_test_4");

  do_test_finished();

  srv.registerPathHandler("/handle", handle);
  srv.start_ipv6(PORT);

  // Don't rely on the exact (but implementation-constant) sequence of events
  // as it currently exists by making either run_test_5 or serverStopped handle
  // the final shutdown.
  do_test_pending();

  runHttpTests([new Test(PREPATH + "/handle")], run_test_5);
}

var testsComplete = false;

function run_test_5() {
  dumpn("*** run_test_5");

  testsComplete = true;
  if (stopped) {
    do_test_finished();
  }
}

const INTERVAL = 500;

function handle(request, response) {
  response.processAsync();

  dumpn("*** stopping server...");
  srv.stop(serverStopped);

  callLater(INTERVAL, function () {
    Assert.ok(!stopped);

    callLater(INTERVAL, function () {
      Assert.ok(!stopped);
      response.finish();

      try {
        response.processAsync();
        do_throw("late processAsync didn't throw?");
      } catch (e) {
        isException(e, Cr.NS_ERROR_UNEXPECTED);
      }
    });
  });
}

var stopped = false;
function serverStopped() {
  dumpn("*** server really, fully shut down now");
  stopped = true;
  if (testsComplete) {
    do_test_finished();
  }
}
