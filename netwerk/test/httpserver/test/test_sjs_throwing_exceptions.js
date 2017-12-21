/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Tests that running an SJS a whole lot of times doesn't have any ill effects
 * (like exceeding open-file limits due to not closing the SJS file each time,
 * then preventing any file from being opened).
 */

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + srv.identity.primaryPort;
});

var srv;

function run_test()
{
  srv = createServer();
  var sjsDir = do_get_file("data/sjs/");
  srv.registerDirectory("/", sjsDir);
  srv.registerContentType("sjs", "sjs");
  srv.start(-1);

  function done()
  {
    do_test_pending();
    srv.stop(function() { do_test_finished(); });
    Assert.equal(gStartCount, TEST_RUNS);
    Assert.ok(lastPassed);
  }

  runHttpTests(tests, done);
}

/***************
 * BEGIN TESTS *
 ***************/

var gStartCount = 0;
var lastPassed = false;

// This hits the open-file limit for me on OS X; your mileage may vary.
const TEST_RUNS = 250;

XPCOMUtils.defineLazyGetter(this, "tests", function() {
  var _tests = new Array(TEST_RUNS + 1);
  var _test = new Test(URL + "/thrower.sjs?throw", null, start_thrower);
  for (var i = 0; i < TEST_RUNS; i++)
    _tests[i] = _test;
  // ...and don't forget to stop!
  _tests[TEST_RUNS] = new Test(URL + "/thrower.sjs", null, start_last);
  return _tests;
});

function start_thrower(ch, cx)
{
  Assert.equal(ch.responseStatus, 500);
  Assert.ok(!ch.requestSucceeded);

  gStartCount++;
}

function start_last(ch, cx)
{
  Assert.equal(ch.responseStatus, 200);
  Assert.ok(ch.requestSucceeded);

  Assert.equal(ch.getResponseHeader("X-Test-Status"), "PASS");

  lastPassed = true;
}
