/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["MockPaymentsUIGlue"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cm = Components.manager;
const Cu = Components.utils;

const CONTRACT_ID = "@mozilla.org/payment/ui-glue;1";

Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
let classID;
let oldFactory;
let newFactory = function(window) {
  return {
    createInstance: function(aOuter, aIID) {
      if (aOuter) {
        throw Components.results.NS_ERROR_NO_AGGREGATION;
      }
      return new MockPaymentsUIGlueInstance(window).QueryInterface(aIID);
    },
    lockFactory: function(aLock) {
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory])
  };
};

this.MockPaymentsUIGlue = {
  init: function(aWindow) {
    try {
      classID = registrar.contractIDToCID(CONTRACT_ID);
      oldFactory = Cm.getClassObject(Cc[CONTRACT_ID], Ci.nsIFactory);
    } catch (ex) {
      oldClassID = "";
      oldFactory = null;
      dump("TEST-INFO | can't get payments ui glue registered component, " +
          "assuming there is none");
    }
    if (oldFactory) {
      registrar.unregisterFactory(classID, oldFactory);
    }
    registrar.registerFactory(classID, "", CONTRACT_ID,
                              new newFactory(aWindow));
  },

  reset: function() {
  },

  cleanup: function() {
    this.reset();
    if (oldFactory) {
      registrar.unregisterFactory(classID, newFactory);
      registrar.registerFactory(classID, "", CONTRACT_ID, oldFactory);
    }
  }
};

function MockPaymentsUIGlueInstance(aWindow) {
  this.window = aWindow;
};

MockPaymentsUIGlueInstance.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPaymentUIGlue]),

  confirmPaymentRequest: function(aRequestId,
                                  aRequests,
                                  aSuccessCb,
                                  aErrorCb) {
    aSuccessCb.onresult(aRequestId, aRequests[0].type);
  },

  showPaymentFlow: function(aRequestId,
                            aPaymentFlowInfo,
                            aErrorCb) {
    let document = this.window.document;
    let frame = document.createElement("iframe");
    frame.setAttribute("mozbrowser", true);
    frame.setAttribute("remote", true);
    document.body.appendChild(frame);
    let docshell = frame.contentWindow
                        .QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebNavigation)
                        .QueryInterface(Ci.nsIDocShell);
    docshell.paymentRequestId = aRequestId;
    frame.src = aPaymentFlowInfo.uri + aPaymentFlowInfo.jwt;
  },

  closePaymentFlow: function(aRequestId) {
    return Promise.resolve();
  }
};
// Expose everything to content. We call reset() here so that all of the relevant
// lazy expandos get added.
MockPaymentsUIGlue.reset();
function exposeAll(obj) {
  var props = {};
  for (var prop in obj)
    props[prop] = 'rw';
  obj.__exposedProps__ = props;
}
exposeAll(MockPaymentsUIGlue);
exposeAll(MockPaymentsUIGlueInstance.prototype);
