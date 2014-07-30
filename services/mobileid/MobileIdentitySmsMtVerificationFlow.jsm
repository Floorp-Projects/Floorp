/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["MobileIdentitySmsMtVerificationFlow"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/MobileIdentityCommon.jsm");
Cu.import("resource://gre/modules/MobileIdentitySmsVerificationFlow.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.MobileIdentitySmsMtVerificationFlow = function(aVerificationOptions,
                                                    aUI,
                                                    aClient) {

  log.debug("MobileIdentitySmsVerificationFlow ${}", aVerificationOptions);

  MobileIdentitySmsVerificationFlow.call(this,
                                         aVerificationOptions,
                                         aUI,
                                         aClient,
                                         this.smsVerifyStrategy);
};

this.MobileIdentitySmsMtVerificationFlow.prototype = {

  __proto__: MobileIdentitySmsVerificationFlow.prototype,

  smsVerifyStrategy: function() {
    return this.client.smsMtVerify(this.sessionToken,
                                   this.verificationOptions.msisdn,
                                   this.verificationOptions.mcc,
                                   this.verificationOptions.mnc,
                                   this.verificationOptions.external);
  }
};
