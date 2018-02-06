/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the download manager backend

do_get_profile();

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://testing-common/httpd.js");

function createURI(aObj) {
  return (aObj instanceof Ci.nsIFile) ? Services.io.newFileURI(aObj) :
                                        Services.io.newURI(aObj);
}
