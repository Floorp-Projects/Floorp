/***************************** BEGIN LICENSE BLOCK *****************************
* Version: MPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with the
* License. You may obtain a copy of the License at http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
* the specific language governing rights and limitations under the License.
*
* The Original Code is DownloadUtils Test Code.
*
* The Initial Developer of the Original Code is Mozilla Foundation.
* Portions created by the Initial Developer are Copyright (C) 2009 the Initial
* Developer. All Rights Reserved.
*
* Contributor(s):
*  Edward Lee <edilee@mozilla.com> (original author)
*
* Alternatively, the contents of this file may be used under the terms of either
* the GNU General Public License Version 2 or later (the "GPL"), or the GNU
* Lesser General Public License Version 2.1 or later (the "LGPL"), in which case
* the provisions of the GPL or the LGPL are applicable instead of those above.
* If you wish to allow use of your version of this file only under the terms of
* either the GPL or the LGPL, and not to allow others to use your version of
* this file under the terms of the MPL, indicate your decision by deleting the
* provisions above and replace them with the notice and other provisions
* required by the GPL or the LGPL. If you do not delete the provisions above, a
* recipient may use your version of this file under the terms of any one of the
* MPL, the GPL or the LGPL.
*
****************************** END LICENSE BLOCK ******************************/

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
let _ = function(some, debug, text, to) print(Array.slice(arguments).join(" "));

_("Make an array of time lefts and expected string to be shown for that time");
let expectedTimes = [
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
