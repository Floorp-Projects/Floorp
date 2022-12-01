/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "IDNService",
  "@mozilla.org/network/idn-service;1",
  "nsIIDNService"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "SELECT_FIRST_IN_UI_LISTS",
  "dom.security.credentialmanagement.identity.select_first_in_ui_lists",
  false
);

function fulfilledPromiseFromFirstListElement(list) {
  if (list.length) {
    return Promise.resolve(list[0]);
  }
  return Promise.reject();
}

/**
 * Class implementing the nsIIdentityCredentialPromptService
 * */
export class IdentityCredentialPromptService {
  classID = Components.ID("{936007db-a957-4f1d-a23d-f7d9403223e6}");
  QueryInterface = ChromeUtils.generateQI([
    "nsIIdentityCredentialPromptService",
  ]);

  /**
   * Ask the user, using a PopupNotification, to select an Identity Provider from a provided list.
   * @param {BrowsingContext} browsingContext - The BrowsingContext of the document requesting an identity credential via navigator.credentials.get()
   * @param {IdentityProvider[]} identityProviders - The list of identity providers the user selects from
   * @returns {Promise<IdentityProvider>} The user-selected identity provider
   */
  showProviderPrompt(browsingContext, identityProviders) {
    // For testing only.
    if (lazy.SELECT_FIRST_IN_UI_LISTS) {
      return fulfilledPromiseFromFirstListElement(identityProviders);
    }
    return new Promise(function(resolve, reject) {
      let browser = browsingContext.top.embedderElement;
      if (!browser) {
        reject();
        return;
      }

      // Localize all strings to be used
      // Bug 1797154 - Convert localization calls to use the async formatValues.
      let localization = new Localization(
        ["preview/identityCredentialNotification.ftl"],
        true
      );
      let headerMessage = localization.formatValueSync(
        "credential-header-providers",
        {
          host: "<>",
        }
      );
      let cancelLabel = localization.formatValueSync("credential-cancel-label");
      let cancelKey = localization.formatValueSync(
        "credential-cancel-accesskey"
      );

      // Build the choices into the panel
      let listBox = browser.ownerDocument.getElementById(
        "identity-credential-provider-selector-container"
      );
      while (listBox.firstChild) {
        listBox.removeChild(listBox.lastChild);
      }
      let itemTemplate = browser.ownerDocument.getElementById(
        "template-credential-provider-list-item"
      );
      for (let providerIndex in identityProviders) {
        let provider = identityProviders[providerIndex];
        let providerURI = new URL(provider.configURL);
        let displayDomain = lazy.IDNService.convertToDisplayIDN(
          providerURI.host,
          {}
        );
        let newItem = itemTemplate.content.firstElementChild.cloneNode(true);
        newItem.firstElementChild.textContent = displayDomain;
        newItem.setAttribute("oncommand", `this.callback(event)`);
        newItem.callback = function(event) {
          let notification = browser.ownerGlobal.PopupNotifications.getNotification(
            "identity-credential",
            browser
          );
          browser.ownerGlobal.PopupNotifications.remove(notification);
          resolve(provider);
          event.stopPropagation();
        };
        listBox.append(newItem);
      }

      // Construct the necessary arguments for notification behavior
      let currentOrigin =
        browsingContext.currentWindowContext.documentPrincipal.originNoSuffix;
      let options = {
        name: currentOrigin,
      };
      let mainAction = {
        label: cancelLabel,
        accessKey: cancelKey,
        callback(event) {
          reject();
        },
      };

      // Show the popup
      browser.ownerDocument.getElementById(
        "identity-credential-provider"
      ).hidden = false;
      browser.ownerDocument.getElementById(
        "identity-credential-account"
      ).hidden = true;
      browser.ownerGlobal.PopupNotifications.show(
        browser,
        "identity-credential",
        headerMessage,
        "identity-credential-notification-icon",
        mainAction,
        null,
        options
      );
    });
  }

  /**
   * Ask the user, using a PopupNotification, to select an account from a provided list.
   * @param {BrowsingContext} browsingContext - The BrowsingContext of the document requesting an identity credential via navigator.credentials.get()
   * @param {IdentityAccountList} accountList - The list of accounts the user selects from
   * @returns {Promise<IdentityAccount>} The user-selected account
   */
  showAccountListPrompt(browsingContext, accountList) {
    // For testing only.
    if (lazy.SELECT_FIRST_IN_UI_LISTS) {
      return fulfilledPromiseFromFirstListElement(accountList.accounts);
    }
    return new Promise(function(resolve, reject) {
      let browser = browsingContext.top.embedderElement;
      if (!browser) {
        reject();
        return;
      }
      let currentOrigin =
        browsingContext.currentWindowContext.documentPrincipal.originNoSuffix;

      // Localize all strings to be used
      // Bug 1797154 - Convert localization calls to use the async formatValues.
      let localization = new Localization(
        ["preview/identityCredentialNotification.ftl"],
        true
      );
      let headerMessage = localization.formatValueSync(
        "credential-header-accounts",
        {
          host: "<>",
        }
      );
      let descriptionMessage = localization.formatValueSync(
        "credential-description-account-explanation",
        {
          host: currentOrigin,
        }
      );
      let cancelLabel = localization.formatValueSync("credential-cancel-label");
      let cancelKey = localization.formatValueSync(
        "credential-cancel-accesskey"
      );

      // Add the description text
      browser.ownerDocument.getElementById(
        "credential-account-explanation"
      ).textContent = descriptionMessage;

      // Build the choices into the panel
      let listBox = browser.ownerDocument.getElementById(
        "identity-credential-account-selector-container"
      );
      while (listBox.firstChild) {
        listBox.removeChild(listBox.lastChild);
      }
      let itemTemplate = browser.ownerDocument.getElementById(
        "template-credential-account-list-item"
      );
      for (let accountIndex in accountList.accounts) {
        let account = accountList.accounts[accountIndex];
        let newItem = itemTemplate.content.firstElementChild.cloneNode(true);
        newItem.firstElementChild.textContent = account.email;
        newItem.setAttribute("oncommand", "this.callback()");
        newItem.callback = function() {
          let notification = browser.ownerGlobal.PopupNotifications.getNotification(
            "identity-credential",
            browser
          );
          browser.ownerGlobal.PopupNotifications.remove(notification);
          resolve(account);
        };
        listBox.append(newItem);
      }

      // Construct the necessary arguments for notification behavior
      let options = {
        name: currentOrigin,
      };
      let mainAction = {
        label: cancelLabel,
        accessKey: cancelKey,
        callback(event) {
          reject();
        },
      };

      // Show the popup
      browser.ownerDocument.getElementById(
        "identity-credential-provider"
      ).hidden = true;
      browser.ownerDocument.getElementById(
        "identity-credential-account"
      ).hidden = false;
      browser.ownerGlobal.PopupNotifications.show(
        browser,
        "identity-credential",
        headerMessage,
        "identity-credential-notification-icon",
        mainAction,
        null,
        options
      );
    });
  }

  /**
   * Close all UI from the other methods of this module for the provided window.
   * @param {BrowsingContext} browsingContext - The BrowsingContext of the document requesting an identity credential via navigator.credentials.get()
   * @returns
   */
  close(browsingContext) {
    let browser = browsingContext.top.embedderElement;
    if (!browser) {
      return;
    }
    let notification = browser.ownerGlobal.PopupNotifications.getNotification(
      "identity-credential",
      browser
    );
    if (notification) {
      browser.ownerGlobal.PopupNotifications.remove(notification);
    }
  }
}
