/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["MobileIdentitySmsMoMtVerificationFlow"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/MobileIdentityCommon.jsm");
Cu.import("resource://gre/modules/MobileIdentitySmsVerificationFlow.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "smsService",
                                   "@mozilla.org/sms/smsservice;1",
                                   "nsISmsService");

// In order to send messages through nsISmsService, we need to implement
// nsIMobileMessageCallback, as the WebSMS API implementation is not usable
// from JS.
function SilentSmsRequest(aDeferred) {
  this.deferred = aDeferred;
}
SilentSmsRequest.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileMessageCallback]),

  classID: Components.ID("{ff46f1a8-e040-4ff4-98a7-d5a5b86a2c3e}"),

  notifyMessageSent: function notifyMessageSent(aMessage) {
    log.debug("Silent message successfully sent");
    this.deferred.resolve(aMessage);
  },

  notifySendMessageFailed: function notifySendMessageFailed(aError) {
    log.error("Error sending silent message " + aError);
    this.deferred.reject(aError);
  }
};

this.MobileIdentitySmsMoMtVerificationFlow = function(aOrigin,
                                                      aServiceId,
                                                      aIccId,
                                                      aMtSender,
                                                      aMoVerifier,
                                                      aUI,
                                                      aClient) {

  log.debug("MobileIdentitySmsMoMtVerificationFlow");

  MobileIdentitySmsVerificationFlow.call(this,
                                         aOrigin,
                                         null, //msisdn
                                         aIccId,
                                         aServiceId,
                                         false, // external
                                         aMtSender,
                                         aMoVerifier,
                                         aUI,
                                         aClient,
                                         this.smsVerifyStrategy);
};

this.MobileIdentitySmsMoMtVerificationFlow.prototype = {

  __proto__: MobileIdentitySmsVerificationFlow.prototype,

  smsVerifyStrategy: function() {
    // In the MO+MT flow we need to send an SMS to the given moVerifier number
    // so the server can find out our phone number to send an SMS back with a
    // verification code.
    let deferred = Promise.defer();
    let silentSmsRequest = new SilentSmsRequest(deferred);

    // The MO SMS body that the server expects contains the API endpoint for
    // the MO verify request and the HAWK ID parameter derived via HKDF from
    // the session token. These parameters should go unnamed and space limited.
    let body = SMS_MO_MT_VERIFY + " " +
               this.client._deriveHawkCredentials(this.sessionToken).id;
    smsService.send(this.verificationOptions.serviceId,
                    this.verificationOptions.moVerifier,
                    body,
                    true, // silent
                    silentSmsRequest);
    log.debug("Sending " + body + " to " + this.verificationOptions.moVerifier);
    return deferred.promise;
  }
};
