/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [ "InsecurePasswordUtils" ];

const Ci = Components.interfaces;
const Cu = Components.utils;
const Cc = Components.classes;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "devtools",
                                  "resource://gre/modules/devtools/Loader.jsm");

Object.defineProperty(this, "WebConsoleUtils", {
  get: function() {
    return devtools.require("devtools/toolkit/webconsole/utils").Utils;
  },
  configurable: true,
  enumerable: true
});

const STRINGS_URI = "chrome://global/locale/security/security.properties";
let l10n = new WebConsoleUtils.l10n(STRINGS_URI);

this.InsecurePasswordUtils = {

  _sendWebConsoleMessage : function (messageTag, domDoc) {
    /*
     * All web console messages are warnings for now so I decided to set the
     * flag here and save a bit of the flag creation in the callers.
     * It's easy to expose this later if needed
     */

    let  windowId = WebConsoleUtils.getInnerWindowId(domDoc.defaultView);
    let category = "Insecure Password Field";
    let flag = Ci.nsIScriptError.warningFlag;
    let message = l10n.getStr(messageTag);
    let consoleMsg = Cc["@mozilla.org/scripterror;1"]
      .createInstance(Ci.nsIScriptError);

    consoleMsg.initWithWindowID(
      message, "", 0, 0, 0, flag, category, windowId);

    Services.console.logMessage(consoleMsg);
  },

  /*
   * Checks whether the passed uri is secure
   * Check Protocol Flags to determine if scheme is secure:
   * URI_DOES_NOT_RETURN_DATA - e.g.
   *   "mailto"
   * URI_IS_LOCAL_RESOURCE - e.g.
   *   "data",
   *   "resource",
   *   "moz-icon"
   * URI_INHERITS_SECURITY_CONTEXT - e.g.
   *   "javascript"
   * URI_SAFE_TO_LOAD_IN_SECURE_CONTEXT - e.g.
   *   "https",
   *   "moz-safe-about"
   *
   *   The use of this logic comes directly from nsMixedContentBlocker.cpp
   *   At the time it was decided to include these protocols since a secure
   *   uri for mixed content blocker means that the resource can't be
   *   easily tampered with because 1) it is sent over an encrypted channel or
   *   2) it is a local resource that never hits the network
   *   or 3) it is a request sent without any response that could alter
   *   the behavior of the page. It was decided to include the same logic
   *   here both to be consistent with MCB and to make sure we cover all
   *   "safe" protocols. Eventually, the code here and the code in MCB
   *   will be moved to a common location that will be referenced from
   *   both places. Look at
   *   https://bugzilla.mozilla.org/show_bug.cgi?id=899099 for more info.
   */
  _checkIfURIisSecure : function(uri) {
    let isSafe = false;
    let netutil = Cc["@mozilla.org/network/util;1"].getService(Ci.nsINetUtil);
    let ph = Ci.nsIProtocolHandler;

    if (netutil.URIChainHasFlags(uri, ph.URI_IS_LOCAL_RESOURCE) ||
        netutil.URIChainHasFlags(uri, ph.URI_DOES_NOT_RETURN_DATA) ||
        netutil.URIChainHasFlags(uri, ph.URI_INHERITS_SECURITY_CONTEXT) ||
        netutil.URIChainHasFlags(uri, ph.URI_SAFE_TO_LOAD_IN_SECURE_CONTEXT)) {

      isSafe = true;
    }

    return isSafe;
  },

  /*
   * Checks whether the passed nested document is insecure
   * or is inside an insecure parent document.
   *
   * We check the chain of frame ancestors all the way until the top document
   * because MITM attackers could replace https:// iframes if they are nested inside
   * http:// documents with their own content, thus creating a security risk
   * and potentially stealing user data. Under such scenario, a user might not
   * get a Mixed Content Blocker message, if the main document is served over HTTP
   * and framing an HTTPS page as it would under the reverse scenario (http
   * inside https).
   */
  _checkForInsecureNestedDocuments : function(domDoc) {
    let uri = domDoc.documentURIObject;
    if (domDoc.defaultView == domDoc.defaultView.parent) {
      // We are at the top, nothing to check here
      return false;
    }
    if (!this._checkIfURIisSecure(uri)) {
      // We are insecure
      return true;
    }
    // I am secure, but check my parent
    return this._checkForInsecureNestedDocuments(domDoc.defaultView.parent.document);
  },


  /*
   * Checks if there are insecure password fields present on the form's document
   * i.e. passwords inside forms with http action, inside iframes with http src,
   * or on insecure web pages. If insecure password fields are present,
   * a log message is sent to the web console to warn developers.
   */
  checkForInsecurePasswords : function (aForm) {
    var domDoc = aForm.ownerDocument;
    let pageURI = domDoc.defaultView.top.document.documentURIObject;
    let isSafePage = this._checkIfURIisSecure(pageURI);

    if (!isSafePage) {
      this._sendWebConsoleMessage("InsecurePasswordsPresentOnPage", domDoc);
    }

    // Check if we are on an iframe with insecure src, or inside another
    // insecure iframe or document.
    if (this._checkForInsecureNestedDocuments(domDoc)) {
      this._sendWebConsoleMessage("InsecurePasswordsPresentOnIframe", domDoc);
      isSafePage = false;
    }

    let isFormSubmitHTTP = false, isFormSubmitHTTPS = false;
    if (aForm.action.match(/^http:\/\//)) {
      this._sendWebConsoleMessage("InsecureFormActionPasswordsPresent", domDoc);
      isFormSubmitHTTP = true;
    } else if (aForm.action.match(/^https:\/\//)) {
      isFormSubmitHTTPS = true;
    }

    // The safety of a password field determined by the form action and the page protocol
    let passwordSafety;
    if (isSafePage) {
      if (isFormSubmitHTTPS) {
        passwordSafety = 0;
      } else if (isFormSubmitHTTP) {
        passwordSafety = 1;
      } else {
        passwordSafety = 2;
      }
    } else {
      if (isFormSubmitHTTPS) {
        passwordSafety = 3;
      } else if (isFormSubmitHTTP) {
        passwordSafety = 4;
      } else {
        passwordSafety = 5;
      }
    }

    Services.telemetry.getHistogramById("PWMGR_LOGIN_PAGE_SAFETY").add(passwordSafety);
  },
};
