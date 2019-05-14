/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* This is a JavaScript module (JSM) to be imported via
  * Components.utils.import() and acts as a singleton. Only the following
  * listed symbols will exposed on import, and only when and where imported.
  */

var EXPORTED_SYMBOLS = ["Password", "DumpPasswords"];

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {Logger} = ChromeUtils.import("resource://tps/logger.jsm");

var nsLoginInfo = new Components.Constructor(
                      "@mozilla.org/login-manager/loginInfo;1",
                      Ci.nsILoginInfo,
                      "init");

var DumpPasswords = function TPS__Passwords__DumpPasswords() {
  let logins = Services.logins.getAllLogins();
  Logger.logInfo("\ndumping password list\n", true);
  for (var i = 0; i < logins.length; i++) {
    Logger.logInfo("* host=" + logins[i].hostname + ", submitURL=" + logins[i].formSubmitURL +
                   ", realm=" + logins[i].httpRealm + ", password=" + logins[i].password +
                   ", passwordField=" + logins[i].passwordField + ", username=" +
                   logins[i].username + ", usernameField=" + logins[i].usernameField, true);
  }
  Logger.logInfo("\n\nend password list\n", true);
};

/**
 * PasswordProps object; holds password properties.
 */
function PasswordProps(props) {
  this.hostname = null;
  this.submitURL = null;
  this.realm = null;
  this.username = "";
  this.password = "";
  this.usernameField = "";
  this.passwordField = "";
  this.delete = false;

  for (var prop in props) {
    if (prop in this) {
      this[prop] = props[prop];
    }
  }
}

/**
 * Password class constructor. Initializes instance properties.
 */
function Password(props) {
  this.props = new PasswordProps(props);
  if ("changes" in props) {
    this.updateProps = new PasswordProps(props);
    for (var prop in props.changes) {
      if (prop in this.updateProps) {
        this.updateProps[prop] = props.changes[prop];
      }
    }
  } else {
    this.updateProps = null;
  }
}

/**
 * Password instance methods.
 */
Password.prototype = {
  /**
   * Create
   *
   * Adds a password entry to the login manager for the password
   * represented by this object's properties. Throws on error.
   *
   * @return the new login guid
   */
  Create() {
    let login = new nsLoginInfo(this.props.hostname, this.props.submitURL,
                                this.props.realm, this.props.username,
                                this.props.password,
                                this.props.usernameField,
                                this.props.passwordField);
    Services.logins.addLogin(login);
    login.QueryInterface(Ci.nsILoginMetaInfo);
    return login.guid;
  },

  /**
   * Find
   *
   * Finds a password entry in the login manager, for the password
   * represented by this object's properties.
   *
   * @return the guid of the password if found, otherwise -1
   */
  Find() {
    let logins = Services.logins.findLogins(this.props.hostname,
                                            this.props.submitURL,
                                            this.props.realm);
    for (var i = 0; i < logins.length; i++) {
      if (logins[i].username == this.props.username &&
          logins[i].password == this.props.password &&
          logins[i].usernameField == this.props.usernameField &&
          logins[i].passwordField == this.props.passwordField) {
        logins[i].QueryInterface(Ci.nsILoginMetaInfo);
        return logins[i].guid;
      }
    }
    return -1;
  },

  /**
   * Update
   *
   * Updates an existing password entry in the login manager with
   * new properties. Throws on error.  The 'old' properties are this
   * object's properties, the 'new' properties are the properties in
   * this object's 'updateProps' object.
   *
   * @return nothing
   */
  Update() {
    let oldlogin = new nsLoginInfo(this.props.hostname,
                                   this.props.submitURL,
                                   this.props.realm,
                                   this.props.username,
                                   this.props.password,
                                   this.props.usernameField,
                                   this.props.passwordField);
    let newlogin = new nsLoginInfo(this.updateProps.hostname,
                                   this.updateProps.submitURL,
                                   this.updateProps.realm,
                                   this.updateProps.username,
                                   this.updateProps.password,
                                   this.updateProps.usernameField,
                                   this.updateProps.passwordField);
    Services.logins.modifyLogin(oldlogin, newlogin);
  },

  /**
   * Remove
   *
   * Removes an entry from the login manager for a password which
   * matches this object's properties. Throws on error.
   *
   * @return nothing
   */
  Remove() {
    let login = new nsLoginInfo(this.props.hostname,
                                this.props.submitURL,
                                this.props.realm,
                                this.props.username,
                                this.props.password,
                                this.props.usernameField,
                                this.props.passwordField);
    Services.logins.removeLogin(login);
  },
};
