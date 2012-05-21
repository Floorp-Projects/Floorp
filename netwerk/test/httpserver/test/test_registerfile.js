/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// tests the registerFile API

const BASE = "http://localhost:4444";

var file = do_get_file("test_registerfile.js");

function onStart(ch, cx)
{
  do_check_eq(ch.responseStatus, 200);
}

function onStop(ch, cx, status, data)
{
  // not sufficient for equality, but not likely to be wrong!
  do_check_eq(data.length, file.fileSize);
}

var test = new Test(BASE + "/foo", null, onStart, onStop);

function run_test()
{
  var srv = createServer();

  try
  {
    srv.registerFile("/foo", do_get_cwd());
    throw "registerFile succeeded!";
  }
  catch (e)
  {
    isException(e, Cr.NS_ERROR_INVALID_ARG);
  }

  srv.registerFile("/foo", file);
  srv.start(4444);

  runHttpTests([test], testComplete(srv));
}
