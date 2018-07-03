/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["SendTab", "FxAccountsCommands"];

ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js");
ChromeUtils.import("resource://gre/modules/Preferences.jsm");
ChromeUtils.defineModuleGetter(this, "PushCrypto",
  "resource://gre/modules/PushCrypto.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://services-common/observers.js");

XPCOMUtils.defineLazyModuleGetters(this, {
  BulkKeyBundle: "resource://services-sync/keys.js",
  CommonUtils: "resource://services-common/utils.js",
  CryptoUtils: "resource://services-crypto/utils.js",
  CryptoWrapper: "resource://services-sync/record.js",
});

class FxAccountsCommands {
  constructor(fxAccounts) {
    this._fxAccounts = fxAccounts;
    this.sendTab = new SendTab(this, fxAccounts);
  }

  async invoke(command, device, payload) {
    const userData = await this._fxAccounts.getSignedInUser();
    if (!userData) {
      throw new Error("No user.");
    }
    const {sessionToken} = userData;
    if (!sessionToken) {
      throw new Error("_send called without a session token.");
    }
    const client = this._fxAccounts.getAccountsClient();
    await client.invokeCommand(sessionToken, command, device.id, payload);
    log.info(`Payload sent to device ${device.id}.`);
  }

  async consumeRemoteCommand(index) {
    if (!Services.prefs.getBoolPref("identity.fxaccounts.commands.enabled", true)) {
      return false;
    }
    log.info(`Consuming command with index ${index}.`);
    const {messages} = await this._fetchRemoteCommands(index, 1);
    if (messages.length != 1) {
      log.warn(`Should have retrieved 1 and only 1 message, got ${messages.length}.`);
    }
    return this._fxAccounts._withCurrentAccountState(async (getUserData, updateUserData) => {
      const {device} = await getUserData(["device"]);
      if (!device) {
        throw new Error("No device registration.");
      }
      const handledCommands = (device.handledCommands || []).concat(messages.map(m => m.index));
      await updateUserData({
        device: {...device, handledCommands}
      });
      await this._handleCommands(messages);

      // Once the handledCommands array length passes a threshold, check the
      // potentially missed remote commands in order to clear it.
      if (handledCommands.length > 20) {
        await this.fetchMissedRemoteCommands();
      }
    });
  }

  fetchMissedRemoteCommands() {
    if (!Services.prefs.getBoolPref("identity.fxaccounts.commands.enabled", true)) {
      return false;
    }
    log.info(`Consuming missed commands.`);
    return this._fxAccounts._withCurrentAccountState(async (getUserData, updateUserData) => {
      const {device} = await getUserData(["device"]);
      if (!device) {
        throw new Error("No device registration.");
      }
      const lastCommandIndex = device.lastCommandIndex || 0;
      const handledCommands = device.handledCommands || [];
      handledCommands.push(lastCommandIndex); // Because the server also returns this command.
      const {index, messages} = await this._fetchRemoteCommands(lastCommandIndex);
      const missedMessages = messages.filter(m => !handledCommands.includes(m.index));
      await updateUserData({
        device: {...device, lastCommandIndex: index, handledCommands: []}
      });
      if (missedMessages.length) {
        log.info(`Handling ${missedMessages.length} missed messages`);
        await this._handleCommands(missedMessages);
      }
    });
  }

  async _fetchRemoteCommands(index, limit = null) {
    const userData = await this._fxAccounts.getSignedInUser();
    if (!userData) {
      throw new Error("No user.");
    }
    const {sessionToken} = userData;
    if (!sessionToken) {
      throw new Error("No session token.");
    }
    const client = this._fxAccounts.getAccountsClient();
    const opts = {index};
    if (limit != null) {
      opts.limit = limit;
    }
    return client.getCommands(sessionToken, opts);
  }

  async _handleCommands(messages) {
    const fxaDevices = await this._fxAccounts.getDeviceList();
    for (const {data} of messages) {
      let {command, payload, sender} = data;
      if (sender) {
        sender = fxaDevices.find(d => d.id == sender);
      }
      switch (command) {
        case COMMAND_SENDTAB:
          try {
            await this.sendTab.handle(sender, payload);
          } catch (e) {
            log.error(`Error while handling incoming Send Tab payload.`, e);
          }
        default:
          log.info(`Unknown command: ${command}.`);
      }
    }
  }
}

/**
 * Send Tab is built on top of FxA commands.
 *
 * Devices exchange keys wrapped in kSync between themselves (getEncryptedKey)
 * during the device registration flow. The FxA server can theorically never
 * retrieve the send tab keys since it doesn't know kSync.
 */
class SendTab {
  constructor(commands, fxAccounts) {
    this._commands = commands;
    this._fxAccounts = fxAccounts;
  }
  /**
   * @param {Device[]} to - Device objects (typically returned by fxAccounts.getDevicesList()).
   * @param {Object} tab
   * @param {string} tab.url
   * @param {string} tab.title
   * @returns A report object, in the shape of
   *          {succeded: [Device], error: [{device: Device, error: Exception}]}
   */
  async send(to, tab) {
    log.info(`Sending a tab to ${to.length} devices.`);
    const encoder = new TextEncoder("utf8");
    const data = {
      entries: [{title: tab.title, url: tab.url}]
    };
    const bytes = encoder.encode(JSON.stringify(data));
    const report = {
      succeeded: [],
      failed: [],
    };
    for (let device of to) {
      try {
        const encrypted = await this._encrypt(bytes, device);
        const payload = {encrypted};
        await this._commands.invoke(COMMAND_SENDTAB, device, payload); // FxA needs an object.
        report.succeeded.push(device);
      } catch (error) {
        log.error("Error while invoking a send tab command.", error);
        report.failed.push({device, error});
      }
    }
    return report;
  }

  // Returns true if the target device is compatible with FxA Commands Send tab.
  async isDeviceCompatible(device) {
    if (!Services.prefs.getBoolPref("identity.fxaccounts.commands.enabled", true) ||
        !device.availableCommands || !device.availableCommands[COMMAND_SENDTAB]) {
      return false;
    }
    const {kid: theirKid} = JSON.parse(device.availableCommands[COMMAND_SENDTAB]);
    const ourKid = await this._getKid();
    return theirKid == ourKid;
  }

  // Handle incoming send tab payload, called by FxAccountsCommands.
  async handle(sender, {encrypted}) {
    if (!sender) {
      log.warn("Incoming tab is from an unknown device (maybe disconnected?)");
    }
    const bytes = await this._decrypt(encrypted);
    const decoder = new TextDecoder("utf8");
    const data = JSON.parse(decoder.decode(bytes));
    const current = data.hasOwnProperty("current") ? data.current :
                                                     data.entries.length - 1;
    const tabSender = {
      id: sender ? sender.id : "",
      name: sender ? sender.name : ""
    };
    const {title, url: uri} = data.entries[current];
    console.log(`Tab received with FxA commands: ${title} from ${tabSender.name}.`);
    Observers.notify("fxaccounts:commands:open-uri", [{uri, title, sender: tabSender}]);
  }

  async _getKid() {
    let {kXCS} = await this._fxAccounts.getKeys();
    return kXCS;
  }

  async _encrypt(bytes, device) {
    let bundle = device.availableCommands[COMMAND_SENDTAB];
    if (!bundle) {
      throw new Error(`Device ${device.id} does not have send tab keys.`);
    }
    const json = JSON.parse(bundle);
    const wrapper = new CryptoWrapper();
    wrapper.deserialize({payload: json});
    const {kSync} = await this._fxAccounts.getKeys();
    const syncKeyBundle = BulkKeyBundle.fromHexKey(kSync);
    let {publicKey, authSecret} = await wrapper.decrypt(syncKeyBundle);
    authSecret = urlsafeBase64Decode(authSecret);
    publicKey = urlsafeBase64Decode(publicKey);

    const {ciphertext: encrypted} = await PushCrypto.encrypt(bytes, publicKey, authSecret);
    return urlsafeBase64Encode(encrypted);
  }

  async _getKeys() {
    const {device} = await this._fxAccounts.getSignedInUser();
    return device && device.sendTabKeys;
  }

  async _decrypt(ciphertext) {
    let {privateKey, publicKey, authSecret} = await this._getKeys();
    publicKey = urlsafeBase64Decode(publicKey);
    authSecret = urlsafeBase64Decode(authSecret);
    ciphertext = new Uint8Array(urlsafeBase64Decode(ciphertext));
    return PushCrypto.decrypt(privateKey, publicKey, authSecret,
                              // The only Push encoding we support.
                              {encoding: "aes128gcm"}, ciphertext);
  }

  async _generateAndPersistKeys() {
    let [publicKey, privateKey] = await PushCrypto.generateKeys();
    publicKey = urlsafeBase64Encode(publicKey);
    let authSecret = PushCrypto.generateAuthenticationSecret();
    authSecret = urlsafeBase64Encode(authSecret);
    const sendTabKeys = {
      publicKey,
      privateKey,
      authSecret
    };
    await this._fxAccounts._withCurrentAccountState(async (getUserData, updateUserData) => {
      const {device} = await getUserData();
      await updateUserData({
        device: {
          ...device,
          sendTabKeys,
        }
      });
    });
    return sendTabKeys;
  }

  async getEncryptedKey() {
    let sendTabKeys = await this._getKeys();
    if (!sendTabKeys) {
      sendTabKeys = await this._generateAndPersistKeys();
    }
    // Strip the private key from the bundle to encrypt.
    const keyToEncrypt = {
      publicKey: sendTabKeys.publicKey,
      authSecret: sendTabKeys.authSecret,
    };
    const {kSync} = await this._fxAccounts.getSignedInUser();
    if (!kSync) {
      return null;
    }
    const wrapper = new CryptoWrapper();
    wrapper.cleartext = keyToEncrypt;
    const keyBundle = BulkKeyBundle.fromHexKey(kSync);
    await wrapper.encrypt(keyBundle);
    const kid = await this._getKid();
    return JSON.stringify({
      kid,
      IV: wrapper.IV,
      hmac: wrapper.hmac,
      ciphertext: wrapper.ciphertext,
    });
  }
}

function urlsafeBase64Encode(buffer) {
  return ChromeUtils.base64URLEncode(new Uint8Array(buffer), {pad: false});
}

function urlsafeBase64Decode(str) {
  return ChromeUtils.base64URLDecode(str, {padding: "reject"});
}
