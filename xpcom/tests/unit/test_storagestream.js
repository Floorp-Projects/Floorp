/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Original Code is XPCOM unit tests.
 *
 * The Initial Developer of the Original Code is
 * Jeff Walden <jwalden+code@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2007
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

function run_test()
{
  test1();
  test2();
  test3();
  test4();
}

/**
 * Checks that getting an input stream from a storage stream which has never had
 * anything written to it throws a not-initialized exception.
 */
function test1()
{
  var ss = Cc["@mozilla.org/storagestream;1"]
             .createInstance(Ci.nsIStorageStream);
  ss.init(1024, 1024, null);

  var out = ss.getOutputStream(0);
  try
  {
    var threw = false;
    var inp2 = ss.newInputStream(0);
  }
  catch (e)
  {
    threw = e.result == Cr.NS_ERROR_NOT_INITIALIZED;
  }
  if (!threw)
    do_throw("must write to a storagestream before creating an " +
             "inputstream for it, even if just .write('', 0)");
}

/**
 * Checks that getting an input stream from a storage stream to which 0 bytes of
 * data have been explicitly written doesn't throw an exception.
 */
function test2()
{
  var ss = Cc["@mozilla.org/storagestream;1"]
             .createInstance(Ci.nsIStorageStream);
  ss.init(1024, 1024, null);

  var out = ss.getOutputStream(0);
  out.write("", 0);
  try
  {
    var inp2 = ss.newInputStream(0);
  }
  catch (e)
  {
    do_throw("shouldn't throw exception when new input stream created");
  }
}

/**
 * Checks that reading any non-zero amount of data from a storage stream
 * which has had 0 bytes written to it explicitly works correctly.
 */
function test3()
{
  var ss = Cc["@mozilla.org/storagestream;1"]
             .createInstance(Ci.nsIStorageStream);
  ss.init(1024, 1024, null);

  var out = ss.getOutputStream(0);
  out.write("", 0);
  try
  {
    var inp = ss.newInputStream(0);
  }
  catch (e)
  {
    do_throw("newInputStream(0) shouldn't throw if write() is called: " + e);
  }

  do_check_true(inp.isNonBlocking(), "next test expects a non-blocking stream");

  try
  {
    var threw = false;
    var bis = BIS(inp);
    var dummy = bis.readByteArray(5);
  }
  catch (e)
  {
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
function test4()
{
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
  while (count < bytes.length)
  {
    var data = bis.read8(1);
    do_check_eq(data, bytes[count++]);
  }

  var threw = false;
  try
  {
    data = bis.read8(1);
  }
  catch (e)
  {
    if (e.result != Cr.NS_ERROR_FAILURE)
      do_throw("wrong error thrown: " + e);
    threw = true;
  }
  if (!threw)
    do_throw("should have thrown but instead returned: '" + data + "'");
}


function BIS(input)
{
  var bis = Cc["@mozilla.org/binaryinputstream;1"]
              .createInstance(Ci.nsIBinaryInputStream);
  bis.setInputStream(input);
  return bis;
}
