/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://testing-common/httpd.js");

function run_test() {
  var httpserver = new HttpServer();
  do_check_neq(httpserver, null);
  do_check_neq(httpserver.QueryInterface(Components.interfaces.nsIHttpServer), null);
}
