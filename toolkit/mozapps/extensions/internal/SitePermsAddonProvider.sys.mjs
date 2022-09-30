/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { computeSha256HashAsString } from "resource://gre/modules/addons/crypto-utils.sys.mjs";
import {
  GATED_PERMISSIONS,
  SITEPERMS_ADDON_PROVIDER_PREF,
  SITEPERMS_ADDON_TYPE,
  isGatedPermissionType,
  isKnownPublicSuffix,
} from "resource://gre/modules/addons/siteperms-addon-utils.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.jsm",
});

const FIRST_CONTENT_PROCESS_TOPIC = "ipc:first-content-process-created";
const SITEPERMS_ADDON_ID_SUFFIX = "@siteperms.mozilla.org";

class SitePermsAddonWrapper {
  // A <string, nsIPermission> Map, whose keys are permission types.
  #permissionsByPermissionType = new Map();

  /**
   * @param {string} siteOrigin: The origin this addon is installed for
   * @param {Array<nsIPermission>} permissions: An array of the initial permissions the user
   *                               granted for the addon origin.
   */
  constructor(siteOrigin, permissions = []) {
    this.siteOrigin = siteOrigin;
    this.principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      this.siteOrigin
    );
    this.id = `${computeSha256HashAsString(
      this.siteOrigin
    )}${SITEPERMS_ADDON_ID_SUFFIX}`;

    for (const perm of permissions) {
      this.#permissionsByPermissionType.set(perm.type, perm);
    }
  }

  /**
   * Returns the list of gated permissions types granted for the instance's origin
   *
   * @return {Array<String>}
   */
  get sitePermissions() {
    return Array.from(this.#permissionsByPermissionType.keys());
  }

  /**
   * Update #permissionsByPermissionType, and calls `uninstall` if there are no remaining gated permissions
   * granted. This is called by SitePermsAddonProvider when it gets a "perm-changed" notification for a gated
   * permission.
   *
   * @param {nsIPermission} permission: The permission being added/removed
   * @param {String} action: The action perm-changed notifies us about
   */
  handlePermissionChange(permission, action) {
    if (action == "added") {
      this.#permissionsByPermissionType.set(permission.type, permission);
    } else if (action == "deleted") {
      this.#permissionsByPermissionType.delete(permission.type);

      if (this.#permissionsByPermissionType.size === 0) {
        this.uninstall();
      }
    }
  }

  get type() {
    return SITEPERMS_ADDON_TYPE;
  }

  get name() {
    // TODO: Localize this string (See Bug 1790313).
    return `Site permissions for ${this.principal.host}`;
  }

  get creator() {}

  get homepageURL() {}

  get description() {}

  get fullDescription() {}

  get version() {
    // We consider the previous implementation attempt (signed addons) to be the initial version,
    // hence the 2.0 for this approach.
    return "2.0";
  }
  get updateDate() {}

  get isActive() {
    return true;
  }

  get appDisabled() {
    return false;
  }

  get userDisabled() {
    return false;
  }
  set userDisabled(aVal) {}

  get size() {
    return 0;
  }

  async updateBlocklistState(options = {}) {}

  get blocklistState() {
    return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
  }

  get scope() {
    return lazy.AddonManager.SCOPE_APPLICATION;
  }

  get pendingOperations() {
    return lazy.AddonManager.PENDING_NONE;
  }

  get operationsRequiringRestart() {
    return lazy.AddonManager.OP_NEEDS_RESTART_NONE;
  }

  get permissions() {
    // The addon only supports PERM_CAN_UNINSTALL and no other AOM permission.
    return lazy.AddonManager.PERM_CAN_UNINSTALL;
  }

  get signedState() {
    // Will make the permission prompt use the webextSitePerms.headerUnsignedWithPerms string
    return lazy.AddonManager.SIGNEDSTATE_MISSING;
  }

  async enable() {}

  async disable() {}

  /**
   * Uninstall the addon, calling AddonManager hooks and removing all granted permissions.
   *
   * @throws Services.perms.removeFromPrincipal could throw, see PermissionManager::AddInternal.
   */
  async uninstall() {
    lazy.AddonManagerPrivate.callAddonListeners("onUninstalling", this, false);
    for (const permission of this.#permissionsByPermissionType.values()) {
      Services.perms.removeFromPrincipal(permission.principal, permission.type);
    }
    lazy.AddonManagerPrivate.callAddonListeners("onUninstalled", this);
  }

  get isCompatible() {
    return true;
  }

  get isPlatformCompatible() {
    return true;
  }

  get providesUpdatesSecurely() {
    return true;
  }

  get foreignInstall() {
    return false;
  }

  get installTelemetryInfo() {
    return { source: "siteperm-addon-provider", method: "synthetic-install" };
  }

  isCompatibleWith(aAppVersion, aPlatformVersion) {
    return true;
  }
}

const SitePermsAddonProvider = {
  get name() {
    return "SitePermsAddonProvider";
  },

  wrappersMapByOrigin: new Map(),

  /**
   * Update wrappersMapByOrigin on perm-changed
   *
   * @param {nsIPermission} permission: The permission being added/removed
   * @param {String} action: The action perm-changed notifies us about
   */
  handlePermissionChange(permission, action = "added") {
    // Bail out if it it's not a gated perm
    if (!isGatedPermissionType(permission.type)) {
      return;
    }

    // Gated APIs should probably not be available on non-secure origins,
    // but let's double check here.
    if (permission.principal.scheme !== "https") {
      return;
    }

    // Install origin cannot be on a known etld (e.g. github.io).
    // We shouldn't get a permission change for those here, but let's
    // be  extra safe
    if (isKnownPublicSuffix(permission.principal.siteOrigin)) {
      return;
    }

    const { siteOrigin } = permission.principal;

    // Pipe the change to the existing addon is there is one.
    if (this.wrappersMapByOrigin.has(siteOrigin)) {
      this.wrappersMapByOrigin
        .get(siteOrigin)
        .handlePermissionChange(permission, action);
    }

    if (action == "added") {
      // We only have one SitePermsAddon per origin, handling multiple permissions.
      if (this.wrappersMapByOrigin.has(siteOrigin)) {
        return;
      }

      const addonWrapper = new SitePermsAddonWrapper(siteOrigin, [permission]);
      this.wrappersMapByOrigin.set(siteOrigin, addonWrapper);
      return;
    }

    if (action == "deleted") {
      if (!this.wrappersMapByOrigin.has(siteOrigin)) {
        return;
      }
      // Only remove the addon if it doesn't have any permissions left.
      if (this.wrappersMapByOrigin.get(siteOrigin).sitePermissions.length) {
        return;
      }
      this.wrappersMapByOrigin.delete(siteOrigin);
    }
  },

  /**
   * Returns a Promise that resolves when handled the list of gated permissions
   * and setup ther observer for the "perm-changed" event.
   *
   * @returns Promise
   */
  lazyInit() {
    if (!this._initPromise) {
      this._initPromise = new Promise(resolve => {
        // Build the initial list of addons per origin
        const perms = Services.perms.getAllByTypes(GATED_PERMISSIONS);
        for (const perm of perms) {
          this.handlePermissionChange(perm);
        }
        Services.obs.addObserver(this, "perm-changed");
        resolve();
      });
    }
    return this._initPromise;
  },

  shutdown() {
    Services.obs.removeObserver(this, "perm-changed");
    this.wrappersMapByOrigin.clear();
    this._initPromise = null;
  },

  /**
   * Get a SitePermsAddonWrapper from an extension id
   *
   * @param {String|null|undefined} id: The extension id,
   * @returns {SitePermsAddonWrapper|undefined}
   */
  async getAddonByID(id) {
    await this.lazyInit();
    if (!id?.endsWith?.(SITEPERMS_ADDON_ID_SUFFIX)) {
      return undefined;
    }

    for (const addon of this.wrappersMapByOrigin.values()) {
      if (addon.id === id) {
        return addon;
      }
    }
    return undefined;
  },

  /**
   * Get a list of SitePermsAddonWrapper for a given list of extension types.
   *
   * @param {Array<String>|null|undefined} types: If null or undefined is passed,
   *        the callsites expect to get all the addons from the provider, without
   *        any filtering.
   * @returns {Array<SitePermsAddonWrapper>}
   */
  async getAddonsByTypes(types) {
    if (
      !this.isEnabled ||
      // `types` can be null/undefined, and in such case we _do_ want to return the addons.
      (Array.isArray(types) && !types.includes(SITEPERMS_ADDON_TYPE))
    ) {
      return [];
    }

    await this.lazyInit();
    return Array.from(this.wrappersMapByOrigin.values());
  },

  get isEnabled() {
    return Services.prefs.getBoolPref(SITEPERMS_ADDON_PROVIDER_PREF, false);
  },

  observe(subject, topic, data) {
    if (topic == FIRST_CONTENT_PROCESS_TOPIC) {
      Services.obs.removeObserver(this, FIRST_CONTENT_PROCESS_TOPIC);

      lazy.AddonManagerPrivate.registerProvider(SitePermsAddonProvider, [
        SITEPERMS_ADDON_TYPE,
      ]);
      Services.obs.notifyObservers(null, "sitepermsaddon-provider-registered");
    } else if (topic === "perm-changed") {
      const perm = subject.QueryInterface(Ci.nsIPermission);
      this.handlePermissionChange(perm, data);
    }
  },

  addFirstContentProcessObserver() {
    Services.obs.addObserver(this, FIRST_CONTENT_PROCESS_TOPIC);
  },
};

// We want to register the SitePermsAddonProvider once the first content process gets created.
SitePermsAddonProvider.addFirstContentProcessObserver();
