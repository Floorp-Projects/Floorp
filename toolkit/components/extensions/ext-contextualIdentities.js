"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-toolkit.js */

XPCOMUtils.defineLazyModuleGetter(this, "ContextualIdentityService",
                                  "resource://gre/modules/ContextualIdentityService.jsm");
XPCOMUtils.defineLazyPreferenceGetter(this, "containersEnabled",
                                      "privacy.userContext.enabled");

const convertIdentity = identity => {
  let result = {
    name: ContextualIdentityService.getUserContextLabel(identity.userContextId),
    icon: identity.icon,
    color: identity.color,
    cookieStoreId: getCookieStoreIdForContainer(identity.userContextId),
  };

  return result;
};

this.contextualIdentities = class extends ExtensionAPI {
  getAPI(context) {
    let self = {
      contextualIdentities: {
        get(cookieStoreId) {
          if (!containersEnabled) {
            return Promise.resolve(false);
          }

          let containerId = getContainerForCookieStoreId(cookieStoreId);
          if (!containerId) {
            return Promise.resolve(null);
          }

          let identity = ContextualIdentityService.getPublicIdentityFromId(containerId);
          return Promise.resolve(convertIdentity(identity));
        },

        query(details) {
          if (!containersEnabled) {
            return Promise.resolve(false);
          }

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
          if (!containersEnabled) {
            return Promise.resolve(false);
          }

          let identity = ContextualIdentityService.create(details.name,
                                                          details.icon,
                                                          details.color);
          return Promise.resolve(convertIdentity(identity));
        },

        update(cookieStoreId, details) {
          if (!containersEnabled) {
            return Promise.resolve(false);
          }

          let containerId = getContainerForCookieStoreId(cookieStoreId);
          if (!containerId) {
            return Promise.resolve(null);
          }

          let identity = ContextualIdentityService.getPublicIdentityFromId(containerId);
          if (!identity) {
            return Promise.resolve(null);
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
            return Promise.resolve(null);
          }

          return Promise.resolve(convertIdentity(identity));
        },

        remove(cookieStoreId) {
          if (!containersEnabled) {
            return Promise.resolve(false);
          }

          let containerId = getContainerForCookieStoreId(cookieStoreId);
          if (!containerId) {
            return Promise.resolve(null);
          }

          let identity = ContextualIdentityService.getPublicIdentityFromId(containerId);
          if (!identity) {
            return Promise.resolve(null);
          }

          // We have to create the identity object before removing it.
          let convertedIdentity = convertIdentity(identity);

          if (!ContextualIdentityService.remove(identity.userContextId)) {
            return Promise.resolve(null);
          }

          return Promise.resolve(convertedIdentity);
        },
      },
    };

    return self;
  }
};
