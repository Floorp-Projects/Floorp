/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 420482 by making sure multiple consumers of DownloadUtils gets the
 * same time remaining time if they provide the same time left but a different
 * "last time".
 */

var Cu = Components.utils;
Cu.import("resource://gre/modules/DownloadUtils.jsm");

function run_test()
{
  // Simulate having multiple downloads requesting time left
  let downloadTimes = {};
  for (let time of [1, 30, 60, 3456, 9999])
    downloadTimes[time] = DownloadUtils.getTimeLeft(time)[0];

  // Pretend we're a download status bar also asking for a time left, but we're
  // using a different "last sec". We need to make sure we get the same time.
  let lastSec = 314;
  for (let [time, text] in Iterator(downloadTimes))
    do_check_eq(DownloadUtils.getTimeLeft(time, lastSec)[0], text);
}
