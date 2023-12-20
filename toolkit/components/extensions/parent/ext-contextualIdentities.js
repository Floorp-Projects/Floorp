/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ContextualIdentityService:
    "resource://gre/modules/ContextualIdentityService.sys.mjs",
});
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "containersEnabled",
  "privacy.userContext.enabled"
);

var { ExtensionPreferencesManager } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPreferencesManager.sys.mjs"
);

var { ExtensionError } = ExtensionUtils;

const CONTAINER_PREF_INSTALL_DEFAULTS = {
  "privacy.userContext.extension": undefined,
};

const CONTAINERS_ENABLED_SETTING_NAME = "privacy.containers";

const CONTAINER_COLORS = new Map([
  ["blue", "#37adff"],
  ["turquoise", "#00c79a"],
  ["green", "#51cd00"],
  ["yellow", "#ffcb00"],
  ["orange", "#ff9f00"],
  ["red", "#ff613d"],
  ["pink", "#ff4bda"],
  ["purple", "#af51f5"],
  ["toolbar", "#7c7c7d"],
]);

const CONTAINER_ICONS = new Set([
  "briefcase",
  "cart",
  "circle",
  "dollar",
  "fence",
  "fingerprint",
  "gift",
  "vacation",
  "food",
  "fruit",
  "pet",
  "tree",
  "chill",
]);

function getContainerIcon(iconName) {
  if (!CONTAINER_ICONS.has(iconName)) {
    throw new ExtensionError(`Invalid icon ${iconName} for container`);
  }
  return `resource://usercontext-content/${iconName}.svg`;
}

function getContainerColor(colorName) {
  if (!CONTAINER_COLORS.has(colorName)) {
    throw new ExtensionError(`Invalid color name ${colorName} for container`);
  }
  return CONTAINER_COLORS.get(colorName);
}

const convertIdentity = identity => {
  let result = {
    name: ContextualIdentityService.getUserContextLabel(identity.userContextId),
    icon: identity.icon,
    iconUrl: getContainerIcon(identity.icon),
    color: identity.color,
    colorCode: getContainerColor(identity.color),
    cookieStoreId: getCookieStoreIdForContainer(identity.userContextId),
  };

  return result;
};

const checkAPIEnabled = () => {
  if (!containersEnabled) {
    throw new ExtensionError("Contextual identities are currently disabled");
  }
};

const convertIdentityFromObserver = wrappedIdentity => {
  let identity = wrappedIdentity.wrappedJSObject;
  let iconUrl, colorCode;
  try {
    iconUrl = getContainerIcon(identity.icon);
    colorCode = getContainerColor(identity.color);
  } catch (e) {
    return null;
  }

  let result = {
    name: identity.name,
    icon: identity.icon,
    iconUrl,
    color: identity.color,
    colorCode,
    cookieStoreId: getCookieStoreIdForContainer(identity.userContextId),
  };

  return result;
};

ExtensionPreferencesManager.addSetting(CONTAINERS_ENABLED_SETTING_NAME, {
  prefNames: Object.keys(CONTAINER_PREF_INSTALL_DEFAULTS),

  setCallback(value) {
    if (value !== true) {
      return {
        ...CONTAINER_PREF_INSTALL_DEFAULTS,
        "privacy.userContext.extension": value,
      };
    }
    return {};
  },
});

this.contextualIdentities = class extends ExtensionAPIPersistent {
  eventRegistrar(eventName) {
    return ({ fire }) => {
      let observer = (subject, topic) => {
        let convertedIdentity = convertIdentityFromObserver(subject);
        if (convertedIdentity) {
          fire.async({ contextualIdentity: convertedIdentity });
        }
      };

      Services.obs.addObserver(observer, eventName);
      return {
        unregister() {
          Services.obs.removeObserver(observer, eventName);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    };
  }

  PERSISTENT_EVENTS = {
    onCreated: this.eventRegistrar("contextual-identity-created"),
    onUpdated: this.eventRegistrar("contextual-identity-updated"),
    onRemoved: this.eventRegistrar("contextual-identity-deleted"),
  };

  onStartup() {
    let { extension } = this;

    if (extension.hasPermission("contextualIdentities")) {
      // Turn on contextual identities, and never turn it off.  We handle
      // this here to ensure prefs are set when an addon is enabled.
      Services.prefs.setBoolPref("privacy.userContext.enabled", true);
      Services.prefs.setBoolPref("privacy.userContext.ui.enabled", true);

      ExtensionPreferencesManager.setSetting(
        extension.id,
        CONTAINERS_ENABLED_SETTING_NAME,
        extension.id
      );
    }
  }

  getAPI(context) {
    let self = {
      contextualIdentities: {
        async get(cookieStoreId) {
          checkAPIEnabled();
          let containerId = getContainerForCookieStoreId(cookieStoreId);
          if (!containerId) {
            throw new ExtensionError(
              `Invalid contextual identity: ${cookieStoreId}`
            );
          }

          let identity =
            ContextualIdentityService.getPublicIdentityFromId(containerId);
          return convertIdentity(identity);
        },

        async query(details) {
          checkAPIEnabled();
          let identities = [];
          ContextualIdentityService.getPublicIdentities().forEach(identity => {
            if (
              details.name &&
              ContextualIdentityService.getUserContextLabel(
                identity.userContextId
              ) != details.name
            ) {
              return;
            }

            identities.push(convertIdentity(identity));
          });

          return identities;
        },

        async create(details) {
          // Lets prevent making containers that are not valid
          getContainerIcon(details.icon);
          getContainerColor(details.color);

          let identity = ContextualIdentityService.create(
            details.name,
            details.icon,
            details.color
          );
          return convertIdentity(identity);
        },

        async update(cookieStoreId, details) {
          checkAPIEnabled();
          let containerId = getContainerForCookieStoreId(cookieStoreId);
          if (!containerId) {
            throw new ExtensionError(
              `Invalid contextual identity: ${cookieStoreId}`
            );
          }

          let identity =
            ContextualIdentityService.getPublicIdentityFromId(containerId);
          if (!identity) {
            throw new ExtensionError(
              `Invalid contextual identity: ${cookieStoreId}`
            );
          }

          if (details.name !== null) {
            identity.name = details.name;
          }

          if (details.color !== null) {
            getContainerColor(details.color);
            identity.color = details.color;
          }

          if (details.icon !== null) {
            getContainerIcon(details.icon);
            identity.icon = details.icon;
          }

          if (
            !ContextualIdentityService.update(
              identity.userContextId,
              identity.name,
              identity.icon,
              identity.color
            )
          ) {
            throw new ExtensionError(
              `Contextual identity failed to update: ${cookieStoreId}`
            );
          }

          return convertIdentity(identity);
        },

        async move(cookieStoreIds, position) {
          checkAPIEnabled();
          if (!Array.isArray(cookieStoreIds)) {
            cookieStoreIds = [cookieStoreIds];
          }

          if (!cookieStoreIds.length) {
            return;
          }

          const totalIds =
            ContextualIdentityService.getPublicIdentities().length;
          if (position < -1 || position > totalIds - cookieStoreIds.length) {
            throw new ExtensionError(`Moving to invalid position ${position}`);
          }

          let userContextIds = [];
          cookieStoreIds.forEach((cookieStoreId, index) => {
            if (cookieStoreIds.indexOf(cookieStoreId) !== index) {
              throw new ExtensionError(
                `Duplicate contextual identity: ${cookieStoreId}`
              );
            }

            let containerId = getContainerForCookieStoreId(cookieStoreId);
            if (!containerId) {
              throw new ExtensionError(
                `Invalid contextual identity: ${cookieStoreId}`
              );
            }

            userContextIds.push(containerId);
          });

          if (!ContextualIdentityService.move(userContextIds, position)) {
            throw new ExtensionError(
              `Contextual identities failed to move: ${cookieStoreIds}`
            );
          }
        },

        async remove(cookieStoreId) {
          checkAPIEnabled();
          let containerId = getContainerForCookieStoreId(cookieStoreId);
          if (!containerId) {
            throw new ExtensionError(
              `Invalid contextual identity: ${cookieStoreId}`
            );
          }

          let identity =
            ContextualIdentityService.getPublicIdentityFromId(containerId);
          if (!identity) {
            throw new ExtensionError(
              `Invalid contextual identity: ${cookieStoreId}`
            );
          }

          // We have to create the identity object before removing it.
          let convertedIdentity = convertIdentity(identity);

          if (!ContextualIdentityService.remove(identity.userContextId)) {
            throw new ExtensionError(
              `Contextual identity failed to remove: ${cookieStoreId}`
            );
          }

          return convertedIdentity;
        },

        onCreated: new EventManager({
          context,
          module: "contextualIdentities",
          event: "onCreated",
          extensionApi: this,
        }).api(),

        onUpdated: new EventManager({
          context,
          module: "contextualIdentities",
          event: "onUpdated",
          extensionApi: this,
        }).api(),

        onRemoved: new EventManager({
          context,
          module: "contextualIdentities",
          event: "onRemoved",
          extensionApi: this,
        }).api(),
      },
    };

    return self;
  }
};
