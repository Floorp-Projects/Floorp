/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

/**
  * This module consolidates various code and data update requests, so flags
  * can be set, Telemetry collected, etc. in a central place.
  */

Cu.import("resource://gre/modules/Log.jsm");
Cu.importGlobalProperties(["XMLHttpRequest"]);

this.EXPORTED_SYMBOLS = [ "ServiceRequest" ];

const logger = Log.repository.getLogger("ServiceRequest");
logger.level = Log.Level.Debug;
logger.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));

/**
  * ServiceRequest is intended to be a drop-in replacement for current users
  * of XMLHttpRequest.
  *
  * @param {Object} options - Options for underlying XHR, e.g. { mozAnon: bool }
  */
class ServiceRequest extends XMLHttpRequest {
  constructor(options) {
    super(options);
  }
  /**
    * Opens an XMLHttpRequest, and sets the NSS "beConservative" flag.
    * Requests are always async.
    *
    * @param {String} method - HTTP method to use, e.g. "GET".
    * @param {String} url - URL to open.
    * @param {Object} options - Additional options (reserved for future use).
    */
  open(method, url, options) {
    super.open(method, url, true);

    // Disable cutting edge features, like TLS 1.3, where middleboxes might brick us
    if (super.channel instanceof Ci.nsIHttpChannelInternal) {
      super.channel.QueryInterface(Ci.nsIHttpChannelInternal).beConservative = true;
    }
  }
}
