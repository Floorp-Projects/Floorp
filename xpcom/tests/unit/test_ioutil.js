/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cr = Components.results;

const util = Cc["@mozilla.org/io-util;1"].getService(Ci.nsIIOUtil);

function run_test()
{
    try {
        util.inputStreamIsBuffered(null);
        do_throw("inputStreamIsBuffered should have thrown");
    } catch (e) {
        do_check_eq(e.result, Cr.NS_ERROR_INVALID_POINTER);
    }

    try {
        util.outputStreamIsBuffered(null);
        do_throw("outputStreamIsBuffered should have thrown");
    } catch (e) {
        do_check_eq(e.result, Cr.NS_ERROR_INVALID_POINTER);
    }

    var s = Cc["@mozilla.org/io/string-input-stream;1"]
              .createInstance(Ci.nsIStringInputStream);
    var body = "This is a test";
    s.setData(body, body.length);
    do_check_eq(util.inputStreamIsBuffered(s), true);
}
