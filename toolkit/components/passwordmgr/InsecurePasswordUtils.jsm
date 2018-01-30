/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* ownerGlobal doesn't exist in content privileged windows. */
/* eslint-disable mozilla/use-ownerGlobal */

this.EXPORTED_SYMBOLS = [ "InsecurePasswordUtils" ];

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;
const STRINGS_URI = "chrome://global/locale/security/security.properties";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gContentSecurityManager",
                                   "@mozilla.org/contentsecuritymanager;1",
                                   "nsIContentSecurityManager");
XPCOMUtils.defineLazyServiceGetter(this, "gScriptSecurityManager",
                                   "@mozilla.org/scriptsecuritymanager;1",
                                   "nsIScriptSecurityManager");
XPCOMUtils.defineLazyModuleGetter(this, "LoginHelper",
                                  "resource://gre/modules/LoginHelper.jsm");

XPCOMUtils.defineLazyGetter(this, "log", () => {
  return LoginHelper.createLogger("InsecurePasswordUtils");
});

/*
 * A module that provides utility functions for form security.
 *
 */
this.InsecurePasswordUtils = {
  _formRootsWarned: new WeakMap(),

  /**
   * Gets the ID of the inner window of this DOM window.
   *
   * @param nsIDOMWindow window
   * @return integer
   *         Inner ID for the given window.
   */
  _getInnerWindowId(window) {
      return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
  },

  _sendWebConsoleMessage(messageTag, domDoc) {
    let windowId = this._getInnerWindowId(domDoc.defaultView);
    let category = "Insecure Password Field";
    // All web console messages are warnings for now.
    let flag = Ci.nsIScriptError.warningFlag;
    let bundle = Services.strings.createBundle(STRINGS_URI);
    let message = bundle.GetStringFromName(messageTag);
    let consoleMsg = Cc["@mozilla.org/scripterror;1"].createInstance(Ci.nsIScriptError);
    consoleMsg.initWithWindowID(message, domDoc.location.href, 0, 0, 0, flag, category, windowId);

    Services.console.logMessage(consoleMsg);
  },

  /**
   * Gets the security state of the passed form.
   *
   * @param {FormLike} aForm A form-like object. @See {FormLikeFactory}
   *
   * @returns {Object} An object with the following boolean values:
   *  isFormSubmitHTTP: if the submit action is an http:// URL
   *  isFormSubmitSecure: if the submit action URL is secure,
   *    either because it is HTTPS or because its origin is considered trustworthy
   */
  _checkFormSecurity(aForm) {
    let isFormSubmitHTTP = false, isFormSubmitSecure = false;
    if (aForm.rootElement instanceof Ci.nsIDOMHTMLFormElement) {
      let uri = Services.io.newURI(aForm.rootElement.action || aForm.rootElement.baseURI);
      let principal = gScriptSecurityManager.createCodebasePrincipal(uri, {});

      if (uri.schemeIs("http")) {
        isFormSubmitHTTP = true;
        if (gContentSecurityManager.isOriginPotentiallyTrustworthy(principal) ||
            // Ignore sites with local IP addresses pointing to local forms.
            (this._isPrincipalForLocalIPAddress(aForm.rootElement.nodePrincipal) &&
             this._isPrincipalForLocalIPAddress(principal))) {
          isFormSubmitSecure = true;
        }
      } else {
        isFormSubmitSecure = true;
      }
    }

    return { isFormSubmitHTTP, isFormSubmitSecure };
  },

  _isPrincipalForLocalIPAddress(aPrincipal) {
    try {
      let uri = aPrincipal.URI;
      if (Services.io.hostnameIsLocalIPAddress(uri)) {
        log.debug("hasInsecureLoginForms: detected local IP address:", uri);
        return true;
      }
    } catch (e) {
      log.debug("hasInsecureLoginForms: unable to check for local IP address:", e);
    }
    return false;
  },

  /**
   * Checks if there are insecure password fields present on the form's document
   * i.e. passwords inside forms with http action, inside iframes with http src,
   * or on insecure web pages.
   *
   * @param {FormLike} aForm A form-like object. @See {LoginFormFactory}
   * @return {boolean} whether the form is secure
   */
  isFormSecure(aForm) {
    let isSafePage = aForm.ownerDocument.defaultView.isSecureContext;

    // Ignore insecure documents with URLs that are local IP addresses.
    // This is done because the vast majority of routers and other devices
    // on the network do not use HTTPS, making this warning show up almost
    // constantly on local connections, which annoys users and hurts our cause.
    if (!isSafePage && this._ignoreLocalIPAddress) {
      let isLocalIP = this._isPrincipalForLocalIPAddress(aForm.rootElement.nodePrincipal);
      let topWindow = aForm.ownerDocument.defaultView.top;
      let topIsLocalIP = this._isPrincipalForLocalIPAddress(topWindow.document.nodePrincipal);

      // Only consider the page safe if the top window has a local IP address
      // and, if this is an iframe, the iframe also has a local IP address.
      if (isLocalIP && topIsLocalIP) {
        isSafePage = true;
      }
    }

    let { isFormSubmitSecure, isFormSubmitHTTP } = this._checkFormSecurity(aForm);

    return isSafePage && (isFormSubmitSecure || !isFormSubmitHTTP);
  },

  /**
   * Report insecure password fields in a form to the web console to warn developers.
   *
   * @param {FormLike} aForm A form-like object. @See {FormLikeFactory}
   */
  reportInsecurePasswords(aForm) {
    if (this._formRootsWarned.has(aForm.rootElement) ||
        this._formRootsWarned.get(aForm.rootElement)) {
      return;
    }

    let domDoc = aForm.ownerDocument;
    let isSafePage = domDoc.defaultView.isSecureContext;

    let { isFormSubmitHTTP, isFormSubmitSecure } = this._checkFormSecurity(aForm);

    if (!isSafePage) {
      if (domDoc.defaultView == domDoc.defaultView.parent) {
        this._sendWebConsoleMessage("InsecurePasswordsPresentOnPage", domDoc);
      } else {
        this._sendWebConsoleMessage("InsecurePasswordsPresentOnIframe", domDoc);
      }
      this._formRootsWarned.set(aForm.rootElement, true);
    } else if (isFormSubmitHTTP && !isFormSubmitSecure) {
      this._sendWebConsoleMessage("InsecureFormActionPasswordsPresent", domDoc);
      this._formRootsWarned.set(aForm.rootElement, true);
    }

    // The safety of a password field determined by the form action and the page protocol
    let passwordSafety;
    if (isSafePage) {
      if (isFormSubmitSecure) {
        passwordSafety = 0;
      } else if (isFormSubmitHTTP) {
        passwordSafety = 1;
      } else {
        passwordSafety = 2;
      }
    } else if (isFormSubmitSecure) {
      passwordSafety = 3;
    } else if (isFormSubmitHTTP) {
      passwordSafety = 4;
    } else {
      passwordSafety = 5;
    }

    Services.telemetry.getHistogramById("PWMGR_LOGIN_PAGE_SAFETY").add(passwordSafety);
  },
};

XPCOMUtils.defineLazyPreferenceGetter(this.InsecurePasswordUtils, "_ignoreLocalIPAddress",
                                      "security.insecure_field_warning.ignore_local_ip_address", true);
