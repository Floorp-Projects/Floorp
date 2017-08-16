"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-toolkit.js */

XPCOMUtils.defineLazyModuleGetter(this, "ContextualIdentityService",
                                  "resource://gre/modules/ContextualIdentityService.jsm");
XPCOMUtils.defineLazyPreferenceGetter(this, "containersEnabled",
                                      "privacy.userContext.enabled");

Cu.import("resource://gre/modules/ExtensionPreferencesManager.jsm");

const CONTAINER_PREF_INSTALL_DEFAULTS = {
  "privacy.userContext.enabled": true,
  "privacy.userContext.longPressBehavior": 2,
  "privacy.userContext.ui.enabled": true,
  "privacy.usercontext.about_newtab_segregation.enabled": true,
};

const CONTAINERS_ENABLED_SETTING_NAME = "privacy.containers";

const convertIdentity = identity => {
  let result = {
    name: ContextualIdentityService.getUserContextLabel(identity.userContextId),
    icon: identity.icon,
    color: identity.color,
    cookieStoreId: getCookieStoreIdForContainer(identity.userContextId),
  };

  return result;
};

const convertIdentityFromObserver = wrappedIdentity => {
  let identity = wrappedIdentity.wrappedJSObject;
  let result = {
    name: identity.name,
    icon: identity.icon,
    color: identity.color,
    cookieStoreId: getCookieStoreIdForContainer(identity.userContextId),
  };

  return result;
};

ExtensionPreferencesManager.addSetting(CONTAINERS_ENABLED_SETTING_NAME, {
  prefNames: Object.keys(CONTAINER_PREF_INSTALL_DEFAULTS),

  setCallback(value) {
    if (value === true) {
      return CONTAINER_PREF_INSTALL_DEFAULTS;
    }

    let prefs = {};
    for (let pref of this.prefNames) {
      prefs[pref] = undefined;
    }
    return prefs;
  },
});

this.contextualIdentities = class extends ExtensionAPI {
  onStartup() {
    let {extension} = this;

    if (extension.hasPermission("contextualIdentities")) {
      ExtensionPreferencesManager.setSetting(extension, CONTAINERS_ENABLED_SETTING_NAME, true);
    }
  }

  getAPI(context) {
    let self = {
      contextualIdentities: {
        get(cookieStoreId) {
          let containerId = getContainerForCookieStoreId(cookieStoreId);
          if (!containerId) {
            return Promise.reject({
              message: `Invalid contextual identitiy: ${cookieStoreId}`,
            });
          }

          let identity = ContextualIdentityService.getPublicIdentityFromId(containerId);
          return Promise.resolve(convertIdentity(identity));
        },

        query(details) {
          let identities = [];
          ContextualIdentityService.getPublicIdentities().forEach(identity => {
            if (details.name &&
                ContextualIdentityService.getUserContextLabel(identity.userContextId) != details.name) {
              return;
            }

            identities.push(convertIdentity(identity));
          });

          return Promise.resolve(identities);
        },

        create(details) {
          let identity = ContextualIdentityService.create(details.name,
                                                          details.icon,
                                                          details.color);
          return Promise.resolve(convertIdentity(identity));
        },

        update(cookieStoreId, details) {
          let containerId = getContainerForCookieStoreId(cookieStoreId);
          if (!containerId) {
            return Promise.reject({
              message: `Invalid contextual identitiy: ${cookieStoreId}`,
            });
          }

          let identity = ContextualIdentityService.getPublicIdentityFromId(containerId);
          if (!identity) {
            return Promise.reject({
              message: `Invalid contextual identitiy: ${cookieStoreId}`,
            });
          }

          if (details.name !== null) {
            identity.name = details.name;
          }

          if (details.color !== null) {
            identity.color = details.color;
          }

          if (details.icon !== null) {
            identity.icon = details.icon;
          }

          if (!ContextualIdentityService.update(identity.userContextId,
                                                identity.name, identity.icon,
                                                identity.color)) {
            return Promise.reject({
              message: `Contextual identitiy failed to update: ${cookieStoreId}`,
            });
          }

          return Promise.resolve(convertIdentity(identity));
        },

        remove(cookieStoreId) {
          let containerId = getContainerForCookieStoreId(cookieStoreId);
          if (!containerId) {
            return Promise.reject({
              message: `Invalid contextual identitiy: ${cookieStoreId}`,
            });
          }

          let identity = ContextualIdentityService.getPublicIdentityFromId(containerId);
          if (!identity) {
            return Promise.reject({
              message: `Invalid contextual identitiy: ${cookieStoreId}`,
            });
          }

          // We have to create the identity object before removing it.
          let convertedIdentity = convertIdentity(identity);

          if (!ContextualIdentityService.remove(identity.userContextId)) {
            return Promise.reject({
              message: `Contextual identitiy failed to remove: ${cookieStoreId}`,
            });
          }

          return Promise.resolve(convertedIdentity);
        },

        onCreated: new EventManager(context, "contextualIdentities.onCreated", fire => {
          let observer = (subject, topic) => {
            fire.async({contextualIdentity: convertIdentityFromObserver(subject)});
          };

          Services.obs.addObserver(observer, "contextual-identity-created");
          return () => {
            Services.obs.removeObserver(observer, "contextual-identity-created");
          };
        }).api(),

        onUpdated: new EventManager(context, "contextualIdentities.onUpdated", fire => {
          let observer = (subject, topic) => {
            fire.async({contextualIdentity: convertIdentityFromObserver(subject)});
          };

          Services.obs.addObserver(observer, "contextual-identity-updated");
          return () => {
            Services.obs.removeObserver(observer, "contextual-identity-updated");
          };
        }).api(),

        onRemoved: new EventManager(context, "contextualIdentities.onRemoved", fire => {
          let observer = (subject, topic) => {
            fire.async({contextualIdentity: convertIdentityFromObserver(subject)});
          };

          Services.obs.addObserver(observer, "contextual-identity-deleted");
          return () => {
            Services.obs.removeObserver(observer, "contextual-identity-deleted");
          };
        }).api(),

      },
    };

    return self;
  }
};
