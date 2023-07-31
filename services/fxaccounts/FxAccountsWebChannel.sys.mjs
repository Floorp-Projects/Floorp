/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Firefox Accounts Web Channel.
 *
 * Uses the WebChannel component to receive messages
 * about account state changes.
 */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

import {
  COMMAND_PROFILE_CHANGE,
  COMMAND_LOGIN,
  COMMAND_LOGOUT,
  COMMAND_DELETE,
  COMMAND_CAN_LINK_ACCOUNT,
  COMMAND_SYNC_PREFERENCES,
  COMMAND_CHANGE_PASSWORD,
  COMMAND_FXA_STATUS,
  COMMAND_PAIR_HEARTBEAT,
  COMMAND_PAIR_SUPP_METADATA,
  COMMAND_PAIR_AUTHORIZE,
  COMMAND_PAIR_DECLINE,
  COMMAND_PAIR_COMPLETE,
  COMMAND_PAIR_PREFERENCES,
  COMMAND_FIREFOX_VIEW,
  FX_OAUTH_CLIENT_ID,
  ON_PROFILE_CHANGE_NOTIFICATION,
  PREF_LAST_FXA_USER,
  WEBCHANNEL_ID,
  log,
  logPII,
} from "resource://gre/modules/FxAccountsCommon.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CryptoUtils: "resource://services-crypto/utils.sys.mjs",
  FxAccountsPairingFlow: "resource://gre/modules/FxAccountsPairing.sys.mjs",
  FxAccountsStorageManagerCanStoreField:
    "resource://gre/modules/FxAccountsStorage.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  Weave: "resource://services-sync/main.sys.mjs",
  WebChannel: "resource://gre/modules/WebChannel.sys.mjs",
});
ChromeUtils.defineLazyGetter(lazy, "fxAccounts", () => {
  return ChromeUtils.importESModule(
    "resource://gre/modules/FxAccounts.sys.mjs"
  ).getFxAccountsSingleton();
});
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "pairingEnabled",
  "identity.fxaccounts.pairing.enabled"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "separatePrivilegedMozillaWebContentProcess",
  "browser.tabs.remote.separatePrivilegedMozillaWebContentProcess",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "separatedMozillaDomains",
  "browser.tabs.remote.separatedMozillaDomains",
  "",
  false,
  val => val.split(",")
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "accountServer",
  "identity.fxaccounts.remote.root",
  null,
  false,
  val => Services.io.newURI(val)
);

// These engines were added years after Sync had been introduced, they need
// special handling since they are system add-ons and are un-available on
// older versions of Firefox.
const EXTRA_ENGINES = ["addresses", "creditcards"];

/**
 * A helper function that extracts the message and stack from an error object.
 * Returns a `{ message, stack }` tuple. `stack` will be null if the error
 * doesn't have a stack trace.
 */
function getErrorDetails(error) {
  // Replace anything that looks like it might be a filepath on Windows or Unix
  let cleanMessage = String(error)
    .replace(/\\.*\\/gm, "[REDACTED]")
    .replace(/\/.*\//gm, "[REDACTED]");
  let details = { message: cleanMessage, stack: null };

  // Adapted from Console.sys.mjs.
  if (error.stack) {
    let frames = [];
    for (let frame = error.stack; frame; frame = frame.caller) {
      frames.push(String(frame).padStart(4));
    }
    details.stack = frames.join("\n");
  }

  return details;
}

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
export function FxAccountsWebChannel(options) {
  if (!options) {
    throw new Error("Missing configuration options");
  }
  if (!options.content_uri) {
    throw new Error("Missing 'content_uri' option");
  }
  this._contentUri = options.content_uri;

  if (!options.channel_id) {
    throw new Error("Missing 'channel_id' option");
  }
  this._webChannelId = options.channel_id;

  // options.helpers is only specified by tests.
  ChromeUtils.defineLazyGetter(this, "_helpers", () => {
    return options.helpers || new FxAccountsWebChannelHelpers(options);
  });

  this._setupChannel();
}

FxAccountsWebChannel.prototype = {
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
      this._webChannelOrigin = Services.io.newURI(this._contentUri);
      this._registerChannel();
    } catch (e) {
      log.error(e);
      throw e;
    }
  },

  _receiveMessage(message, sendingContext) {
    const { command, data } = message;
    let shouldCheckRemoteType =
      lazy.separatePrivilegedMozillaWebContentProcess &&
      lazy.separatedMozillaDomains.some(function (val) {
        return (
          lazy.accountServer.asciiHost == val ||
          lazy.accountServer.asciiHost.endsWith("." + val)
        );
      });
    let { currentRemoteType } = sendingContext.browsingContext;
    if (shouldCheckRemoteType && currentRemoteType != "privilegedmozilla") {
      log.error(
        `Rejected FxA webchannel message from remoteType = ${currentRemoteType}`
      );
      return;
    }

    let browser = sendingContext.browsingContext.top.embedderElement;
    switch (command) {
      case COMMAND_PROFILE_CHANGE:
        Services.obs.notifyObservers(
          null,
          ON_PROFILE_CHANGE_NOTIFICATION,
          data.uid
        );
        break;
      case COMMAND_LOGIN:
        this._helpers
          .login(data)
          .catch(error => this._sendError(error, message, sendingContext));
        break;
      case COMMAND_LOGOUT:
      case COMMAND_DELETE:
        this._helpers
          .logout(data.uid)
          .catch(error => this._sendError(error, message, sendingContext));
        break;
      case COMMAND_CAN_LINK_ACCOUNT:
        let canLinkAccount = this._helpers.shouldAllowRelink(data.email);

        let response = {
          command,
          messageId: message.messageId,
          data: { ok: canLinkAccount },
        };

        log.debug("FxAccountsWebChannel response", response);
        this._channel.send(response, sendingContext);
        break;
      case COMMAND_SYNC_PREFERENCES:
        this._helpers.openSyncPreferences(browser, data.entryPoint);
        break;
      case COMMAND_PAIR_PREFERENCES:
        if (lazy.pairingEnabled) {
          let window = browser.ownerGlobal;
          // We should close the FxA tab after we open our pref page
          let selectedTab = window.gBrowser.selectedTab;
          window.switchToTabHavingURI(
            "about:preferences?action=pair#sync",
            true,
            {
              ignoreQueryString: true,
              replaceQueryString: true,
              adoptIntoActiveWindow: true,
              ignoreFragment: "whenComparing",
              triggeringPrincipal:
                Services.scriptSecurityManager.getSystemPrincipal(),
            }
          );
          // close the tab
          window.gBrowser.removeTab(selectedTab);
        }
        break;
      case COMMAND_FIREFOX_VIEW:
        this._helpers.openFirefoxView(browser, data.entryPoint);
        break;
      case COMMAND_CHANGE_PASSWORD:
        this._helpers
          .changePassword(data)
          .catch(error => this._sendError(error, message, sendingContext));
        break;
      case COMMAND_FXA_STATUS:
        log.debug("fxa_status received");

        const service = data && data.service;
        const isPairing = data && data.isPairing;
        const context = data && data.context;
        this._helpers
          .getFxaStatus(service, sendingContext, isPairing, context)
          .then(fxaStatus => {
            let response = {
              command,
              messageId: message.messageId,
              data: fxaStatus,
            };
            this._channel.send(response, sendingContext);
          })
          .catch(error => this._sendError(error, message, sendingContext));
        break;
      case COMMAND_PAIR_HEARTBEAT:
      case COMMAND_PAIR_SUPP_METADATA:
      case COMMAND_PAIR_AUTHORIZE:
      case COMMAND_PAIR_DECLINE:
      case COMMAND_PAIR_COMPLETE:
        log.debug(`Pairing command ${command} received`);
        const { channel_id: channelId } = data;
        delete data.channel_id;
        const flow = lazy.FxAccountsPairingFlow.get(channelId);
        if (!flow) {
          log.warn(`Could not find a pairing flow for ${channelId}`);
          return;
        }
        flow.onWebChannelMessage(command, data).then(replyData => {
          this._channel.send(
            {
              command,
              messageId: message.messageId,
              data: replyData,
            },
            sendingContext
          );
        });
        break;
      default:
        log.warn("Unrecognized FxAccountsWebChannel command", command);
        // As a safety measure we also terminate any pending FxA pairing flow.
        lazy.FxAccountsPairingFlow.finalizeAll();
        break;
    }
  },

  _sendError(error, incomingMessage, sendingContext) {
    log.error("Failed to handle FxAccountsWebChannel message", error);
    this._channel.send(
      {
        command: incomingMessage.command,
        messageId: incomingMessage.messageId,
        data: {
          error: getErrorDetails(error),
        },
      },
      sendingContext
    );
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
     *        @param sendingContext.browsingContext {BrowsingContext}
     *               The browsingcontext from which the
     *               WebChannelMessageToChrome was sent.
     *        @param sendingContext.eventTarget {EventTarget}
     *               The <EventTarget> where the message was sent.
     *        @param sendingContext.principal {Principal}
     *               The <Principal> of the EventTarget where the message was sent.
     * @private
     *
     */
    let listener = (webChannelId, message, sendingContext) => {
      if (message) {
        log.debug("FxAccountsWebChannel message received", message.command);
        if (logPII()) {
          log.debug("FxAccountsWebChannel message details", message);
        }
        try {
          this._receiveMessage(message, sendingContext);
        } catch (error) {
          this._sendError(error, message, sendingContext);
        }
      }
    };

    this._channelCallback = listener;
    this._channel = new lazy.WebChannel(
      this._webChannelId,
      this._webChannelOrigin
    );
    this._channel.listen(listener);
    log.debug(
      "FxAccountsWebChannel registered: " +
        this._webChannelId +
        " with origin " +
        this._webChannelOrigin.prePath
    );
  },
};

export function FxAccountsWebChannelHelpers(options) {
  options = options || {};

  this._fxAccounts = options.fxAccounts || lazy.fxAccounts;
  this._weaveXPCOM = options.weaveXPCOM || null;
  this._privateBrowsingUtils =
    options.privateBrowsingUtils || lazy.PrivateBrowsingUtils;
}

FxAccountsWebChannelHelpers.prototype = {
  // If the last fxa account used for sync isn't this account, we display
  // a modal dialog checking they really really want to do this...
  // (This is sync-specific, so ideally would be in sync's identity module,
  // but it's a little more seamless to do here, and sync is currently the
  // only fxa consumer, so...
  shouldAllowRelink(acctName) {
    return (
      !this._needRelinkWarning(acctName) || this._promptForRelink(acctName)
    );
  },

  /**
   * stores sync login info it in the fxaccounts service
   *
   * @param accountData the user's account data and credentials
   */
  async login(accountData) {
    // We don't act on customizeSync anymore, it used to open a dialog inside
    // the browser to selecte the engines to sync but we do it on the web now.
    log.debug("Webchannel is logging a user in.");
    delete accountData.customizeSync;

    // Save requested services for later.
    const requestedServices = accountData.services;
    delete accountData.services;

    // the user has already been shown the "can link account"
    // screen. No need to keep this data around.
    delete accountData.verifiedCanLinkAccount;

    // Remember who it was so we can log out next time.
    if (accountData.verified) {
      this.setPreviousAccountNameHashPref(accountData.email);
    }

    await this._fxAccounts.telemetry.recordConnection(
      Object.keys(requestedServices || {}),
      "webchannel"
    );

    // A sync-specific hack - we want to ensure sync has been initialized
    // before we set the signed-in user.
    // XXX - probably not true any more, especially now we have observerPreloads
    // in FxAccounts.jsm?
    let xps =
      this._weaveXPCOM ||
      Cc["@mozilla.org/weave/service;1"].getService(Ci.nsISupports)
        .wrappedJSObject;
    await xps.whenLoaded();
    await this._fxAccounts._internal.setSignedInUser(accountData);

    if (requestedServices) {
      // User has enabled Sync.
      if (requestedServices.sync) {
        const { offeredEngines, declinedEngines } = requestedServices.sync;
        if (offeredEngines && declinedEngines) {
          EXTRA_ENGINES.forEach(engine => {
            if (
              offeredEngines.includes(engine) &&
              !declinedEngines.includes(engine)
            ) {
              // These extra engines are disabled by default.
              Services.prefs.setBoolPref(
                `services.sync.engine.${engine}`,
                true
              );
            }
          });
          log.debug("Received declined engines", declinedEngines);
          lazy.Weave.Service.engineManager.setDeclined(declinedEngines);
          declinedEngines.forEach(engine => {
            Services.prefs.setBoolPref(`services.sync.engine.${engine}`, false);
          });
        }
        log.debug("Webchannel is enabling sync");
        await xps.Weave.Service.configure();
      }
    }
  },

  /**
   * logout the fxaccounts service
   *
   * @param the uid of the account which have been logged out
   */
  async logout(uid) {
    let fxa = this._fxAccounts;
    let userData = await fxa._internal.getUserAccountData(["uid"]);
    if (userData && userData.uid === uid) {
      await fxa.telemetry.recordDisconnection(null, "webchannel");
      // true argument is `localOnly`, because server-side stuff
      // has already been taken care of by the content server
      await fxa.signOut(true);
    }
  },

  /**
   * Check if `sendingContext` is in private browsing mode.
   */
  isPrivateBrowsingMode(sendingContext) {
    if (!sendingContext) {
      log.error("Unable to check for private browsing mode, assuming true");
      return true;
    }

    let browser = sendingContext.browsingContext.top.embedderElement;
    const isPrivateBrowsing =
      this._privateBrowsingUtils.isBrowserPrivate(browser);
    log.debug("is private browsing", isPrivateBrowsing);
    return isPrivateBrowsing;
  },

  /**
   * Check whether sending fxa_status data should be allowed.
   */
  shouldAllowFxaStatus(service, sendingContext, isPairing, context) {
    // Return user data for any service in non-PB mode. In PB mode,
    // only return user data if service==="sync" or is in pairing mode
    // (as service will be equal to the OAuth client ID and not "sync").
    //
    // This behaviour allows users to click the "Manage Account"
    // link from about:preferences#sync while in PB mode and things
    // "just work". While in non-PB mode, users can sign into
    // Pocket w/o entering their password a 2nd time, while in PB
    // mode they *will* have to enter their email/password again.
    //
    // The difference in behaviour is to try to match user
    // expectations as to what is and what isn't part of the browser.
    // Sync is viewed as an integral part of the browser, interacting
    // with FxA as part of a Sync flow should work all the time. If
    // Sync is broken in PB mode, users will think Firefox is broken.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1323853
    log.debug("service", service);
    return (
      !this.isPrivateBrowsingMode(sendingContext) ||
      service === "sync" ||
      context === "fx_desktop_v3" ||
      isPairing
    );
  },

  /**
   * Get fxa_status information. Resolves to { signedInUser: <user_data> }.
   * If returning status information is not allowed or no user is signed into
   * Sync, `user_data` will be null.
   */
  async getFxaStatus(service, sendingContext, isPairing, context) {
    let signedInUser = null;

    if (
      this.shouldAllowFxaStatus(service, sendingContext, isPairing, context)
    ) {
      const userData = await this._fxAccounts._internal.getUserAccountData([
        "email",
        "sessionToken",
        "uid",
        "verified",
      ]);
      if (userData) {
        signedInUser = {
          email: userData.email,
          sessionToken: userData.sessionToken,
          uid: userData.uid,
          verified: userData.verified,
        };
      }
    }

    return {
      signedInUser,
      clientId: FX_OAUTH_CLIENT_ID,
      capabilities: {
        multiService: true,
        pairing: lazy.pairingEnabled,
        engines: this._getAvailableExtraEngines(),
      },
    };
  },

  _getAvailableExtraEngines() {
    return EXTRA_ENGINES.filter(engineName => {
      try {
        return Services.prefs.getBoolPref(
          `services.sync.engine.${engineName}.available`
        );
      } catch (e) {
        return false;
      }
    });
  },

  async changePassword(credentials) {
    // If |credentials| has fields that aren't handled by accounts storage,
    // updateUserAccountData will throw - mainly to prevent errors in code
    // that hard-codes field names.
    // However, in this case the field names aren't really in our control.
    // We *could* still insist the server know what fields names are valid,
    // but that makes life difficult for the server when Firefox adds new
    // features (ie, new fields) - forcing the server to track a map of
    // versions to supported field names doesn't buy us much.
    // So we just remove field names we know aren't handled.
    let newCredentials = {
      device: null, // Force a brand new device registration.
      // We force the re-encryption of the send tab keys using the new sync key after the password change
      encryptedSendTabKeys: null,
    };
    for (let name of Object.keys(credentials)) {
      if (
        name == "email" ||
        name == "uid" ||
        lazy.FxAccountsStorageManagerCanStoreField(name)
      ) {
        newCredentials[name] = credentials[name];
      } else {
        log.info("changePassword ignoring unsupported field", name);
      }
    }
    await this._fxAccounts._internal.updateUserAccountData(newCredentials);
    await this._fxAccounts._internal.updateDeviceRegistration();
  },

  /**
   * Get the hash of account name of the previously signed in account
   */
  getPreviousAccountNameHashPref() {
    try {
      return Services.prefs.getStringPref(PREF_LAST_FXA_USER);
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
    Services.prefs.setStringPref(
      PREF_LAST_FXA_USER,
      lazy.CryptoUtils.sha256Base64(acctName)
    );
  },

  /**
   * Open Sync Preferences in the current tab of the browser
   *
   * @param {Object} browser the browser in which to open preferences
   * @param {String} [entryPoint] entryPoint to use for logging
   */
  openSyncPreferences(browser, entryPoint) {
    let uri = "about:preferences";
    if (entryPoint) {
      uri += "?entrypoint=" + encodeURIComponent(entryPoint);
    }
    uri += "#sync";

    browser.loadURI(Services.io.newURI(uri), {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
  },

  /**
   * Open Firefox View in the browser's window
   *
   * @param {Object} browser the browser in whose window we'll open Firefox View
   * @param {String} [entryPoint] entryPoint Optional string to use for logging
   */
  openFirefoxView(browser, entryPoint) {
    browser.ownerGlobal.FirefoxViewHandler.openTab(entryPoint);
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
    return (
      prevAcctHash && prevAcctHash != lazy.CryptoUtils.sha256Base64(acctName)
    );
  },

  /**
   * Show the user a warning dialog that the data from the previous account
   * and the new account will be merged.
   *
   * @private
   */
  _promptForRelink(acctName) {
    let sb = Services.strings.createBundle(
      "chrome://browser/locale/syncSetup.properties"
    );
    let continueLabel = sb.GetStringFromName("continue.label");
    let title = sb.GetStringFromName("relinkVerify.title");
    let description = sb.formatStringFromName("relinkVerify.description", [
      acctName,
    ]);
    let body =
      sb.GetStringFromName("relinkVerify.heading") + "\n\n" + description;
    let ps = Services.prompt;
    let buttonFlags =
      ps.BUTTON_POS_0 * ps.BUTTON_TITLE_IS_STRING +
      ps.BUTTON_POS_1 * ps.BUTTON_TITLE_CANCEL +
      ps.BUTTON_POS_1_DEFAULT;

    // If running in context of the browser chrome, window does not exist.
    let pressed = Services.prompt.confirmEx(
      null,
      title,
      body,
      buttonFlags,
      continueLabel,
      null,
      null,
      null,
      {}
    );
    return pressed === 0; // 0 is the "continue" button
  },
};

var singleton;

// The entry-point for this module, which ensures only one of our channels is
// ever created - we require this because the WebChannel is global in scope
// (eg, it uses the observer service to tell interested parties of interesting
// things) and allowing multiple channels would cause such notifications to be
// sent multiple times.
export var EnsureFxAccountsWebChannel = () => {
  let contentUri = Services.urlFormatter.formatURLPref(
    "identity.fxaccounts.remote.root"
  );
  if (singleton && singleton._contentUri !== contentUri) {
    singleton.tearDown();
    singleton = null;
  }
  if (!singleton) {
    try {
      if (contentUri) {
        // The FxAccountsWebChannel listens for events and updates
        // the state machine accordingly.
        singleton = new FxAccountsWebChannel({
          content_uri: contentUri,
          channel_id: WEBCHANNEL_ID,
        });
      } else {
        log.warn("FxA WebChannel functionaly is disabled due to no URI pref.");
      }
    } catch (ex) {
      log.error("Failed to create FxA WebChannel", ex);
    }
  }
};
