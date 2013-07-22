/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Tests that the Content-Length header in incoming requests is interpreted as
 * a decimal number, even if it has the form (including leading zero) of an
 * octal number.
 */

var srv;

function run_test()
{
  srv = createServer();
  srv.registerPathHandler("/content-length", contentLength);
  srv.start(-1);

  runHttpTests(tests, testComplete(srv));
}

const REQUEST_DATA = "12345678901234567";

function contentLength(request, response)
{
  do_check_eq(request.method, "POST");
  do_check_eq(request.getHeader("Content-Length"), "017");

  var body = new ScriptableInputStream(request.bodyInputStream);

  var avail;
  var data = "";
  while ((avail = body.available()) > 0)
    data += body.read(avail);

  do_check_eq(data, REQUEST_DATA);
}

/***************
 * BEGIN TESTS *
 ***************/

XPCOMUtils.defineLazyGetter(this, 'tests', function() {
  return [
           new Test("http://localhost:" + srv.identity.primaryPort + "/content-length",
                    init_content_length),
  ];
});

function init_content_length(ch)
{
  var content = Cc["@mozilla.org/io/string-input-stream;1"]
                  .createInstance(Ci.nsIStringInputStream);
  content.data = REQUEST_DATA;

  ch.QueryInterface(Ci.nsIUploadChannel)
    .setUploadStream(content, "text/plain", REQUEST_DATA.length);

  // Override the values implicitly set by setUploadStream above.
  ch.requestMethod = "POST";
  ch.setRequestHeader("Content-Length", "017", false); // 17 bytes, not 15
}
