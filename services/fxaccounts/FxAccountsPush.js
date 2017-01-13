/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/Task.jsm");

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
   * Instance of FxAccounts or a mocked object.
   */
  fxAccounts: null,
  /**
   * Component ID of this service, helps register this component.
   */
  classID: Components.ID("{1b7db999-2ecd-4abf-bb95-a726896798ca}"),
  /**
   * Register used interfaces in this service
   */
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
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
      this.pushService = Cc["@mozilla.org/push/Service;1"].getService(Ci.nsIPushService);
    }

    if (options.fxAccounts) {
      this.fxAccounts = options.fxAccounts;
    } else {
      XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
        "resource://gre/modules/FxAccounts.jsm");
    }

    // listen to new push messages, push changes and logout events
    Services.obs.addObserver(this, this.pushService.pushTopic, false);
    Services.obs.addObserver(this, this.pushService.subscriptionChangeTopic, false);
    Services.obs.addObserver(this, ONLOGOUT_NOTIFICATION, false);

    this.log.debug("FxAccountsPush initialized");
  },
  /**
   * Registers a new endpoint with the Push Server
   *
   * @returns {Promise}
   *          Promise always resolves with a subscription or a null if failed to subscribe.
   */
  registerPushEndpoint() {
    this.log.trace("FxAccountsPush registerPushEndpoint");

    return new Promise((resolve) => {
      this.pushService.subscribe(FXA_PUSH_SCOPE_ACCOUNT_UPDATE,
        Services.scriptSecurityManager.getSystemPrincipal(),
        (result, subscription) => {
          if (Components.isSuccessCode(result)) {
            this.log.debug("FxAccountsPush got subscription");
            resolve(subscription);
          } else {
            this.log.warn("FxAccountsPush failed to subscribe", result);
            resolve(null);
          }
        });
    });
  },
  /**
   * Standard observer interface to listen to push messages, changes and logout.
   *
   * @param subject
   * @param topic
   * @param data
   * @returns {Promise}
   */
  _observe(subject, topic, data) {
    this.log.trace(`observed topic=${topic}, data=${data}, subject=${subject}`);
    switch (topic) {
      case this.pushService.pushTopic:
        if (data === FXA_PUSH_SCOPE_ACCOUNT_UPDATE) {
          let message = subject.QueryInterface(Ci.nsIPushMessage);
          return this._onPushMessage(message);
        }
        break;
      case this.pushService.subscriptionChangeTopic:
        if (data === FXA_PUSH_SCOPE_ACCOUNT_UPDATE) {
          return this._onPushSubscriptionChange();
        }
        break;
      case ONLOGOUT_NOTIFICATION:
        // user signed out, we need to stop polling the Push Server
        return this.unsubscribe().catch(err => {
          this.log.error("Error during unsubscribe", err);
        });
        break;
      default:
        break;
    }
  },
  /**
   * Wrapper around _observe that catches errors
   */
  observe(subject, topic, data) {
    Promise.resolve()
      .then(() => this._observe(subject, topic, data))
      .catch(err => this.log.error(err));
  },
  /**
   * Fired when the Push server sends a notification.
   *
   * @private
   * @returns {Promise}
   */
  _onPushMessage(message) {
    this.log.trace("FxAccountsPushService _onPushMessage");
    if (!message.data) {
      // Use the empty signal to check the verification state of the account right away
      this.log.debug("empty push message - checking account status");
      return this.fxAccounts.checkVerificationStatus();
    }
    let payload = message.data.json();
    this.log.debug(`push command: ${payload.command}`);
    switch (payload.command) {
      case ON_DEVICE_CONNECTED_NOTIFICATION:
        Services.obs.notifyObservers(null, ON_DEVICE_CONNECTED_NOTIFICATION, payload.data.deviceName);
        break;
      case ON_DEVICE_DISCONNECTED_NOTIFICATION:
        return this.fxAccounts.handleDeviceDisconnection(payload.data.id);
        break;
      case ON_PASSWORD_CHANGED_NOTIFICATION:
      case ON_PASSWORD_RESET_NOTIFICATION:
        return this._onPasswordChanged();
        break;
      case ON_COLLECTION_CHANGED_NOTIFICATION:
        Services.obs.notifyObservers(null, ON_COLLECTION_CHANGED_NOTIFICATION, payload.data.collections);
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
  _onPasswordChanged: Task.async(function* () {
    if (!(yield this.fxAccounts.sessionStatus())) {
      yield this.fxAccounts.resetCredentials();
      Services.obs.notifyObservers(null, ON_ACCOUNT_STATE_CHANGE_NOTIFICATION, null);
    }
  }),
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
    return this.fxAccounts.updateDeviceRegistration();
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
    return new Promise((resolve) => {
      this.pushService.unsubscribe(FXA_PUSH_SCOPE_ACCOUNT_UPDATE,
        Services.scriptSecurityManager.getSystemPrincipal(),
        (result, ok) => {
          if (Components.isSuccessCode(result)) {
            if (ok === true) {
              this.log.debug("FxAccountsPushService unsubscribed");
            } else {
              this.log.debug("FxAccountsPushService had no subscription to unsubscribe");
            }
          } else {
            this.log.warn("FxAccountsPushService failed to unsubscribe", result);
          }
          return resolve(ok);
        });
    });
  },
};

// Service registration below registers with FxAccountsComponents.manifest
const components = [FxAccountsPushService];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);

// The following registration below helps with testing this service.
this.EXPORTED_SYMBOLS=["FxAccountsPushService"];
