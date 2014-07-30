/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["MobileIdentitySmsVerificationFlow"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/MobileIdentityCommon.jsm");
Cu.import("resource://gre/modules/MobileIdentityVerificationFlow.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

#ifdef MOZ_B2G_RIL
XPCOMUtils.defineLazyServiceGetter(this, "smsService",
                                   "@mozilla.org/sms/smsservice;1",
                                   "nsISmsService");
#endif

this.MobileIdentitySmsVerificationFlow = function(aVerificationOptions,
                                                  aUI,
                                                  aClient,
                                                  aVerifyStrategy) {

  // SMS MT or SMS MO+MT specific verify strategy.
  this.smsVerifyStrategy = aVerifyStrategy;

  log.debug("aVerificationOptions ${}", aVerificationOptions);
  MobileIdentityVerificationFlow.call(this, aVerificationOptions, aUI, aClient,
                                      this._verifyStrategy, this._cleanupStrategy);
};

this.MobileIdentitySmsVerificationFlow.prototype = {

  __proto__: MobileIdentityVerificationFlow.prototype,

  observedSilentNumber: null,

  onSilentSms: null,

  _verifyStrategy: function() {
    if (!this.smsVerifyStrategy) {
      return Promise.reject(ERROR_INTERNAL_UNEXPECTED);
    }

    // Even if the user selection is given to us as a possible external phone
    // number, it is also possible that the phone number introduced by the
    // user belongs to one of the SIMs inserted in the device which MSISDN
    // is unknown for us, so we always observe for incoming messages coming
    // from the given mtSender.

#ifdef MOZ_B2G_RIL
    this.observedSilentNumber = this.verificationOptions.mtSender;
    try {
      smsService.addSilentNumber(this.observedSilentNumber);
    } catch (e) {
      log.warn("We are already listening for that number");
    }

    this.onSilentSms = (function(aSubject, aTopic, aData) {
      log.debug("Got silent message " + aSubject.sender + " - " + aSubject.body);
      // We might have observed a notification of an incoming silent message
      // for other number. In that case, we just bail out.
      if (aSubject.sender != this.observedSilentNumber) {
        return;
      }

      // We got the SMS containing the verification code.

      // If the phone number we are trying to verify is or can be an external
      // phone number (meaning that it doesn't belong to any of the inserted
      // SIMs) we will be receiving an human readable SMS containing a short
      // verification code. In this case we need to parse the SMS body to
      // extract the verification code.
      // Otherwise, we just use the whole SMS body as it should contain a long
      // verification code.
      let verificationCode = aSubject.body;
      if (this.verificationOptions.external) {
        // We just take the numerical characters from the body.
        verificationCode = aSubject.body.replace(/[^0-9]/g,'');
      }

      log.debug("Verification code: " + verificationCode);

      this.verificationCodeDeferred.resolve(verificationCode);
    }).bind(this);

    Services.obs.addObserver(this.onSilentSms,
                             SILENT_SMS_RECEIVED_TOPIC,
                             false);
    log.debug("Observing messages from " + this.observedSilentNumber);
#endif

    return this.smsVerifyStrategy();
  },

  _cleanupStrategy: function() {
#ifdef MOZ_B2G_RIL
    smsService.removeSilentNumber(this.observedSilentNumber);
    Services.obs.removeObserver(this.onSilentSms,
                                SILENT_SMS_RECEIVED_TOPIC);
    this.observedSilentNumber = null;
    this.onSilentSms = null;
#endif
  }
};
