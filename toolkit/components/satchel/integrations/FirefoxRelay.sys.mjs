/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { FirefoxRelayTelemetry } from "resource://gre/modules/FirefoxRelayTelemetry.mjs";
import {
  LoginHelper,
  OptInFeature,
  ParentAutocompleteOption,
} from "resource://gre/modules/LoginHelper.sys.mjs";
import { TelemetryUtils } from "resource://gre/modules/TelemetryUtils.sys.mjs";
import { showConfirmation } from "resource://gre/modules/FillHelpers.sys.mjs";

const lazy = {};

// Static configuration
const gConfig = (function () {
  const baseUrl = Services.prefs.getStringPref(
    "signon.firefoxRelay.base_url",
    undefined
  );
  return {
    scope: ["profile", "https://identity.mozilla.com/apps/relay"],
    addressesUrl: baseUrl + `relayaddresses/`,
    acceptTermsUrl: baseUrl + `terms-accepted-user/`,
    profilesUrl: baseUrl + `profiles/`,
    learnMoreURL: Services.urlFormatter.formatURLPref(
      "signon.firefoxRelay.learn_more_url"
    ),
    manageURL: Services.urlFormatter.formatURLPref(
      "signon.firefoxRelay.manage_url"
    ),
    relayFeaturePref: "signon.firefoxRelay.feature",
    termsOfServiceUrl: Services.urlFormatter.formatURLPref(
      "signon.firefoxRelay.terms_of_service_url"
    ),
    privacyPolicyUrl: Services.urlFormatter.formatURLPref(
      "signon.firefoxRelay.privacy_policy_url"
    ),
  };
})();

ChromeUtils.defineLazyGetter(lazy, "log", () =>
  LoginHelper.createLogger("FirefoxRelay")
);
ChromeUtils.defineLazyGetter(lazy, "fxAccounts", () =>
  ChromeUtils.importESModule(
    "resource://gre/modules/FxAccounts.sys.mjs"
  ).getFxAccountsSingleton()
);
ChromeUtils.defineLazyGetter(lazy, "strings", function () {
  return new Localization([
    "branding/brand.ftl",
    "browser/firefoxRelay.ftl",
    "toolkit/branding/brandings.ftl",
  ]);
});

if (Services.appinfo.processType !== Services.appinfo.PROCESS_TYPE_DEFAULT) {
  throw new Error("FirefoxRelay.sys.mjs should only run in the parent process");
}

// Using 418 to avoid conflict with other standard http error code
const AUTH_TOKEN_ERROR_CODE = 418;

let gFlowId;

async function getRelayTokenAsync() {
  try {
    return await lazy.fxAccounts.getOAuthToken({ scope: gConfig.scope });
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
      await showErrorAsync(browser, "firefox-relay-must-login-to-account");
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

async function getReusableMasksAsync(browser, _origin) {
  const response = await fetchWithReauth(
    browser,
    headers =>
      new Request(gConfig.addressesUrl, {
        method: "GET",
        headers,
      })
  );

  if (!response) {
    // fetchWithReauth only returns undefined if login / obtaining a token failed.
    // Otherwise, it will return a response object.
    return [undefined, AUTH_TOKEN_ERROR_CODE];
  }

  if (response.ok) {
    return [await response.json(), response.status];
  }

  lazy.log.error(
    `failed to find reusable Relay masks: ${response.status}:${response.statusText}`
  );
  await showErrorAsync(browser, "firefox-relay-get-reusable-masks-failed", {
    status: response.status,
  });

  return [undefined, response.status];
}

/**
 * Show localized notification.
 *
 * @param {*} browser
 * @param {*} messageId from browser/firefoxRelay.ftl
 * @param {object} messageArgs
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
      popupIconURL: "chrome://browser/content/logos/relay.svg",
      learnMoreURL: gConfig.learnMoreURL,
    }
  );
}

function customizeNotificationHeader(notification) {
  if (!notification) {
    return;
  }
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

async function showReusableMasksAsync(browser, origin, error) {
  const [reusableMasks, status] = await getReusableMasksAsync(browser, origin);
  if (!reusableMasks) {
    FirefoxRelayTelemetry.recordRelayReusePanelEvent("shown", gFlowId, status);
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
      FirefoxRelayTelemetry.recordRelayReusePanelEvent(
        "get_unlimited_masks",
        gFlowId
      );
      browser.ownerGlobal.openWebLinkIn(gConfig.manageURL, "tab");
    },
  };

  let notification;

  function getReusableMasksList() {
    return notification?.owner.panel.getElementsByClassName(
      "reusable-relay-masks"
    )[0];
  }

  function notificationShown() {
    if (!notification) {
      return;
    }

    customizeNotificationHeader(notification);

    notification.owner.panel.getElementsByClassName(
      "error-message"
    )[0].textContent = error.detail || "";

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

        button.addEventListener(
          "click",
          () => {
            notification.remove();
            lazy.log.info("Reusing Relay mask");
            fillUsername(mask.full_address);
            showConfirmation(
              browser,
              "confirmation-hint-firefox-relay-mask-reused"
            );
            FirefoxRelayTelemetry.recordRelayReusePanelEvent(
              "reuse_mask",
              gFlowId
            );
          },
          { once: true }
        );
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
        FirefoxRelayTelemetry.recordRelayReusePanelEvent("shown", gFlowId);
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
      new Request(gConfig.addressesUrl, {
        method: "POST",
        headers,
        body,
      })
  );

  if (!response) {
    FirefoxRelayTelemetry.recordRelayUsernameFilledEvent(
      "shown",
      gFlowId,
      AUTH_TOKEN_ERROR_CODE
    );
    return undefined;
  }

  if (response.ok) {
    lazy.log.info(`generated Relay mask`);
    const result = await response.json();
    showConfirmation(browser, "confirmation-hint-firefox-relay-mask-created");
    return result.full_address;
  }

  if (response.status == 403) {
    const error = await response.json();
    if (error?.error_code == "free_tier_limit") {
      FirefoxRelayTelemetry.recordRelayUsernameFilledEvent(
        "shown",
        gFlowId,
        error?.error_code
      );
      return showReusableMasksAsync(browser, origin, error);
    }
  }

  lazy.log.error(
    `failed to generate Relay mask: ${response.status}:${response.statusText}`
  );

  await showErrorAsync(browser, "firefox-relay-mask-generation-failed", {
    status: response.status,
  });

  FirefoxRelayTelemetry.recordRelayReusePanelEvent(
    "shown",
    gFlowId,
    response.status
  );

  return undefined;
}

function isSignup(scenarioName) {
  return scenarioName == "SignUpFormScenario";
}

class RelayOffered {
  async *autocompleteItemsAsync(_origin, scenarioName, hasInput) {
    if (
      !hasInput &&
      isSignup(scenarioName) &&
      (await hasFirefoxAccountAsync()) &&
      !Services.prefs.prefIsLocked("signon.firefoxRelay.feature")
    ) {
      const [title, subtitle] = await formatMessages(
        "firefox-relay-opt-in-title-1",
        "firefox-relay-opt-in-subtitle-1"
      );
      yield new ParentAutocompleteOption(
        "chrome://browser/content/logos/relay.svg",
        title,
        subtitle,
        "PasswordManager:offerRelayIntegration",
        {
          telemetry: {
            flowId: gFlowId,
            scenarioName,
          },
        }
      );
      FirefoxRelayTelemetry.recordRelayOfferedEvent(
        "shown",
        gFlowId,
        scenarioName
      );
    }
  }

  async notifyServerTermsAcceptedAsync(browser) {
    const response = await fetchWithReauth(
      browser,
      headers =>
        new Request(gConfig.acceptTermsUrl, {
          method: "POST",
          headers,
        })
    );

    if (!response?.ok) {
      lazy.log.error(
        `failed to notify server that terms are accepted : ${response?.status}:${response?.statusText}`
      );

      let error;
      try {
        error = await response?.json();
      } catch {}
      await showErrorAsync(browser, "firefox-relay-mask-generation-failed", {
        status: error?.detail || response.status,
      });
      return false;
    }

    return true;
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
    const [enableStrings, disableStrings, postponeStrings] =
      await formatMessages(
        "firefox-relay-opt-in-confirmation-enable-button",
        "firefox-relay-opt-in-confirmation-disable",
        "firefox-relay-opt-in-confirmation-postpone"
      );
    const enableIntegration = {
      label: enableStrings.label,
      accessKey: enableStrings.accesskey,
      dismiss: true,
      callback: async () => {
        lazy.log.info("user opted in to Firefox Relay integration");
        // Capture the flowId here since async operations might take some time to resolve
        // and by then gFlowId might have another value
        const flowId = gFlowId;
        if (await this.notifyServerTermsAcceptedAsync(browser)) {
          feature.markAsEnabled();
          FirefoxRelayTelemetry.recordRelayOptInPanelEvent("enabled", flowId);
          fillUsername(await generateUsernameAsync(browser, origin));
        }
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
        FirefoxRelayTelemetry.recordRelayOptInPanelEvent("postponed", gFlowId);
      },
    };
    const disableIntegration = {
      label: disableStrings.label,
      accessKey: disableStrings.accesskey,
      dismiss: true,
      callback() {
        lazy.log.info("user opted out from Firefox Relay integration");
        feature.markAsDisabled();
        FirefoxRelayTelemetry.recordRelayOptInPanelEvent("disabled", gFlowId);
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
        learnMoreURL: gConfig.learnMoreURL,
        eventCallback: event => {
          switch (event) {
            case "shown":
              customizeNotificationHeader(notification);
              const document = notification.owner.panel.ownerDocument;
              const tosLink = document.getElementById(
                "firefox-relay-offer-tos-url"
              );
              tosLink.href = gConfig.termsOfServiceUrl;
              const privacyLink = document.getElementById(
                "firefox-relay-offer-privacy-url"
              );
              privacyLink.href = gConfig.privacyPolicyUrl;
              const content = document.querySelector(
                `popupnotification[id=${notification.id}-notification] popupnotificationcontent`
              );
              const line3 = content.querySelector(
                "[id=firefox-relay-offer-what-relay-provides]"
              );
              document.l10n.setAttributes(
                line3,
                "firefox-relay-offer-what-relay-provides",
                {
                  useremail: fxaUser.email,
                }
              );
              FirefoxRelayTelemetry.recordRelayOptInPanelEvent(
                "shown",
                gFlowId
              );
              break;
          }
        },
      }
    );
    getRelayTokenAsync();
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
      const [title] = await formatMessages("firefox-relay-use-mask-title");
      yield new ParentAutocompleteOption(
        "chrome://browser/content/logos/relay.svg",
        title,
        "", // when the user has opted-in, there is no subtitle content
        "PasswordManager:generateRelayUsername",
        {
          telemetry: {
            flowId: gFlowId,
          },
        }
      );
      FirefoxRelayTelemetry.recordRelayUsernameFilledEvent("shown", gFlowId);
    }
  }

  async generateUsername(browser, origin) {
    return generateUsernameAsync(browser, origin);
  }
}

class RelayDisabled {}

class RelayFeature extends OptInFeature {
  constructor() {
    super(RelayOffered, RelayEnabled, RelayDisabled, gConfig.relayFeaturePref);
    Services.telemetry.setEventRecordingEnabled("relay_integration", true);
    // Update the config when the signon.firefoxRelay.base_url pref is changed.
    // This is added mainly for tests.
    Services.prefs.addObserver(
      "signon.firefoxRelay.base_url",
      this.updateConfig
    );
  }

  get learnMoreUrl() {
    return gConfig.learnMoreURL;
  }

  updateConfig() {
    const newBaseUrl = Services.prefs.getStringPref(
      "signon.firefoxRelay.base_url"
    );
    gConfig.addressesUrl = newBaseUrl + `relayaddresses/`;
    gConfig.profilesUrl = newBaseUrl + `profiles/`;
    gConfig.acceptTermsUrl = newBaseUrl + `terms-accepted-user/`;
  }

  async autocompleteItemsAsync({ origin, scenarioName, hasInput }) {
    const result = [];

    // Generate a flowID to unique identify a series of user action. FlowId
    // allows us to link users' interaction on different UI component (Ex. autocomplete, notification)
    // We can use flowID to build the Funnel Diagram
    // This value need to always be regenerated in the entry point of an user
    // action so we overwrite the previous one.
    gFlowId = TelemetryUtils.generateUUID();

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

export const FirefoxRelay = new RelayFeature();
