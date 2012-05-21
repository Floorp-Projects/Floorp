/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// requests for files ending with a caret (^) are handled specially to enable
// htaccess-like functionality without the need to explicitly disable display
// of such files

const PREFIX = "http://localhost:4444";

var tests =
  [
   new Test(PREFIX + "/bar.html^",
            null, start_bar_html_, null),
   new Test(PREFIX + "/foo.html^",
            null, start_foo_html_, null),
   new Test(PREFIX + "/normal-file.txt",
            null, start_normal_file_txt, null),
   new Test(PREFIX + "/folder^/file.txt",
            null, start_folder__file_txt, null),

   new Test(PREFIX + "/foo/bar.html^",
            null, start_bar_html_, null),
   new Test(PREFIX + "/foo/foo.html^",
            null, start_foo_html_, null),
   new Test(PREFIX + "/foo/normal-file.txt",
            null, start_normal_file_txt, null),
   new Test(PREFIX + "/foo/folder^/file.txt",
            null, start_folder__file_txt, null),

   new Test(PREFIX + "/end-caret^/bar.html^",
            null, start_bar_html_, null),
   new Test(PREFIX + "/end-caret^/foo.html^",
            null, start_foo_html_, null),
   new Test(PREFIX + "/end-caret^/normal-file.txt",
            null, start_normal_file_txt, null),
   new Test(PREFIX + "/end-caret^/folder^/file.txt",
            null, start_folder__file_txt, null)
  ];


function run_test()
{
  var srv = createServer();

  // make sure underscores work in directories "mounted" in directories with
  // folders starting with _
  var nameDir = do_get_file("data/name-scheme/");
  srv.registerDirectory("/", nameDir);
  srv.registerDirectory("/foo/", nameDir);
  srv.registerDirectory("/end-caret^/", nameDir);

  srv.start(4444);

  runHttpTests(tests, testComplete(srv));
}


// TEST DATA

function start_bar_html_(ch, cx)
{
  do_check_eq(ch.responseStatus, 200);

  do_check_eq(ch.getResponseHeader("Content-Type"), "text/html");
}

function start_foo_html_(ch, cx)
{
  do_check_eq(ch.responseStatus, 404);
}

function start_normal_file_txt(ch, cx)
{
  do_check_eq(ch.responseStatus, 200);
  do_check_eq(ch.getResponseHeader("Content-Type"), "text/plain");
}

function start_folder__file_txt(ch, cx)
{
  do_check_eq(ch.responseStatus, 200);
  do_check_eq(ch.getResponseHeader("Content-Type"), "text/plain");
}
