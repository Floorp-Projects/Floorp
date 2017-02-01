/* jshint moz: true, esnext: true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");
const {
  PushCrypto,
  getCryptoParams,
} = Cu.import("resource://gre/modules/PushCrypto.jsm", {});

XPCOMUtils.defineLazyServiceGetter(this, "PushService",
  "@mozilla.org/push/Service;1", "nsIPushService");
XPCOMUtils.defineLazyGetter(this, "_decoder", () => new TextDecoder());

const FXA_PUSH_SCOPE = "chrome://fxa-push";
const Log = Cu.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog.bind("FxAccountsPush");

function FxAccountsPush() {
  Services.obs.addObserver(this, "FxAccountsPush:ReceivedPushMessageToDecode", false);

  EventDispatcher.instance.sendRequestForResult({
    type: "FxAccountsPush:Initialized"
  });
}

FxAccountsPush.prototype = {
  observe: function (subject, topic, data) {
    switch (topic) {
      case "android-push-service":
        if (data === "android-fxa-subscribe") {
          this._subscribe();
        } else if (data === "android-fxa-unsubscribe") {
          this._unsubscribe();
        }
        break;
      case "FxAccountsPush:ReceivedPushMessageToDecode":
        this._decodePushMessage(data);
        break;
    }
  },

  _subscribe() {
    Log.i("FxAccountsPush _subscribe");
    return new Promise((resolve, reject) => {
      PushService.subscribe(FXA_PUSH_SCOPE,
        Services.scriptSecurityManager.getSystemPrincipal(),
        (result, subscription) => {
          if (Components.isSuccessCode(result)) {
            Log.d("FxAccountsPush got subscription");
            resolve(subscription);
          } else {
            Log.w("FxAccountsPush failed to subscribe", result);
            reject(new Error("FxAccountsPush failed to subscribe"));
          }
        });
    })
    .then(subscription => {
      EventDispatcher.instance.sendRequest({
        type: "FxAccountsPush:Subscribe:Response",
        subscription: {
          pushCallback: subscription.endpoint,
          pushPublicKey: urlsafeBase64Encode(subscription.getKey('p256dh')),
          pushAuthKey: urlsafeBase64Encode(subscription.getKey('auth'))
        }
      });
    })
    .catch(err => {
      Log.i("Error when registering FxA push endpoint " + err);
    });
  },

  _unsubscribe() {
    Log.i("FxAccountsPush _unsubscribe");
    return new Promise((resolve) => {
      PushService.unsubscribe(FXA_PUSH_SCOPE,
        Services.scriptSecurityManager.getSystemPrincipal(),
        (result, ok) => {
          if (Components.isSuccessCode(result)) {
            if (ok === true) {
              Log.d("FxAccountsPush unsubscribed");
            } else {
              Log.d("FxAccountsPush had no subscription to unsubscribe");
            }
          } else {
            Log.w("FxAccountsPush failed to unsubscribe", result);
          }
          return resolve(ok);
        });
    }).catch(err => {
      Log.e("Error during unsubscribe", err);
    });
  },

  _decodePushMessage(data) {
    Log.i("FxAccountsPush _decodePushMessage");
    data = JSON.parse(data);
    let { headers, message } = this._messageAndHeaders(data);
    return new Promise((resolve, reject) => {
      PushService.getSubscription(FXA_PUSH_SCOPE,
        Services.scriptSecurityManager.getSystemPrincipal(),
        (result, subscription) => {
          if (!subscription) {
            return reject(new Error("No subscription found"));
          }
          return resolve(subscription);
        });
    }).then(subscription => {
      return PushCrypto.decrypt(subscription.p256dhPrivateKey,
                                new Uint8Array(subscription.getKey("p256dh")),
                                new Uint8Array(subscription.getKey("auth")),
                                headers, message);
    })
    .then(plaintext => {
      let decryptedMessage = plaintext ? _decoder.decode(plaintext) : "";
      EventDispatcher.instance.sendRequestForResult({
        type: "FxAccountsPush:ReceivedPushMessageToDecode:Response",
        message: decryptedMessage
      });
    })
    .catch(err => {
      Log.d("Error while decoding incoming message : " + err);
    });
  },

  // Copied from PushServiceAndroidGCM
  _messageAndHeaders(data) {
    // Default is no data (and no encryption).
    let message = null;
    let headers = null;

    if (data.message && data.enc && (data.enckey || data.cryptokey)) {
      headers = {
        encryption_key: data.enckey,
        crypto_key: data.cryptokey,
        encryption: data.enc,
        encoding: data.con,
      };
      // Ciphertext is (urlsafe) Base 64 encoded.
      message = ChromeUtils.base64URLDecode(data.message, {
        // The Push server may append padding.
        padding: "ignore",
      });
    }
    return { headers, message };
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  classID: Components.ID("{d1bbb0fd-1d47-4134-9c12-d7b1be20b721}")
};

function urlsafeBase64Encode(key) {
  return ChromeUtils.base64URLEncode(new Uint8Array(key), { pad: false });
}

var components = [ FxAccountsPush ];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
