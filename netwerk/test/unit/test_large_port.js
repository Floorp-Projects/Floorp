/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Ensure that non-16-bit URIs are rejected

"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;

const StandardURL = Components.Constructor("@mozilla.org/network/standard-url;1",
                                           "nsIStandardURL",
                                           "init");
function run_test()
{
    // Bug 1301621 makes invalid ports throw
    Assert.throws(() => {
        new StandardURL(Ci.nsIStandardURL.URLTYPE_AUTHORITY, 65536,
                "http://localhost", "UTF-8", null)
    }, "invalid port during creation");
    let url = new StandardURL(Ci.nsIStandardURL.URLTYPE_AUTHORITY, 65535,
                              "http://localhost", "UTF-8", null)
                .QueryInterface(Ci.nsIStandardURL)

    Assert.throws(() => {
        url.setDefaultPort(65536);
    }, "invalid port in setDefaultPort");
    Assert.throws(() => {
        url.port = 65536;
    }, "invalid port in port setter");

    Assert.equal(url.QueryInterface(Ci.nsIURI).port, -1);
    do_test_finished();
}

