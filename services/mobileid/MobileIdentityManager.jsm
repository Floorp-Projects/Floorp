/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["MobileIdentityManager"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/MobileIdentityCommon.jsm");
Cu.import("resource://gre/modules/MobileIdentityUIGlueCommon.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "MobileIdentityCredentialsStore",
  "resource://gre/modules/MobileIdentityCredentialsStore.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "MobileIdentityClient",
  "resource://gre/modules/MobileIdentityClient.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "MobileIdentitySmsMtVerificationFlow",
  "resource://gre/modules/MobileIdentitySmsMtVerificationFlow.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "MobileIdentitySmsMoMtVerificationFlow",
  "resource://gre/modules/MobileIdentitySmsMoMtVerificationFlow.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PhoneNumberUtils",
  "resource://gre/modules/PhoneNumberUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "jwcrypto",
  "resource://gre/modules/identity/jwcrypto.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageListenerManager");

XPCOMUtils.defineLazyServiceGetter(this, "permissionManager",
                                   "@mozilla.org/permissionmanager;1",
                                   "nsIPermissionManager");

XPCOMUtils.defineLazyServiceGetter(this, "securityManager",
                                   "@mozilla.org/scriptsecuritymanager;1",
                                   "nsIScriptSecurityManager");

XPCOMUtils.defineLazyServiceGetter(this, "appsService",
                                   "@mozilla.org/AppsService;1",
                                   "nsIAppsService");

#ifdef MOZ_B2G_RIL
XPCOMUtils.defineLazyServiceGetter(this, "Ril",
                                   "@mozilla.org/ril;1",
                                   "nsIRadioInterfaceLayer");

XPCOMUtils.defineLazyServiceGetter(this, "IccProvider",
                                   "@mozilla.org/ril/content-helper;1",
                                   "nsIIccProvider");

XPCOMUtils.defineLazyServiceGetter(this, "MobileConnectionService",
                                   "@mozilla.org/mobileconnection/mobileconnectionservice;1",
                                   "nsIMobileConnectionService");
#endif


this.MobileIdentityManager = {

  init: function() {
    log.debug("MobileIdentityManager init");
    Services.obs.addObserver(this, "xpcom-shutdown", false);
    ppmm.addMessageListener(GET_ASSERTION_IPC_MSG, this);
    this.messageManagers = {};
    this.keyPairs = {};
    this.certificates = {};
  },

  receiveMessage: function(aMessage) {
    log.debug("Received " + aMessage.name);

    if (aMessage.name !== GET_ASSERTION_IPC_MSG) {
      return;
    }

    let msg = aMessage.json;

    // We save the message target message manager so we can later dispatch
    // back messages without broadcasting to all child processes.
    let promiseId = msg.promiseId;
    this.messageManagers[promiseId] = aMessage.target;

    this.getMobileIdAssertion(aMessage.principal, promiseId, msg.options);
  },

  observe: function(subject, topic, data) {
    if (topic != "xpcom-shutdown") {
      return;
    }

    ppmm.removeMessageListener(GET_ASSERTION_IPC_MSG, this);
    Services.obs.removeObserver(this, "xpcom-shutdown");
    this.messageManagers = null;
  },

  /*********************************************************
   * Getters
   ********************************************************/
#ifdef MOZ_B2G_RIL
  // We have these getters to allow mocking RIL stuff from the tests.
  get ril() {
    if (this._ril) {
      return this._ril;
    }
    return Ril;
  },

  get iccProvider() {
    if (this._iccProvider) {
      return this._iccProvider;
    }
    return IccProvider;
  },

  get mobileConnectionService() {
    if (this._mobileConnectionService) {
      return this._mobileConnectionService;
    }
    return MobileConnectionService;
  },
#endif

  get iccInfo() {
    if (this._iccInfo) {
      return this._iccInfo;
    }
#ifdef MOZ_B2G_RIL
    let self = this;
    let iccListener = {
      notifyStkCommand: function() {},

      notifyStkSessionEnd: function() {},

      notifyCardStateChanged: function() {},

      notifyIccInfoChanged: function() {
        // If we receive a notification about an ICC info change, we clear
        // the ICC related caches so they can be rebuilt with the new changes.

        log.debug("ICC info changed observed. Clearing caches");

        // We don't need to keep listening for changes until we rebuild the
        // cache again.
        for (let i = 0; i < self._iccInfo.length; i++) {
          self.iccProvider.unregisterIccMsg(self._iccInfo[i].clientId,
                                            iccListener);
        }

        self._iccInfo = null;
        self._iccIds = null;
      }
    };

    // _iccInfo is a local cache containing the information about the SIM cards
    // that it is interesting for the Mobile ID flow.
    // The index of this array does not necesarily need to match the real
    // identifier of the SIM card ("clientId" or "serviceId" in RIL language).
    this._iccInfo = [];

    for (let i = 0; i < this.ril.numRadioInterfaces; i++) {
      let rilContext = this.ril.getRadioInterface(i).rilContext;
      if (!rilContext) {
        log.warn("Tried to get the RIL context for an invalid service ID " + i);
        continue;
      }

      let info = rilContext.iccInfo;
      if (!info) {
        log.warn("No ICC info");
        continue;
      }

      let connection = this.mobileConnectionService.getItemByServiceId(i);
      let voice = connection && connection.voice;
      let data = connection && connection.data;
      let operator = null;
      if (voice &&
          voice.network &&
          voice.network.shortName &&
          voice.network.shortName.length) {
        operator = voice.network.shortName;
      } else if (data &&
                 data.network &&
                 data.network.shortName &&
                 data.network.shortName.length) {
        operator = data.network.shortName;
      }

      this._iccInfo.push({
        // Because it is possible that the _iccInfo array index doesn't match
        // the real client ID, we need to store this value for later usage.
        clientId: i,
        iccId: info.iccid,
        mcc: info.mcc,
        mnc: info.mnc,
        // GSM SIMs may have MSISDN while CDMA SIMs may have MDN
        msisdn: info.msisdn || info.mdn || null,
        operator: operator,
        roaming: voice && voice.roaming
      });

      // We need to subscribe to ICC change notifications so we can refresh
      // the cache if any change is observed.
      this.iccProvider.registerIccMsg(i, iccListener);
    }

    return this._iccInfo;
#endif
    return null;
  },

  get iccIds() {
#ifdef MOZ_B2G_RIL
    if (this._iccIds) {
      return this._iccIds;
    }

    this._iccIds = [];
    if (!this.iccInfo) {
      return this._iccIds;
    }

    for (let i = 0; i < this.iccInfo.length; i++) {
      this._iccIds.push(this.iccInfo[i].iccId);
    }

    return this._iccIds;
#endif
    return null;
  },

  get credStore() {
    if (!this._credStore) {
      this._credStore = new MobileIdentityCredentialsStore();
      this._credStore.init();
    }
    return this._credStore;
  },

  get ui() {
    if (!this._ui) {
      this._ui = Cc["@mozilla.org/services/mobileid-ui-glue;1"]
                   .createInstance(Ci.nsIMobileIdentityUIGlue);
      this._ui.oncancel = this.onUICancel.bind(this);
      this._ui.onresendcode = this.onUIResendCode.bind(this);
    }
    return this._ui;
  },

  get client() {
    if (!this._client) {
      this._client = new MobileIdentityClient();
    }
    return this._client;
  },

  get isMultiSim() {
    return this.iccInfo && this.iccInfo.length > 1;
  },

  getVerificationOptionsForIcc: function(aServiceId) {
    log.debug("getVerificationOptionsForIcc " + aServiceId);
    log.debug("iccInfo ${}", this.iccInfo[aServiceId]);
    // First of all we need to check if we already have existing credentials
    // for the given SIM information (ICC ID or MSISDN). If we have no valid
    // credentials, we have to check with the server which options do we have
    // to verify the associated phone number.
    return this.credStore.getByIccId(this.iccInfo[aServiceId].iccId)
    .then(
      (creds) => {
        if (creds) {
          return creds;
        }
        return this.credStore.getByMsisdn(this.iccInfo[aServiceId].msisdn);
      }
    )
    .then(
      (creds) => {
        if (creds) {
          this.iccInfo[aServiceId].credentials = creds;
          return;
        }
        // We have no credentials for this SIM, so we need to ask the server
        // which options do we have to verify the phone number.
        // But we need to be online...
        if (Services.io.offline) {
          return Promise.reject(ERROR_OFFLINE);
        }
        return this.client.discover(this.iccInfo[aServiceId].msisdn,
                                    this.iccInfo[aServiceId].mcc,
                                    this.iccInfo[aServiceId].mnc,
                                    this.iccInfo[aServiceId].roaming);
      }
    )
    .then(
      (result) => {
        // If we already have credentials for this ICC and no discover request
        // is done, we just bail out.
        if (!result || !result.verificationMethods) {
          return;
        }
        log.debug("Discover result ${}", result);
        this.iccInfo[aServiceId].verificationMethods = result.verificationMethods;
        this.iccInfo[aServiceId].verificationDetails = result.verificationDetails;
        this.iccInfo[aServiceId].canDoSilentVerification =
          (result.verificationMethods.indexOf(SMS_MO_MT) != -1);
        return;
      }
    );
  },

  getVerificationOptions: function() {
    log.debug("getVerificationOptions");
    // We try to get if we already have credentials for any of the inserted
    // SIM cards if any is available and we try to get the possible
    // verification mechanisms for these SIM cards.
    // All this information will be stored in iccInfo.
    if (!this.iccInfo || !this.iccInfo.length) {
      let deferred = Promise.defer();
      deferred.resolve(null);
      return deferred.promise;
    }

    let promises = [];
    for (let i = 0; i < this.iccInfo.length; i++) {
      promises.push(this.getVerificationOptionsForIcc(i));
    }
    return Promise.all(promises);
  },

  getKeyPair: function(aSessionToken) {
    if (this.keyPairs[aSessionToken] &&
        this.keyPairs[aSessionToken].validUntil > this.client.hawk.now()) {
      return Promise.resolve(this.keyPairs[aSessionToken].keyPair);
    }

    let validUntil = this.client.hawk.now() + KEY_LIFETIME;
    let deferred = Promise.defer();
    jwcrypto.generateKeyPair("DS160", (error, kp) => {
      if (error) {
        return deferred.reject(error);
      }
      this.keyPairs[aSessionToken] = {
        keyPair: kp,
        validUntil: validUntil
      };
      delete this.certificates[aSessionToken];
      deferred.resolve(kp);
    });

    return deferred.promise;
  },

  getCertificate: function(aSessionToken, aPublicKey) {
    log.debug("getCertificate");
    if (this.certificates[aSessionToken] &&
        this.certificates[aSessionToken].validUntil > this.client.hawk.now()) {
      return Promise.resolve(this.certificates[aSessionToken].cert);
    }

    if (Services.io.offline) {
      return Promise.reject(ERROR_OFFLINE);
    }

    let validUntil = this.client.hawk.now() + KEY_LIFETIME;
    let deferred = Promise.defer();
    this.client.sign(aSessionToken, CERTIFICATE_LIFETIME,
                     aPublicKey)
    .then(
      (signedCert) => {
        log.debug("Got signed certificate");
        this.certificates[aSessionToken] = {
          cert: signedCert.cert,
          validUntil: validUntil
        };
        deferred.resolve(signedCert.cert);
      },
      deferred.reject
    );
    return deferred.promise;
  },

  /*********************************************************
   * Setters (for test only purposes)
   ********************************************************/
  set ui(aUi) {
    this._ui = aUi;
  },

  set credStore(aCredStore) {
    this._credStore = aCredStore;
  },

  set client(aClient) {
    this._client = aClient;
  },

  set iccInfo(aIccInfo) {
    this._iccInfo = aIccInfo;
  },

  /*********************************************************
   * UI callbacks
   ********************************************************/

  onUICancel: function() {
    log.debug("UI cancel");
    if (this.activeVerificationFlow) {
      this.rejectVerification();
    }
  },

  onUIResendCode: function() {
    log.debug("UI resend code");
    if (!this.activeVerificationFlow) {
      return;
    }
    this.doVerification();
  },

  /*********************************************************
   * Result helpers
   *********************************************************/
  success: function(aPromiseId, aResult) {
    let mm = this.messageManagers[aPromiseId];
    mm.sendAsyncMessage("MobileId:GetAssertion:Return:OK", {
      promiseId: aPromiseId,
      result: aResult
    });
  },

  error: function(aPromiseId, aError) {
    let mm = this.messageManagers[aPromiseId];
    mm.sendAsyncMessage("MobileId:GetAssertion:Return:KO", {
      promiseId: aPromiseId,
      error: aError
    });
  },

  /*********************************************************
   * Permissions helper
   ********************************************************/

  addPermission: function(aPrincipal) {
    permissionManager.addFromPrincipal(aPrincipal, MOBILEID_PERM,
                                       Ci.nsIPermissionManager.ALLOW_ACTION);
  },

  /*********************************************************
   * Phone number verification
   ********************************************************/

  rejectVerification: function(aReason) {
    if (!this.activeVerificationDeferred) {
      return;
    }
    this.activeVerificationDeferred.reject(aReason);
    this.activeVerificationDeferred = null;
    this.cleanupVerification(true /* unregister */);
  },

  resolveVerification: function(aResult) {
    if (!this.activeVerificationDeferred) {
      return;
    }
    this.activeVerificationDeferred.resolve(aResult);
    this.activeVerificationDeferred = null;
    this.cleanupVerification();
  },

  cleanupVerification: function(aUnregister = false) {
    if (!this.activeVerificationFlow) {
      return;
    }
    this.activeVerificationFlow.cleanup(aUnregister);
    this.activeVerificationFlow = null;
  },

  doVerification: function() {
    this.activeVerificationFlow.doVerification()
    .then(
      (verificationResult) => {
        log.debug("onVerificationResult ");
        if (!verificationResult || !verificationResult.sessionToken ||
            !verificationResult.msisdn) {
          return this.rejectVerification(
            ERROR_INTERNAL_INVALID_VERIFICATION_RESULT
          );
        }
        this.resolveVerification(verificationResult);
      }
    )
    .then(
      null,
      reason => {
        // Verification timeout.
        log.warn("doVerification " + reason);
      }
    );
  },

  _verificationFlow: function(aToVerify, aOrigin) {
    log.debug("toVerify ${}", aToVerify);

    // We create the corresponding verification flow and save its instance
    // in case that we need to cancel it or retrigger it because the user
    // requested its cancelation or a resend of the verification code.
    if (aToVerify.verificationMethod.indexOf(SMS_MT) != -1 &&
        aToVerify.msisdn &&
        aToVerify.verificationDetails &&
        aToVerify.verificationDetails.mtSender) {
      this.activeVerificationFlow = new MobileIdentitySmsMtVerificationFlow({
          origin: aOrigin,
          msisdn: aToVerify.msisdn,
          mcc: aToVerify.mcc,
          mnc: aToVerify.mnc,
          iccId: aToVerify.iccId,
          external: aToVerify.serviceId === undefined,
          mtSender: aToVerify.verificationDetails.mtSender
        },
        this.ui,
        this.client
      );
#ifdef MOZ_B2G_RIL
    } else if (aToVerify.verificationMethod.indexOf(SMS_MO_MT) != -1 &&
               aToVerify.serviceId &&
               aToVerify.verificationDetails &&
               aToVerify.verificationDetails.moVerifier &&
               aToVerify.verificationDetails.mtSender) {
      this.activeVerificationFlow = new MobileIdentitySmsMoMtVerificationFlow({
          origin: aOrigin,
          serviceId: aToVerify.serviceId,
          iccId: aToVerify.iccId,
          mtSender: aToVerify.verificationDetails.mtSender,
          moVerifier: aToVerify.verificationDetails.moVerifier
        },
        this.ui,
        this.client
      );
#endif
    } else {
      return Promise.reject(ERROR_INTERNAL_CANNOT_VERIFY_SELECTION);
    }

    if (!this.activeVerificationFlow) {
      return Promise.reject(ERROR_INTERNAL_CANNOT_CREATE_VERIFICATION_FLOW);
    }

    this.activeVerificationDeferred = Promise.defer();
    this.doVerification();
    return this.activeVerificationDeferred.promise;
  },

  verificationFlow: function(aUserSelection, aOrigin) {
    log.debug("verificationFlow ${}", aUserSelection);

    if (!aUserSelection) {
      return Promise.reject(ERROR_INTERNAL_INVALID_USER_SELECTION);
    }

    let serviceId = aUserSelection.serviceId || undefined;
    // We check if the user entered phone number corresponds with any of the
    // inserted SIMs known phone numbers.
    if (aUserSelection.msisdn && this.iccInfo) {
      for (let i = 0; i < this.iccInfo.length; i++) {
        if (aUserSelection.msisdn == this.iccInfo[i].msisdn) {
          serviceId = i;
          break;
        }
      }
    }

    let toVerify = {};

    if (serviceId !== undefined) {
      log.debug("iccInfo ${}", this.iccInfo[serviceId]);
      toVerify.serviceId = serviceId;
      toVerify.iccId = this.iccInfo[serviceId].iccId;
      toVerify.msisdn = this.iccInfo[serviceId].msisdn;
      toVerify.mcc = this.iccInfo[serviceId].mcc;
      toVerify.mnc = this.iccInfo[serviceId].mnc;
      toVerify.verificationMethod =
        this.iccInfo[serviceId].verificationMethods[0];
      toVerify.verificationDetails =
        this.iccInfo[serviceId].verificationDetails[toVerify.verificationMethod];
      return this._verificationFlow(toVerify, aOrigin);
    } else {
      toVerify.msisdn = aUserSelection.msisdn;
      toVerify.mcc = aUserSelection.mcc;
      return this.client.discover(aUserSelection.msisdn,
                                  aUserSelection.mcc)
      .then(
        (discoverResult) => {
          if (!discoverResult || !discoverResult.verificationMethods) {
            return Promise.reject(ERROR_INTERNAL_UNEXPECTED);
          }
          log.debug("discoverResult ${}", discoverResult);
          toVerify.verificationMethod = discoverResult.verificationMethods[0];
          toVerify.verificationDetails =
            discoverResult.verificationDetails[toVerify.verificationMethod];
          return this._verificationFlow(toVerify, aOrigin);
        }
      );
    }
  },


  /*********************************************************
   * UI prompt functions.
   ********************************************************/

  // The phone number prompt will be used to confirm that the user wants to
  // verify and share a known phone number and to allow her to introduce an
  // external phone or to select between phone numbers or SIM cards (if the
  // phones are not known) in a multi-SIM scenario.
  // This prompt will be considered as the permission prompt and its choice
  // will be remembered per origin by default.
  prompt: function prompt(aPrincipal, aManifestURL, aPhoneInfo) {
    log.debug("prompt ${principal} ${manifest} ${phoneInfo}", {
      principal: aPrincipal,
      manifest: aManifestURL,
      phoneInfo: aPhoneInfo
    });

    let phoneInfoArray = [];

    if (aPhoneInfo) {
      phoneInfoArray.push(aPhoneInfo);
    }

    if (this.iccInfo) {
      for (let i = 0; i < this.iccInfo.length; i++) {
        // If we don't know the msisdn, there is no previous credentials and
        // a silent verification is not possible or if the msisdn is the one
        // that is already chosen, we don't list this SIM as an option.
        if ((!this.iccInfo[i].msisdn && !this.iccInfo[i].credentials &&
            !this.iccInfo[i].canDoSilentVerification) ||
            ((aPhoneInfo) &&
             (this.iccInfo[i].msisdn == aPhoneInfo.msisdn ||
              this.iccInfo[i].iccId == aPhoneInfo.iccId))) {
          continue;
        }

        let phoneInfo = new MobileIdentityUIGluePhoneInfo(
          this.iccInfo[i].msisdn,
          this.iccInfo[i].operator,
          i,                      // service ID
          this.iccInfo[i].iccId,  // iccId
          false                   // primary
        );
        phoneInfoArray.push(phoneInfo);
      }
    }

    return this.ui.startFlow(aManifestURL, phoneInfoArray)
    .then(
      (result) => {
        log.debug("startFlow result ${} ", result);
        if (!result ||
            (!result.phoneNumber && (result.serviceId === undefined))) {
          return Promise.reject(ERROR_INTERNAL_INVALID_PROMPT_RESULT);
        }

        let msisdn;
        let mcc;

        // If the user selected one of the existing SIM cards we have to check
        // that we either have the MSISDN for that SIM, we have already existing
        // credentials or we can do a silent verification that does not require
        // us to have the MSISDN in advance.
        // result.serviceId can be "0".
        if (result.serviceId !== undefined &&
            result.serviceId !== null) {
          let icc = this.iccInfo[result.serviceId];
          log.debug("icc ${}", icc);
          if (!icc || !icc.msisdn && !icc.canDoSilentVerification &&
              !icc.credentials) {
            return Promise.reject(ERROR_INTERNAL_CANNOT_VERIFY_SELECTION);
          }
          msisdn = icc.msisdn;
          mcc = icc.mcc;
        } else {
          msisdn = result.prefix ? result.prefix + result.phoneNumber
                                 : result.phoneNumber;
          mcc = result.mcc;
        }

        // We need to check that the selected phone number is valid and
        // if it is not notify the UI about the error and allow the user to
        // retry.
        if (msisdn && mcc &&
            !PhoneNumberUtils.parseWithMCC(msisdn, mcc)) {
          this.ui.error(ERROR_INVALID_PHONE_NUMBER);
          return this.prompt(aPrincipal, aManifestURL, aPhoneInfo);
        }

        log.debug("Selected msisdn (if any): " + msisdn + " - " + mcc);

        // The user gave permission for the requester origin, so we store it.
        this.addPermission(aPrincipal);

        return {
          msisdn: msisdn,
          mcc: mcc,
          serviceId: result.serviceId
        };
      }
    );
  },

  promptAndVerify: function(aPrincipal, aManifestURL, aCreds) {
    log.debug("promptAndVerify " + aPrincipal + ", " + aManifestURL +
              ", ${}", aCreds);
    let userSelection;

    if (Services.io.offline) {
      return Promise.reject(ERROR_OFFLINE);
    }

    // Before prompting the user we need to check with the server the
    // phone number verification methods that are possible with the
    // SIMs inserted in the device.
    return this.getVerificationOptions()
    .then(
      () => {
        // If we have an exisiting credentials, we add its associated
        // phone number information to the list of choices to present
        // to the user within the selection prompt.
        let phoneInfo;
        if (aCreds) {
          phoneInfo = new MobileIdentityUIGluePhoneInfo(
            aCreds.msisdn,
            null,           // operator
            undefined,      // service ID
            aCreds.iccId,   // iccId
            true            // primary
          );
        }
        return this.prompt(aPrincipal, aManifestURL, phoneInfo);
      }
    )
    .then(
      (promptResult) => {
        log.debug("promptResult ${}", promptResult);
        // If we had credentials and the user didn't change her
        // selection we return them. Otherwise, we need to verify
        // the new number.
        if (promptResult.msisdn && aCreds &&
            promptResult.msisdn == aCreds.msisdn) {
          return aCreds;
        }

        // We might already have credentials for the user selected icc. In
        // that case, we update the credentials store with the new origin and
        // return the credentials.
        if (promptResult.serviceId) {
          let creds = this.iccInfo[promptResult.serviceId].credentials;
          if (creds) {
            this.credStore.add(creds.iccId, creds.msisdn, aPrincipal.origin,
                               creds.sessionToken, this.iccIds);
            return creds;
          }
        }

        // Or we might already have credentials for the selected phone
        // number and so we do the same: update the credentials store with the
        // new origin and return the credentials.
        return this.credStore.getByMsisdn(promptResult.msisdn)
        .then(
          (creds) => {
            if (creds) {
              this.credStore.add(creds.iccId, creds.msisdn, aPrincipal.origin,
                                 creds.sessionToken, this.iccIds);
              return creds;
            }
            // Otherwise, we need to verify the new number selected by the
            // user.
            return this.verificationFlow(promptResult, aPrincipal.origin);
          }
        );
      }
    );
  },

  /*********************************************************
   * Credentials check
   *********************************************************/

  checkNewCredentials: function(aOldCreds, aNewCreds, aOrigin) {
    // If there were previous credentials and the user changed her
    // choice, we need to remove the origin from the old credentials.
    if (aNewCreds.msisdn != aOldCreds.msisdn) {
      return this.credStore.removeOrigin(aOldCreds.msisdn,
                                         aOrigin)
      .then(
        () => {
          return aNewCreds;
        }
      );
    } else {
      // Otherwise, we update the status of the SIM cards in the device
      // so we know that the user decided not to take the chance to change
      // her selection. We won't bother her again until a new SIM card
      // change is detected.
      return this.credStore.setDeviceIccIds(aOldCreds.msisdn, this.iccIds)
      .then(
        () => {
          return aOldCreds;
        }
      );
    }
  },

  /*********************************************************
   * Assertion generation
   ********************************************************/

  generateAssertion: function(aCredentials, aOrigin) {
    if (!aCredentials.sessionToken) {
      return Promise.reject(ERROR_INTERNAL_INVALID_TOKEN);
    }

    let deferred = Promise.defer();

    this.getKeyPair(aCredentials.sessionToken)
    .then(
      (keyPair) => {
        log.debug("keyPair " + keyPair.serializedPublicKey);
        let options = {
          duration: ASSERTION_LIFETIME,
          now: this.client.hawk.now(),
          localtimeOffsetMsec: this.client.hawk.localtimeOffsetMsec
        };

        this.getCertificate(aCredentials.sessionToken,
                            keyPair.serializedPublicKey)
        .then(
          (signedCert) => {
            log.debug("generateAssertion " + signedCert);
            jwcrypto.generateAssertion(signedCert, keyPair,
                                       aOrigin, options,
                                       (error, assertion) => {
              if (error) {
                log.error("Error generating assertion " + err);
                deferred.reject(error);
                return;
              }
              this.credStore.add(aCredentials.iccId,
                                 aCredentials.msisdn,
                                 aOrigin,
                                 aCredentials.sessionToken,
                                 this.iccIds)
              .then(
                () => {
                  deferred.resolve(assertion);
                }
              );
            });
          }, deferred.reject
        );
      }
    );

    return deferred.promise;
  },

  getMobileIdAssertion: function(aPrincipal, aPromiseId, aOptions) {
    log.debug("getMobileIdAssertion ${}", aPrincipal);

    let uri = Services.io.newURI(aPrincipal.origin, null, null);
    let principal = securityManager.getAppCodebasePrincipal(
      uri, aPrincipal.appId, aPrincipal.isInBrowserElement);
    let manifestURL = appsService.getManifestURLByLocalId(aPrincipal.appId);

    let permission = permissionManager.testPermissionFromPrincipal(
      principal,
      MOBILEID_PERM
    );

    if (permission == Ci.nsIPermissionManager.DENY_ACTION ||
        permission == Ci.nsIPermissionManager.UNKNOWN_ACTION) {
      this.error(aPromiseId, ERROR_PERMISSION_DENIED);
      return;
    }

    let _creds;

    // First of all we look if we already have credentials for this origin.
    // If we don't have credentials it means that it is the first time that
    // the caller requested an assertion.
    this.credStore.getByOrigin(aPrincipal.origin)
    .then(
      (creds) => {
        log.debug("creds ${creds} - ${origin}", { creds: creds,
                                                  origin: aPrincipal.origin });
        if (!creds || !creds.sessionToken) {
          log.debug("No credentials");
          return;
        }

        _creds = creds;

        // Even if we already have credentials for this origin, the consumer
        // of the API might want to force the identity selection dialog.
        if (aOptions.forceSelection || aOptions.refreshCredentials) {
          return this.promptAndVerify(principal, manifestURL, creds)
          .then(
            (newCreds) => {
              return this.checkNewCredentials(creds, newCreds,
                                              principal.origin);
            }
          );
        }

        // SIM change scenario.

        // It is possible that the SIM cards inserted in the device at the
        // moment of the previous verification where we obtained the credentials
        // has changed. In that case, we need to let the user knowabout this
        // situation. Otherwise, we just return the credentials.
        log.debug("Looking for SIM changes. Credentials ICCS ${creds} " +
                  "Device ICCS ${device}", { creds: creds.deviceIccIds,
                                             device: this.iccIds });
        let simChanged = (creds.deviceIccIds == null && this.iccIds != null) ||
                         (creds.deviceIccIds != null && this.iccIds == null);

        if (!simChanged &&
            creds.deviceIccIds != null &&
            this.iccIds != null) {
          simChanged = creds.deviceIccIds.length != this.iccIds.length;
        }

        if (!simChanged &&
            creds.deviceIccIds != null &&
            this.iccIds != null) {
          let intersection = creds.deviceIccIds.filter((n) => {
            return this.iccIds.indexOf(n) != -1;
          });
          simChanged = intersection.length != creds.deviceIccIds.length ||
                       intersection.length != this.iccIds.length;
        }

        if (!simChanged) {
          return creds;
        }

        // At this point we know that the SIM associated with the credentials
        // is not present in the device any more or a new SIM has been detected,
        // so we need to ask the user what to do.
        return this.promptAndVerify(principal, manifestURL, creds)
        .then(
          (newCreds) => {
            return this.checkNewCredentials(creds, newCreds,
                                            principal.origin);
          }
        );
      }
    )
    .then(
      (creds) => {
        // Even if we have credentails it is possible that the user has
        // removed the permission to share its mobile id with this origin, so
        // we check the permission and if it is not granted, we ask the user
        // before generating and sharing the assertion.
        // If we've just prompted the user in the previous step, the permission
        // is already granted and stored so we just progress the credentials.
        // But we have to refresh the cached permission before checking.
        if (creds) {
          permission = permissionManager.testPermissionFromPrincipal(
            principal,
            MOBILEID_PERM
          );
          if (permission == Ci.nsIPermissionManager.ALLOW_ACTION) {
            return creds;
          }
          return this.promptAndVerify(principal, manifestURL, creds);
        }
        return this.promptAndVerify(principal, manifestURL);
      }
    )
    .then(
      (creds) => {
        if (creds) {
          return this.generateAssertion(creds, principal.origin);
        }
        return Promise.reject(ERROR_INTERNAL_CANNOT_GENERATE_ASSERTION);
      }
    )
    .then(
      (assertion) => {
        if (!assertion) {
          return Promise.reject(ERROR_INTERNAL_CANNOT_GENERATE_ASSERTION);
        }

        // Get the verified phone number from the assertion.
        let segments = assertion.split(".");
        if (!segments) {
          return Promise.reject(ERROR_INVALID_ASSERTION);
        }

        // We need to translate the base64 alphabet used in JWT to our base64
        // alphabet before calling atob.
        let decodedPayload = JSON.parse(atob(segments[1].replace(/-/g, '+')
                                                        .replace(/_/g, '/')));

        if (!decodedPayload || !decodedPayload.verifiedMSISDN) {
          return Promise.reject(ERROR_INVALID_ASSERTION);
        }

        this.ui.verified(decodedPayload.verifiedMSISDN);

        this.success(aPromiseId, assertion);
      }
    )
    .then(
      null,
      (error) => {
        log.error("getMobileIdAssertion rejected with ${}", error);

        // If we got an invalid token error means that the credentials that
        // we have are not valid anymore and so we need to refresh them. We
        // do that removing the stored credentials and starting over. We also
        // make sure that we do this only once.
        if (error === ERROR_INVALID_AUTH_TOKEN &&
            !aOptions.refreshCredentials) {
          log.debug("Need to get new credentials");
          aOptions.refreshCredentials = true;
          _creds && this.credStore.delete(_creds.msisdn);
          this.getMobileIdAssertion(aPrincipal, aPromiseId, aOptions);
          return;
        }

        // Notify the error to the UI.
        this.ui.error(error);

        this.error(aPromiseId, error);
      }
    );
  },

};

MobileIdentityManager.init();
