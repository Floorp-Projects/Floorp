/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function nsLoginInfo() {}

nsLoginInfo.prototype = {

  classID : Components.ID("{0f2f347c-1e4f-40cc-8efd-792dea70a85e}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsILoginInfo, Ci.nsILoginMetaInfo]),

  //
  // nsILoginInfo interfaces...
  //

  hostname      : null,
  formSubmitURL : null,
  httpRealm     : null,
  username      : null,
  password      : null,
  usernameField : null,
  passwordField : null,

  init : function (aHostname, aFormSubmitURL, aHttpRealm,
                   aUsername,      aPassword,
                   aUsernameField, aPasswordField) {
    this.hostname      = aHostname;
    this.formSubmitURL = aFormSubmitURL;
    this.httpRealm     = aHttpRealm;
    this.username      = aUsername;
    this.password      = aPassword;
    this.usernameField = aUsernameField;
    this.passwordField = aPasswordField;
  },

  matches : function (aLogin, ignorePassword) {
    if (this.hostname      != aLogin.hostname      ||
        this.httpRealm     != aLogin.httpRealm     ||
        this.username      != aLogin.username)
      return false;

    if (!ignorePassword && this.password != aLogin.password)
      return false;

    // If either formSubmitURL is blank (but not null), then match.
    if (this.formSubmitURL != "" && aLogin.formSubmitURL != "" &&
        this.formSubmitURL != aLogin.formSubmitURL)
      return false;

    // The .usernameField and .passwordField values are ignored.

    return true;
  },

  equals : function (aLogin) {
    if (this.hostname      != aLogin.hostname      ||
        this.formSubmitURL != aLogin.formSubmitURL ||
        this.httpRealm     != aLogin.httpRealm     ||
        this.username      != aLogin.username      ||
        this.password      != aLogin.password      ||
        this.usernameField != aLogin.usernameField ||
        this.passwordField != aLogin.passwordField)
      return false;

    return true;
  },

  clone : function() {
    let clone = Cc["@mozilla.org/login-manager/loginInfo;1"].
                createInstance(Ci.nsILoginInfo);
    clone.init(this.hostname, this.formSubmitURL, this.httpRealm,
               this.username, this.password,
               this.usernameField, this.passwordField);

    // Copy nsILoginMetaInfo props
    clone.QueryInterface(Ci.nsILoginMetaInfo);
    clone.guid = this.guid;
    clone.timeCreated = this.timeCreated;
    clone.timeLastUsed = this.timeLastUsed;
    clone.timePasswordChanged = this.timePasswordChanged;
    clone.timesUsed = this.timesUsed;

    return clone;
  },

  //
  // nsILoginMetaInfo interfaces...
  //

  guid : null,
  timeCreated : null,
  timeLastUsed : null,
  timePasswordChanged : null,
  timesUsed : null

}; // end of nsLoginInfo implementation

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([nsLoginInfo]);
