/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

function loadUtilsScript() {
  var loader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
               getService(Ci.mozIJSSubScriptLoader);
  loader.loadSubScript("chrome://global/content/contentAreaUtils.js");
}

function test_urlSecurityCheck() {
  var nullPrincipal = Cc["@mozilla.org/nullprincipal;1"].
                      createInstance(Ci.nsIPrincipal);

  const HTTP_URI = "http://www.mozilla.org/";
  const CHROME_URI = "chrome://browser/content/browser.xul";
  const DISALLOW_INHERIT_PRINCIPAL =
    Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL;

  try {
    urlSecurityCheck(makeURI(HTTP_URI), nullPrincipal,
                     DISALLOW_INHERIT_PRINCIPAL);
  }
  catch(ex) {
    do_throw("urlSecurityCheck should not throw when linking to a http uri with a null principal");
  }

  // urlSecurityCheck also supports passing the url as a string
  try {
    urlSecurityCheck(HTTP_URI, nullPrincipal,
                     DISALLOW_INHERIT_PRINCIPAL);
  }
  catch(ex) {
    do_throw("urlSecurityCheck failed to handle the http URI as a string (uri spec)");
  }

  let shouldThrow = true;
  try {
    urlSecurityCheck(CHROME_URI, nullPrincipal,
                     DISALLOW_INHERIT_PRINCIPAL);
  }
  catch(ex) { 
    shouldThrow = false;
  }
  if (shouldThrow)
    do_throw("urlSecurityCheck should throw when linking to a chrome uri with a null principal");
}

function test_stringBundle() {
  // This test verifies that the elements that can be used as file picker title
  //  keys in the save* functions are actually present in the string bundle.
  //  These keys are part of the contentAreaUtils.js public API.
  var validFilePickerTitleKeys = [
    "SaveImageTitle",
    "SaveVideoTitle",
    "SaveAudioTitle",
    "SaveLinkTitle",
  ];

  for (let [, filePickerTitleKey] in Iterator(validFilePickerTitleKeys)) {
    // Just check that the string exists
    try {
      ContentAreaUtils.stringBundle.GetStringFromName(filePickerTitleKey);
    } catch (e) {
      do_throw("Error accessing file picker title key: " + filePickerTitleKey);
    }
  }
}

function run_test()
{
  loadUtilsScript();
  test_urlSecurityCheck();
  test_stringBundle();
}
