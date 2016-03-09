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
   */
  observe(subject, topic, data) {
   this.log.trace(`observed topic=${topic}, data=${data}, subject=${subject}`);
   switch (topic) {
     case this.pushService.pushTopic:
       if (data === FXA_PUSH_SCOPE_ACCOUNT_UPDATE) {
         this._onPushMessage();
       }
       break;
     case this.pushService.subscriptionChangeTopic:
       if (data === FXA_PUSH_SCOPE_ACCOUNT_UPDATE) {
         this._onPushSubscriptionChange();
       }
       break;
     case ONLOGOUT_NOTIFICATION:
       // user signed out, we need to stop polling the Push Server
       this.unsubscribe().catch(err => {
         this.log.error("Error during unsubscribe", err);
       });
       break;
     default:
       break;
   }
  },
  /**
   * Fired when the Push server sends a notification.
   *
   * @private
   */
  _onPushMessage() {
    this.log.trace("FxAccountsPushService _onPushMessage");
    // Use this signal to check the verification state of the account right away
    this.fxAccounts.checkVerificationStatus();
  },
  /**
   * Fired when the Push server drops a subscription, or the subscription identifier changes.
   *
   * https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Reference/Interface/nsIPushService#Receiving_Push_Messages
   *
   * @private
   */
  _onPushSubscriptionChange() {
    this.log.trace("FxAccountsPushService _onPushSubscriptionChange");
    this.fxAccounts.updateDeviceRegistration();
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
        })
    })
  },
};

// Service registration below registers with FxAccountsComponents.manifest
const components = [FxAccountsPushService];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);

// The following registration below helps with testing this service.
this.EXPORTED_SYMBOLS=["FxAccountsPushService"];
