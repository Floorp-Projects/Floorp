/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// exercises support for mod_cern_meta-style header/status line modification
var srv;

XPCOMUtils.defineLazyGetter(this, 'PREFIX', function() {
  return "http://localhost:" + srv.identity.primaryPort;
});

XPCOMUtils.defineLazyGetter(this, 'tests', function() {
  return [
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
});

function run_test()
{
  srv = createServer();

  var cernDir = do_get_file("data/cern_meta/");
  srv.registerDirectory("/", cernDir);

  srv.start(-1);

  runHttpTests(tests, testComplete(srv));
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
