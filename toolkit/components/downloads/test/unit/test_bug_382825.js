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
 * The Original Code is Download Manager Test Code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

// This tests the retryDownload function of nsIDownloadManager.  This function
// was added in Bug 382825.

const nsIDownloadManager = Ci.nsIDownloadManager;
const dm = Cc["@mozilla.org/download-manager;1"].getService(nsIDownloadManager);

function test_retry_canceled()
{
  var dl = addDownload();

  // since we are going to be retrying a failed download, we need to inflate
  // this so it doesn't stop our server
  gDownloadCount++;

  dm.cancelDownload(dl.id);

  do_check_eq(nsIDownloadManager.DOWNLOAD_CANCELED, dl.state);

  // Our download object will no longer be updated.
  dm.retryDownload(dl.id);
}

function test_retry_bad()
{
  try {
    dm.retryDownload(0);
    do_throw("Hey!  We expect to get an exception with this!");
  } catch(e) {
    do_check_eq(Components.lastResult, Cr.NS_ERROR_NOT_AVAILABLE);
  }
}

var tests = [test_retry_canceled, test_retry_bad];

var httpserv = null;
function run_test()
{
  httpserv = new nsHttpServer();
  httpserv.registerDirectory("/", do_get_cwd());
  httpserv.start(4444);

  dm.addListener(getDownloadListener());

  for (var i = 0; i < tests.length; i++)
    tests[i]();
}
