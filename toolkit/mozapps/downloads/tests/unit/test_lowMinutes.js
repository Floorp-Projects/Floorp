/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 448344 to make sure when we're in low minutes, we show both minutes
 * and seconds; but continue to show only minutes when we have plenty.
 */

Components.utils.import("resource://gre/modules/DownloadUtils.jsm");

/**
 * Print some debug message to the console. All arguments will be printed,
 * separated by spaces.
 *
 * @param [arg0, arg1, arg2, ...]
 *        Any number of arguments to print out
 * @usage _("Hello World") -> prints "Hello World"
 * @usage _(1, 2, 3) -> prints "1 2 3"
 */
var _ = (some, debug, text, to) => print(Array.slice(arguments).join(" "));

_("Make an array of time lefts and expected string to be shown for that time");
var expectedTimes = [
  [1.1, "A few seconds remaining", "under 4sec -> few"],
  [2.5, "A few seconds remaining", "under 4sec -> few"],
  [3.9, "A few seconds remaining", "under 4sec -> few"],
  [5.3, "5 seconds remaining", "truncate seconds"],
  [1.1 * 60, "1 minute, 6 seconds remaining", "under 4min -> show sec"],
  [2.5 * 60, "2 minutes, 30 seconds remaining", "under 4min -> show sec"],
  [3.9 * 60, "3 minutes, 54 seconds remaining", "under 4min -> show sec"],
  [5.3 * 60, "5 minutes remaining", "over 4min -> only show min"],
  [1.1 * 3600, "1 hour, 6 minutes remaining", "over 1hr -> show min/sec"],
  [2.5 * 3600, "2 hours, 30 minutes remaining", "over 1hr -> show min/sec"],
  [3.9 * 3600, "3 hours, 54 minutes remaining", "over 1hr -> show min/sec"],
  [5.3 * 3600, "5 hours, 18 minutes remaining", "over 1hr -> show min/sec"],
];
_(expectedTimes.join("\n"));

function run_test()
{
  expectedTimes.forEach(function([time, expectStatus, comment]) {
    _("Running test with time", time);
    _("Test comment:", comment);
    let [status, last] = DownloadUtils.getTimeLeft(time);

    _("Got status:", status, "last:", last);
    _("Expecting..", expectStatus);
    do_check_eq(status, expectStatus);

    _();
  });
}
