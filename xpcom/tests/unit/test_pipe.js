/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const CC = Components.Constructor;

var Pipe = CC("@mozilla.org/pipe;1", "nsIPipe", "init");

function run_test()
{
  test_not_initialized();
  test_ends_are_threadsafe();
}

function test_not_initialized()
{
  var p = Cc["@mozilla.org/pipe;1"]
            .createInstance(Ci.nsIPipe);
  try
  {
    var dummy = p.outputStream;
    throw Cr.NS_ERROR_FAILURE;
  }
  catch (e)
  {
    if (e.result != Cr.NS_ERROR_NOT_INITIALIZED)
      do_throw("using a pipe before initializing it should throw NS_ERROR_NOT_INITIALIZED");
  }
}

function test_ends_are_threadsafe()
{
  var p, is, os;

  p = new Pipe(true, true, 1024, 1, null);
  is = p.inputStream.QueryInterface(Ci.nsIClassInfo);
  os = p.outputStream.QueryInterface(Ci.nsIClassInfo);
  do_check_true(Boolean(is.flags & Ci.nsIClassInfo.THREADSAFE));
  do_check_true(Boolean(os.flags & Ci.nsIClassInfo.THREADSAFE));

  p = new Pipe(true, false, 1024, 1, null);
  is = p.inputStream.QueryInterface(Ci.nsIClassInfo);
  os = p.outputStream.QueryInterface(Ci.nsIClassInfo);
  do_check_true(Boolean(is.flags & Ci.nsIClassInfo.THREADSAFE));
  do_check_true(Boolean(os.flags & Ci.nsIClassInfo.THREADSAFE));

  p = new Pipe(false, true, 1024, 1, null);
  is = p.inputStream.QueryInterface(Ci.nsIClassInfo);
  os = p.outputStream.QueryInterface(Ci.nsIClassInfo);
  do_check_true(Boolean(is.flags & Ci.nsIClassInfo.THREADSAFE));
  do_check_true(Boolean(os.flags & Ci.nsIClassInfo.THREADSAFE));

  p = new Pipe(false, false, 1024, 1, null);
  is = p.inputStream.QueryInterface(Ci.nsIClassInfo);
  os = p.outputStream.QueryInterface(Ci.nsIClassInfo);
  do_check_true(Boolean(is.flags & Ci.nsIClassInfo.THREADSAFE));
  do_check_true(Boolean(os.flags & Ci.nsIClassInfo.THREADSAFE));
}
