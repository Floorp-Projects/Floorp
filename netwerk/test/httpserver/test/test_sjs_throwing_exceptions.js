/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is httpd.js code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Walden <jwalden+code@mit.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Tests that running an SJS a whole lot of times doesn't have any ill effects
 * (like exceeding open-file limits due to not closing the SJS file each time,
 * then preventing any file from being opened).
 */

const PORT = 4444;

function run_test()
{
  var srv = createServer();
  var sjsDir = do_get_file("netwerk/test/httpserver/test/data/sjs/");
  srv.registerDirectory("/", sjsDir);
  srv.registerContentType("sjs", "sjs");
  srv.start(PORT);

  function done()
  {
    srv.stop();
    do_check_eq(gStartCount, TEST_RUNS);
    do_check_true(lastPassed);
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

var test = new Test("http://localhost:4444/thrower.sjs?throw",
                    null, start_thrower);

var tests = new Array(TEST_RUNS + 1);
for (var i = 0; i < TEST_RUNS; i++)
  tests[i] = test;

// ...and don't forget to stop!
tests[TEST_RUNS] = new Test("http://localhost:4444/thrower.sjs",
                            null, start_last);

function start_thrower(ch, cx)
{
  do_check_eq(ch.responseStatus, 500);
  do_check_false(ch.requestSucceeded);

  gStartCount++;
}

function start_last(ch, cx)
{
  do_check_eq(ch.responseStatus, 200);
  do_check_true(ch.requestSucceeded);

  do_check_eq(ch.getResponseHeader("X-Test-Status"), "PASS");

  lastPassed = true;
}
