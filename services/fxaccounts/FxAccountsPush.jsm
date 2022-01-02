/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { Async } = ChromeUtils.import("resource://services-common/async.js");
const {
  FXA_PUSH_SCOPE_ACCOUNT_UPDATE,
  ONLOGOUT_NOTIFICATION,
  ON_ACCOUNT_DESTROYED_NOTIFICATION,
  ON_COLLECTION_CHANGED_NOTIFICATION,
  ON_COMMAND_RECEIVED_NOTIFICATION,
  ON_DEVICE_CONNECTED_NOTIFICATION,
  ON_DEVICE_DISCONNECTED_NOTIFICATION,
  ON_PASSWORD_CHANGED_NOTIFICATION,
  ON_PASSWORD_RESET_NOTIFICATION,
  ON_PROFILE_CHANGE_NOTIFICATION,
  ON_PROFILE_UPDATED_NOTIFICATION,
  ON_VERIFY_LOGIN_NOTIFICATION,
  log,
} = ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js");

/**
 * FxAccountsPushService manages Push notifications for Firefox Accounts in the browser
 *
 * @param [options]
 *        Object, custom options that used for testing
 * @constructor
 */
function FxAccountsPushService(options = {}) {
  this.log = log;

  if (options.log) {
    // allow custom log for testing purposes
    this.log = options.log;
  }

  this.log.debug("FxAccountsPush loading service");
  this.wrappedJSObject = this;
  this.initialize(options);
}

FxAccountsPushService.prototype = {
  /**
   * Helps only initialize observers once.
   */
  _initialized: false,
  /**
   * Instance of the nsIPushService or a mocked object.
   */
  pushService: null,
  /**
   * Instance of FxAccountsInternal or a mocked object.
   */
  fxai: null,
  /**
   * Component ID of this service, helps register this component.
   */
  classID: Components.ID("{1b7db999-2ecd-4abf-bb95-a726896798ca}"),
  /**
   * Register used interfaces in this service
   */
  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
  /**
   * Initialize the service and register all the required observers.
   *
   * @param [options]
   */
  initialize(options) {
    if (this._initialized) {
      return false;
    }

    this._initialized = true;

    if (options.pushService) {
      this.pushService = options.pushService;
    } else {
      this.pushService = Cc["@mozilla.org/push/Service;1"].getService(
        Ci.nsIPushService
      );
    }

    if (options.fxai) {
      this.fxai = options.fxai;
    } else {
      let { fxAccounts } = ChromeUtils.import(
        "resource://gre/modules/FxAccounts.jsm",
        {}
      );
      this.fxai = fxAccounts._internal;
    }

    this.asyncObserver = Async.asyncObserver(this, this.log);
    // We use an async observer because a device waking up can
    // observe multiple "Send Tab received" push notifications at the same time.
    // The way these notifications are handled is as follows:
    // Read index from storage, make network request, update the index.
    // You can imagine what happens when multiple calls race: we load
    // the same index multiple times and receive the same exact tabs, multiple times.
    // The async observer will ensure we make these network requests serially.
    Services.obs.addObserver(this.asyncObserver, this.pushService.pushTopic);
    Services.obs.addObserver(
      this.asyncObserver,
      this.pushService.subscriptionChangeTopic
    );
    Services.obs.addObserver(this.asyncObserver, ONLOGOUT_NOTIFICATION);

    this.log.debug("FxAccountsPush initialized");
    return true;
  },
  /**
   * Registers a new endpoint with the Push Server
   *
   * @returns {Promise}
   *          Promise always resolves with a subscription or a null if failed to subscribe.
   */
  registerPushEndpoint() {
    this.log.trace("FxAccountsPush registerPushEndpoint");

    return new Promise(resolve => {
      this.pushService.subscribe(
        FXA_PUSH_SCOPE_ACCOUNT_UPDATE,
        Services.scriptSecurityManager.getSystemPrincipal(),
        (result, subscription) => {
          if (Components.isSuccessCode(result)) {
            this.log.debug("FxAccountsPush got subscription");
            resolve(subscription);
          } else {
            this.log.warn("FxAccountsPush failed to subscribe", result);
            resolve(null);
          }
        }
      );
    });
  },
  /**
   * Async observer interface to listen to push messages, changes and logout.
   *
   * @param subject
   * @param topic
   * @param data
   * @returns {Promise}
   */
  async observe(subject, topic, data) {
    try {
      this.log.trace(
        `observed topic=${topic}, data=${data}, subject=${subject}`
      );
      switch (topic) {
        case this.pushService.pushTopic:
          if (data === FXA_PUSH_SCOPE_ACCOUNT_UPDATE) {
            let message = subject.QueryInterface(Ci.nsIPushMessage);
            await this._onPushMessage(message);
          }
          break;
        case this.pushService.subscriptionChangeTopic:
          if (data === FXA_PUSH_SCOPE_ACCOUNT_UPDATE) {
            await this._onPushSubscriptionChange();
          }
          break;
        case ONLOGOUT_NOTIFICATION:
          // user signed out, we need to stop polling the Push Server
          try {
            await this.unsubscribe();
          } catch (err) {
            this.log.error("Error during unsubscribe", err);
          }
          break;
      }
    } catch (err) {
      this.log.error(err);
    }
  },

  /**
   * Fired when the Push server sends a notification.
   *
   * @private
   * @returns {Promise}
   */
  async _onPushMessage(message) {
    this.log.trace("FxAccountsPushService _onPushMessage");
    if (!message.data) {
      // Use the empty signal to check the verification state of the account right away
      this.log.debug("empty push message - checking account status");
      this.fxai.checkVerificationStatus();
      return;
    }
    let payload = message.data.json();
    this.log.debug(`push command: ${payload.command}`);
    switch (payload.command) {
      case ON_COMMAND_RECEIVED_NOTIFICATION:
        await this.fxai.commands.pollDeviceCommands(payload.data.index);
        break;
      case ON_DEVICE_CONNECTED_NOTIFICATION:
        Services.obs.notifyObservers(
          null,
          ON_DEVICE_CONNECTED_NOTIFICATION,
          payload.data.deviceName
        );
        break;
      case ON_DEVICE_DISCONNECTED_NOTIFICATION:
        this.fxai._handleDeviceDisconnection(payload.data.id);
        return;
      case ON_PROFILE_UPDATED_NOTIFICATION:
        // We already have a "profile updated" notification sent via WebChannel,
        // let's just re-use that.
        Services.obs.notifyObservers(null, ON_PROFILE_CHANGE_NOTIFICATION);
        return;
      case ON_PASSWORD_CHANGED_NOTIFICATION:
      case ON_PASSWORD_RESET_NOTIFICATION:
        this._onPasswordChanged();
        return;
      case ON_ACCOUNT_DESTROYED_NOTIFICATION:
        this.fxai._handleAccountDestroyed(payload.data.uid);
        return;
      case ON_COLLECTION_CHANGED_NOTIFICATION:
        Services.obs.notifyObservers(
          null,
          ON_COLLECTION_CHANGED_NOTIFICATION,
          payload.data.collections
        );
        return;
      case ON_VERIFY_LOGIN_NOTIFICATION:
        Services.obs.notifyObservers(
          null,
          ON_VERIFY_LOGIN_NOTIFICATION,
          JSON.stringify(payload.data)
        );
        break;
      default:
        this.log.warn("FxA Push command unrecognized: " + payload.command);
    }
  },
  /**
   * Check the FxA session status after a password change/reset event.
   * If the session is invalid, reset credentials and notify listeners of
   * ON_ACCOUNT_STATE_CHANGE_NOTIFICATION that the account may have changed
   *
   * @returns {Promise}
   * @private
   */
  _onPasswordChanged() {
    return this.fxai.withCurrentAccountState(async state => {
      return this.fxai.checkAccountStatus(state);
    });
  },
  /**
   * Fired when the Push server drops a subscription, or the subscription identifier changes.
   *
   * https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Reference/Interface/nsIPushService#Receiving_Push_Messages
   *
   * @returns {Promise}
   * @private
   */
  _onPushSubscriptionChange() {
    this.log.trace("FxAccountsPushService _onPushSubscriptionChange");
    return this.fxai.updateDeviceRegistration();
  },
  /**
   * Unsubscribe from the Push server
   *
   * Ref: https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Reference/Interface/nsIPushService#unsubscribe()
   *
   * @returns {Promise}
   * @private
   */
  unsubscribe() {
    this.log.trace("FxAccountsPushService unsubscribe");
    return new Promise(resolve => {
      this.pushService.unsubscribe(
        FXA_PUSH_SCOPE_ACCOUNT_UPDATE,
        Services.scriptSecurityManager.getSystemPrincipal(),
        (result, ok) => {
          if (Components.isSuccessCode(result)) {
            if (ok === true) {
              this.log.debug("FxAccountsPushService unsubscribed");
            } else {
              this.log.debug(
                "FxAccountsPushService had no subscription to unsubscribe"
              );
            }
          } else {
            this.log.warn(
              "FxAccountsPushService failed to unsubscribe",
              result
            );
          }
          return resolve(ok);
        }
      );
    });
  },

  /**
   * Get our Push server subscription.
   *
   * Ref: https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Reference/Interface/nsIPushService#getSubscription()
   *
   * @returns {Promise}
   */
  getSubscription() {
    return new Promise(resolve => {
      this.pushService.getSubscription(
        FXA_PUSH_SCOPE_ACCOUNT_UPDATE,
        Services.scriptSecurityManager.getSystemPrincipal(),
        (result, subscription) => {
          if (!subscription) {
            this.log.info("FxAccountsPushService no subscription found");
            return resolve(null);
          }
          return resolve(subscription);
        }
      );
    });
  },
};

var EXPORTED_SYMBOLS = ["FxAccountsPushService"];
