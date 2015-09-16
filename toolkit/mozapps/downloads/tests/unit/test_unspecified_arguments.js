/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Make sure passing null and nothing to various variable-arg DownloadUtils
 * methods provide the same result.
 */

var Cu = Components.utils;
Cu.import("resource://gre/modules/DownloadUtils.jsm");

function run_test()
{
  do_check_eq(DownloadUtils.getDownloadStatus(1000, null, null, null) + "",
              DownloadUtils.getDownloadStatus(1000) + "");
  do_check_eq(DownloadUtils.getDownloadStatus(1000, null, null) + "",
              DownloadUtils.getDownloadStatus(1000, null) + "");

  do_check_eq(DownloadUtils.getTransferTotal(1000, null) + "",
              DownloadUtils.getTransferTotal(1000) + "");

  do_check_eq(DownloadUtils.getTimeLeft(1000, null) + "",
              DownloadUtils.getTimeLeft(1000) + "");
}
