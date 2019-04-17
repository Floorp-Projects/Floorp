/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 448344 to make sure when we're in low minutes, we show both minutes
 * and seconds; but continue to show only minutes when we have plenty.
 */

const {DownloadUtils} = ChromeUtils.import("resource://gre/modules/DownloadUtils.jsm");

/**
 * Print some debug message to the console. All arguments will be printed,
 * separated by spaces.
 *
 * @param [arg0, arg1, arg2, ...]
 *        Any number of arguments to print out
 * @usage _("Hello World") -> prints "Hello World"
 * @usage _(1, 2, 3) -> prints "1 2 3"
 */
var _ = function(some, debug, text, to) {
  print(Array.from(arguments).join(" "));
};

_("Make an array of time lefts and expected string to be shown for that time");
var expectedTimes = [
  [1.1, "A few seconds left", "under 4sec -> few"],
  [2.5, "A few seconds left", "under 4sec -> few"],
  [3.9, "A few seconds left", "under 4sec -> few"],
  [5.3, "5s left", "truncate seconds"],
  [1.1 * 60, "1m 6s left", "under 4min -> show sec"],
  [2.5 * 60, "2m 30s left", "under 4min -> show sec"],
  [3.9 * 60, "3m 54s left", "under 4min -> show sec"],
  [5.3 * 60, "5m left", "over 4min -> only show min"],
  [1.1 * 3600, "1h 6m left", "over 1hr -> show min/sec"],
  [2.5 * 3600, "2h 30m left", "over 1hr -> show min/sec"],
  [3.9 * 3600, "3h 54m left", "over 1hr -> show min/sec"],
  [5.3 * 3600, "5h 18m left", "over 1hr -> show min/sec"],
];
_(expectedTimes.join("\n"));

function run_test() {
  expectedTimes.forEach(function([time, expectStatus, comment]) {
    _("Running test with time", time);
    _("Test comment:", comment);
    let [status, last] = DownloadUtils.getTimeLeft(time);

    _("Got status:", status, "last:", last);
    _("Expecting..", expectStatus);
    Assert.equal(status, expectStatus);

    _();
  });
}
