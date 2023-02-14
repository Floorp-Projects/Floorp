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
    return Promise.resolve(0);
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
   * @param {IdentityInternalManifest[]} identityManifests - The manifests corresponding 1-to-1 with identityProviders
   * @returns {Promise<number>} The user-selected identity provider
   */
  showProviderPrompt(browsingContext, identityProviders, identityManifests) {
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

      if (identityProviders.length != identityManifests.length) {
        reject();
      }

      // Localize all strings to be used
      // Bug 1797154 - Convert localization calls to use the async formatValues.
      let localization = new Localization(
        ["preview/identityCredentialNotification.ftl"],
        true
      );
      let headerMessage = localization.formatValueSync(
        "identity-credential-header-providers"
      );
      let [accept, cancel] = localization.formatMessagesSync([
        { id: "identity-credential-accept-button" },
        { id: "identity-credential-cancel-button" },
      ]);

      let cancelLabel = cancel.attributes.find(x => x.name == "label").value;
      let cancelKey = cancel.attributes.find(x => x.name == "accesskey").value;
      let acceptLabel = accept.attributes.find(x => x.name == "label").value;
      let acceptKey = accept.attributes.find(x => x.name == "accesskey").value;

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
      for (const [providerIndex, provider] of identityProviders.entries()) {
        let providerURL = new URL(provider.configURL);
        let displayDomain = lazy.IDNService.convertToDisplayIDN(
          providerURL.host,
          {}
        );
        let newItem = itemTemplate.content.firstElementChild.cloneNode(true);
        let newRadio = newItem.getElementsByClassName(
          "identity-credential-list-item-radio"
        )[0];
        newRadio.value = providerIndex;
        newRadio.addEventListener("change", function(event) {
          for (let item of listBox.children) {
            item.classList.remove("checked");
          }
          if (event.target.checked) {
            event.target.parentElement.classList.add("checked");
          }
        });
        if (providerIndex == 0) {
          newRadio.checked = true;
          newItem.classList.add("checked");
        }
        newItem.getElementsByClassName(
          "identity-credential-list-item-label"
        )[0].textContent = displayDomain;
        listBox.append(newItem);
      }

      // Construct the necessary arguments for notification behavior
      let options = {
        hideClose: true,
        eventCallback: (topic, nextRemovalReason, isCancel) => {
          if (topic == "removed" && isCancel) {
            reject();
          }
        },
      };
      let mainAction = {
        label: acceptLabel,
        accessKey: acceptKey,
        callback(event) {
          let result = listBox.querySelector(
            ".identity-credential-list-item-radio:checked"
          ).value;
          resolve(parseInt(result));
        },
      };
      let secondaryActions = [
        {
          label: cancelLabel,
          accessKey: cancelKey,
          callback(event) {
            reject();
          },
        },
      ];

      // Show the popup
      browser.ownerDocument.getElementById(
        "identity-credential-provider"
      ).hidden = false;
      browser.ownerDocument.getElementById(
        "identity-credential-policy"
      ).hidden = true;
      browser.ownerDocument.getElementById(
        "identity-credential-account"
      ).hidden = true;
      browser.ownerGlobal.PopupNotifications.show(
        browser,
        "identity-credential",
        headerMessage,
        "identity-credential-notification-icon",
        mainAction,
        secondaryActions,
        options
      );
    });
  }

  /**
   * Ask the user, using a PopupNotification, to approve or disapprove of the policies of the Identity Provider.
   * @param {BrowsingContext} browsingContext - The BrowsingContext of the document requesting an identity credential via navigator.credentials.get()
   * @param {IdentityProvider} identityProvider - The Identity Provider that the user has selected to use
   * @param {IdentityInternalManifest} identityManifest - The Identity Provider that the user has selected to use's manifest
   * @param {IdentityCredentialMetadata} identityCredentialMetadata - The metadata displayed to the user
   * @returns {Promise<bool>} A boolean representing the user's acceptance of the metadata.
   */
  showPolicyPrompt(
    browsingContext,
    identityProvider,
    identityManifest,
    identityCredentialMetadata
  ) {
    // For testing only.
    if (lazy.SELECT_FIRST_IN_UI_LISTS) {
      return Promise.resolve(true);
    }
    if (
      !identityCredentialMetadata ||
      !identityCredentialMetadata.privacy_policy_url ||
      !identityCredentialMetadata.terms_of_service_url
    ) {
      return Promise.resolve(true);
    }
    return new Promise(function(resolve, reject) {
      let browser = browsingContext.top.embedderElement;
      if (!browser) {
        reject();
        return;
      }

      let providerURL = new URL(identityProvider.configURL);
      let providerDisplayDomain = lazy.IDNService.convertToDisplayIDN(
        providerURL.host,
        {}
      );
      let currentBaseDomain =
        browsingContext.currentWindowContext.documentPrincipal.baseDomain;
      // Localize the description
      // Bug 1797154 - Convert localization calls to use the async formatValues.
      let localization = new Localization(
        ["preview/identityCredentialNotification.ftl"],
        true
      );
      let [accept, cancel] = localization.formatMessagesSync([
        { id: "identity-credential-accept-button" },
        { id: "identity-credential-cancel-button" },
      ]);

      let cancelLabel = cancel.attributes.find(x => x.name == "label").value;
      let cancelKey = cancel.attributes.find(x => x.name == "accesskey").value;
      let acceptLabel = accept.attributes.find(x => x.name == "label").value;
      let acceptKey = accept.attributes.find(x => x.name == "accesskey").value;

      let title = localization.formatValueSync(
        "identity-credential-policy-title",
        {
          provider: providerDisplayDomain,
        }
      );

      let privacyPolicyAnchor = browser.ownerDocument.getElementById(
        "identity-credential-privacy-policy"
      );
      privacyPolicyAnchor.href = identityCredentialMetadata.privacy_policy_url;
      let termsOfServiceAnchor = browser.ownerDocument.getElementById(
        "identity-credential-terms-of-service"
      );
      termsOfServiceAnchor.href =
        identityCredentialMetadata.terms_of_service_url;

      // Populate the content of the policy panel
      let description = browser.ownerDocument.getElementById(
        "identity-credential-policy-explanation"
      );
      description.setAttribute(
        "data-l10n-args",
        JSON.stringify({
          host: currentBaseDomain,
          provider: providerDisplayDomain,
        })
      );
      browser.ownerDocument.l10n.setAttributes(
        description,
        "identity-credential-policy-description",
        {
          host: currentBaseDomain,
          provider: providerDisplayDomain,
        }
      );

      // Construct the necessary arguments for notification behavior
      let options = {
        hideClose: true,
        eventCallback: (topic, nextRemovalReason, isCancel) => {
          if (topic == "removed" && isCancel) {
            reject();
          }
        },
      };
      let mainAction = {
        label: acceptLabel,
        accessKey: acceptKey,
        callback(event) {
          resolve(true);
        },
      };
      let secondaryActions = [
        {
          label: cancelLabel,
          accessKey: cancelKey,
          callback(event) {
            resolve(false);
          },
        },
      ];

      // Show the popup
      let ownerDocument = browser.ownerDocument;
      ownerDocument.l10n.translateFragment(description);
      ownerDocument.getElementById(
        "identity-credential-provider"
      ).hidden = true;
      ownerDocument.getElementById("identity-credential-policy").hidden = false;
      ownerDocument.getElementById("identity-credential-account").hidden = true;
      browser.ownerGlobal.PopupNotifications.show(
        browser,
        "identity-credential",
        title,
        "identity-credential-notification-icon",
        mainAction,
        secondaryActions,
        options
      );
    });
  }

  /**
   * Ask the user, using a PopupNotification, to select an account from a provided list.
   * @param {BrowsingContext} browsingContext - The BrowsingContext of the document requesting an identity credential via navigator.credentials.get()
   * @param {IdentityAccountList} accountList - The list of accounts the user selects from
   * @param {IdentityProvider} provider - The selected identity provider
   * @param {IdentityInternalManifest} providerManifest - The manifest of the selected identity provider
   * @returns {Promise<IdentityAccount>} The user-selected account
   */
  showAccountListPrompt(
    browsingContext,
    accountList,
    provider,
    providerManifest
  ) {
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

      // Localize all strings to be used
      // Bug 1797154 - Convert localization calls to use the async formatValues.
      let localization = new Localization(
        ["preview/identityCredentialNotification.ftl"],
        true
      );
      let providerURL = new URL(provider.configURL);
      let displayDomain = lazy.IDNService.convertToDisplayIDN(
        providerURL.host,
        {}
      );
      let headerMessage = localization.formatValueSync(
        "identity-credential-header-accounts",
        {
          provider: displayDomain,
        }
      );
      let [accept, cancel] = localization.formatMessagesSync([
        { id: "identity-credential-accept-button" },
        { id: "identity-credential-cancel-button" },
      ]);

      let cancelLabel = cancel.attributes.find(x => x.name == "label").value;
      let cancelKey = cancel.attributes.find(x => x.name == "accesskey").value;
      let acceptLabel = accept.attributes.find(x => x.name == "label").value;
      let acceptKey = accept.attributes.find(x => x.name == "accesskey").value;

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
      for (const [accountIndex, account] of accountList.accounts.entries()) {
        let newItem = itemTemplate.content.firstElementChild.cloneNode(true);
        let newRadio = newItem.getElementsByClassName(
          "identity-credential-list-item-radio"
        )[0];
        newRadio.value = accountIndex;
        newRadio.addEventListener("change", function(event) {
          for (let item of listBox.children) {
            item.classList.remove("checked");
          }
          if (event.target.checked) {
            event.target.parentElement.classList.add("checked");
          }
        });
        if (accountIndex == 0) {
          newRadio.checked = true;
          newItem.classList.add("checked");
        }
        newItem.getElementsByClassName(
          "identity-credential-list-item-label-name"
        )[0].textContent = account.name;
        newItem.getElementsByClassName(
          "identity-credential-list-item-label-email"
        )[0].textContent = account.email;
        listBox.append(newItem);
      }

      // Construct the necessary arguments for notification behavior
      let options = {
        hideClose: true,
        eventCallback: (topic, nextRemovalReason, isCancel) => {
          if (topic == "removed" && isCancel) {
            reject();
          }
        },
      };
      let mainAction = {
        label: acceptLabel,
        accessKey: acceptKey,
        callback(event) {
          let result = listBox.querySelector(
            ".identity-credential-list-item-radio:checked"
          ).value;
          resolve(parseInt(result));
        },
      };
      let secondaryActions = [
        {
          label: cancelLabel,
          accessKey: cancelKey,
          callback(event) {
            reject();
          },
        },
      ];

      // Show the popup
      browser.ownerDocument.getElementById(
        "identity-credential-provider"
      ).hidden = true;
      browser.ownerDocument.getElementById(
        "identity-credential-policy"
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
        secondaryActions,
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
      browser.ownerGlobal.PopupNotifications.remove(notification, true);
    }
  }
}
