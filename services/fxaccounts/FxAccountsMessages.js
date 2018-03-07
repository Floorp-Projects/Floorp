/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["FxAccountsMessages", /* the rest are for testing only */
                          "FxAccountsMessagesSender", "FxAccountsMessagesReceiver",
                          "FxAccountsMessagesHandler"];

ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js");
ChromeUtils.import("resource://gre/modules/Preferences.jsm");
ChromeUtils.defineModuleGetter(this, "PushCrypto",
  "resource://gre/modules/PushCrypto.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://services-common/observers.js");

const AES128GCM_ENCODING = "aes128gcm"; // The only Push encoding we support.
const TOPICS = {
  SEND_TAB: "sendtab"
};

class FxAccountsMessages {
  constructor(fxAccounts, options = {}) {
    this.fxAccounts = fxAccounts;
    this.sender = options.sender || new FxAccountsMessagesSender(fxAccounts);
    this.receiver = options.receiver || new FxAccountsMessagesReceiver(fxAccounts);
  }

  _isDeviceMessagesAware(device) {
    return device.capabilities && device.capabilities.includes(CAPABILITY_MESSAGES);
  }

  canReceiveSendTabMessages(device) {
    return this._isDeviceMessagesAware(device) &&
           device.capabilities.includes(CAPABILITY_MESSAGES_SENDTAB);
  }

  consumeRemoteMessages() {
    if (!Services.prefs.getBoolPref("identity.fxaccounts.messages.enabled", true)) {
      return Promise.resolve();
    }
    return this.receiver.consumeRemoteMessages();
  }

  /**
   * @param {Device[]} to - Device objects (typically returned by fxAccounts.getDevicesList()).
   * @param {Object} tab
   * @param {string} tab.url
   * @param {string} tab.title
   */
  async sendTab(to, tab) {
    log.info(`Sending a tab to ${to.length} devices.`);
    const ourDeviceId = await this.fxAccounts.getDeviceId();
    const payload = {
      topic: TOPICS.SEND_TAB,
      data: {
        from: ourDeviceId,
        entries: [{title: tab.title, url: tab.url}]
      }
    };
    return this.sender.send(TOPICS.SEND_TAB, to, payload);
  }
}

class FxAccountsMessagesSender {
  constructor(fxAccounts) {
    this.fxAccounts = fxAccounts;
  }

  async send(topic, to, data) {
    const userData = await this.fxAccounts.getSignedInUser();
    if (!userData) {
      throw new Error("No user.");
    }
    const {sessionToken} = userData;
    if (!sessionToken) {
      throw new Error("_send called without a session token.");
    }
    const encoder = new TextEncoder("utf8");
    const client = this.fxAccounts.getAccountsClient();
    for (const device of to) {
      try {
        const bytes = encoder.encode(JSON.stringify(data));
        const payload = await this._encrypt(bytes, device, encoder);
        await client.sendMessage(sessionToken, topic, device.id, payload);
        log.info(`Payload sent to device ${device.id}.`);
      } catch (e) {
        log.error(`Could not send data to device ${device.id}.`, e);
      }
    }
  }

  async _encrypt(bytes, device) {
    let {pushPublicKey, pushAuthKey} = device;
    if (!pushPublicKey || !pushAuthKey) {
      throw new Error(`Device ${device.id} does not have push keys.`);
    }
    pushPublicKey = ChromeUtils.base64URLDecode(pushPublicKey, {padding: "ignore"});
    pushAuthKey = ChromeUtils.base64URLDecode(pushAuthKey, {padding: "ignore"});
    const {ciphertext} = await PushCrypto.encrypt(bytes, pushPublicKey, pushAuthKey);
    return ChromeUtils.base64URLEncode(ciphertext, {pad: false});
  }
}

class FxAccountsMessagesReceiver {
  constructor(fxAccounts, options = {}) {
    this.fxAccounts = fxAccounts;
    this.handler = options.handler || new FxAccountsMessagesHandler(this.fxAccounts);
  }

  async consumeRemoteMessages() {
    log.info(`Consuming unread messages.`);
    const messages = await this._fetchMessages();
    if (!messages || !messages.length) {
      log.info(`No new messages.`);
      return;
    }
    const decoder = new TextDecoder("utf8");
    const keys = await this._getOwnKeys();
    const payloads = [];
    for (const {index, data} of messages) {
      try {
        const bytes = await this._decrypt(data, keys);
        const payload = JSON.parse(decoder.decode(bytes));
        payloads.push(payload);
      } catch (e) {
        log.error(`Could not unwrap message ${index}`, e);
      }
    }
    if (payloads.length) {
      await this.handler.handle(payloads);
    }
  }

  async _fetchMessages() {
    return this.fxAccounts._withCurrentAccountState(async (getUserData, updateUserData) => {
      const userData = await getUserData(["sessionToken", "device"]);
      if (!userData) {
        throw new Error("No user.");
      }
      const {sessionToken, device} = userData;
      if (!sessionToken) {
        throw new Error("No session token.");
      }
      if (!device) {
        throw new Error("No device registration.");
      }
      const opts = {};
      if (device.messagesIndex) {
        opts.index = device.messagesIndex;
      }
      const client = this.fxAccounts.getAccountsClient();
      log.info(`Fetching unread messages with ${JSON.stringify(opts)}.`);
      const {index: newIndex, messages} = await client.getMessages(sessionToken, opts);
      await updateUserData({
        device: {...device, messagesIndex: newIndex}
      });
      return messages;
    });
  }

  async _getOwnKeys() {
    const subscription = await this.fxAccounts.getPushSubscription();
    return {
      pushPrivateKey: subscription.p256dhPrivateKey,
      pushPublicKey: new Uint8Array(subscription.getKey("p256dh")),
      pushAuthKey: new Uint8Array(subscription.getKey("auth"))
    };
  }

  async _decrypt(ciphertext, {pushPrivateKey, pushPublicKey, pushAuthKey}) {
    ciphertext = ChromeUtils.base64URLDecode(ciphertext, {padding: "reject"});
    return PushCrypto.decrypt(pushPrivateKey, pushPublicKey,
                              pushAuthKey,
                              {encoding: AES128GCM_ENCODING},
                              ciphertext);
  }
}

class FxAccountsMessagesHandler {
  constructor(fxAccounts) {
    this.fxAccounts = fxAccounts;
  }

  async handle(payloads) {
    const sendTabPayloads = [];
    for (const payload of payloads) {
      switch (payload.topic) {
        case TOPICS.SEND_TAB:
          sendTabPayloads.push(payload.data);
        default:
          log.info(`Unknown messages topic: ${payload.topic}.`);
      }
    }

    // Only one type of payload so far!
    if (sendTabPayloads.length) {
      await this._handleSendTabPayloads(sendTabPayloads);
    }
  }

  async _handleSendTabPayloads(payloads) {
    const toDisplay = [];
    const fxaDevices = await this.fxAccounts.getDeviceList();
    for (const payload of payloads) {
      const current = payload.hasOwnProperty("current") ? payload.current :
                                                          payload.entries.length - 1;
      const device = fxaDevices.find(d => d.id == payload.from);
      if (!device) {
        log.warn("Incoming tab is from an unknown device (maybe disconnected?)");
      }
      const sender = {
        id: device ? device.id : "",
        name: device ? device.name : ""
      };
      const {title, url: uri} = payload.entries[current];
      toDisplay.push({uri, title, sender});
    }

    Observers.notify("fxaccounts:messages:display-tabs", toDisplay);
  }
}

