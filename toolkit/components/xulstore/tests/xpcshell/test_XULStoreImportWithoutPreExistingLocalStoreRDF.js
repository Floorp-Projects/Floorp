/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/◦
*/

"use strict"

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/osfile.jsm")

var XULStore = null;
var browserURI = "chrome://browser/content/browser.xul";
var aboutURI = "about:config";

function run_test() {
  do_get_profile();
  run_next_test();
}

function checkOldStore() {
  checkArrays([], getIDs(browserURI));
  checkArrays([], getAttributes(browserURI, "addon-bar"));
  checkArrays([],
              getAttributes(browserURI, "main-window"));
  checkArrays([], getAttributes(browserURI, "sidebar-title"));

  checkValue(browserURI, "addon-bar", "collapsed", "");
  checkValue(browserURI, "main-window", "width", "");
  checkValue(browserURI, "main-window", "height", "");
  checkValue(browserURI, "main-window", "screenX", "");
  checkValue(browserURI, "main-window", "screenY", "");
  checkValue(browserURI, "main-window", "sizemode", "");
  checkValue(browserURI, "sidebar-title", "value", "");

  checkArrays([], getIDs(aboutURI));
  checkArrays([], getAttributes(aboutURI, "lockCol"));
  checkArrays([], getAttributes(aboutURI, "prefCol"));

  checkValue(aboutURI, "prefCol", "ordinal", "");
  checkValue(aboutURI, "prefCol", "sortDirection", "");
  checkValue(aboutURI, "lockCol", "ordinal", "");
}

add_task(async function testImportWithoutPreExistingLocalStoreRDF() {
  XULStore = Cc["@mozilla.org/xul/xulstore;1"].getService(Ci.nsIXULStore);
  checkOldStore();
});
