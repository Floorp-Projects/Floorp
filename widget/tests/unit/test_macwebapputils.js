/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//Basic tests to verify that MacWebAppUtils works

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cu = Components.utils;
var Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function test_find_app()
{
  var mwaUtils = Cc["@mozilla.org/widget/mac-web-app-utils;1"].
  createInstance(Ci.nsIMacWebAppUtils);
  let sig = "com.apple.TextEdit";

  let path;
  path = mwaUtils.pathForAppWithIdentifier(sig);
  info("TextEdit path: " + path + "\n");
  Assert.notEqual(path, "");
}

function test_dont_find_fake_app()
{
  var mwaUtils = Cc["@mozilla.org/widget/mac-web-app-utils;1"].
  createInstance(Ci.nsIMacWebAppUtils);
  let sig = "calliope.penitentiary.dramamine";

  let path;
  path = mwaUtils.pathForAppWithIdentifier(sig);
  Assert.equal(path, "");
}


function run_test()
{
  test_find_app();
  test_dont_find_fake_app();
}
