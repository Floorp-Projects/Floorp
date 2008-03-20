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
 * The Original Code is DownloadUtils Test Code.
 *
 * The Initial Developer of the Original Code is
 * Edward Lee <edward.lee@engineering.uiuc.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/**
 * Test bug 420482 by making sure multiple consumers of DownloadUtils gets the
 * same time remaining time if they provide the same time left but a different
 * "last time".
 */

let Cu = Components.utils;
Cu.import("resource://gre/modules/DownloadUtils.jsm");

function run_test()
{
  // Simulate having multiple downloads requesting time left
  let downloadTimes = {};
  for each (let time in [1, 30, 60, 3456, 9999])
    downloadTimes[time] = DownloadUtils.getTimeLeft(time)[0];

  // Pretend we're a download status bar also asking for a time left, but we're
  // using a different "last sec". We need to make sure we get the same time.
  let lastSec = 314;
  for (let [time, text] in Iterator(downloadTimes))
    do_check_eq(DownloadUtils.getTimeLeft(time, lastSec)[0], text);
}
