/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  // "text/plain" has an 0xFF character appended to it.  This means it's an
  // invalid string, which is tricky to enter using a text editor (I used
  // emacs' hexl-mode).  It also means an ordinary text editor might drop it
  // or convert it to something that *is* valid (in UTF8).  So we measure
  // its length to make sure this hasn't happened.
  var badMimeType = "text/plainÿ";
  do_check_eq(badMimeType.length, 11);

  try {
    var type = Cc["@mozilla.org/mime;1"].
               getService(Ci.nsIMIMEService).
               getFromTypeAndExtension(badMimeType, "txt");
  } catch (e if (e instanceof Ci.nsIException &&
                 e.result == Cr.NS_ERROR_NOT_AVAILABLE)) {
    // This is an expected exception, thrown if the type can't be determined
  } finally {
  }
  // Not crashing is good enough
  do_check_eq(true, true);
}
