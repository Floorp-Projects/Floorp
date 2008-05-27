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
 * The Original Code is MozJSHTTP code.
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
  var nameDir = do_get_file("netwerk/test/httpserver/test/data/name-scheme/");
  srv.registerDirectory("/", nameDir);
  srv.registerDirectory("/foo/", nameDir);
  srv.registerDirectory("/end-caret^/", nameDir);

  srv.start(4444);

  runHttpTests(tests, function() { srv.stop(); });
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
