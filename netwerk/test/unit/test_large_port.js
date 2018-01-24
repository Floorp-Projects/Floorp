/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Ensure that non-16-bit URIs are rejected

"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;

function run_test()
{
    let mutator = Cc["@mozilla.org/network/standard-url-mutator;1"]
                    .createInstance(Ci.nsIURIMutator);
    Assert.ok(mutator, "Mutator constructor works");

    let url = Cc["@mozilla.org/network/standard-url-mutator;1"]
                .createInstance(Ci.nsIStandardURLMutator)
                .init(Ci.nsIStandardURL.URLTYPE_AUTHORITY, 65535,
                      "http://localhost", "UTF-8", null)
                .finalize();

    // Bug 1301621 makes invalid ports throw
    Assert.throws(() => {
        url = Cc["@mozilla.org/network/standard-url-mutator;1"]
                .createInstance(Ci.nsIStandardURLMutator)
                .init(Ci.nsIStandardURL.URLTYPE_AUTHORITY, 65536,
                      "http://localhost", "UTF-8", null)
                .finalize();
    }, "invalid port during creation");

    Assert.throws(() => {
        url = url.mutate()
                 .QueryInterface(Ci.nsIStandardURLMutator)
                 .setDefaultPort(65536)
                 .finalize();
    }, "invalid port in setDefaultPort");
    Assert.throws(() => {
        url = url.mutate()
                 .setPort(65536)
                 .finalize();
    }, "invalid port in port setter");

    Assert.equal(url.port, -1);
    do_test_finished();
}

