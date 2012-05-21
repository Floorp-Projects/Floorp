/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// make sure response.write works for strings, and coerces other args to strings

var tests =
  [
   new Test("http://localhost:4444/writeString",
            null, check_1234, succeeded),
   new Test("http://localhost:4444/writeInt",
            null, check_1234, succeeded),
  ];

function run_test()
{
  var srv = createServer();

  srv.registerPathHandler("/writeString", writeString);
  srv.registerPathHandler("/writeInt", writeInt);
  srv.start(4444);

  runHttpTests(tests, testComplete(srv));
}


// TEST DATA

function succeeded(ch, cx, status, data)
{
  do_check_true(Components.isSuccessCode(status));
  do_check_eq(data.map(function(v) String.fromCharCode(v)).join(""), "1234");
}

function check_1234(ch, cx)
{
  do_check_eq(ch.getResponseHeader("Content-Length"), "4");
}

// PATH HANDLERS

function writeString(metadata, response)
{
  response.write("1234");
}

function writeInt(metadata, response)
{
  response.write(1234);
}
