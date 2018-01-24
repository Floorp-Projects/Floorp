/* -*- Mode: javascript; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This test exercises the nsIStandardURL "setDefaultPort" API. */

"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;

function run_test() {
  function stringToURL(str) {
    return Cc["@mozilla.org/network/standard-url-mutator;1"]
             .createInstance(Ci.nsIStandardURLMutator)
             .init(Ci.nsIStandardURL.URLTYPE_AUTHORITY, 80, str, "UTF-8", null)
             .finalize()
             .QueryInterface(Ci.nsIStandardURL);
  }

  // Create a nsStandardURL:
  var origUrlStr = "http://foo.com/";
  var stdUrl = stringToURL(origUrlStr);
  Assert.equal(-1, stdUrl.port);

  // Changing default port shouldn't adjust the value returned by "port",
  // or the string representation.
  let def100Url = stdUrl.mutate()
                        .QueryInterface(Ci.nsIStandardURLMutator)
                        .setDefaultPort(100)
                        .finalize();
  Assert.equal(-1, def100Url.port);
  Assert.equal(def100Url.spec, origUrlStr);

  // Changing port directly should update .port and .spec, though:
  let port200Url = stdUrl.mutate()
                         .setPort("200")
                         .finalize();
  Assert.equal(200, port200Url.port);
  Assert.equal(port200Url.spec, "http://foo.com:200/");

  // ...but then if we change default port to match the custom port,
  // the custom port should reset to -1 and disappear from .spec:
  let def200Url = port200Url.mutate()
                            .QueryInterface(Ci.nsIStandardURLMutator)
                            .setDefaultPort(200)
                            .finalize();
  Assert.equal(-1, def200Url.port);
  Assert.equal(def200Url.spec, origUrlStr);

  // And further changes to default port should not make custom port reappear.
  let def300Url = def200Url.mutate()
                           .QueryInterface(Ci.nsIStandardURLMutator)
                           .setDefaultPort(300)
                           .finalize();
  Assert.equal(-1, def300Url.port);
  Assert.equal(def300Url.spec, origUrlStr);
}
