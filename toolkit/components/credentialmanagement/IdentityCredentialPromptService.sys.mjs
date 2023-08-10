/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

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

ChromeUtils.defineESModuleGetters(lazy, {
  GeckoViewIdentityCredential:
    "resource://gre/modules/GeckoViewIdentityCredential.sys.mjs",
});
const BEST_HEADER_ICON_SIZE = 16;
const BEST_ICON_SIZE = 32;

// Used in plain mochitests to enable automation
function fulfilledPromiseFromFirstListElement(list) {
  if (list.length) {
    return Promise.resolve(0);
  }
  return Promise.reject();
}

// Converts a "blob" to a data URL
function blobToDataUrl(blob) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.addEventListener("loadend", function () {
      if (reader.error) {
        reject(reader.error);
      }
      resolve(reader.result);
    });
    reader.readAsDataURL(blob);
  });
}

// Converts a URL into a data:// url, suitable for inclusion in Chrome UI
async function fetchToDataUrl(url) {
  let result = await fetch(url);
  if (!result.ok) {
    throw result.status;
  }
  let blob = await result.blob();
  let data = blobToDataUrl(blob);
  return data;
}

/**
 * Class implementing the nsIIdentityCredentialPromptService
 * */
export class IdentityCredentialPromptService {
  classID = Components.ID("{936007db-a957-4f1d-a23d-f7d9403223e6}");
  QueryInterface = ChromeUtils.generateQI([
    "nsIIdentityCredentialPromptService",
  ]);

  async loadIconFromManifest(
    providerManifest,
    bestIconSize = BEST_ICON_SIZE,
    defaultIcon = null
  ) {
    if (providerManifest?.branding?.icons?.length) {
      // Prefer a vector icon, then an exactly sized icon,
      // the the largest icon available.
      let iconsArray = providerManifest.branding.icons;
      let vectorIcon = iconsArray.find(icon => !icon.size);
      if (vectorIcon) {
        return fetchToDataUrl(vectorIcon.url);
      }
      let exactIcon = iconsArray.find(icon => icon.size == bestIconSize);
      if (exactIcon) {
        return fetchToDataUrl(exactIcon.url);
      }
      let biggestIcon = iconsArray.sort(
        (iconA, iconB) => iconB.size - iconA.size
      )[0];
      if (biggestIcon) {
        return fetchToDataUrl(biggestIcon.url);
      }
    }

    return defaultIcon;
  }

  /**
   * Ask the user, using a PopupNotification, to select an Identity Provider from a provided list.
   * @param {BrowsingContext} browsingContext - The BrowsingContext of the document requesting an identity credential via navigator.credentials.get()
   * @param {IdentityProviderConfig[]} identityProviders - The list of identity providers the user selects from
   * @param {IdentityProviderAPIConfig[]} identityManifests - The manifests corresponding 1-to-1 with identityProviders
   * @returns {Promise<number>} The user-selected identity provider
   */
  async showProviderPrompt(
    browsingContext,
    identityProviders,
    identityManifests
  ) {
    // For testing only.
    if (lazy.SELECT_FIRST_IN_UI_LISTS) {
      return fulfilledPromiseFromFirstListElement(identityProviders);
    }
    let browser = browsingContext.top.embedderElement;
    if (!browser) {
      throw new Error("Null browser provided");
    }

    if (identityProviders.length != identityManifests.length) {
      throw new Error("Mismatch argument array length");
    }

    // Map each identity manifest to a promise that would resolve to its icon
    let promises = identityManifests.map(async providerManifest => {
      // we don't need to set default icon because default icon is already set on popup-notifications.inc
      const iconResult = await this.loadIconFromManifest(providerManifest);
      // If we didn't have a manifest with an icon, push a rejection.
      // This will be replaced with the default icon.
      return iconResult ? iconResult : Promise.reject();
    });

    const providerNames = identityManifests.map(
      providerManifest => providerManifest?.branding?.name
    );

    // Sanity check that we made one promise per IDP.
    if (promises.length != identityManifests.length) {
      throw new Error("Mismatch promise array length");
    }

    let iconResults = await Promise.allSettled(promises);
    if (AppConstants.platform === "android") {
      const providers = [];
      for (const [providerIndex, provider] of identityProviders.entries()) {
        let providerURL = new URL(provider.configURL);
        let displayDomain = lazy.IDNService.convertToDisplayIDN(
          providerURL.host,
          {}
        );

        let iconResult = iconResults[providerIndex];
        const data = {
          id: providerIndex,
          icon: iconResult.value,
          name: providerNames[providerIndex],
          domain: displayDomain,
        };
        providers.push(data);
      }

      return new Promise((resolve, reject) => {
        lazy.GeckoViewIdentityCredential.onShowProviderPrompt(
          browsingContext,
          providers,
          resolve,
          reject
        );
      });
    }

    // Localize all strings to be used
    // Bug 1797154 - Convert localization calls to use the async formatValues.
    let localization = new Localization(
      ["browser/identityCredentialNotification.ftl"],
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

      // Create the radio button,
      // including the check callback and the initial state
      let newRadio = newItem.getElementsByClassName(
        "identity-credential-list-item-radio"
      )[0];
      newRadio.value = providerIndex;
      newRadio.addEventListener("change", function (event) {
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

      // Set the icon to the data url if we have one
      let iconResult = iconResults[providerIndex];
      if (iconResult.status == "fulfilled") {
        let newIcon = newItem.getElementsByClassName(
          "identity-credential-list-item-icon"
        )[0];
        newIcon.setAttribute("src", iconResult.value);
      }

      // Set the words that the user sees in the selection
      newItem.getElementsByClassName(
        "identity-credential-list-item-label-primary"
      )[0].textContent = providerNames[providerIndex] || displayDomain;
      newItem.getElementsByClassName(
        "identity-credential-list-item-label-secondary"
      )[0].hidden = true;

      if (providerNames[providerIndex] && displayDomain) {
        newItem.getElementsByClassName(
          "identity-credential-list-item-label-secondary"
        )[0].hidden = false;
        newItem.getElementsByClassName(
          "identity-credential-list-item-label-secondary"
        )[0].textContent = displayDomain;
      }

      // Add the new item to the DOM!
      listBox.append(newItem);
    }

    // Create a new promise to wrap the callbacks of the popup buttons
    return new Promise((resolve, reject) => {
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
      browser.ownerDocument.getElementById(
        "identity-credential-header"
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
   * @param {IdentityProviderConfig} identityProvider - The Identity Provider that the user has selected to use
   * @param {IdentityProviderAPIConfig} identityManifest - The Identity Provider that the user has selected to use's manifest
   * @param {IdentityCredentialMetadata} identityCredentialMetadata - The metadata displayed to the user
   * @returns {Promise<bool>} A boolean representing the user's acceptance of the metadata.
   */
  async showPolicyPrompt(
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

    let iconResult = await this.loadIconFromManifest(
      identityManifest,
      BEST_HEADER_ICON_SIZE,
      "chrome://global/skin/icons/defaultFavicon.svg"
    );

    const providerName = identityManifest?.branding?.name;

    return new Promise(function (resolve, reject) {
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

      if (AppConstants.platform === "android") {
        lazy.GeckoViewIdentityCredential.onShowPolicyPrompt(
          browsingContext,
          identityCredentialMetadata.privacy_policy_url,
          identityCredentialMetadata.terms_of_service_url,
          providerDisplayDomain,
          currentBaseDomain,
          iconResult,
          resolve,
          reject
        );
      } else {
        // Localize the description
        // Bug 1797154 - Convert localization calls to use the async formatValues.
        let localization = new Localization(
          ["browser/identityCredentialNotification.ftl"],
          true
        );
        let [accept, cancel] = localization.formatMessagesSync([
          { id: "identity-credential-accept-button" },
          { id: "identity-credential-cancel-button" },
        ]);

        let cancelLabel = cancel.attributes.find(x => x.name == "label").value;
        let cancelKey = cancel.attributes.find(
          x => x.name == "accesskey"
        ).value;
        let acceptLabel = accept.attributes.find(x => x.name == "label").value;
        let acceptKey = accept.attributes.find(
          x => x.name == "accesskey"
        ).value;

        let title = localization.formatValueSync(
          "identity-credential-policy-title",
          {
            provider: providerName || providerDisplayDomain,
          }
        );

        if (iconResult) {
          let headerIcon = browser.ownerDocument.getElementsByClassName(
            "identity-credential-header-icon"
          )[0];
          headerIcon.setAttribute("src", iconResult);
        }

        const headerText = browser.ownerDocument.getElementById(
          "identity-credential-header-text"
        );
        headerText.textContent = title;

        let privacyPolicyAnchor = browser.ownerDocument.getElementById(
          "identity-credential-privacy-policy"
        );
        privacyPolicyAnchor.href =
          identityCredentialMetadata.privacy_policy_url;
        let termsOfServiceAnchor = browser.ownerDocument.getElementById(
          "identity-credential-terms-of-service"
        );
        termsOfServiceAnchor.href =
          identityCredentialMetadata.terms_of_service_url;

        // Populate the content of the policy panel
        let description = browser.ownerDocument.getElementById(
          "identity-credential-policy-explanation"
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
        ownerDocument.getElementById(
          "identity-credential-provider"
        ).hidden = true;
        ownerDocument.getElementById(
          "identity-credential-policy"
        ).hidden = false;
        ownerDocument.getElementById(
          "identity-credential-account"
        ).hidden = true;
        ownerDocument.getElementById(
          "identity-credential-header"
        ).hidden = false;
        browser.ownerGlobal.PopupNotifications.show(
          browser,
          "identity-credential",
          "",
          "identity-credential-notification-icon",
          mainAction,
          secondaryActions,
          options
        );
      }
    });
  }

  /**
   * Ask the user, using a PopupNotification, to select an account from a provided list.
   * @param {BrowsingContext} browsingContext - The BrowsingContext of the document requesting an identity credential via navigator.credentials.get()
   * @param {IdentityProviderAccountList} accountList - The list of accounts the user selects from
   * @param {IdentityProviderConfig} provider - The selected identity provider
   * @param {IdentityProviderAPIConfig} providerManifest - The manifest of the selected identity provider
   * @returns {Promise<IdentityProviderAccount>} The user-selected account
   */
  async showAccountListPrompt(
    browsingContext,
    accountList,
    provider,
    providerManifest
  ) {
    // For testing only.
    if (lazy.SELECT_FIRST_IN_UI_LISTS) {
      return fulfilledPromiseFromFirstListElement(accountList.accounts);
    }

    let browser = browsingContext.top.embedderElement;
    if (!browser) {
      throw new Error("Null browser provided");
    }

    // Map to an array of promises that resolve to a data URL,
    // encoding the corresponding account's picture
    let promises = accountList.accounts.map(async account => {
      if (!account?.picture) {
        throw new Error("Missing picture");
      }
      return fetchToDataUrl(account.picture);
    });

    // Sanity check that we made one promise per account.
    if (promises.length != accountList.accounts.length) {
      throw new Error("Incorrect number of promises obtained");
    }

    let pictureResults = await Promise.allSettled(promises);

    // Localize all strings to be used
    // Bug 1797154 - Convert localization calls to use the async formatValues.
    let localization = new Localization(
      ["browser/identityCredentialNotification.ftl"],
      true
    );
    const providerName = providerManifest?.branding?.name;
    let providerURL = new URL(provider.configURL);
    let displayDomain = lazy.IDNService.convertToDisplayIDN(
      providerURL.host,
      {}
    );

    let headerIconResult = await this.loadIconFromManifest(
      providerManifest,
      BEST_HEADER_ICON_SIZE,
      "chrome://global/skin/icons/defaultFavicon.svg"
    );

    if (AppConstants.platform === "android") {
      const accounts = [];

      for (const [accountIndex, account] of accountList.accounts.entries()) {
        var picture = "";
        let pictureResult = pictureResults[accountIndex];
        if (pictureResult.status == "fulfilled") {
          picture = pictureResult.value;
        }
        account.name;
        account.email;
        const data = {
          id: accountIndex,
          icon: picture,
          name: account.name,
          email: account.email,
        };
        accounts.push(data);
        console.log(data);
      }

      const provider = {
        name: providerName || displayDomain,
        domain: displayDomain,
        icon: headerIconResult,
      };

      const result = {
        provider,
        accounts,
      };

      return new Promise((resolve, reject) => {
        lazy.GeckoViewIdentityCredential.onShowAccountsPrompt(
          browsingContext,
          result,
          resolve,
          reject
        );
      });
    }

    let headerMessage = localization.formatValueSync(
      "identity-credential-header-accounts",
      {
        provider: providerName || displayDomain,
      }
    );

    let [accept, cancel] = localization.formatMessagesSync([
      { id: "identity-credential-sign-in-button" },
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

      // Add the new radio button, including pre-selection and the callback
      let newRadio = newItem.getElementsByClassName(
        "identity-credential-list-item-radio"
      )[0];
      newRadio.value = accountIndex;
      newRadio.addEventListener("change", function (event) {
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

      // Change the default picture if one exists
      let pictureResult = pictureResults[accountIndex];
      if (pictureResult.status == "fulfilled") {
        let newPicture = newItem.getElementsByClassName(
          "identity-credential-list-item-icon"
        )[0];
        newPicture.setAttribute("src", pictureResult.value);
      }

      // Add information to the label
      newItem.getElementsByClassName(
        "identity-credential-list-item-label-primary"
      )[0].textContent = account.name;
      newItem.getElementsByClassName(
        "identity-credential-list-item-label-secondary"
      )[0].textContent = account.email;

      // Add the item to the DOM!
      listBox.append(newItem);
    }

    // Create a new promise to wrap the callbacks of the popup buttons
    return new Promise(function (resolve, reject) {
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

      if (headerIconResult) {
        let headerIcon = browser.ownerDocument.getElementsByClassName(
          "identity-credential-header-icon"
        )[0];
        headerIcon.setAttribute("src", headerIconResult);
      }

      const headerText = browser.ownerDocument.getElementById(
        "identity-credential-header-text"
      );
      headerText.textContent = headerMessage;

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
      browser.ownerDocument.getElementById(
        "identity-credential-header"
      ).hidden = false;
      browser.ownerGlobal.PopupNotifications.show(
        browser,
        "identity-credential",
        "",
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
    if (!browser || AppConstants.platform === "android") {
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
