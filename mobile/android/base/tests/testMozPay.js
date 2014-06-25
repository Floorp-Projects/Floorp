// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Components.utils.import("resource://gre/modules/SharedPreferences.jsm");
Components.utils.import("resource://gre/modules/Promise.jsm");

let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"].getService(Ci.nsIMessageListenerManager);
let deferred = 0;
let shouldPass = true;
let reqId = 0;
function getRequestId(increment) {
  reqId += increment;
  return "Request" + reqId;
}

let paymentSuccess = {
  receiveMessage: function(aMsg) {
    let msg = aMsg.json;
    if (shouldPass) {
      do_check_eq(msg.requestId, getRequestId(0));
    } else {
      do_throw("Test should not have passed");
    }
    deferred.resolve();
  }
}

let paymentFailed = {
  receiveMessage: function(aMsg) {
    let msg = aMsg.json;
    if (shouldPass) {
      do_throw("Test should not have failed: " + msg.errorMsg);
    } else {
      do_check_eq(msg.requestId, getRequestId(0));
      do_check_eq(msg.errorMsg, "FAILED CORRECTLY");
    }
    deferred.resolve();
  }
}

add_task(function test_get_set() {
  let ui = Cc["@mozilla.org/payment/ui-glue;1"].getService(Ci.nsIPaymentUIGlue);
  deferred = Promise.defer();
  let id = getRequestId(1);
  ui.confirmPaymentRequest(id,
                           [{ wrappedJSObject: { type: "Fake Provider" } }],
                           function(aRequestId, type) {
                             do_check_eq(type, "Fake Provider");
                             deferred.resolve();
                           },
                           function(id, msg) {
                             do_throw("confirmPaymentRequest should not have failed");
                             deferred.resolve();
                           });
  yield deferred.promise;
});

add_task(function test_default() {
  ppmm.addMessageListener("Payment:Success", paymentSuccess);
  ppmm.addMessageListener("Payment:Failed", paymentFailed);

  let ui = Cc["@mozilla.org/payment/ui-glue;1"].getService(Ci.nsIPaymentUIGlue);
  deferred = Promise.defer();
  let id = getRequestId(1);
  shouldPass = true;
  ui.showPaymentFlow(id,
                     {
                       uri: "chrome://roboextender/content/paymentsUI.html",
                       jwt: "#pass"
                     },
                     function(id, msg) {
                       do_throw("confirmPaymentRequest should not have failed");
                       deferred.resolve();
                     });
  yield deferred.promise;

  deferred = Promise.defer();
  let id = getRequestId(1);
  shouldPass = false;
  ui.showPaymentFlow(id,
                     {
                       uri: "chrome://roboextender/content/paymentsUI.html",
                       jwt: "#fail"
                     },
                     function(id, msg) {
                       do_throw("confirmPaymentRequest should not have failed");
                       deferred.resolve();
                     });

  yield deferred.promise;

  ppmm.removeMessageListener("Payment:Success", paymentSuccess);
  ppmm.removeMessageListener("Payment:Failed", paymentFailed);
});

run_next_test();
