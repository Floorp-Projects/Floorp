/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Crossweave.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Griffin <jgriffin@mozilla.com>
 *   Philipp von Weitershausen <philipp@weitershausen.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

 /* This is a JavaScript module (JSM) to be imported via 
  * Components.utils.import() and acts as a singleton. Only the following 
  * listed symbols will exposed on import, and only when and where imported. 
  */

var EXPORTED_SYMBOLS = ["Password", "DumpPasswords"];

const CC = Components.classes;
const CI = Components.interfaces;
const CU = Components.utils;

CU.import("resource://gre/modules/Services.jsm");
CU.import("resource://tps/logger.jsm");

let nsLoginInfo = new Components.Constructor(
                      "@mozilla.org/login-manager/loginInfo;1",  
                      CI.nsILoginInfo,  
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
    if (prop in this)
      this[prop] = props[prop];
  }
}

/**
 * Password class constructor. Initializes instance properties.
 */
function Password(props) {
  this.props = new PasswordProps(props);
  if ("changes" in props) {
    this.updateProps = new PasswordProps(props);
    for (var prop in props.changes)
      if (prop in this.updateProps)
        this.updateProps[prop] = props.changes[prop];
  }
  else {
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
  Create: function() {
    let login = new nsLoginInfo(this.props.hostname, this.props.submitURL,
                                this.props.realm, this.props.username,
                                this.props.password, 
                                this.props.usernameField,
                                this.props.passwordField);
    Services.logins.addLogin(login);
    login.QueryInterface(CI.nsILoginMetaInfo);
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
  Find: function() {
    let logins = Services.logins.findLogins({},
                                            this.props.hostname,
                                            this.props.submitURL,
                                            this.props.realm);
    for (var i = 0; i < logins.length; i++) {
      if (logins[i].username == this.props.username &&
          logins[i].password == this.props.password &&
          logins[i].usernameField == this.props.usernameField &&
          logins[i].passwordField == this.props.passwordField) {
        logins[i].QueryInterface(CI.nsILoginMetaInfo);
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
  Update: function() {
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
  Remove: function() {
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
