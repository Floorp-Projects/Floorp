/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// REST client for
// https://github.com/mozilla-services/msisdn-gateway/blob/master/API.md

"use strict";

this.EXPORTED_SYMBOLS = ["MobileIdentityClient"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://services-common/hawkclient.js");
Cu.import("resource://services-common/hawkrequest.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-crypto/utils.js");
Cu.import("resource://gre/modules/MobileIdentityCommon.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Services.jsm");

this.MobileIdentityClient = function(aServerUrl) {
  let serverUrl = aServerUrl || SERVER_URL;
  let forceHttps = false;
  try {
    // TODO: Force https in production. Bug 1021595.
    forceHttps = Services.prefs.getBoolPref(PREF_FORCE_HTTPS);
  } catch(e) {
    log.warn("Getting force HTTPS pref failed. If this was not intentional " +
             "check that " + PREF_FORCE_HTTPS + " is defined");
  }

  log.debug("Force HTTPS " + forceHttps);

  if (forceHttps && !/^https/.exec(serverUrl.toLowerCase())) {
    throw new Error(ERROR_INTERNAL_HTTP_NOT_ALLOWED);
  }

  this.hawk = new HawkClient(SERVER_URL);
  this.hawk.observerPrefix = "MobileId:hawk";
};

this.MobileIdentityClient.prototype = {

  discover: function(aMsisdn, aMcc, aMnc, aRoaming) {
    return this._request(DISCOVER, "POST", null, {
      msisdn: aMsisdn || undefined,
      mcc: aMcc,
      mnc: aMnc,
      roaming: aRoaming
    });
  },

  register: function() {
    return this._request(REGISTER, "POST", null, {});
  },

  smsMtVerify: function(aSessionToken, aMsisdn, aWantShortCode = false) {
    let credentials = this._deriveHawkCredentials(aSessionToken);
    return this._request(SMS_MT_VERIFY, "POST", credentials, {
      msisdn: aMsisdn,
      shortVerificationCode: aWantShortCode
    });
  },

  verifyCode: function(aSessionToken, aVerificationCode) {
    log.debug("verificationCode " + aVerificationCode);
    let credentials = this._deriveHawkCredentials(aSessionToken);
    return this._request(SMS_VERIFY_CODE, "POST", credentials, {
      code: aVerificationCode
    });
  },

  sign: function(aSessionToken, aDuration, aPublicKey) {
    let credentials = this._deriveHawkCredentials(aSessionToken);
    return this._request(SIGN, "POST", credentials, {
      duration: aDuration,
      publicKey: aPublicKey
    });
  },

  unregister: function(aSessionToken) {
    let credentials = this._deriveHawkCredentials(aSessionToken);
    return this._request(UNREGISTER, "POST", credentials, {});
  },

  /**
   * The MobileID server expects requests to certain endpoints to be
   * authorized using Hawk.
   *
   * Hawk credentials are derived using shared secrets.
   *
   * @param tokenHex
   *        The current session token encoded in hex
   * @param context
   *        A context for the credentials
   * @param size
   *        The size in bytes of the expected derived buffer
   * @return credentials
   *        Returns an object:
   *        {
   *          algorithm: sha256
   *          id: the Hawk id (from the first 32 bytes derived)
   *          key: the Hawk key (from bytes 32 to 64)
   *        }
   */
  _deriveHawkCredentials: function(aSessionToken) {
    return deriveHawkCredentials(aSessionToken, CREDENTIALS_DERIVATION_INFO,
                                 CREDENTIALS_DERIVATION_SIZE, true /*hexKey*/);
  },

  /**
   * A general method for sending raw API calls to the mobile id verification
   * server.
   * All request bodies and responses are JSON.
   *
   * @param path
   *        API endpoint path
   * @param method
   *        The HTTP request method
   * @param credentials
   *        Hawk credentials
   * @param jsonPayload
   *        A JSON payload
   * @return Promise
   *        Returns a promise that resolves to the JSON response of the API
   *        call, or is rejected with an error.
   */
  _request: function(path, method, credentials, jsonPayload) {
    let deferred = Promise.defer();

    this.hawk.request(path, method, credentials, jsonPayload).then(
      (response) => {
        log.debug("MobileIdentityClient -> response.body " + response.body);
        try {
          let responseObj = JSON.parse(response.body);
          deferred.resolve(responseObj);
        } catch (err) {
          deferred.reject({error: err});
        }
      },

      (error) => {
        log.error("MobileIdentityClient -> Error ${}", error);
        deferred.reject(SERVER_ERRNO_TO_ERROR[error.errno] || ERROR_UNKNOWN);
      }
    );

    return deferred.promise;
  },

};
