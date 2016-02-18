/* -*- Mode: javascript; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This test exercises the nsIStandardURL "setDefaultPort" API. */

"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;

const StandardURL = Components.Constructor("@mozilla.org/network/standard-url;1",
                                           "nsIStandardURL",
                                           "init");
function run_test() {
  function stringToURL(str) {
    return (new StandardURL(Ci.nsIStandardURL.URLTYPE_AUTHORITY, 80,
			    str, "UTF-8", null))
      .QueryInterface(Ci.nsIStandardURL);
  }

  // Create a nsStandardURL:
  var origUrlStr = "http://foo.com/";
  var stdUrl = stringToURL(origUrlStr);
  var stdUrlAsUri = stdUrl.QueryInterface(Ci.nsIURI);
  do_check_eq(-1, stdUrlAsUri.port);

  // Changing default port shouldn't adjust the value returned by "port",
  // or the string representation.
  stdUrl.setDefaultPort(100);
  do_check_eq(-1, stdUrlAsUri.port);
  do_check_eq(stdUrlAsUri.spec, origUrlStr);

  // Changing port directly should update .port and .spec, though:
  stdUrlAsUri.port = "200";
  do_check_eq(200, stdUrlAsUri.port);
  do_check_eq(stdUrlAsUri.spec, "http://foo.com:200/");

  // ...but then if we change default port to match the custom port,
  // the custom port should reset to -1 and disappear from .spec:
  stdUrl.setDefaultPort(200);
  do_check_eq(-1, stdUrlAsUri.port);
  do_check_eq(stdUrlAsUri.spec, origUrlStr);

  // And further changes to default port should not make custom port reappear.
  stdUrl.setDefaultPort(300);
  do_check_eq(-1, stdUrlAsUri.port);
  do_check_eq(stdUrlAsUri.spec, origUrlStr);
}
