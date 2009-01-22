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
 * The Original Code is Weave.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Mills <thunder@mozilla.com>
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

const EXPORTED_SYMBOLS = ['Auth', 'BasicAuthenticator', 'NoOpAuthenticator'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");

Function.prototype.async = Async.sugar;

Utils.lazy(this, 'Auth', AuthMgr);

// XXX: the authenticator api will probably need to be changed to support
// other methods (digest, oauth, etc)

function NoOpAuthenticator() {}
NoOpAuthenticator.prototype = {
  onRequest: function NoOpAuth_onRequest(headers) {
    return headers;
  }
};

function BasicAuthenticator(identity) {
  this._id = identity;
}
BasicAuthenticator.prototype = {
  onRequest: function BasicAuth_onRequest(headers) {
    headers['Authorization'] = 'Basic ' +
      btoa(this._id.username + ':' + this._id.password);
    return headers;
  }
};

function AuthMgr() {
  this._init();
}
AuthMgr.prototype = {
  defaultAuthenticator: null,

  _init: function AuthMgr__init() {
    this._authenticators = {};
    this.defaultAuthenticator = new NoOpAuthenticator();
  },

  registerAuthenticator: function AuthMgr_register(match, authenticator) {
    this._authenticators[match] = authenticator;
  },

  lookupAuthenticator: function AuthMgr_lookup(uri) {
    for (let match in this._authenticators) {
      if (uri.match(match))
        return this._authenticators[match];
    }
    return this.defaultAuthenticator;
  }
};
