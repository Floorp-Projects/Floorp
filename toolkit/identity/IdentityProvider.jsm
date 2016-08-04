/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/identity/LogUtils.jsm");
Cu.import("resource://gre/modules/identity/Sandbox.jsm");

this.EXPORTED_SYMBOLS = ["IdentityProvider"];
const FALLBACK_PROVIDER = "browserid.org";

XPCOMUtils.defineLazyModuleGetter(this,
                                  "jwcrypto",
                                  "resource://gre/modules/identity/jwcrypto.jsm");

function log(...aMessageArgs) {
  Logger.log.apply(Logger, ["IDP"].concat(aMessageArgs));
}
function reportError(...aMessageArgs) {
  Logger.reportError.apply(Logger, ["IDP"].concat(aMessageArgs));
}


function IdentityProviderService() {
  XPCOMUtils.defineLazyModuleGetter(this,
                                    "_store",
                                    "resource://gre/modules/identity/IdentityStore.jsm",
                                    "IdentityStore");

  this.reset();
}

IdentityProviderService.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIObserver]),
  _sandboxConfigured: false,

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "quit-application-granted":
        Services.obs.removeObserver(this, "quit-application-granted");
        this.shutdown();
        break;
    }
  },

  reset: function IDP_reset() {
    // Clear the provisioning flows.  Provision flows contain an
    // identity, idpParams (how to reach the IdP to provision and
    // authenticate), a callback (a completion callback for when things
    // are done), and a provisioningFrame (which is the provisioning
    // sandbox).  Additionally, two callbacks will be attached:
    // beginProvisioningCallback and genKeyPairCallback.
    this._provisionFlows = {};

    // Clear the authentication flows.  Authentication flows attach
    // to provision flows.  In the process of provisioning an id, it
    // may be necessary to authenticate with an IdP.  The authentication
    // flow maintains the state of that authentication process.
    this._authenticationFlows = {};
  },

  getProvisionFlow: function getProvisionFlow(aProvId, aErrBack) {
    let provFlow = this._provisionFlows[aProvId];
    if (provFlow) {
      return provFlow;
    }

    let err = "No provisioning flow found with id " + aProvId;
    log("ERROR:", err);
    if (typeof aErrBack === 'function') {
      aErrBack(err);
    }

    return undefined;
  },

  shutdown: function RP_shutdown() {
    this.reset();

    if (this._sandboxConfigured) {
      // Tear down message manager listening on the hidden window
      Cu.import("resource://gre/modules/DOMIdentity.jsm");
      DOMIdentity._configureMessages(Services.appShell.hiddenDOMWindow, false);
      this._sandboxConfigured = false;
    }

    Services.obs.removeObserver(this, "quit-application-granted");
  },

  get securityLevel() {
    return 1;
  },

  get certDuration() {
    switch (this.securityLevel) {
      default:
        return 3600;
    }
  },

  /**
   * Provision an Identity
   *
   * @param aIdentity
   *        (string) the email we're logging in with
   *
   * @param aIDPParams
   *        (object) parameters of the IdP
   *
   * @param aCallback
   *        (function) callback to invoke on completion
   *                   with first-positional parameter the error.
   */
  _provisionIdentity: function _provisionIdentity(aIdentity, aIDPParams, aProvId, aCallback) {
    let provPath = aIDPParams.idpParams.provisioning;
    let url = Services.io.newURI("https://" + aIDPParams.domain, null, null).resolve(provPath);
    log("_provisionIdentity: identity:", aIdentity, "url:", url);

    // If aProvId is not null, then we already have a flow
    // with a sandbox.  Otherwise, get a sandbox and create a
    // new provision flow.

    if (aProvId) {
      // Re-use an existing sandbox
      log("_provisionIdentity: re-using sandbox in provisioning flow with id:", aProvId);
      this._provisionFlows[aProvId].provisioningSandbox.reload();

    } else {
      this._createProvisioningSandbox(url, function createdSandbox(aSandbox) {
        // create a provisioning flow, using the sandbox id, and
        // stash callback associated with this provisioning workflow.

        let provId = aSandbox.id;
        this._provisionFlows[provId] = {
          identity: aIdentity,
          idpParams: aIDPParams,
          securityLevel: this.securityLevel,
          provisioningSandbox: aSandbox,
          callback: function doCallback(aErr) {
            aCallback(aErr, provId);
          },
        };

        log("_provisionIdentity: Created sandbox and provisioning flow with id:", provId);
        // XXX bug 769862 - provisioning flow should timeout after N seconds

      }.bind(this));
    }
  },

  // DOM Methods
  /**
   * the provisioning iframe sandbox has called navigator.id.beginProvisioning()
   *
   * @param aCaller
   *        (object)  the iframe sandbox caller with all callbacks and
   *                  other information.  Callbacks include:
   *                  - doBeginProvisioningCallback(id, duration_s)
   *                  - doGenKeyPairCallback(pk)
   */
  beginProvisioning: function beginProvisioning(aCaller) {
    log("beginProvisioning:", aCaller.id);

    // Expect a flow for this caller already to be underway.
    let provFlow = this.getProvisionFlow(aCaller.id, aCaller.doError);

    // keep the caller object around
    provFlow.caller = aCaller;

    let identity = provFlow.identity;
    let frame = provFlow.provisioningFrame;

    // Determine recommended length of cert.
    let duration = this.certDuration;

    // Make a record that we have begun provisioning.  This is required
    // for genKeyPair.
    provFlow.didBeginProvisioning = true;

    // Let the sandbox know to invoke the callback to beginProvisioning with
    // the identity and cert length.
    return aCaller.doBeginProvisioningCallback(identity, duration);
  },

  /**
   * the provisioning iframe sandbox has called
   * navigator.id.raiseProvisioningFailure()
   *
   * @param aProvId
   *        (int)  the identifier of the provisioning flow tied to that sandbox
   * @param aReason
   */
  raiseProvisioningFailure: function raiseProvisioningFailure(aProvId, aReason) {
    reportError("Provisioning failure", aReason);

    // look up the provisioning caller and its callback
    let provFlow = this.getProvisionFlow(aProvId);

    // Sandbox is deleted in _cleanUpProvisionFlow in case we re-use it.

    // This may be either a "soft" or "hard" fail.  If it's a
    // soft fail, we'll flow through setAuthenticationFlow, where
    // the provision flow data will be copied into a new auth
    // flow.  If it's a hard fail, then the callback will be
    // responsible for cleaning up the now defunct provision flow.

    // invoke the callback with an error.
    provFlow.callback(aReason);
  },

  /**
   * When navigator.id.genKeyPair is called from provisioning iframe sandbox.
   * Generates a keypair for the current user being provisioned.
   *
   * @param aProvId
   *        (int)  the identifier of the provisioning caller tied to that sandbox
   *
   * It is an error to call genKeypair without receiving the callback for
   * the beginProvisioning() call first.
   */
  genKeyPair: function genKeyPair(aProvId) {
    // Look up the provisioning caller and make sure it's valid.
    let provFlow = this.getProvisionFlow(aProvId);

    if (!provFlow.didBeginProvisioning) {
      let errStr = "ERROR: genKeyPair called before beginProvisioning";
      log(errStr);
      provFlow.callback(errStr);
      return;
    }

    // Ok generate a keypair
    jwcrypto.generateKeyPair(jwcrypto.ALGORITHMS.DS160, function gkpCb(err, kp) {
      log("in gkp callback");
      if (err) {
        log("ERROR: genKeyPair:", err);
        provFlow.callback(err);
        return;
      }

      provFlow.kp = kp;

      // Serialize the publicKey of the keypair and send it back to the
      // sandbox.
      log("genKeyPair: generated keypair for provisioning flow with id:", aProvId);
      provFlow.caller.doGenKeyPairCallback(provFlow.kp.serializedPublicKey);
    }.bind(this));
  },

  /**
   * When navigator.id.registerCertificate is called from provisioning iframe
   * sandbox.
   *
   * Sets the certificate for the user for which a certificate was requested
   * via a preceding call to beginProvisioning (and genKeypair).
   *
   * @param aProvId
   *        (integer) the identifier of the provisioning caller tied to that
   *                  sandbox
   *
   * @param aCert
   *        (String)  A JWT representing the signed certificate for the user
   *                  being provisioned, provided by the IdP.
   */
  registerCertificate: function registerCertificate(aProvId, aCert) {
    log("registerCertificate:", aProvId, aCert);

    // look up provisioning caller, make sure it's valid.
    let provFlow = this.getProvisionFlow(aProvId);

    if (!provFlow.caller) {
      reportError("registerCertificate", "No provision flow or caller");
      return;
    }
    if (!provFlow.kp)  {
      let errStr = "Cannot register a certificate without a keypair";
      reportError("registerCertificate", errStr);
      provFlow.callback(errStr);
      return;
    }

    // store the keypair and certificate just provided in IDStore.
    this._store.addIdentity(provFlow.identity, provFlow.kp, aCert);

    // Great success!
    provFlow.callback(null);

    // Clean up the flow.
    this._cleanUpProvisionFlow(aProvId);
  },

  /**
   * Begin the authentication process with an IdP
   *
   * @param aProvId
   *        (int) the identifier of the provisioning flow which failed
   *
   * @param aCallback
   *        (function) to invoke upon completion, with
   *                   first-positional-param error.
   */
  _doAuthentication: function _doAuthentication(aProvId, aIDPParams) {
    log("_doAuthentication: provId:", aProvId, "idpParams:", aIDPParams);
    // create an authentication caller and its identifier AuthId
    // stash aIdentity, idpparams, and callback in it.

    // extract authentication URL from idpParams
    let authPath = aIDPParams.idpParams.authentication;
    let authURI = Services.io.newURI("https://" + aIDPParams.domain, null, null).resolve(authPath);

    // beginAuthenticationFlow causes the "identity-auth" topic to be
    // observed.  Since it's sending a notification to the DOM, there's
    // no callback.  We wait for the DOM to trigger the next phase of
    // provisioning.
    this._beginAuthenticationFlow(aProvId, authURI);

    // either we bind the AuthID to the sandbox ourselves, or UX does that,
    // in which case we need to tell UX the AuthId.
    // Currently, the UX creates the UI and gets the AuthId from the window
    // and sets is with setAuthenticationFlow
  },

  /**
   * The authentication frame has called navigator.id.beginAuthentication
   *
   * IMPORTANT: the aCaller is *always* non-null, even if this is called from
   * a regular content page. We have to make sure, on every DOM call, that
   * aCaller is an expected authentication-flow identifier. If not, we throw
   * an error or something.
   *
   * @param aCaller
   *        (object)  the authentication caller
   *
   */
  beginAuthentication: function beginAuthentication(aCaller) {
    log("beginAuthentication: caller id:", aCaller.id);

    // Begin the authentication flow after having concluded a provisioning
    // flow.  The aCaller that the DOM gives us will have the same ID as
    // the provisioning flow we just concluded.  (see setAuthenticationFlow)
    let authFlow = this._authenticationFlows[aCaller.id];
    if (!authFlow) {
      return aCaller.doError("beginAuthentication: no flow for caller id", aCaller.id);
    }

    authFlow.caller = aCaller;

    let identity = this._provisionFlows[authFlow.provId].identity;

    // tell the UI to start the authentication process
    log("beginAuthentication: authFlow:", aCaller.id, "identity:", identity);
    return authFlow.caller.doBeginAuthenticationCallback(identity);
  },

  /**
   * The auth frame has called navigator.id.completeAuthentication
   *
   * @param aAuthId
   *        (int)  the identifier of the authentication caller tied to that sandbox
   *
   */
  completeAuthentication: function completeAuthentication(aAuthId) {
    log("completeAuthentication:", aAuthId);

    // look up the AuthId caller, and get its callback.
    let authFlow = this._authenticationFlows[aAuthId];
    if (!authFlow) {
      reportError("completeAuthentication", "No auth flow with id", aAuthId);
      return;
    }
    let provId = authFlow.provId;

    // delete caller
    delete authFlow['caller'];
    delete this._authenticationFlows[aAuthId];

    let provFlow = this.getProvisionFlow(provId);
    provFlow.didAuthentication = true;
    let subject = {
      rpId: provFlow.rpId,
      identity: provFlow.identity,
    };
    Services.obs.notifyObservers({ wrappedJSObject: subject }, "identity-auth-complete", aAuthId);
  },

  /**
   * The auth frame has called navigator.id.cancelAuthentication
   *
   * @param aAuthId
   *        (int)  the identifier of the authentication caller
   *
   */
  cancelAuthentication: function cancelAuthentication(aAuthId) {
    log("cancelAuthentication:", aAuthId);

    // look up the AuthId caller, and get its callback.
    let authFlow = this._authenticationFlows[aAuthId];
    if (!authFlow) {
      reportError("cancelAuthentication", "No auth flow with id:", aAuthId);
      return;
    }
    let provId = authFlow.provId;

    // delete caller
    delete authFlow['caller'];
    delete this._authenticationFlows[aAuthId];

    let provFlow = this.getProvisionFlow(provId);
    provFlow.didAuthentication = true;
    Services.obs.notifyObservers(null, "identity-auth-complete", aAuthId);

    // invoke callback with ERROR.
    let errStr = "Authentication canceled by IDP";
    log("ERROR: cancelAuthentication:", errStr);
    provFlow.callback(errStr);
  },

  /**
   * Called by the UI to set the ID and caller for the authentication flow after it gets its ID
   */
  setAuthenticationFlow: function(aAuthId, aProvId) {
    // this is the transition point between the two flows,
    // provision and authenticate.  We tell the auth flow which
    // provisioning flow it is started from.
    log("setAuthenticationFlow: authId:", aAuthId, "provId:", aProvId);
    this._authenticationFlows[aAuthId] = { provId: aProvId };
    this._provisionFlows[aProvId].authId = aAuthId;
  },

  /**
   * Load the provisioning URL in a hidden frame to start the provisioning
   * process.
   */
  _createProvisioningSandbox: function _createProvisioningSandbox(aURL, aCallback) {
    log("_createProvisioningSandbox:", aURL);

    if (!this._sandboxConfigured) {
      // Configure message manager listening on the hidden window
      Cu.import("resource://gre/modules/DOMIdentity.jsm");
      DOMIdentity._configureMessages(Services.appShell.hiddenDOMWindow, true);
      this._sandboxConfigured = true;
    }

    new Sandbox(aURL, aCallback);
  },

  /**
   * Load the authentication UI to start the authentication process.
   */
  _beginAuthenticationFlow: function _beginAuthenticationFlow(aProvId, aURL) {
    log("_beginAuthenticationFlow:", aProvId, aURL);
    let propBag = {provId: aProvId};

    Services.obs.notifyObservers({wrappedJSObject:propBag}, "identity-auth", aURL);
  },

  /**
   * Clean up a provision flow and the authentication flow and sandbox
   * that may be attached to it.
   */
  _cleanUpProvisionFlow: function _cleanUpProvisionFlow(aProvId) {
    log('_cleanUpProvisionFlow:', aProvId);
    let prov = this._provisionFlows[aProvId];

    // Clean up the sandbox, if there is one.
    if (prov.provisioningSandbox) {
      let sandbox = this._provisionFlows[aProvId]['provisioningSandbox'];
      if (sandbox.free) {
        log('_cleanUpProvisionFlow: freeing sandbox');
        sandbox.free();
      }
      delete this._provisionFlows[aProvId]['provisioningSandbox'];
    }

    // Clean up a related authentication flow, if there is one.
    if (this._authenticationFlows[prov.authId]) {
      delete this._authenticationFlows[prov.authId];
    }

    // Finally delete the provision flow
    delete this._provisionFlows[aProvId];
  }

};

this.IdentityProvider = new IdentityProviderService();
