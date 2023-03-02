/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["FirefoxRelay"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const {
  LoginHelper,
  OptInFeature,
  ParentAutocompleteOption,
} = ChromeUtils.import("resource://gre/modules/LoginHelper.jsm");

const lazy = {};

// Static configuration
const config = (function() {
  const baseUrl = Services.prefs.getStringPref(
    "signon.firefoxRelay.base_url",
    undefined
  );
  return {
    scope: ["https://identity.mozilla.com/apps/relay"],
    addressesUrl: baseUrl + `relayaddresses/`,
    profilesUrl: baseUrl + `profiles/`,
    learnMoreURL: Services.urlFormatter.formatURLPref(
      "signon.firefoxRelay.learn_more_url"
    ),
    relayFeaturePref: "signon.firefoxRelay.feature",
  };
})();

XPCOMUtils.defineLazyGetter(lazy, "log", () =>
  LoginHelper.createLogger("FirefoxRelay")
);
XPCOMUtils.defineLazyGetter(lazy, "fxAccounts", () =>
  ChromeUtils.import(
    "resource://gre/modules/FxAccounts.jsm"
  ).getFxAccountsSingleton()
);
XPCOMUtils.defineLazyGetter(lazy, "strings", function() {
  return new Localization([
    "branding/brand.ftl",
    "browser/branding/sync-brand.ftl",
    "browser/branding/brandings.ftl",
    "browser/firefoxRelay.ftl",
  ]);
});

if (Services.appinfo.processType !== Services.appinfo.PROCESS_TYPE_DEFAULT) {
  throw new Error("FirefoxRelay.jsm should only run in the parent process");
}

async function getRelayTokenAsync() {
  try {
    return await lazy.fxAccounts.getOAuthToken({ scope: config.scope });
  } catch (e) {
    console.error(`There was an error getting the user's token: ${e.message}`);
    return undefined;
  }
}

async function hasFirefoxAccountAsync() {
  if (!lazy.fxAccounts.constructor.config.isProductionConfig()) {
    return false;
  }

  return lazy.fxAccounts.hasLocalSession();
}

async function fetchWithReauth(
  browser,
  createRequest,
  canGetFreshOAuthToken = true
) {
  const relayToken = await getRelayTokenAsync();
  if (!relayToken) {
    if (browser) {
      await showErrorAsync(browser, "firefox-relay-must-login-to-fxa");
    }
    return undefined;
  }

  const headers = new Headers({
    Authorization: `Bearer ${relayToken}`,
    Accept: "application/json",
    "Accept-Language": Services.locale.requestedLocales,
    "Content-Type": "application/json",
  });

  const request = createRequest(headers);
  const response = await fetch(request);

  if (canGetFreshOAuthToken && response.status == 401) {
    await lazy.fxAccounts.removeCachedOAuthToken({ token: relayToken });
    return fetchWithReauth(browser, createRequest, false);
  }
  return response;
}

async function isRelayUserAsync() {
  if (!(await hasFirefoxAccountAsync())) {
    return false;
  }

  const response = await fetchWithReauth(
    null,
    headers => new Request(config.profilesUrl, { headers })
  );
  if (!response) {
    return false;
  }

  if (!response.ok) {
    lazy.log.error(
      `failed to check if user is a Relay user: ${response.status}:${
        response.statusText
      }:${await response.text()}`
    );
  }

  return response.ok;
}

async function getReusableMasksAsync(browser, _origin) {
  const response = await fetchWithReauth(
    browser,
    headers =>
      new Request(config.addressesUrl, {
        method: "GET",
        headers,
      })
  );

  if (!response) {
    return undefined;
  }

  if (response.ok) {
    return response.json();
  }

  lazy.log.error(
    `failed to find reusable Relay masks: ${response.status}:${response.statusText}`
  );
  await showErrorAsync(browser, "firefox-relay-get-reusable-masks-failed", {
    status: response.status,
  });
  // Services.telemetry.recordEvent("pwmgr", "make_relay_fail", "relay");

  return undefined;
}

/**
 * Show confirmation tooltip
 * @param browser
 * @param messageId message ID from browser/browser.properties
 */
function showConfirmation(browser, messageId) {
  const anchor = browser.ownerDocument.getElementById("identity-icon");
  anchor.ownerGlobal.ConfirmationHint.show(anchor, messageId, {});
}

/**
 * Show localized notification.
 * @param browser
 * @param messageId messageId from browser/firefoxRelay.ftl
 * @param messageArgs
 */
async function showErrorAsync(browser, messageId, messageArgs) {
  const { PopupNotifications } = browser.ownerGlobal.wrappedJSObject;
  const [message] = await lazy.strings.formatValues([
    { id: messageId, args: messageArgs },
  ]);
  PopupNotifications.show(
    browser,
    "relay-integration-error",
    message,
    "password-notification-icon",
    null,
    null,
    {
      autofocus: true,
      removeOnDismissal: true,
      popupIconURL: "page-icon:https://relay.firefox.com",
      learnMoreURL: config.learnMoreURL,
    }
  );
}

function customizeNotificationHeader(notification) {
  const document = notification.owner.panel.ownerDocument;
  const description = document.querySelector(
    `description[popupid=${notification.id}]`
  );
  const headerTemplate = document.getElementById("firefox-relay-header");
  description.replaceChildren(headerTemplate.firstChild.cloneNode(true));
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

async function showReusableMasksAsync(browser, origin, errorMessage) {
  const reusableMasks = await getReusableMasksAsync(browser, origin);
  if (!reusableMasks) {
    return null;
  }

  let fillUsername;
  const fillUsernamePromise = new Promise(resolve => (fillUsername = resolve));
  const [getUnlimitedMasksStrings] = await formatMessages(
    "firefox-relay-get-unlimited-masks"
  );
  const getUnlimitedMasks = {
    label: getUnlimitedMasksStrings.label,
    accessKey: getUnlimitedMasksStrings.accesskey,
    dismiss: true,
    async callback() {
      browser.ownerGlobal.openWebLinkIn(config.learnMoreURL, "tab");
    },
  };

  let notification;

  function getReusableMasksList() {
    return notification.owner.panel.getElementsByClassName(
      "reusable-relay-masks"
    )[0];
  }

  function notificationShown() {
    customizeNotificationHeader(notification);

    notification.owner.panel.getElementsByClassName(
      "error-message"
    )[0].textContent = errorMessage;

    // rebuild "reuse mask" buttons list
    const list = getReusableMasksList();
    list.innerHTML = "";

    const document = list.ownerDocument;
    const fragment = document.createDocumentFragment();
    reusableMasks
      .filter(mask => mask.enabled)
      .forEach(mask => {
        const button = document.createElement("button");

        const maskFullAddress = document.createElement("span");
        maskFullAddress.textContent = mask.full_address;
        button.appendChild(maskFullAddress);

        const maskDescription = document.createElement("span");
        maskDescription.textContent =
          mask.description || mask.generated_for || mask.used_on;
        button.appendChild(maskDescription);

        button.addEventListener("click", () => {
          notification.remove();
          lazy.log.info("Reusing Relay mask");
          fillUsername(mask.full_address);
        });
        fragment.appendChild(button);
      });
    list.appendChild(fragment);
  }

  function notificationRemoved() {
    const list = getReusableMasksList();
    list.innerHTML = "";
  }

  function onNotificationEvent(event) {
    switch (event) {
      case "removed":
        notificationRemoved();
        break;
      case "shown":
        notificationShown();
        break;
    }
  }

  const { PopupNotifications } = browser.ownerGlobal.wrappedJSObject;
  notification = PopupNotifications.show(
    browser,
    "relay-integration-reuse-masks",
    "", // content is provided after popup shown
    "password-notification-icon",
    getUnlimitedMasks,
    [],
    {
      autofocus: true,
      removeOnDismissal: true,
      eventCallback: onNotificationEvent,
    }
  );

  return fillUsernamePromise;
}

async function generateUsernameAsync(browser, origin) {
  const body = JSON.stringify({
    enabled: true,
    description: origin.substr(0, 64),
    generated_for: origin.substr(0, 255),
    used_on: origin,
  });

  const response = await fetchWithReauth(
    browser,
    headers =>
      new Request(config.addressesUrl, {
        method: "POST",
        headers,
        body,
      })
  );

  if (!response) {
    return undefined;
  }

  if (response.ok) {
    lazy.log.info(`generated Relay mask`);
    const result = await response.json();
    showConfirmation(browser, "confirmation-hint-firefox-relay-mask-generated");
    // Services.telemetry.recordEvent("pwmgr", "make_relay", "relay");
    return result.full_address;
  }

  if (response.status == 403) {
    const error = await response.json();
    if (error?.error_code == "free_tier_limit") {
      return showReusableMasksAsync(browser, origin, error.detail);
    }
  }

  lazy.log.error(
    `failed to generate Relay mask: ${response.status}:${response.statusText}`
  );

  await showErrorAsync(browser, "firefox-relay-mask-generation-failed", {
    status: response.status,
  });
  // Services.telemetry.recordEvent("pwmgr", "make_relay_fail", "relay");

  return undefined;
}

function isSignup(scenarioName) {
  return scenarioName == "SignUpFormScenario";
}

class RelayOffered {
  #isRelayUser;

  async *autocompleteItemsAsync(_origin, scenarioName, hasInput) {
    if (!hasInput && isSignup(scenarioName)) {
      if (this.#isRelayUser === undefined) {
        this.#isRelayUser = await isRelayUserAsync();
      }

      if (this.#isRelayUser) {
        const [title, subtitle] = await formatMessages(
          "firefox-relay-opt-in-title",
          "firefox-relay-opt-in-subtitle"
        );
        yield new ParentAutocompleteOption(
          "page-icon:https://relay.firefox.com",
          title,
          subtitle,
          "PasswordManager:offerRelayIntegration",
          null
        );
      }
    }
  }

  async offerRelayIntegration(feature, browser, origin) {
    const fxaUser = await lazy.fxAccounts.getSignedInUser();
    if (!fxaUser) {
      return null;
    }
    const { PopupNotifications } = browser.ownerGlobal.wrappedJSObject;
    let fillUsername;
    const fillUsernamePromise = new Promise(
      resolve => (fillUsername = resolve)
    );
    const [
      enableStrings,
      disableStrings,
      postponeStrings,
    ] = await formatMessages(
      "firefox-relay-opt-in-confirmation-enable",
      "firefox-relay-opt-in-confirmation-disable",
      "firefox-relay-opt-in-confirmation-postpone"
    );
    const enableIntegration = {
      label: enableStrings.label,
      accessKey: enableStrings.accesskey,
      dismiss: true,
      async callback() {
        lazy.log.info("user opted in to Firefox Relay integration");
        feature.markAsEnabled();
        fillUsername(await generateUsernameAsync(browser, origin));
      },
    };
    const postpone = {
      label: postponeStrings.label,
      accessKey: postponeStrings.accesskey,
      dismiss: true,
      callback() {
        lazy.log.info(
          "user decided not to decide about Firefox Relay integration"
        );
        feature.markAsOffered();
      },
    };
    const disableIntegration = {
      label: disableStrings.label,
      accessKey: disableStrings.accesskey,
      dismiss: true,
      callback() {
        lazy.log.info("user opted out from Firefox Relay integration");
        feature.markAsDisabled();
      },
    };
    let notification;
    feature.markAsOffered();
    notification = PopupNotifications.show(
      browser,
      "relay-integration-offer",
      "", // content is provided after popup shown
      "password-notification-icon",
      enableIntegration,
      [postpone, disableIntegration],
      {
        autofocus: true,
        removeOnDismissal: true,
        learnMoreURL: config.learnMoreURL,
        eventCallback: event => {
          switch (event) {
            case "shown":
              customizeNotificationHeader(notification);
              const document = notification.owner.panel.ownerDocument;
              const content = document.querySelector(
                `popupnotification[id=${notification.id}-notification] popupnotificationcontent`
              );
              const line3 = content.querySelector(
                "[id=firefox-relay-offer-what-relay-does]"
              );
              document.l10n.setAttributes(
                line3,
                "firefox-relay-offer-what-relay-does",
                {
                  sitename: origin,
                  useremail: fxaUser.email,
                }
              );
              break;
          }
        },
      }
    );

    return fillUsernamePromise;
  }
}

class RelayEnabled {
  async *autocompleteItemsAsync(origin, scenarioName, hasInput) {
    if (
      !hasInput &&
      isSignup(scenarioName) &&
      (await hasFirefoxAccountAsync())
    ) {
      const [title, subtitle] = await formatMessages(
        "firefox-relay-generate-mask-title",
        "firefox-relay-generate-mask-subtitle"
      );
      yield new ParentAutocompleteOption(
        "page-icon:https://relay.firefox.com",
        title,
        subtitle,
        "PasswordManager:generateRelayUsername",
        origin
      );
    }
  }

  async generateUsername(browser, origin) {
    return generateUsernameAsync(browser, origin);
  }
}

class RelayDisabled {}

class RelayFeature extends OptInFeature {
  constructor() {
    super(RelayOffered, RelayEnabled, RelayDisabled, config.relayFeaturePref);
  }

  get learnMoreUrl() {
    return config.learnMoreURL;
  }

  async autocompleteItemsAsync({ origin, scenarioName, hasInput }) {
    const result = [];

    if (this.implementation.autocompleteItemsAsync) {
      for await (const item of this.implementation.autocompleteItemsAsync(
        origin,
        scenarioName,
        hasInput
      )) {
        result.push(item);
      }
    }

    return result;
  }

  async generateUsername(browser, origin) {
    return this.implementation.generateUsername?.(browser, origin);
  }

  async offerRelayIntegration(browser, origin) {
    return this.implementation.offerRelayIntegration?.(this, browser, origin);
  }
}

const FirefoxRelay = new RelayFeature();
