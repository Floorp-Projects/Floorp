/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

function write(file, str) {
    var stream = Cc["@mozilla.org/network/safe-file-output-stream;1"]
                   .createInstance(Ci.nsIFileOutputStream);
    stream.init(file, -1, -1, 0);
    do {
        var written = stream.write(str, str.length);
        if (written == str.length)
            break;
        str = str.substring(written);
    } while (1);
    stream.QueryInterface(Ci.nsISafeOutputStream).finish();
    stream.close();
}

function checkFile(file, str) {
    var stream = Cc["@mozilla.org/network/file-input-stream;1"]
                   .createInstance(Ci.nsIFileInputStream);
    stream.init(file, -1, -1, 0);

    var scriptStream = Cc["@mozilla.org/scriptableinputstream;1"]
                         .createInstance(Ci.nsIScriptableInputStream);
    scriptStream.init(stream);

    do_check_eq(scriptStream.read(scriptStream.available()), str);
    scriptStream.close();
}

function run_test()
{
    var filename = "\u0913";
    var file = Cc["@mozilla.org/file/directory_service;1"]
                 .getService(Ci.nsIProperties)
                 .get("TmpD", Ci.nsIFile);
    file.append(filename);

    write(file, "First write");
    checkFile(file, "First write");

    write(file, "Second write");
    checkFile(file, "Second write");
}
