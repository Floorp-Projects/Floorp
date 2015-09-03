/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Firefox Accounts Web Channel.
 *
 * Uses the WebChannel component to receive messages
 * about account state changes.
 */

this.EXPORTED_SYMBOLS = ["EnsureFxAccountsWebChannel"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WebChannel",
                                  "resource://gre/modules/WebChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
                                  "resource://gre/modules/FxAccounts.jsm");

const COMMAND_PROFILE_CHANGE       = "profile:change";
const COMMAND_CAN_LINK_ACCOUNT     = "fxaccounts:can_link_account";
const COMMAND_LOGIN                = "fxaccounts:login";
const COMMAND_LOGOUT               = "fxaccounts:logout";
const COMMAND_DELETE               = "fxaccounts:delete";

const PREF_LAST_FXA_USER           = "identity.fxaccounts.lastSignedInUserHash";
const PREF_SYNC_SHOW_CUSTOMIZATION = "services.sync-setup.ui.showCustomizationDialog";

/**
 * Create a new FxAccountsWebChannel to listen for account updates
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

  // options.helpers is only specified by tests.
  this._helpers = options.helpers || new FxAccountsWebChannelHelpers(options);

  this._setupChannel();
};

this.FxAccountsWebChannel.prototype = {
  /**
   * WebChannel that is used to communicate with content page
   */
  _channel: null,

  /**
   * Helpers interface that does the heavy lifting.
   */
  _helpers: null,

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
      log.error(e);
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
        log.debug("FxAccountsWebChannel message received", message);
        let command = message.command;
        let data = message.data;

        switch (command) {
          case COMMAND_PROFILE_CHANGE:
            Services.obs.notifyObservers(null, ON_PROFILE_CHANGE_NOTIFICATION, data.uid);
            break;
          case COMMAND_LOGIN:
            this._helpers.login(data);
            break;
          case COMMAND_LOGOUT:
          case COMMAND_DELETE:
            this._helpers.logout(data.uid);
            break;
          case COMMAND_CAN_LINK_ACCOUNT:
            let canLinkAccount = this._helpers.shouldAllowRelink(data.email);

            let response = {
              command: command,
              messageId: message.messageId,
              data: { ok: canLinkAccount }
            };

            log.debug("FxAccountsWebChannel response", response);
            this._channel.send(response, sendingContext);
            break;
          default:
            log.warn("Unrecognized FxAccountsWebChannel command", command);
            break;
        }
      }
    };

    this._channelCallback = listener;
    this._channel = new WebChannel(this._webChannelId, this._webChannelOrigin);
    this._channel.listen(listener);
    log.debug("FxAccountsWebChannel registered: " + this._webChannelId + " with origin " + this._webChannelOrigin.prePath);
  }
};

this.FxAccountsWebChannelHelpers = function(options) {
  options = options || {};

  this._fxAccounts = options.fxAccounts || fxAccounts;
};

this.FxAccountsWebChannelHelpers.prototype = {
  // If the last fxa account used for sync isn't this account, we display
  // a modal dialog checking they really really want to do this...
  // (This is sync-specific, so ideally would be in sync's identity module,
  // but it's a little more seamless to do here, and sync is currently the
  // only fxa consumer, so...
  shouldAllowRelink(acctName) {
    return !this._needRelinkWarning(acctName) ||
            this._promptForRelink(acctName);
  },

  /**
   * New users are asked in the content server whether they want to
   * customize which data should be synced. The user is only shown
   * the dialog listing the possible data types upon verification.
   *
   * Save a bit into prefs that is read on verification to see whether
   * to show the list of data types that can be saved.
   */
  setShowCustomizeSyncPref(showCustomizeSyncPref) {
    Services.prefs.setBoolPref(PREF_SYNC_SHOW_CUSTOMIZATION, showCustomizeSyncPref);
  },

  getShowCustomizeSyncPref(showCustomizeSyncPref) {
    return Services.prefs.getBoolPref(PREF_SYNC_SHOW_CUSTOMIZATION);
  },

  /**
   * stores sync login info it in the fxaccounts service
   *
   * @param accountData the user's account data and credentials
   */
  login(accountData) {
    if (accountData.customizeSync) {
      this.setShowCustomizeSyncPref(true);
      delete accountData.customizeSync;
    }

    // the user has already been shown the "can link account"
    // screen. No need to keep this data around.
    delete accountData.verifiedCanLinkAccount;

    // Remember who it was so we can log out next time.
    this.setPreviousAccountNameHashPref(accountData.email);

    // A sync-specific hack - we want to ensure sync has been initialized
    // before we set the signed-in user.
    let xps = Cc["@mozilla.org/weave/service;1"]
              .getService(Ci.nsISupports)
              .wrappedJSObject;
    return xps.whenLoaded().then(() => {
      return this._fxAccounts.setSignedInUser(accountData);
    });
  },

  /**
   * logoust the fxaccounts service
   *
   * @param the uid of the account which have been logged out
   */
  logout(uid) {
    return fxAccounts.getSignedInUser().then(userData => {
      if (userData.uid === uid) {
        return fxAccounts.signOut();
      }
    });
  },

  /**
   * Get the hash of account name of the previously signed in account
   */
  getPreviousAccountNameHashPref() {
    try {
      return Services.prefs.getComplexValue(PREF_LAST_FXA_USER, Ci.nsISupportsString).data;
    } catch (_) {
      return "";
    }
  },

  /**
   * Given an account name, set the hash of the previously signed in account
   *
   * @param acctName the account name of the user's account.
   */
  setPreviousAccountNameHashPref(acctName) {
    let string = Cc["@mozilla.org/supports-string;1"]
                 .createInstance(Ci.nsISupportsString);
    string.data = this.sha256(acctName);
    Services.prefs.setComplexValue(PREF_LAST_FXA_USER, Ci.nsISupportsString, string);
  },

  /**
   * Given a string, returns the SHA265 hash in base64
   */
  sha256(str) {
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                      .createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";
    // Data is an array of bytes.
    let data = converter.convertToByteArray(str, {});
    let hasher = Cc["@mozilla.org/security/hash;1"]
                   .createInstance(Ci.nsICryptoHash);
    hasher.init(hasher.SHA256);
    hasher.update(data, data.length);

    return hasher.finish(true);
  },

  /**
   * If a user signs in using a different account, the data from the
   * previous account and the new account will be merged. Ask the user
   * if they want to continue.
   *
   * @private
   */
  _needRelinkWarning(acctName) {
    let prevAcctHash = this.getPreviousAccountNameHashPref();
    return prevAcctHash && prevAcctHash != this.sha256(acctName);
  },

  /**
   * Show the user a warning dialog that the data from the previous account
   * and the new account will be merged.
   *
   * @private
   */
  _promptForRelink(acctName) {
    let sb = Services.strings.createBundle("chrome://browser/locale/syncSetup.properties");
    let continueLabel = sb.GetStringFromName("continue.label");
    let title = sb.GetStringFromName("relinkVerify.title");
    let description = sb.formatStringFromName("relinkVerify.description",
                                              [acctName], 1);
    let body = sb.GetStringFromName("relinkVerify.heading") +
               "\n\n" + description;
    let ps = Services.prompt;
    let buttonFlags = (ps.BUTTON_POS_0 * ps.BUTTON_TITLE_IS_STRING) +
                      (ps.BUTTON_POS_1 * ps.BUTTON_TITLE_CANCEL) +
                      ps.BUTTON_POS_1_DEFAULT;

    // If running in context of the browser chrome, window does not exist.
    var targetWindow = typeof window === 'undefined' ? null : window;
    let pressed = Services.prompt.confirmEx(targetWindow, title, body, buttonFlags,
                                       continueLabel, null, null, null,
                                       {});
    return pressed === 0; // 0 is the "continue" button
  }
};

let singleton;
// The entry-point for this module, which ensures only one of our channels is
// ever created - we require this because the WebChannel is global in scope
// (eg, it uses the observer service to tell interested parties of interesting
// things) and allowing multiple channels would cause such notifications to be
// sent multiple times.
this.EnsureFxAccountsWebChannel = function() {
  if (!singleton) {
    let contentUri = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.webchannel.uri");
    // The FxAccountsWebChannel listens for events and updates
    // the state machine accordingly.
    singleton = new this.FxAccountsWebChannel({
      content_uri: contentUri,
      channel_id: WEBCHANNEL_ID,
    });
  }
}
