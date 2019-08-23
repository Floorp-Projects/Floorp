/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

const { debug, warn } = GeckoViewUtils.initLogging("GeckoViewPush"); // eslint-disable-line no-unused-vars

ChromeUtils.defineModuleGetter(
  this,
  "EventDispatcher",
  "resource://gre/modules/Messaging.jsm"
);

// Observer notification topics for push messages and subscription status
// changes. These are duplicated and used in `nsIPushNotifier`. They're exposed
// on `nsIPushService` so that JS callers only need to import this service.
const OBSERVER_TOPIC_PUSH = "push-message";
const OBSERVER_TOPIC_SUBSCRIPTION_CHANGE = "push-subscription-change";
const OBSERVER_TOPIC_SUBSCRIPTION_MODIFIED = "push-subscription-modified";

function createSubscription({
  scope,
  principal,
  browserPublicKey,
  authSecret,
  endpoint,
  appServerKey,
}) {
  const decodedBrowserKey = ChromeUtils.base64URLDecode(browserPublicKey, {
    padding: "ignore",
  });
  const decodedAuthSecret = ChromeUtils.base64URLDecode(authSecret, {
    padding: "ignore",
  });

  return new PushSubscription({
    endpoint: endpoint,
    scope,
    p256dhKey: decodedBrowserKey,
    authenticationSecret: decodedAuthSecret,
    appServerKey,
  });
}

function scopeWithAttrs(scope, attrs) {
  return scope + ChromeUtils.originAttributesToSuffix(attrs);
}

function PushService() {
  this.wrappedJSObject = this;
}

PushService.prototype = {
  classID: Components.ID("{a54d84d7-98a4-4fec-b664-e42e512ae9cc}"),
  contractID: "@mozilla.org/push/Service;1",
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
    Ci.nsIPushService,
    Ci.nsIPushQuotaManager,
    Ci.nsIPushErrorReporter,
  ]),

  pushTopic: OBSERVER_TOPIC_PUSH,
  subscriptionChangeTopic: OBSERVER_TOPIC_SUBSCRIPTION_CHANGE,
  subscriptionModifiedTopic: OBSERVER_TOPIC_SUBSCRIPTION_MODIFIED,

  // nsIPushService methods

  subscribe(scope, principal, callback) {
    this.subscribeWithKey(scope, principal, null, callback);
  },

  async subscribeWithKey(scope, principal, appServerKey, callback) {
    try {
      const response = await EventDispatcher.instance.sendRequestForResult({
        type: "GeckoView:PushSubscribe",
        scope: scopeWithAttrs(scope, principal.originAttributes),
        appServerKey: appServerKey
          ? ChromeUtils.base64URLEncode(new Uint8Array(appServerKey), {
              pad: true,
            })
          : null,
      });

      let subscription = null;
      if (response) {
        subscription = createSubscription({
          ...response,
          scope,
          principal,
          appServerKey,
        });
      }

      callback.onPushSubscription(Cr.NS_OK, subscription);
    } catch (e) {
      callback.onPushSubscription(Cr.NS_ERROR_FAILURE, null);
    }
  },

  async unsubscribe(scope, principal, callback) {
    try {
      await EventDispatcher.instance.sendRequestForResult({
        type: "GeckoView:PushUnsubscribe",
        scope: scopeWithAttrs(scope, principal.originAttributes),
      });

      callback.onUnsubscribe(Cr.NS_OK, true);
    } catch (e) {
      callback.onUnsubscribe(Cr.NS_ERROR_FAILURE, false);
    }
  },

  async getSubscription(scope, principal, callback) {
    try {
      const response = await EventDispatcher.instance.sendRequestForResult({
        type: "GeckoView:PushGetSubscription",
        scope: scopeWithAttrs(scope, principal.originAttributes),
      });

      let subscription = null;
      if (response) {
        subscription = createSubscription({
          ...response,
          scope,
          principal,
        });
      }

      callback.onPushSubscription(Cr.NS_OK, subscription);
    } catch (e) {
      callback.onPushSubscription(Cr.NS_ERROR_FAILURE, null);
    }
  },

  clearForDomain(domain, callback) {
    callback.onClear(Cr.NS_OK);
  },

  // nsIPushQuotaManager methods

  notificationForOriginShown(origin) {},

  notificationForOriginClosed(origin) {},

  // nsIPushErrorReporter methods

  reportDeliveryError(messageId, reason) {},
};

/** `PushSubscription` instances are passed to all subscription callbacks. */
function PushSubscription(props) {
  this._props = props;
}

PushSubscription.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPushSubscription]),

  /** The URL for sending messages to this subscription. */
  get endpoint() {
    return this._props.endpoint;
  },

  /** The last time a message was sent to this subscription. */
  get lastPush() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  /** The total number of messages sent to this subscription. */
  get pushCount() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  /**
   * The app will take care of throttling, so we don't
   * care about the quota stuff here.
   */
  get quota() {
    return -1;
  },

  /**
   * Indicates whether this subscription was created with the system principal.
   * System subscriptions are exempt from the background message quota and
   * permission checks.
   */
  get isSystemSubscription() {
    return false;
  },

  /** The private key used to decrypt incoming push messages, in JWK format */
  get p256dhPrivateKey() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  /**
   * Indicates whether this subscription is subject to the background message
   * quota.
   */
  quotaApplies() {
    return false;
  },

  /**
   * Indicates whether this subscription exceeded the background message quota,
   * or the user revoked the notification permission. The caller must request a
   * new subscription to continue receiving push messages.
   */
  isExpired() {
    return false;
  },

  /**
   * Returns a key for encrypting messages sent to this subscription. JS
   * callers receive the key buffer as a return value, while C++ callers
   * receive the key size and buffer as out parameters.
   */
  getKey(name) {
    switch (name) {
      case "p256dh":
        return this._getRawKey(this._props.p256dhKey);

      case "auth":
        return this._getRawKey(this._props.authenticationSecret);

      case "appServer":
        return this._getRawKey(this._props.appServerKey);
    }
    return [];
  },

  _getRawKey(key) {
    if (!key) {
      return [];
    }
    return new Uint8Array(key);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PushService]);
