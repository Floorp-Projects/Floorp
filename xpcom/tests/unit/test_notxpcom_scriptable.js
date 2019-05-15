/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kCID = Components.ID("{1f9f7181-e6c5-4f4c-8f71-08005cec8468}");
const kContract = "@testing/notxpcomtest";

function run_test() {
  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

  ok(Ci.nsIScriptableWithNotXPCOM);

  let method1Called = false;

  let testObject = {
    QueryInterface: ChromeUtils.generateQI([Ci.nsIScriptableOK,
                                            Ci.nsIScriptableWithNotXPCOM]),

    method1() {
      method1Called = true;
    },

    method2() {
      ok(false, "method2 should not have been called!");
    },

    method3() {
      ok(false, "mehod3 should not have been called!");
    },

    jsonly: true,
  };

  let factory = {
    QueryInterface: ChromeUtils.generateQI([Ci.nsIFactory]),

    createInstance(outer, iid) {
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

  xpcomObject.QueryInterface(Ci.nsIScriptableOK);

  xpcomObject.method1();
  ok(method1Called);

  try {
    xpcomObject.QueryInterface(Ci.nsIScriptableWithNotXPCOM);
    ok(false, "Should not have implemented nsIScriptableWithNotXPCOM");
  } catch (e) {
    ok(true, "Should not have implemented nsIScriptableWithNotXPCOM. Correctly threw error: " + e);
  }
  strictEqual(xpcomObject.method2, undefined);
}
