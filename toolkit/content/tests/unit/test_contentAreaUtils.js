/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is bug 342485 unit test.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Asaf Romano <mano@mozilla.com>
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

  try {
    urlSecurityCheck(CHROME_URI, nullPrincipal,
                     DISALLOW_INHERIT_PRINCIPAL);
    do_throw("urlSecurityCheck should throw when linking to a chrome uri with a null principal");
  }
  catch(ex) { }
}

function run_test()
{
  loadUtilsScript();
  test_urlSecurityCheck();
}
