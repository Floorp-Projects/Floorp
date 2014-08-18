/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Firefox Accounts OAuth browser login helper.
 * Uses the WebChannel component to receive OAuth messages and complete login flows.
 */

this.EXPORTED_SYMBOLS = ["FxAccountsOAuthClient"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
XPCOMUtils.defineLazyModuleGetter(this, "WebChannel",
                                  "resource://gre/modules/WebChannel.jsm");
Cu.importGlobalProperties(["URL"]);

/**
 * Create a new FxAccountsOAuthClient for browser some service.
 *
 * @param {Object} options Options
 *   @param {Object} options.parameters
 *   Opaque alphanumeric token to be included in verification links
 *     @param {String} options.parameters.client_id
 *     OAuth id returned from client registration
 *     @param {String} options.parameters.state
 *     A value that will be returned to the client as-is upon redirection
 *     @param {String} options.parameters.oauth_uri
 *     The FxA OAuth server uri
 *     @param {String} options.parameters.content_uri
 *     The FxA Content server uri
 *     @param {String} [options.parameters.scope]
 *     Optional. A colon-separated list of scopes that the user has authorized
 *     @param {String} [options.parameters.action]
 *     Optional. If provided, should be either signup or signin.
 *   @param [authorizationEndpoint] {String}
 *   Optional authorization endpoint for the OAuth server
 * @constructor
 */
this.FxAccountsOAuthClient = function(options) {
  this._validateOptions(options);
  this.parameters = options.parameters;
  this._configureChannel();

  let authorizationEndpoint = options.authorizationEndpoint || "/authorization";

  try {
    this._fxaOAuthStartUrl = new URL(this.parameters.oauth_uri + authorizationEndpoint + "?");
  } catch (e) {
    throw new Error("Invalid OAuth Url");
  }

  let params = this._fxaOAuthStartUrl.searchParams;
  params.append("client_id", this.parameters.client_id);
  params.append("state", this.parameters.state);
  params.append("scope", this.parameters.scope || "");
  params.append("action", this.parameters.action || "signin");
  params.append("webChannelId", this._webChannelId);

};

this.FxAccountsOAuthClient.prototype = {
  /**
   * Function that gets called once the OAuth flow is complete.
   * The callback will receive null as it's argument if there is a state mismatch or an object with
   * code and state properties otherwise.
   */
  onComplete: null,
  /**
   * Configuration object that stores all OAuth parameters.
   */
  parameters: null,
  /**
   * WebChannel that is used to communicate with content page.
   */
  _channel: null,
  /**
   * Boolean to indicate if this client has completed an OAuth flow.
   */
  _complete: false,
  /**
   * The url that opens the Firefox Accounts OAuth flow.
   */
  _fxaOAuthStartUrl: null,
  /**
   * WebChannel id.
   */
  _webChannelId: null,
  /**
   * WebChannel origin, used to validate origin of messages.
   */
  _webChannelOrigin: null,
  /**
   * Opens a tab at "this._fxaOAuthStartUrl".
   * Registers a WebChannel listener and sets up a callback if needed.
   */
  launchWebFlow: function () {
    if (!this._channelCallback) {
      this._registerChannel();
    }

    if (this._complete) {
      throw new Error("This client already completed the OAuth flow");
    } else {
      let opener = Services.wm.getMostRecentWindow("navigator:browser").gBrowser;
      opener.selectedTab = opener.addTab(this._fxaOAuthStartUrl.href);
    }
  },

  /**
   * Release all resources that are in use.
   */
  tearDown: function() {
    this.onComplete = null;
    this._complete = true;
    this._channel.stopListening();
    this._channel = null;
  },

  /**
   * Configures WebChannel id and origin
   *
   * @private
   */
  _configureChannel: function() {
    this._webChannelId = "oauth_" + this.parameters.client_id;

    // if this.parameters.content_uri is present but not a valid URI, then this will throw an error.
    try {
      this._webChannelOrigin = Services.io.newURI(this.parameters.content_uri, null, null);
    } catch (e) {
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
    let listener = function (webChannelId, message, target) {
      if (message) {
        let command = message.command;
        let data = message.data;

        switch (command) {
          case "oauth_complete":
            // validate the state parameter and call onComplete
            let result = null;
            if (this.parameters.state === data.state) {
              result = {
                code: data.code,
                state: data.state
              };
              log.debug("OAuth flow completed.");
            } else {
              log.debug("OAuth flow failed. State doesn't match");
            }

            if (this.onComplete) {
              this.onComplete(result);
            }
            // onComplete will be called for this client only once
            // calling onComplete again will result in a failure of the OAuth flow
            this.tearDown();

            // if the message asked to close the tab
            if (data.closeWindow && target && target.contentWindow) {
              target.contentWindow.close();
            }
            break;
        }
      }
    };

    this._channelCallback = listener.bind(this);
    this._channel = new WebChannel(this._webChannelId, this._webChannelOrigin);
    this._channel.listen(this._channelCallback);
    log.debug("Channel registered: " + this._webChannelId + " with origin " + this._webChannelOrigin.prePath);
  },

  /**
   * Validates the required FxA OAuth parameters
   *
   * @param options {Object}
   *        OAuth client options
   * @private
   */
  _validateOptions: function (options) {
    if (!options || !options.parameters) {
      throw new Error("Missing 'parameters' configuration option");
    }

    ["oauth_uri", "client_id", "content_uri", "state"].forEach(option => {
      if (!options.parameters[option]) {
        throw new Error("Missing 'parameters." + option + "' parameter");
      }
    });
  },
};
