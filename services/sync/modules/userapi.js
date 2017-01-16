/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "UserAPI10Client",
];

var {utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/rest.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/util.js");

/**
 * A generic client for the user API 1.0 service.
 *
 * http://docs.services.mozilla.com/reg/apis.html
 *
 * Instances are constructed with the base URI of the service.
 */
this.UserAPI10Client = function UserAPI10Client(baseURI) {
  this._log = Log.repository.getLogger("Sync.UserAPI");
  this._log.level = Log.Level[Svc.Prefs.get("log.logger.userapi")];

  this.baseURI = baseURI;
}
UserAPI10Client.prototype = {
  USER_CREATE_ERROR_CODES: {
    2: "Incorrect or missing captcha.",
    4: "User exists.",
    6: "JSON parse failure.",
    7: "Missing password field.",
    9: "Requested password not strong enough.",
    12: "No email address on file.",
  },

  /**
   * Determine whether a specified username exists.
   *
   * Callback receives the following arguments:
   *
   *   (Error) Describes error that occurred or null if request was
   *           successful.
   *   (boolean) True if user exists. False if not. null if there was an error.
   */
  usernameExists: function usernameExists(username, cb) {
    if (typeof(cb) != "function") {
      throw new Error("cb must be a function.");
    }

    let url = this.baseURI + username;
    let request = new RESTRequest(url);
    request.get(this._onUsername.bind(this, cb, request));
  },

  /**
   * Obtain the Weave (Sync) node for a specified user.
   *
   * The callback receives the following arguments:
   *
   *   (Error)  Describes error that occurred or null if request was successful.
   *   (string) Username request is for.
   *   (string) URL of user's node. If null and there is no error, no node could
   *            be assigned at the time of the request.
   */
  getWeaveNode: function getWeaveNode(username, password, cb) {
    if (typeof(cb) != "function") {
      throw new Error("cb must be a function.");
    }

    let request = this._getRequest(username, "/node/weave", password);
    request.get(this._onWeaveNode.bind(this, cb, request));
  },

  /**
   * Change a password for the specified user.
   *
   * @param username
   *        (string) The username whose password to change.
   * @param oldPassword
   *        (string) The old, current password.
   * @param newPassword
   *        (string) The new password to switch to.
   */
  changePassword: function changePassword(username, oldPassword, newPassword, cb) {
    let request = this._getRequest(username, "/password", oldPassword);
    request.onComplete = this._onChangePassword.bind(this, cb, request);
    request.post(CommonUtils.encodeUTF8(newPassword));
  },

  createAccount: function createAccount(email, password, captchaChallenge,
                                        captchaResponse, cb) {
    let username = IdentityManager.prototype.usernameFromAccount(email);
    let body = JSON.stringify({
      "email":             email,
      "password":          Utils.encodeUTF8(password),
      "captcha-challenge": captchaChallenge,
      "captcha-response":  captchaResponse
    });

    let url = this.baseURI + username;
    let request = new RESTRequest(url);

    if (this.adminSecret) {
      request.setHeader("X-Weave-Secret", this.adminSecret);
    }

    request.onComplete = this._onCreateAccount.bind(this, cb, request);
    request.put(body);
  },

  _getRequest: function _getRequest(username, path, password = null) {
    let url = this.baseURI + username + path;
    let request = new RESTRequest(url);

    if (password) {
      let up = username + ":" + password;
      request.setHeader("authorization", "Basic " + btoa(up));
    }

    return request;
  },

  _onUsername: function _onUsername(cb, request, error) {
    if (error) {
      cb(error, null);
      return;
    }

    let body = request.response.body;
    if (body == "0") {
      cb(null, false);
    } else if (body == "1") {
      cb(null, true);
    } else {
      cb(new Error("Unknown response from server: " + body), null);
    }
  },

  _onWeaveNode: function _onWeaveNode(cb, request, error) {
    if (error) {
      cb.network = true;
      cb(error, null);
      return;
    }

    let response = request.response;

    if (response.status == 200) {
      let body = response.body;
      if (body == "null") {
        cb(null, null);
        return;
      }

      cb(null, body);
      return;
    }

    error = new Error("Sync node retrieval failed.");
    switch (response.status) {
      case 400:
        error.denied = true;
        break;
      case 404:
        error.notFound = true;
        break;
      default:
        error.message = "Unexpected response code: " + response.status;
    }

    cb(error, null);
  },

  _onChangePassword: function _onChangePassword(cb, request, error) {
    this._log.info("Password change response received: " +
                   request.response.status);
    if (error) {
      cb(error);
      return;
    }

    let response = request.response;
    if (response.status != 200) {
      cb(new Error("Password changed failed: " + response.body));
      return;
    }

    cb(null);
  },

  _onCreateAccount: function _onCreateAccount(cb, request, error) {
    let response = request.response;

    this._log.info("Create account response: " + response.status + " " +
                   response.body);

    if (error) {
      cb(new Error("HTTP transport error."), null);
      return;
    }

    if (response.status == 200) {
      cb(null, response.body);
      return;
    }

    error = new Error("Could not create user.");
    error.body = response.body;

    cb(error, null);
  },
};
Object.freeze(UserAPI10Client.prototype);
