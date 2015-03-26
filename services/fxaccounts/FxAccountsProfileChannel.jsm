/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Firefox Accounts Profile update helper.
 * Uses the WebChannel component to receive messages about account changes.
 */

this.EXPORTED_SYMBOLS = ["FxAccountsProfileChannel"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");

XPCOMUtils.defineLazyModuleGetter(this, "WebChannel",
                                  "resource://gre/modules/WebChannel.jsm");

const PROFILE_CHANGE_COMMAND = "profile:change";

/**
 * Create a new FxAccountsProfileChannel to listen to profile updates
 *
 * @param {Object} options Options
 *   @param {Object} options.parameters
 *     @param {String} options.parameters.content_uri
 *     The FxA Content server uri
 * @constructor
 */
this.FxAccountsProfileChannel = function(options) {
  if (!options) {
    throw new Error("Missing configuration options");
  }
  if (!options["content_uri"]) {
    throw new Error("Missing 'content_uri' option");
  }
  this.parameters = options;

  this._setupChannel();
};

this.FxAccountsProfileChannel.prototype = {
  /**
   * Configuration object
   */
  parameters: null,
  /**
   * WebChannel that is used to communicate with content page
   */
  _channel: null,
  /**
   * WebChannel origin, used to validate origin of messages
   */
  _webChannelOrigin: null,

  /**
   * Release all resources that are in use.
   */
  tearDown: function() {
    this._channel.stopListening();
    this._channel = null;
    this._channelCallback = null;
  },

  /**
   * Configures and registers a new WebChannel
   *
   * @private
   */
  _setupChannel: function() {
    // if this.parameters.content_uri is present but not a valid URI, then this will throw an error.
    try {
      this._webChannelOrigin = Services.io.newURI(this.parameters.content_uri, null, null);
      this._registerChannel();
    } catch (e) {
      log.error(e);
      throw e;
    }
  },

  /**
   * Create a new channel with the WebChannelBroker, setup a callback listener
   * @private
   */
  _registerChannel: function() {
    /**
     * Processes messages that are called back from the FxAccountsChannel
     *
     * @param webChannelId {String}
     *        Command webChannelId
     * @param message {Object}
     *        Command message
     * @param target {EventTarget}
     *        Channel message event target
     * @private
     */
    let listener = (webChannelId, message, target) => {
      if (message) {
        let command = message.command;
        let data = message.data;
        switch (command) {
          case PROFILE_CHANGE_COMMAND:
            Services.obs.notifyObservers(null, ON_PROFILE_CHANGE_NOTIFICATION, data.uid);
          break;
        }
      }
    };

    this._channelCallback = listener;
    this._channel = new WebChannel(PROFILE_WEBCHANNEL_ID, this._webChannelOrigin);
    this._channel.listen(this._channelCallback);
    log.debug("Channel registered: " + PROFILE_WEBCHANNEL_ID + " with origin " + this._webChannelOrigin.prePath);
  }

};
