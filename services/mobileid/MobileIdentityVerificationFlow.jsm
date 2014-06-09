/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["MobileIdentityVerificationFlow"];

const { utils: Cu, classes: Cc, interfaces: Ci } = Components;

Cu.import("resource://gre/modules/MobileIdentityCommon.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.MobileIdentityVerificationFlow = function(aVerificationOptions,
                                               aUI,
                                               aClient,
                                               aVerifyStrategy,
                                               aCleanupStrategy) {
  this.verificationOptions = aVerificationOptions;
  this.ui = aUI;
  this.client = aClient;
  this.retries = VERIFICATIONCODE_RETRIES;
  this.verifyStrategy = aVerifyStrategy;
  this.cleanupStrategy = aCleanupStrategy;
};

MobileIdentityVerificationFlow.prototype = {

  doVerification: function() {
    log.debug("Start verification flow");
    return this.register()
    .then(
      (registerResult) => {
        log.debug("Register result ${}", registerResult);
        if (!registerResult || !registerResult.msisdnSessionToken) {
          return Promise.reject(ERROR_INTERNAL_UNEXPECTED);
        }
        this.sessionToken = registerResult.msisdnSessionToken;
        return this._doVerification();
      }
    )
  },

  _doVerification: function() {
    log.debug("_doVerification");
    // We save the timestamp of the start of the verification timeout to be
    // able to provide to the UI the remaining time on each retry.
    if (!this.timer) {
      log.debug("Creating verification code timer");
      this.timerCreation = Date.now();
      this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      this.timer.initWithCallback(this.onVerificationCodeTimeout.bind(this),
                                  VERIFICATIONCODE_TIMEOUT,
                                  this.timer.TYPE_ONE_SHOT);
    }

    if (!this.verifyStrategy) {
      return Promise.reject(ERROR_INTERNAL_INVALID_VERIFICATION_FLOW);
    }

    this.verificationCodeDeferred = Promise.defer();

    this.verifyStrategy()
    .then(
      () => {
        // If the verification flow can be for an external phone number,
        // we need to ask the user for the verification code.
        // In that case we don't do a notification about the verification
        // process being done until the user enters the verification code
        // in the UI.
        if (this.verificationOptions.external) {
          let timeLeft = 0;
          if (this.timer) {
            timeLeft = this.timerCreation + VERIFICATIONCODE_TIMEOUT -
                       Date.now();
          }
          this.ui.verificationCodePrompt(this.retries,
                                         VERIFICATIONCODE_TIMEOUT / 1000,
                                         timeLeft / 1000)
          .then(
            (verificationCode) => {
              if (!verificationCode) {
                return this.verificationCodeDeferred.reject(
                  ERROR_INTERNAL_INVALID_PROMPT_RESULT);
              }
              // If the user got the verification code that means that the
              // introduced phone number didn't belong to any of the inserted
              // SIMs.
              this.ui.verify();
              this.verificationCodeDeferred.resolve(verificationCode);
            }
          );
        } else {
          this.ui.verify();
        }
      },
      (reason) => {
        this.verificationCodeDeferred.reject(reason);
      }
    );
    return this.verificationCodeDeferred.promise.then(
      this.onVerificationCode.bind(this)
    );
  },

  // When we receive a verification code from the UI, we check it against
  // the server. If the verification code is incorrect, we decrease the
  // number of retries left and allow the user to try again. If there is no
  // possible retry left, we notify about this error so the UI can allow the
  // user to request the resend of a new verification code.
  onVerificationCode: function(aVerificationCode) {
    log.debug("onVerificationCode " + aVerificationCode);
    if (!aVerificationCode) {
      this.ui.error(ERROR_INVALID_VERIFICATION_CODE);
      return this._doVerification();
    }

    // Before checking the verification code against the server we set the
    // "verifying" flag to queue timeout expiration events received before
    // the server request is completed. If the server request is positive
    // we will discard the timeout event, otherwise we will progress the
    // event to the UI to allow the user to retry.
    this.verifying = true;

    return this.verifyCode(aVerificationCode)
    .then(
      (result) => {
        if (!result) {
          return Promise.reject(INTERNAL_UNEXPECTED);
        }
        // The code was correct!
        // At this point the phone number is verified.
        // We return the given verification options with the session token
        // to be stored in the credentials store. With this data we will be
        // asking the server to give us a certificate to generate assertions.
        this.verificationOptions.sessionToken = this.sessionToken;
        this.verificationOptions.msisdn = result.msisdn ||
                                          this.verificationOptions.msisdn;
        return this.verificationOptions;
      },
      (error) => {
        log.error("Verification code error " + error);
        this.retries--;
        log.error("Retries left " + this.retries);
        if (!this.retries) {
          this.ui.error(ERROR_NO_RETRIES_LEFT);
          return Promise.reject(ERROR_NO_RETRIES_LEFT);
        }
        this.verifying = false;
        if (this.queuedTimeout) {
          this.onVerificationCodeTimeout();
        }
        return this._doVerification();
      }
    );
  },

  onVerificationCodeTimeout: function() {
    // It is possible that we get the timeout when we are checking a
    // verification code with the server. In that case, we queue the
    // timeout to be triggered after we receive the reply from the server
    // if needed.
    if (this.verifying) {
      this.queuedTimeout = true;
      return;
    }

    // When the verification process times out we do a clean up, reject
    // the corresponding promise and notify the UI about the timeout.
    if (this.verificationCodeDeferred) {
      this.verificationCodeDeferred.reject(ERROR_VERIFICATION_CODE_TIMEOUT);
    }
    this.ui.error(ERROR_VERIFICATION_CODE_TIMEOUT);
  },

  register: function() {
    return this.client.register();
  },

  verifyCode: function(aVerificationCode) {
    return this.client.verifyCode(this.sessionToken, aVerificationCode);
  },

  unregister: function() {
    return this.client.unregister(this.sessionToken);
  },

  cleanup: function(aUnregister = false) {
    log.debug("Verification flow cleanup");

    this.queuedTimeout = false;
    this.retries = VERIFICATIONCODE_RETRIES;

    if (this.timer) {
      this.timer.cancel();
      this.timer = null;
    }

    if (aUnregister) {
      this.unregister().
      then(
        () => {
          this.sessionToken = null;
        }
      );
    }

    if (this.cleanupStrategy) {
      this.cleanupStrategy();
    }
  }
};
