/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-unused-vars: ["error", { "varsIgnorePattern": "unusedVariable" }] */

function run_test() {
  test1();
  test2();
  test3();
  test4();
}

/**
 * Checks that getting an input stream from a storage stream which has never had
 * anything written to it throws a not-initialized exception.
 */
function test1() {
  var ss = Cc["@mozilla.org/storagestream;1"]
             .createInstance(Ci.nsIStorageStream);
  ss.init(1024, 1024, null);

  var unusedVariable = ss.getOutputStream(0);
  var inp2 = ss.newInputStream(0);
  do_check_eq(inp2.available(), 0);
  do_check_true(inp2.isNonBlocking());

  var sis =
      Cc["@mozilla.org/scriptableinputstream;1"]
        .createInstance(Ci.nsIScriptableInputStream);
  sis.init(inp2);

  var threw = false;
  try {
    sis.read(1);
  } catch (ex) {
    if (ex.result == Cr.NS_BASE_STREAM_WOULD_BLOCK) {
      threw = true;
    } else {
      throw ex;
    }
  }
  do_check_true(threw);
}

/**
 * Checks that getting an input stream from a storage stream to which 0 bytes of
 * data have been explicitly written doesn't throw an exception.
 */
function test2() {
  var ss = Cc["@mozilla.org/storagestream;1"]
             .createInstance(Ci.nsIStorageStream);
  ss.init(1024, 1024, null);

  var out = ss.getOutputStream(0);
  out.write("", 0);
  try {
    ss.newInputStream(0);
  } catch (e) {
    do_throw("shouldn't throw exception when new input stream created");
  }
}

/**
 * Checks that reading any non-zero amount of data from a storage stream
 * which has had 0 bytes written to it explicitly works correctly.
 */
function test3() {
  var ss = Cc["@mozilla.org/storagestream;1"]
             .createInstance(Ci.nsIStorageStream);
  ss.init(1024, 1024, null);

  var out = ss.getOutputStream(0);
  out.write("", 0);
  try {
    var inp = ss.newInputStream(0);
  } catch (e) {
    do_throw("newInputStream(0) shouldn't throw if write() is called: " + e);
  }

  do_check_true(inp.isNonBlocking(), "next test expects a non-blocking stream");

  try {
    var threw = false;
    var bis = BIS(inp);
    bis.readByteArray(5);
  } catch (e) {
    if (e.result != Cr.NS_BASE_STREAM_WOULD_BLOCK)
      do_throw("wrong error thrown: " + e);
    threw = true;
  }
  do_check_true(threw,
                "should have thrown (nsStorageInputStream is nonblocking)");
}

/**
 * Basic functionality test for storagestream: write data to it, get an input
 * stream, and read the data back to see that it matches.
 */
function test4() {
  var bytes = [65, 66, 67, 68, 69, 70, 71, 72, 73, 74];

  var ss = Cc["@mozilla.org/storagestream;1"]
             .createInstance(Ci.nsIStorageStream);
  ss.init(1024, 1024, null);

  var outStream = ss.getOutputStream(0);

  var bos = Cc["@mozilla.org/binaryoutputstream;1"]
              .createInstance(Ci.nsIBinaryOutputStream);
  bos.setOutputStream(outStream);

  bos.writeByteArray(bytes, bytes.length);
  bos.close();

  var inp = ss.newInputStream(0);
  var bis = BIS(inp);

  var count = 0;
  while (count < bytes.length) {
    var data = bis.read8(1);
    do_check_eq(data, bytes[count++]);
  }

  var threw = false;
  try {
    data = bis.read8(1);
  } catch (e) {
    if (e.result != Cr.NS_ERROR_FAILURE)
      do_throw("wrong error thrown: " + e);
    threw = true;
  }
  if (!threw)
    do_throw("should have thrown but instead returned: '" + data + "'");
}


function BIS(input) {
  var bis = Cc["@mozilla.org/binaryinputstream;1"]
              .createInstance(Ci.nsIBinaryInputStream);
  bis.setInputStream(input);
  return bis;
}
