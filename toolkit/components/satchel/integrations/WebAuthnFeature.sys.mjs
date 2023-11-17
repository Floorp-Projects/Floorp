/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  LoginHelper,
  ParentAutocompleteOption,
} from "resource://gre/modules/LoginHelper.sys.mjs";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "webauthnService",
  "@mozilla.org/webauthn/service;1",
  "nsIWebAuthnService"
);

ChromeUtils.defineLazyGetter(
  lazy,
  "strings",
  () => new Localization(["browser/webauthnDialog.ftl"])
);
ChromeUtils.defineLazyGetter(lazy, "log", () =>
  LoginHelper.createLogger("WebAuthnFeature")
);

if (Services.appinfo.processType !== Services.appinfo.PROCESS_TYPE_DEFAULT) {
  throw new Error(
    "PasskeySupport.sys.mjs should only run in the parent process"
  );
}

async function formatMessages(...ids) {
  for (let i in ids) {
    if (typeof ids[i] == "string") {
      ids[i] = { id: ids[i] };
    }
  }

  const messages = await lazy.strings.formatMessages(ids);
  return messages.map(message => {
    if (message.attributes) {
      return message.attributes.reduce(
        (result, { name, value }) => ({ ...result, [name]: value }),
        {}
      );
    }
    return message.value;
  });
}

class WebAuthnSupport {
  async *#getAutocompleteItemsAsync(browsingContextId, formOrigin) {
    let transactionId = lazy.webauthnService.hasPendingConditionalGet(
      browsingContextId,
      formOrigin
    );
    if (transactionId == 0) {
      // No pending transaction
      return;
    }
    let credentials = lazy.webauthnService.getAutoFillEntries(transactionId);
    for (const credential of credentials) {
      yield new ParentAutocompleteOption(
        "chrome://browser/content/logos/etp-mobile.svg", // TODO: Use WebAuthn logo and strings
        credential.userName,
        credential.rpId,
        "PasswordManager:promptForAuthenticator",
        {
          selection: {
            transactionId,
            credentialId: credential.credentialId,
          },
        }
      );
    }
    // `getAutoFillEntries` may not return all of the credentials on the device
    // (in particular it will not include credentials with a protection policy
    // that forbids silent discovery), so we include a catch-all entry in the
    // list. If the user selects this entry, the WebAuthn transaction will
    // proceed using the modal UI.
    const [title, subtitle] = await formatMessages(
      "webauthn-use-passkey-title",
      "webauthn-use-passkey-subtitle"
    );
    yield new ParentAutocompleteOption(
      "chrome://browser/content/logos/etp-mobile.svg", // TODO: Use WebAuthn logo and strings
      title,
      subtitle,
      "PasswordManager:promptForAuthenticator",
      {
        selection: {
          transactionId,
        },
      }
    );
  }

  /**
   *
   * @param {int} browsingContextId the browsing context ID associated with this request
   * @param {string} formOrigin
   * @param {string} scenarioName can be "SignUpFormScenario" or undefined
   * @param {string} isWebAuthn indicates whether "webauthn" was included in the input's autocomplete value
   * @returns {ParentAutocompleteOption} the optional WebAuthn autocomplete item
   */
  async autocompleteItemsAsync(
    browsingContextId,
    formOrigin,
    scenarioName,
    isWebAuthn
  ) {
    const result = [];
    if (scenarioName !== "SignUpFormScenario" || isWebAuthn) {
      for await (const item of this.#getAutocompleteItemsAsync(
        browsingContextId,
        formOrigin
      )) {
        result.push(item);
      }
    }
    return result;
  }

  async promptForAuthenticator(browser, selection) {
    lazy.log.info("Prompting to authenticate with relying party.");
    if (selection.credentialId) {
      lazy.webauthnService.selectAutoFillEntry(
        selection.transactionId,
        selection.credentialId
      );
    } else {
      lazy.webauthnService.resumeConditionalGet(selection.transactionId);
    }
  }
}

export const WebAuthnFeature = new WebAuthnSupport();
