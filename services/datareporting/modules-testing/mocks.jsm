/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["MockPolicyListener"];

const {utils: Cu} = Components;

Cu.import("resource://services-common/log4moz.js");


this.MockPolicyListener = function MockPolicyListener() {
  this._log = Log4Moz.repository.getLogger("Services.DataReporting.Testing.MockPolicyListener");
  this._log.level = Log4Moz.Level["Debug"];

  this.requestDataUploadCount = 0;
  this.lastDataRequest = null;

  this.requestRemoteDeleteCount = 0;
  this.lastRemoteDeleteRequest = null;

  this.notifyUserCount = 0;
  this.lastNotifyRequest = null;
}

MockPolicyListener.prototype = {
  onRequestDataUpload: function onRequestDataUpload(request) {
    this._log.info("onRequestDataUpload invoked.");
    this.requestDataUploadCount++;
    this.lastDataRequest = request;
  },

  onRequestRemoteDelete: function onRequestRemoteDelete(request) {
    this._log.info("onRequestRemoteDelete invoked.");
    this.requestRemoteDeleteCount++;
    this.lastRemoteDeleteRequest = request;
  },

  onNotifyDataPolicy: function onNotifyDataPolicy(request) {
    this._log.info("onNotifyUser invoked.");
    this.notifyUserCount++;
    this.lastNotifyRequest = request;
  },
};

