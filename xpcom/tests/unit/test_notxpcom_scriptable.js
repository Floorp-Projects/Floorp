/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const kCID = Components.ID("{1f9f7181-e6c5-4f4c-8f71-08005cec8468}");
const kContract = "@testing/notxpcomtest";

function run_test()
{
  let manifest = do_get_file("xpcomtest.manifest");
  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.autoRegister(manifest);

  ok(Ci.ScriptableWithNotXPCOM);

  let method1Called = false;

  let testObject = {
    QueryInterface: XPCOMUtils.generateQI([Ci.ScriptableOK,
                                           Ci.ScriptableWithNotXPCOM,
                                           Ci.ScriptableWithNotXPCOMBase]),

    method1: function() {
      method1Called = true;
    },

    method2: function() {
      ok(false, "method2 should not have been called!");
    },

    method3: function() {
      ok(false, "mehod3 should not have been called!");
    },

    jsonly: true,
  };

  let factory = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory]),

    createInstance: function(outer, iid) {
      if (outer) {
        throw Cr.NS_ERROR_NO_AGGREGATION;
      }
      return testObject.QueryInterface(iid);
    },
  };

  registrar.registerFactory(kCID, null, kContract, factory);

  let xpcomObject = Cc[kContract].createInstance();
  ok(xpcomObject);
  strictEqual(xpcomObject.jsonly, undefined);

  xpcomObject.QueryInterface(Ci.ScriptableOK);

  xpcomObject.method1();
  ok(method1Called);

  try {
    xpcomObject.QueryInterface(Ci.ScriptableWithNotXPCOM);
    ok(false, "Should not have implemented ScriptableWithNotXPCOM");
  }
  catch(e) {
    ok(true, "Should not have implemented ScriptableWithNotXPCOM. Correctly threw error: " + e);
  }
  strictEqual(xpcomObject.method2, undefined);

  try {
    xpcomObject.QueryInterface(Ci.ScriptableWithNotXPCOMBase);
    ok(false, "Should not have implemented ScriptableWithNotXPCOMBase");
  }
  catch (e) {
    ok(true, "Should not have implemented ScriptableWithNotXPCOMBase. Correctly threw error: " + e);
  }
  strictEqual(xpcomObject.method3, undefined);
}

