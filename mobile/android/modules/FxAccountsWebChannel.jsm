// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Firefox Accounts Web Channel.
 *
 * Use the WebChannel component to receive messages about account
 * state changes.
 */
this.EXPORTED_SYMBOLS = ["EnsureFxAccountsWebChannel"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components; /*global Components */

Cu.import("resource://gre/modules/Accounts.jsm"); /*global Accounts */
Cu.import("resource://gre/modules/Notifications.jsm"); /*global Notifications */
Cu.import("resource://gre/modules/Prompt.jsm"); /*global Prompt */
Cu.import("resource://gre/modules/Services.jsm"); /*global Services */
Cu.import("resource://gre/modules/WebChannel.jsm"); /*global WebChannel */

const log = Cu.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog.bind("FxAccounts");

const WEBCHANNEL_ID = "account_updates";

const COMMAND_LOADED               = "fxaccounts:loaded";
const COMMAND_CAN_LINK_ACCOUNT     = "fxaccounts:can_link_account";
const COMMAND_LOGIN                = "fxaccounts:login";
const COMMAND_CHANGE_PASSWORD      = "fxaccounts:change_password";

/**
 * Create a new FxAccountsWebChannel to listen for account updates.
 *
 * @param {Object} options Options
 *   @param {Object} options
 *     @param {String} options.content_uri
 *     The FxA Content server uri
 *     @param {String} options.channel_id
 *     The ID of the WebChannel
 *     @param {String} options.helpers
 *     Helpers functions. Should only be passed in for testing.
 * @constructor
 */
this.FxAccountsWebChannel = function(options) {
  if (!options) {
    throw new Error("Missing configuration options");
  }
  if (!options["content_uri"]) {
    throw new Error("Missing 'content_uri' option");
  }
  this._contentUri = options.content_uri;

  if (!options["channel_id"]) {
    throw new Error("Missing 'channel_id' option");
  }
  this._webChannelId = options.channel_id;

  this._setupChannel();
};

this.FxAccountsWebChannel.prototype = {
  /**
   * WebChannel that is used to communicate with content page
   */
  _channel: null,

  /**
   * WebChannel ID.
   */
  _webChannelId: null,
  /**
   * WebChannel origin, used to validate origin of messages
   */
  _webChannelOrigin: null,

  /**
   * Release all resources that are in use.
   */
  tearDown() {
    this._channel.stopListening();
    this._channel = null;
    this._channelCallback = null;
  },

  /**
   * Configures and registers a new WebChannel
   *
   * @private
   */
  _setupChannel() {
    // if this.contentUri is present but not a valid URI, then this will throw an error.
    try {
      this._webChannelOrigin = Services.io.newURI(this._contentUri, null, null);
      this._registerChannel();
    } catch (e) {
      log.e(e);
      throw e;
    }
  },

  /**
   * Create a new channel with the WebChannelBroker, setup a callback listener
   * @private
   */
  _registerChannel() {
    /**
     * Processes messages that are called back from the FxAccountsChannel
     *
     * @param webChannelId {String}
     *        Command webChannelId
     * @param message {Object}
     *        Command message
     * @param sendingContext {Object}
     *        Message sending context.
     *        @param sendingContext.browser {browser}
     *               The <browser> object that captured the
     *               WebChannelMessageToChrome.
     *        @param sendingContext.eventTarget {EventTarget}
     *               The <EventTarget> where the message was sent.
     *        @param sendingContext.principal {Principal}
     *               The <Principal> of the EventTarget where the message was sent.
     * @private
     *
     */
    let listener = (webChannelId, message, sendingContext) => {
      if (message) {
        let command = message.command;
        let data = message.data;
        log.d("FxAccountsWebChannel message received, command: " + command);

        // Respond to the message with true or false.
        let respond = (data) => {
          let response = {
            command: command,
            messageId: message.messageId,
            data: data
          };
          log.d("Sending response to command: " + command);
          this._channel.send(response, sendingContext);
        };

        switch (command) {
          case COMMAND_LOADED:
            let mm = sendingContext.browser.docShell
              .QueryInterface(Ci.nsIInterfaceRequestor)
              .getInterface(Ci.nsIContentFrameMessageManager);
            mm.sendAsyncMessage(COMMAND_LOADED);
            break;

          case COMMAND_CAN_LINK_ACCOUNT:
            // Temporarily accept any login.
            respond({ ok: true });
            break;

          case COMMAND_LOGIN:
            // Either create a new Android Account or re-connect an existing
            // Android Account here.  There's not much to be done if we don't
            // succeed or get an error.
            Accounts.getFirefoxAccount().then(account => {
              if (!account) {
                return Accounts.createFirefoxAccountFromJSON(data).then(success => {
                  if (!success) {
                    throw new Error("Could not create Firefox Account!");
                  }
                  return success;
                });
              } else {
                return Accounts.updateFirefoxAccountFromJSON(data).then(success => {
                  if (!success) {
                    throw new Error("Could not update Firefox Account!");
                  }
                  return success;
                });
              }
            })
            .then(success => {
              if (!success) {
                throw new Error("Could not create or update Firefox Account!");
              }
              log.i("Created or updated Firefox Account.");
            })
            .catch(e => {
              log.e(e.toString());
            });
            break;

          case COMMAND_CHANGE_PASSWORD:
            // Only update an existing Android Account.
            Accounts.getFirefoxAccount().then(account => {
              if (!account) {
                throw new Error("Can't change password of non-existent Firefox Account!");
              }
              return Accounts.updateFirefoxAccountFromJSON(data);
            })
            .then(success => {
              if (!success) {
                throw new Error("Could not change Firefox Account password!");
              }
              log.i("Changed Firefox Account password.");
            })
            .catch(e => {
              log.e(e.toString());
            });
            break;

          default:
            log.w("Ignoring unrecognized FxAccountsWebChannel command: " + JSON.stringify(command));
            break;
        }
      }
    };

    this._channelCallback = listener;
    this._channel = new WebChannel(this._webChannelId, this._webChannelOrigin);
    this._channel.listen(listener);

    log.d("FxAccountsWebChannel registered: " + this._webChannelId + " with origin " + this._webChannelOrigin.prePath);
  }
};

let singleton;
// The entry-point for this module, which ensures only one of our channels is
// ever created - we require this because the WebChannel is global in scope and
// allowing multiple channels would cause such notifications to be sent multiple
// times.
this.EnsureFxAccountsWebChannel = function() {
  if (!singleton) {
    let contentUri = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.webchannel.uri");
    // The FxAccountsWebChannel listens for events and updates the Java layer.
    singleton = new this.FxAccountsWebChannel({
      content_uri: contentUri,
      channel_id: WEBCHANNEL_ID,
    });
  }
};
