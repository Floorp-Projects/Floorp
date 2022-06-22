/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["SendTab", "FxAccountsCommands"];

const {
  COMMAND_SENDTAB,
  COMMAND_SENDTAB_TAIL,
  SCOPE_OLD_SYNC,
  log,
} = ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js");
const lazy = {};
ChromeUtils.defineModuleGetter(
  lazy,
  "PushCrypto",
  "resource://gre/modules/PushCrypto.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Observers } = ChromeUtils.import(
  "resource://services-common/observers.js"
);

XPCOMUtils.defineLazyModuleGetters(lazy, {
  BulkKeyBundle: "resource://services-sync/keys.js",
  CryptoWrapper: "resource://services-sync/record.js",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "INVALID_SHAREABLE_SCHEMES",
  "services.sync.engine.tabs.filteredSchemes",
  "",
  null,
  val => {
    return new Set(val.split("|"));
  }
);

class FxAccountsCommands {
  constructor(fxAccountsInternal) {
    this._fxai = fxAccountsInternal;
    this.sendTab = new SendTab(this, fxAccountsInternal);
    this._invokeRateLimitExpiry = 0;
  }

  async availableCommands() {
    if (
      !Services.prefs.getBoolPref("identity.fxaccounts.commands.enabled", true)
    ) {
      return {};
    }
    const encryptedSendTabKeys = await this.sendTab.getEncryptedSendTabKeys();
    if (!encryptedSendTabKeys) {
      // This will happen if the account is not verified yet.
      return {};
    }
    return {
      [COMMAND_SENDTAB]: encryptedSendTabKeys,
    };
  }

  async invoke(command, device, payload) {
    const { sessionToken } = await this._fxai.getUserAccountData([
      "sessionToken",
    ]);
    const client = this._fxai.fxAccountsClient;
    const now = Date.now();
    if (now < this._invokeRateLimitExpiry) {
      const remaining = (this._invokeRateLimitExpiry - now) / 1000;
      throw new Error(
        `Invoke for ${command} is rate-limited for ${remaining} seconds.`
      );
    }
    try {
      let info = await client.invokeCommand(
        sessionToken,
        command,
        device.id,
        payload
      );
      if (!info.enqueued || !info.notified) {
        // We want an error log here to help diagnose users who report failure.
        log.error("Sending was only partially successful", info);
      } else {
        log.info("Successfully sent", info);
      }
    } catch (err) {
      if (err.code && err.code === 429 && err.retryAfter) {
        this._invokeRateLimitExpiry = Date.now() + err.retryAfter * 1000;
      }
      throw err;
    }
    log.info(`Payload sent to device ${device.id}.`);
  }

  /**
   * Poll and handle device commands for the current device.
   * This method can be called either in response to a Push message,
   * or by itself as a "commands recovery" mechanism.
   *
   * @param {Number} notifiedIndex "Command received" push messages include
   * the index of the command that triggered the message. We use it as a
   * hint when we have no "last command index" stored.
   */
  async pollDeviceCommands(notifiedIndex = 0) {
    // Whether the call to `pollDeviceCommands` was initiated by a Push message from the FxA
    // servers in response to a message being received or simply scheduled in order
    // to fetch missed messages.
    if (
      !Services.prefs.getBoolPref("identity.fxaccounts.commands.enabled", true)
    ) {
      return false;
    }
    log.info(`Polling device commands.`);
    await this._fxai.withCurrentAccountState(async state => {
      const { device } = await state.getUserAccountData(["device"]);
      if (!device) {
        throw new Error("No device registration.");
      }
      // We increment lastCommandIndex by 1 because the server response includes the current index.
      // If we don't have a `lastCommandIndex` stored, we fall back on the index from the push message we just got.
      const lastCommandIndex = device.lastCommandIndex + 1 || notifiedIndex;
      // We have already received this message before.
      if (notifiedIndex > 0 && notifiedIndex < lastCommandIndex) {
        return;
      }
      const { index, messages } = await this._fetchDeviceCommands(
        lastCommandIndex
      );
      if (messages.length) {
        await state.updateUserAccountData({
          device: { ...device, lastCommandIndex: index },
        });
        log.info(`Handling ${messages.length} messages`);
        await this._handleCommands(messages, notifiedIndex);
      }
    });
    return true;
  }

  async _fetchDeviceCommands(index, limit = null) {
    const userData = await this._fxai.getUserAccountData();
    if (!userData) {
      throw new Error("No user.");
    }
    const { sessionToken } = userData;
    if (!sessionToken) {
      throw new Error("No session token.");
    }
    const client = this._fxai.fxAccountsClient;
    const opts = { index };
    if (limit != null) {
      opts.limit = limit;
    }
    return client.getCommands(sessionToken, opts);
  }

  _getReason(notifiedIndex, messageIndex) {
    // The returned reason value represents an explanation for why the command associated with the
    // message of the given `messageIndex` is being handled. If `notifiedIndex` is zero the command
    // is a part of a fallback polling process initiated by "Sync Now" ["poll"]. If `notifiedIndex` is
    // greater than `messageIndex` this is a push command that was previously missed ["push-missed"],
    // otherwise we assume this is a push command with no missed messages ["push"].
    if (notifiedIndex == 0) {
      return "poll";
    } else if (notifiedIndex > messageIndex) {
      return "push-missed";
    }
    // Note: The returned reason might be "push" in the case where a user sends multiple tabs
    // in quick succession. We are not attempting to distinguish this from other push cases at
    // present.
    return "push";
  }

  async _handleCommands(messages, notifiedIndex) {
    try {
      await this._fxai.device.refreshDeviceList();
    } catch (e) {
      log.warn("Error refreshing device list", e);
    }
    // We debounce multiple incoming tabs so we show a single notification.
    const tabsReceived = [];
    for (const { index, data } of messages) {
      const { command, payload, sender: senderId } = data;
      const reason = this._getReason(notifiedIndex, index);
      const sender =
        senderId && this._fxai.device.recentDeviceList
          ? this._fxai.device.recentDeviceList.find(d => d.id == senderId)
          : null;
      if (!sender) {
        log.warn(
          "Incoming command is from an unknown device (maybe disconnected?)"
        );
      }
      switch (command) {
        case COMMAND_SENDTAB:
          try {
            const { title, uri } = await this.sendTab.handle(
              senderId,
              payload,
              reason
            );
            log.info(
              `Tab received with FxA commands: ${title} from ${
                sender ? sender.name : "Unknown device"
              }.`
            );
            // This should eventually be rare to hit as all platforms will be using the same
            // scheme filter list, but we have this here in the case other platforms
            // haven't caught up and/or trying to send invalid uris using older versions
            const scheme = Services.io.newURI(uri).scheme;
            if (lazy.INVALID_SHAREABLE_SCHEMES.has(scheme)) {
              throw new Error("Invalid scheme found for received URI.");
            }
            tabsReceived.push({ title, uri, sender });
          } catch (e) {
            log.error(`Error while handling incoming Send Tab payload.`, e);
          }
          break;
        default:
          log.info(`Unknown command: ${command}.`);
      }
    }
    if (tabsReceived.length) {
      this._notifyFxATabsReceived(tabsReceived);
    }
  }

  _notifyFxATabsReceived(tabsReceived) {
    Observers.notify("fxaccounts:commands:open-uri", tabsReceived);
  }
}

/**
 * Send Tab is built on top of FxA commands.
 *
 * Devices exchange keys wrapped in the oldsync key between themselves (getEncryptedSendTabKeys)
 * during the device registration flow. The FxA server can theoretically never
 * retrieve the send tab keys since it doesn't know the oldsync key.
 *
 * Note about the keys:
 * The server has the `pushPublicKey`. The FxA server encrypt the send-tab payload again using the
 * push keys - after the client has encrypted the payload using the send-tab keys.
 * The push keys are different from the send-tab keys. The FxA server uses
 * the push keys to deliver the tabs using same mechanism we use for web-push.
 * However, clients use the send-tab keys for end-to-end encryption.
 */
class SendTab {
  constructor(commands, fxAccountsInternal) {
    this._commands = commands;
    this._fxai = fxAccountsInternal;
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
    const flowID = this._fxai.telemetry.generateFlowID();
    const encoder = new TextEncoder("utf8");
    const data = { entries: [{ title: tab.title, url: tab.url }] };
    const report = {
      succeeded: [],
      failed: [],
    };
    for (let device of to) {
      try {
        const streamID = this._fxai.telemetry.generateFlowID();
        const targetData = Object.assign({ flowID, streamID }, data);
        const bytes = encoder.encode(JSON.stringify(targetData));
        const encrypted = await this._encrypt(bytes, device);
        // FxA expects an object as the payload, but we only have a single encrypted string; wrap it.
        // If you add any plaintext items to this payload, please carefully consider the privacy implications
        // of revealing that data to the FxA server.
        const payload = { encrypted };
        await this._commands.invoke(COMMAND_SENDTAB, device, payload);
        this._fxai.telemetry.recordEvent(
          "command-sent",
          COMMAND_SENDTAB_TAIL,
          this._fxai.telemetry.sanitizeDeviceId(device.id),
          { flowID, streamID }
        );
        report.succeeded.push(device);
      } catch (error) {
        log.error("Error while invoking a send tab command.", error);
        report.failed.push({ device, error });
      }
    }
    return report;
  }

  // Returns true if the target device is compatible with FxA Commands Send tab.
  isDeviceCompatible(device) {
    return (
      Services.prefs.getBoolPref(
        "identity.fxaccounts.commands.enabled",
        true
      ) &&
      device.availableCommands &&
      device.availableCommands[COMMAND_SENDTAB]
    );
  }

  // Handle incoming send tab payload, called by FxAccountsCommands.
  async handle(senderID, { encrypted }, reason) {
    const bytes = await this._decrypt(encrypted);
    const decoder = new TextDecoder("utf8");
    const data = JSON.parse(decoder.decode(bytes));
    const { flowID, streamID, entries } = data;
    const current = data.hasOwnProperty("current")
      ? data.current
      : entries.length - 1;
    const { title, url: uri } = entries[current];
    // `flowID` and `streamID` are in the top-level of the JSON, `entries` is
    // an array of "tabs" with `current` being what index is the one we care
    // about, or the last one if not specified.
    this._fxai.telemetry.recordEvent(
      "command-received",
      COMMAND_SENDTAB_TAIL,
      this._fxai.telemetry.sanitizeDeviceId(senderID),
      { flowID, streamID, reason }
    );

    return {
      title,
      uri,
    };
  }

  async _encrypt(bytes, device) {
    let bundle = device.availableCommands[COMMAND_SENDTAB];
    if (!bundle) {
      throw new Error(`Device ${device.id} does not have send tab keys.`);
    }
    const oldsyncKey = await this._fxai.keys.getKeyForScope(SCOPE_OLD_SYNC);
    // Older clients expect this to be hex, due to pre-JWK sync key ids :-(
    const ourKid = this._fxai.keys.kidAsHex(oldsyncKey);
    const { kid: theirKid } = JSON.parse(
      device.availableCommands[COMMAND_SENDTAB]
    );
    if (theirKid != ourKid) {
      throw new Error("Target Send Tab key ID is different from ours");
    }
    const json = JSON.parse(bundle);
    const wrapper = new lazy.CryptoWrapper();
    wrapper.deserialize({ payload: json });
    const syncKeyBundle = lazy.BulkKeyBundle.fromJWK(oldsyncKey);
    let { publicKey, authSecret } = await wrapper.decrypt(syncKeyBundle);
    authSecret = urlsafeBase64Decode(authSecret);
    publicKey = urlsafeBase64Decode(publicKey);

    const { ciphertext: encrypted } = await lazy.PushCrypto.encrypt(
      bytes,
      publicKey,
      authSecret
    );
    return urlsafeBase64Encode(encrypted);
  }

  async _getPersistedSendTabKeys() {
    const { device } = await this._fxai.getUserAccountData(["device"]);
    return device && device.sendTabKeys;
  }

  async _decrypt(ciphertext) {
    let {
      privateKey,
      publicKey,
      authSecret,
    } = await this._getPersistedSendTabKeys();
    publicKey = urlsafeBase64Decode(publicKey);
    authSecret = urlsafeBase64Decode(authSecret);
    ciphertext = new Uint8Array(urlsafeBase64Decode(ciphertext));
    return lazy.PushCrypto.decrypt(
      privateKey,
      publicKey,
      authSecret,
      // The only Push encoding we support.
      { encoding: "aes128gcm" },
      ciphertext
    );
  }

  async _generateAndPersistSendTabKeys() {
    let [publicKey, privateKey] = await lazy.PushCrypto.generateKeys();
    publicKey = urlsafeBase64Encode(publicKey);
    let authSecret = lazy.PushCrypto.generateAuthenticationSecret();
    authSecret = urlsafeBase64Encode(authSecret);
    const sendTabKeys = {
      publicKey,
      privateKey,
      authSecret,
    };
    await this._fxai.withCurrentAccountState(async state => {
      const { device } = await state.getUserAccountData(["device"]);
      await state.updateUserAccountData({
        device: {
          ...device,
          sendTabKeys,
        },
      });
    });
    return sendTabKeys;
  }

  async _getPersistedEncryptedSendTabKey() {
    const { encryptedSendTabKeys } = await this._fxai.getUserAccountData([
      "encryptedSendTabKeys",
    ]);
    return encryptedSendTabKeys;
  }

  async _generateAndPersistEncryptedSendTabKey() {
    let sendTabKeys = await this._getPersistedSendTabKeys();
    if (!sendTabKeys) {
      log.info("Could not find sendtab keys, generating them");
      sendTabKeys = await this._generateAndPersistSendTabKeys();
    }
    // Strip the private key from the bundle to encrypt.
    const keyToEncrypt = {
      publicKey: sendTabKeys.publicKey,
      authSecret: sendTabKeys.authSecret,
    };
    if (!(await this._fxai.keys.canGetKeyForScope(SCOPE_OLD_SYNC))) {
      log.info("Can't fetch keys, so unable to determine sendtab keys");
      return null;
    }
    let oldsyncKey;
    try {
      oldsyncKey = await this._fxai.keys.getKeyForScope(SCOPE_OLD_SYNC);
    } catch (ex) {
      log.warn("Failed to fetch keys, so unable to determine sendtab keys", ex);
      return null;
    }
    const wrapper = new lazy.CryptoWrapper();
    wrapper.cleartext = keyToEncrypt;
    const keyBundle = lazy.BulkKeyBundle.fromJWK(oldsyncKey);
    await wrapper.encrypt(keyBundle);
    const encryptedSendTabKeys = JSON.stringify({
      // Older clients expect this to be hex, due to pre-JWK sync key ids :-(
      kid: this._fxai.keys.kidAsHex(oldsyncKey),
      IV: wrapper.IV,
      hmac: wrapper.hmac,
      ciphertext: wrapper.ciphertext,
    });
    await this._fxai.withCurrentAccountState(async state => {
      await state.updateUserAccountData({
        encryptedSendTabKeys,
      });
    });
    return encryptedSendTabKeys;
  }

  async getEncryptedSendTabKeys() {
    let encryptedSendTabKeys = await this._getPersistedEncryptedSendTabKey();
    const sendTabKeys = await this._getPersistedSendTabKeys();
    if (!encryptedSendTabKeys || !sendTabKeys) {
      log.info("Generating and persisting encrypted sendtab keys");
      // `_generateAndPersistEncryptedKeys` requires the sync key
      // which cannot be accessed if the login manager is locked
      // (i.e when the primary password is locked) or if the sync keys
      // aren't accessible (account isn't verified)
      // so this function could fail to retrieve the keys
      // however, device registration will trigger when the account
      // is verified, so it's OK
      // Note that it's okay to persist those keys, because they are
      // already persisted in plaintext and the encrypted bundle
      // does not include the sync-key (the sync key is used to encrypt
      // it though)
      encryptedSendTabKeys = await this._generateAndPersistEncryptedSendTabKey();
    }
    return encryptedSendTabKeys;
  }
}

function urlsafeBase64Encode(buffer) {
  return ChromeUtils.base64URLEncode(new Uint8Array(buffer), { pad: false });
}

function urlsafeBase64Decode(str) {
  return ChromeUtils.base64URLDecode(str, { padding: "reject" });
}
