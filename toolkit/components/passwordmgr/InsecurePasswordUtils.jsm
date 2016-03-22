/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [ "InsecurePasswordUtils" ];

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;
const STRINGS_URI = "chrome://global/locale/security/security.properties";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "devtools",
                                  "resource://devtools/shared/Loader.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LoginManagerContent",
                                  "resource://gre/modules/LoginManagerContent.jsm");
XPCOMUtils.defineLazyGetter(this, "WebConsoleUtils", () => {
  return this.devtools.require("devtools/shared/webconsole/utils").Utils;
});
XPCOMUtils.defineLazyGetter(this, "l10n", () => {
  return new this.WebConsoleUtils.L10n(STRINGS_URI);
});

this.InsecurePasswordUtils = {
  _sendWebConsoleMessage(messageTag, domDoc) {
    let windowId = WebConsoleUtils.getInnerWindowId(domDoc.defaultView);
    let category = "Insecure Password Field";
    // All web console messages are warnings for now.
    let flag = Ci.nsIScriptError.warningFlag;
    let message = l10n.getStr(messageTag);
    let consoleMsg = Cc["@mozilla.org/scripterror;1"].createInstance(Ci.nsIScriptError);
    consoleMsg.initWithWindowID(message, domDoc.location.href, 0, 0, 0, flag, category, windowId);

    Services.console.logMessage(consoleMsg);
  },

  /**
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
  _checkForInsecureNestedDocuments(domDoc) {
    if (domDoc.defaultView == domDoc.defaultView.parent) {
      // We are at the top, nothing to check here
      return false;
    }
    if (!LoginManagerContent.isDocumentSecure(domDoc)) {
      // We are insecure
      return true;
    }
    // I am secure, but check my parent
    return this._checkForInsecureNestedDocuments(domDoc.defaultView.parent.document);
  },


  /**
   * Checks if there are insecure password fields present on the form's document
   * i.e. passwords inside forms with http action, inside iframes with http src,
   * or on insecure web pages. If insecure password fields are present,
   * a log message is sent to the web console to warn developers.
   *
   * @param {FormLike} aForm A form-like object. @See {FormLikeFactory}
   */
  checkForInsecurePasswords(aForm) {
    let domDoc = aForm.ownerDocument;
    let topDocument = domDoc.defaultView.top.document;
    let isSafePage = LoginManagerContent.isDocumentSecure(topDocument);

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
    // Note that aForm.action can be a relative path (e.g. "", "/login", "//example.com", etc.)
    // but we don't warn about those since we would have already warned about the form's document
    // not being safe above.
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
