/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Helpers for using OS Key Store.
 */

"use strict";

var EXPORTED_SYMBOLS = ["OSKeyStore"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "nativeOSKeyStore",
  "@mozilla.org/security/oskeystore;1",
  Ci.nsIOSKeyStore
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "osReauthenticator",
  "@mozilla.org/security/osreauthenticator;1",
  Ci.nsIOSReauthenticator
);

// Skip reauth during tests, only works in non-official builds.
const TEST_ONLY_REAUTH = "toolkit.osKeyStore.unofficialBuildOnlyLogin";

var OSKeyStore = {
  /**
   * On macOS this becomes part of the name label visible on Keychain Acesss as
   * "org.mozilla.nss.keystore.firefox" (where "firefox" is the MOZ_APP_NAME).
   */
  STORE_LABEL: AppConstants.MOZ_APP_NAME,

  /**
   * Consider the module is initialized as locked. OS might unlock without a
   * prompt.
   * @type {Boolean}
   */
  _isLocked: true,

  _pendingUnlockPromise: null,

  /**
   * @returns {boolean} True if logged in (i.e. decrypt(reauth = false) will
   *                    not retrigger a dialog) and false if not.
   *                    User might log out elsewhere in the OS, so even if this
   *                    is true a prompt might still pop up.
   */
  get isLoggedIn() {
    return !this._isLocked;
  },

  /**
   * @returns {boolean} True if there is another login dialog existing and false
   *                    otherwise.
   */
  get isUIBusy() {
    return !!this._pendingUnlockPromise;
  },

  /**
   * If the test pref exists, this method will dispatch a observer message and
   * resolves to simulate successful reauth, or rejects to simulate failed reauth.
   *
   * @returns {Promise<undefined>} Resolves when sucessful login, rejects when
   *                               login fails.
   */
  async _reauthInTests() {
    // Skip this reauth because there is no way to mock the
    // native dialog in the testing environment, for now.
    log.debug("_ensureReauth: _testReauth: ", this._testReauth);
    switch (this._testReauth) {
      case "pass":
        Services.obs.notifyObservers(
          null,
          "oskeystore-testonly-reauth",
          "pass"
        );
        return { authenticated: true, auth_details: "success" };
      case "cancel":
        Services.obs.notifyObservers(
          null,
          "oskeystore-testonly-reauth",
          "cancel"
        );
        throw new Components.Exception(
          "Simulating user cancelling login dialog",
          Cr.NS_ERROR_FAILURE
        );
      default:
        throw new Components.Exception(
          "Unknown test pref value",
          Cr.NS_ERROR_FAILURE
        );
    }
  },

  /**
   * Ensure the store in use is logged in. It will display the OS
   * login prompt or do nothing if it's logged in already. If an existing login
   * prompt is already prompted, the result from it will be used instead.
   *
   * Note: This method must set _pendingUnlockPromise before returning the
   * promise (i.e. the first |await|), otherwise we'll risk re-entry.
   * This is why there aren't an |await| in the method. The method is marked as
   * |async| to communicate that it's async.
   *
   * @param   {boolean|string} reauth If set to a string, prompt the reauth login dialog,
   *                                  showing the string on the native OS login dialog.
   *                                  Otherwise `false` will prevent showing the prompt.
   * @param   {string} dialogCaption  The string will be shown on the native OS
   *                                  login dialog as the dialog caption (usually Product Name).
   * @param   {Window?} parentWindow  The window of the caller, used to center the
   *                                  OS prompt in the middle of the application window.
   * @param   {boolean} generateKeyIfNotAvailable Makes key generation optional
   *                                  because it will currently cause more
   *                                  problems for us down the road on macOS since the application
   *                                  that creates the Keychain item is the only one that gets
   *                                  access to the key in the future and right now that key isn't
   *                                  specific to the channel or profile. This means if a user uses
   *                                  both DevEdition and Release on the same OS account (not
   *                                  unreasonable for a webdev.) then when you want to simply
   *                                  re-auth the user for viewing passwords you may also get a
   *                                  KeyChain prompt to allow the app to access the stored key even
   *                                  though that's not at all relevant for the re-auth. We skip the
   *                                  code here so that we can postpone deciding on how we want to
   *                                  handle this problem (multiple channels) until we actually use
   *                                  the key storage. If we start creating keys on macOS by running
   *                                  this code we'll potentially have to do extra work to cleanup
   *                                  the mess later.
   * @returns {Promise<Object>}       Object with the following properties:
   *                                    authenticated: {boolean} Set to true if the user successfully authenticated.
   *                                    auth_details: {String?} Details of the authentication result.
   */
  async ensureLoggedIn(
    reauth = false,
    dialogCaption = "",
    parentWindow = null,
    generateKeyIfNotAvailable = true
  ) {
    if (
      (typeof reauth != "boolean" && typeof reauth != "string") ||
      reauth === true ||
      reauth === ""
    ) {
      throw new Error(
        "reauth is required to either be `false` or a non-empty string"
      );
    }

    if (this._pendingUnlockPromise) {
      log.debug("ensureLoggedIn: Has a pending unlock operation");
      return this._pendingUnlockPromise;
    }
    log.debug(
      "ensureLoggedIn: Creating new pending unlock promise. reauth: ",
      reauth
    );

    let unlockPromise;
    if (typeof reauth == "string") {
      // Only allow for local builds
      if (
        UpdateUtils.getUpdateChannel(false) == "default" &&
        this._testReauth
      ) {
        unlockPromise = this._reauthInTests();
      } else if (
        AppConstants.platform == "win" ||
        (AppConstants.platform == "macosx" &&
          AppConstants.isPlatformAndVersionAtLeast("macosx", "16"))
      ) {
        // The OS auth dialog is not supported on macOS < 10.12
        // (Darwin 16) due to various issues (bug 1622304 and bug 1622303).

        // On Windows, this promise rejects when the user cancels login dialog, see bug 1502121.
        // On macOS this resolves to false, so we would need to check it.
        unlockPromise = osReauthenticator
          .asyncReauthenticateUser(reauth, dialogCaption, parentWindow)
          .then(reauthResult => {
            if (typeof reauthResult[0] == "boolean" && !reauthResult[0]) {
              throw new Components.Exception(
                "User canceled OS reauth entry",
                Cr.NS_ERROR_FAILURE
              );
            }
            let result = {
              authenticated: true,
              auth_details: "success",
            };
            if (reauthResult.length == 2 && reauthResult[1]) {
              result.auth_details += "_no_password";
            }
            return result;
          });
      } else {
        log.debug("ensureLoggedIn: Skipping reauth on unsupported platforms");
        unlockPromise = Promise.resolve({
          authenticated: true,
          auth_details: "success_unsupported_platform",
        });
      }
    } else {
      unlockPromise = Promise.resolve({ authenticated: true });
    }

    if (generateKeyIfNotAvailable) {
      unlockPromise = unlockPromise.then(async reauthResult => {
        if (!(await nativeOSKeyStore.asyncSecretAvailable(this.STORE_LABEL))) {
          log.debug(
            "ensureLoggedIn: Secret unavailable, attempt to generate new secret."
          );
          let recoveryPhrase = await nativeOSKeyStore.asyncGenerateSecret(
            this.STORE_LABEL
          );
          // TODO We should somehow have a dialog to ask the user to write this down,
          // and another dialog somewhere for the user to restore the secret with it.
          // (Intentionally not printing it out in the console)
          log.debug(
            "ensureLoggedIn: Secret generated. Recovery phrase length: " +
              recoveryPhrase.length
          );
        }
        return reauthResult;
      });
    }

    unlockPromise = unlockPromise.then(
      reauthResult => {
        log.debug("ensureLoggedIn: Logged in");
        this._pendingUnlockPromise = null;
        this._isLocked = false;

        return reauthResult;
      },
      err => {
        log.debug("ensureLoggedIn: Not logged in", err);
        this._pendingUnlockPromise = null;
        this._isLocked = true;

        return { authenticated: false, auth_details: "fail" };
      }
    );

    this._pendingUnlockPromise = unlockPromise;

    return this._pendingUnlockPromise;
  },

  /**
   * Decrypts cipherText.
   *
   * Note: In the event of an rejection, check the result property of the Exception
   *       object. Handles NS_ERROR_ABORT as user has cancelled the action (e.g.,
   *       don't show that dialog), apart from other errors (e.g., gracefully
   *       recover from that and still shows the dialog.)
   *
   * @param   {string}         cipherText Encrypted string including the algorithm details.
   * @param   {boolean|string} reauth     If set to a string, prompt the reauth login dialog.
   *                                      The string may be shown on the native OS
   *                                      login dialog. Empty strings and `true` are disallowed.
   * @returns {Promise<string>}           resolves to the decrypted string, or rejects otherwise.
   */
  async decrypt(cipherText, reauth = false) {
    if (!(await this.ensureLoggedIn(reauth)).authenticated) {
      throw Components.Exception(
        "User canceled OS unlock entry",
        Cr.NS_ERROR_ABORT
      );
    }
    let bytes = await nativeOSKeyStore.asyncDecryptBytes(
      this.STORE_LABEL,
      cipherText
    );
    return String.fromCharCode.apply(String, bytes);
  },

  /**
   * Encrypts a string and returns cipher text containing algorithm information used for decryption.
   *
   * @param   {string} plainText Original string without encryption.
   * @returns {Promise<string>} resolves to the encrypted string (with algorithm), otherwise rejects.
   */
  async encrypt(plainText) {
    if (!(await this.ensureLoggedIn()).authenticated) {
      throw Components.Exception(
        "User canceled OS unlock entry",
        Cr.NS_ERROR_ABORT
      );
    }

    // Convert plain text into a UTF-8 binary string
    plainText = unescape(encodeURIComponent(plainText));

    // Convert it to an array
    let textArr = [];
    for (let char of plainText) {
      textArr.push(char.charCodeAt(0));
    }

    let rawEncryptedText = await nativeOSKeyStore.asyncEncryptBytes(
      this.STORE_LABEL,
      textArr
    );

    // Mark the output with a version number.
    return rawEncryptedText;
  },

  /**
   * Resolve when the login dialogs are closed, immediately if none are open.
   *
   * An existing MP dialog will be focused and will request attention.
   *
   * @returns {Promise<boolean>}
   *          Resolves with whether the user is logged in to MP.
   */
  async waitForExistingDialog() {
    if (this.isUIBusy) {
      return this._pendingUnlockPromise;
    }
    return this.isLoggedIn;
  },

  /**
   * Remove the store. For tests.
   */
  async cleanup() {
    return nativeOSKeyStore.asyncDeleteSecret(this.STORE_LABEL);
  },
};

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let ConsoleAPI = ChromeUtils.import("resource://gre/modules/Console.jsm", {})
    .ConsoleAPI;
  return new ConsoleAPI({
    maxLogLevelPref: "toolkit.osKeyStore.loglevel",
    prefix: "OSKeyStore",
  });
});

XPCOMUtils.defineLazyPreferenceGetter(
  OSKeyStore,
  "_testReauth",
  TEST_ONLY_REAUTH,
  ""
);
