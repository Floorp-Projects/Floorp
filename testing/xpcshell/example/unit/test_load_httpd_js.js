/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

function run_test() {
  var httpserver = new HttpServer();
  Assert.notEqual(httpserver, null);
  Assert.notEqual(httpserver.QueryInterface(Ci.nsIHttpServer), null);
}
