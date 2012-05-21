/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Make sure setIndexHandler works as expected

var srv, serverBasePath;

function run_test()
{
  srv = createServer();
  serverBasePath = do_get_cwd();
  srv.registerDirectory("/", serverBasePath);
  srv.setIndexHandler(myIndexHandler);
  srv.start(4444);

  runHttpTests(tests, testComplete(srv));
}


var tests = [];
var test;

test = new Test("http://localhost:4444/",
                init, startCustomIndexHandler, stopCustomIndexHandler);
tests.push(test);
function init(ch)
{
  ch.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE; // important!
}
function startCustomIndexHandler(ch, cx)
{
  do_check_eq(ch.getResponseHeader("Content-Length"), "10");
  srv.setIndexHandler(null);
}
function stopCustomIndexHandler(ch, cx, status, data)
{
  do_check_true(Components.isSuccessCode(status));
  do_check_eq(String.fromCharCode.apply(null, data), "directory!");
}

test = new Test("http://localhost:4444/",
                init, startDefaultIndexHandler, stopDefaultIndexHandler);
tests.push(test);
function startDefaultIndexHandler(ch, cx)
{
  do_check_eq(ch.responseStatus, 200);
}
function stopDefaultIndexHandler(ch, cx, status, data)
{
  do_check_true(Components.isSuccessCode(status));
}

// PATH HANDLERS

function myIndexHandler(metadata, response)
{
  var dir = metadata.getProperty("directory");
  do_check_true(dir != null);
  do_check_true(dir instanceof Ci.nsIFile);
  do_check_true(dir.equals(serverBasePath));

  response.write("directory!");
}
