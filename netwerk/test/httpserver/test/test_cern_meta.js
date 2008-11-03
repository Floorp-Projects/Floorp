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
 * The Original Code is httpd.js code.
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

// exercises support for mod_cern_meta-style header/status line modification

const PREFIX = "http://localhost:4444";

var tests =
  [
   new Test(PREFIX + "/test_both.html",
            null, start_testBoth, null),
   new Test(PREFIX + "/test_ctype_override.txt",
            null, start_test_ctype_override_txt, null),
   new Test(PREFIX + "/test_status_override.html",
            null, start_test_status_override_html, null),
   new Test(PREFIX + "/test_status_override_nodesc.txt",
            null, start_test_status_override_nodesc_txt, null),
   new Test(PREFIX + "/caret_test.txt^",
            null, start_caret_test_txt_, null)
  ];

function run_test()
{
  var srv = createServer();

  var cernDir = do_get_file("netwerk/test/httpserver/test/data/cern_meta/");
  srv.registerDirectory("/", cernDir);

  srv.start(4444);

  runHttpTests(tests, function() { srv.stop(); });
}


// TEST DATA

function start_testBoth(ch, cx)
{
  do_check_eq(ch.responseStatus, 501);
  do_check_eq(ch.responseStatusText, "Unimplemented");

  do_check_eq(ch.getResponseHeader("Content-Type"), "text/plain");
}

function start_test_ctype_override_txt(ch, cx)
{
  do_check_eq(ch.getResponseHeader("Content-Type"), "text/html");
}

function start_test_status_override_html(ch, cx)
{
  do_check_eq(ch.responseStatus, 404);
  do_check_eq(ch.responseStatusText, "Can't Find This");
}

function start_test_status_override_nodesc_txt(ch, cx)
{
  do_check_eq(ch.responseStatus, 732);
  do_check_eq(ch.responseStatusText, "");
}

function start_caret_test_txt_(ch, cx)
{
  do_check_eq(ch.responseStatus, 500);
  do_check_eq(ch.responseStatusText, "This Isn't A Server Error");

  do_check_eq(ch.getResponseHeader("Foo-RFC"), "3092");
  do_check_eq(ch.getResponseHeader("Shaving-Cream-Atom"), "Illudium Phosdex");
}
