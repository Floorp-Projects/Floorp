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
  isPrincipalInSitePermissionsBlocklist,
} from "resource://gre/modules/addons/siteperms-addon-utils.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.sys.mjs",
});
ChromeUtils.defineLazyGetter(
  lazy,
  "addonsBundle",
  () => new Localization(["toolkit/about/aboutAddons.ftl"], true)
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "SITEPERMS_ADDON_PROVIDER_ENABLED",
  SITEPERMS_ADDON_PROVIDER_PREF,
  false
);

const FIRST_CONTENT_PROCESS_TOPIC = "ipc:first-content-process-created";
const SITEPERMS_ADDON_ID_SUFFIX = "@siteperms.mozilla.org";

// Generate a per-session random salt, which is then used to generate
// per-siteOrigin hashed strings used as the addon id in SitePermsAddonWrapper constructor
// (expected to be matching new addon id generated for the same siteOrigin during
// the same browsing session and different ones in new browsing sessions).
//
// NOTE: `generateSalt` is exported for testing purpose, should not be
// used outside of tests.
let SALT;
export function generateSalt() {
  // Throw if we're not in test and SALT is already defined
  if (
    typeof SALT !== "undefined" &&
    !Services.env.exists("XPCSHELL_TEST_PROFILE_DIR")
  ) {
    throw new Error("This should only be called from XPCShell tests");
  }
  SALT = crypto.getRandomValues(new Uint8Array(12)).join("");
}

function getSalt() {
  if (!SALT) {
    generateSalt();
  }
  return SALT;
}

class SitePermsAddonWrapper {
  // An array of nsIPermission granted for the siteOrigin.
  // We can't use a Set as handlePermissionChange might be called with different
  // nsIPermission instance for the same permission (in the generic sense)
  #permissions = [];

  // This will be set to true in the `uninstall` method to recognize when a perm-changed notification
  // is actually triggered by the SitePermsAddonWrapper uninstall method itself.
  isUninstalling = false;

  /**
   * @param {string} siteOriginNoSuffix: The origin this addon is installed for
   *                                     WITHOUT the suffix generated from the
   *                                     origin attributes (see:
   *                                     nsIPrincipal.siteOriginNoSuffix).
   * @param {Array<nsIPermission>} permissions: An array of the initial
   *                                            permissions the user granted
   *                                            for the addon origin.
   */
  constructor(siteOriginNoSuffix, permissions = []) {
    this.siteOrigin = siteOriginNoSuffix;
    this.principal =
      Services.scriptSecurityManager.createContentPrincipalFromOrigin(
        this.siteOrigin
      );
    // Use a template string for the concat in case `siteOrigin` isn't a string.
    const saltedValue = `${this.siteOrigin}${getSalt()}`;
    this.id = `${computeSha256HashAsString(
      saltedValue
    )}${SITEPERMS_ADDON_ID_SUFFIX}`;

    for (const perm of permissions) {
      this.#permissions.push(perm);
    }
  }

  get isUninstalled() {
    return this.#permissions.length === 0;
  }

  /**
   * Returns the list of gated permissions types granted for the instance's origin
   *
   * @return {Array<String>}
   */
  get sitePermissions() {
    return Array.from(new Set(this.#permissions.map(perm => perm.type)));
  }

  /**
   * Update #permissions, and calls `uninstall` if there are no remaining gated permissions
   * granted. This is called by SitePermsAddonProvider when it gets a "perm-changed" notification for a gated
   * permission.
   *
   * @param {nsIPermission} permission: The permission being added/removed
   * @param {String} action: The action perm-changed notifies us about
   */
  handlePermissionChange(permission, action) {
    if (action == "added") {
      this.#permissions.push(permission);
    } else if (action == "deleted") {
      // We want to remove the registered permission for the right principal (looking into originSuffix so we
      // can unregister revoked permission on a specific context, private window, ...).
      this.#permissions = this.#permissions.filter(
        perm =>
          !(
            perm.type == permission.type &&
            perm.principal.originSuffix === permission.principal.originSuffix
          )
      );

      if (this.#permissions.length === 0) {
        this.uninstall();
      }
    }
  }

  get type() {
    return SITEPERMS_ADDON_TYPE;
  }

  get name() {
    return lazy.addonsBundle.formatValueSync("addon-sitepermission-host", {
      host: this.principal.host,
    });
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
    if (this.isUninstalling) {
      return;
    }
    try {
      this.isUninstalling = true;
      lazy.AddonManagerPrivate.callAddonListeners(
        "onUninstalling",
        this,
        false
      );
      const permissions = [...this.#permissions];
      for (const permission of permissions) {
        try {
          Services.perms.removeFromPrincipal(
            permission.principal,
            permission.type
          );
          // Only remove the permission from the array if it was successfully removed from the principal
          this.#permissions.splice(this.#permissions.indexOf(permission), 1);
        } catch (err) {
          Cu.reportError(err);
        }
      }
      lazy.AddonManagerPrivate.callAddonListeners("onUninstalled", this);
    } finally {
      this.isUninstalling = false;
    }
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

class SitePermsAddonInstalling extends SitePermsAddonWrapper {
  #install = null;

  /**
   * @param {string} siteOriginNoSuffix: The origin this addon is installed
   *                                     for, WITHOUT the suffix generated from
   *                                     the origin attributes (see:
   *                                     nsIPrincipal.siteOriginNoSuffix).
   * @param {SitePermsAddonInstall} install: The SitePermsAddonInstall instance
   *                                         calling this constructor.
   */
  constructor(siteOriginNoSuffix, install) {
    // SitePermsAddonWrapper expect an array of nsIPermission as its second parameter.
    // Since we don't have a proper permission here, we pass an object with the properties
    // being used in the class.
    const permission = {
      principal: install.principal,
      type: install.newSitePerm,
    };

    super(siteOriginNoSuffix, [permission]);
    this.#install = install;
  }

  get existingAddon() {
    return SitePermsAddonProvider.wrappersMapByOrigin.get(this.siteOrigin);
  }

  uninstall() {
    // While about:addons tab is already open, new addon cards for newly installed
    // addons are created from the `onInstalled` AOM events, for the `SitePermsAddonWrapper`
    // the `onInstalling` and `onInstalled` events are emitted by `SitePermsAddonInstall`
    // and the addon instance is going to be a `SitePermsAddonInstalling` instance if
    // there wasn't an AddonCard for the same addon id yet.
    //
    // To make sure that all permissions will be uninstalled if a user uninstall the
    // addon from an AddonCard created from a `SitePermsAddonInstalling` instance,
    // we forward calls to the uninstall method of the existing `SitePermsAddonWrapper`
    // instance being tracked by the `SitePermsAddonProvider`.
    // If there isn't any then removing only the single permission added along with the
    // `SitePremsAddonInstalling` is going to be enough.
    if (this.existingAddon) {
      return this.existingAddon.uninstall();
    }

    return super.uninstall();
  }

  validInstallOrigins() {
    // Always return true from here,
    // actual checks are done from AddonManagerInternal.getSitePermsAddonInstallForWebpage
    return true;
  }
}

// Numeric id included in the install telemetry events to correlate multiple events related
// to the same install or update flow.
let nextInstallId = 0;

class SitePermsAddonInstall {
  #listeners = new Set();
  #installEvents = {
    INSTALL_CANCELLED: "onInstallCancelled",
    INSTALL_ENDED: "onInstallEnded",
    INSTALL_FAILED: "onInstallFailed",
  };

  /**
   * @param {nsIPrincipal} installingPrincipal
   * @param {String} sitePerm
   */
  constructor(installingPrincipal, sitePerm) {
    this.principal = installingPrincipal;
    this.newSitePerm = sitePerm;
    this.state = lazy.AddonManager.STATE_DOWNLOADED;
    this.addon = new SitePermsAddonInstalling(
      this.principal.siteOriginNoSuffix,
      this
    );
    this.installId = ++nextInstallId;
  }

  get installTelemetryInfo() {
    return this.addon.installTelemetryInfo;
  }

  async checkPrompt() {
    // `promptHandler` can be set from `AddonManagerInternal.setupPromptHandler`
    if (this.promptHandler) {
      let info = {
        // TODO: Investigate if we need to handle addon "update", i.e. granting new
        // gated permission on an origin other permissions were already granted for (Bug 1790778).
        existingAddon: null,
        addon: this.addon,
        icon: "chrome://mozapps/skin/extensions/category-sitepermission.svg",
        // Used in AMTelemetry to detect the install flow related to this prompt.
        install: this,
      };

      try {
        await this.promptHandler(info);
      } catch (err) {
        if (this.error < 0) {
          this.state = lazy.AddonManager.STATE_INSTALL_FAILED;
          // In some cases onOperationCancelled is called during failures
          // to install/uninstall/enable/disable addons.  We may need to
          // do that here in the future.
          this.#callInstallListeners(this.#installEvents.INSTALL_FAILED);
        } else if (this.state !== lazy.AddonManager.STATE_CANCELLED) {
          this.cancel();
        }
        return;
      }
    }

    this.state = lazy.AddonManager.STATE_PROMPTS_DONE;
    this.install();
  }

  install() {
    if (this.state === lazy.AddonManager.STATE_PROMPTS_DONE) {
      lazy.AddonManagerPrivate.callAddonListeners("onInstalling", this.addon);
      Services.perms.addFromPrincipal(
        this.principal,
        this.newSitePerm,
        Services.perms.ALLOW_ACTION
      );
      this.state = lazy.AddonManager.STATE_INSTALLED;
      this.#callInstallListeners(this.#installEvents.INSTALL_ENDED);
      lazy.AddonManagerPrivate.callAddonListeners("onInstalled", this.addon);
      this.addon.install = null;
      return;
    }

    if (this.state !== lazy.AddonManager.STATE_DOWNLOADED) {
      this.state = lazy.AddonManager.STATE_INSTALL_FAILED;
      this.#callInstallListeners(this.#installEvents.INSTALL_FAILED);
      return;
    }

    this.checkPrompt();
  }

  cancel() {
    // This method can be called if the install is already cancelled.
    // We don't want to go further in such case as it would lead to duplicated Telemetry events.
    if (this.state == lazy.AddonManager.STATE_CANCELLED) {
      console.error("SitePermsAddonInstall#cancel called twice on ", this);
      return;
    }

    this.state = lazy.AddonManager.STATE_CANCELLED;
    this.#callInstallListeners(this.#installEvents.INSTALL_CANCELLED);
  }

  /**
   * Add a listener for the install events
   *
   * @param {Object} listener
   * @param {Function} [listener.onDownloadEnded]
   * @param {Function} [listener.onInstallCancelled]
   * @param {Function} [listener.onInstallEnded]
   * @param {Function} [listener.onInstallFailed]
   */
  addListener(listener) {
    this.#listeners.add(listener);
  }

  /**
   * Remove a listener
   *
   * @param {Object} listener: The same object reference that was used for `addListener`
   */
  removeListener(listener) {
    this.#listeners.delete(listener);
  }

  /**
   * Call the listeners callbacks for a given event.
   *
   * @param {String} eventName: The event to fire. Should be one of `this.#installEvents`
   */
  #callInstallListeners(eventName) {
    if (!Object.values(this.#installEvents).includes(eventName)) {
      console.warn(`Unknown "${eventName}" "event`);
      return;
    }

    lazy.AddonManagerPrivate.callInstallListeners(
      eventName,
      Array.from(this.#listeners),
      this
    );
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

    if (isPrincipalInSitePermissionsBlocklist(permission.principal)) {
      return;
    }

    const { siteOriginNoSuffix } = permission.principal;

    // Install origin cannot be on a known etld (e.g. github.io).
    // We shouldn't get a permission change for those here, but let's
    // be  extra safe
    if (isKnownPublicSuffix(siteOriginNoSuffix)) {
      return;
    }

    // Pipe the change to the existing addon if there is one.
    if (this.wrappersMapByOrigin.has(siteOriginNoSuffix)) {
      this.wrappersMapByOrigin
        .get(siteOriginNoSuffix)
        .handlePermissionChange(permission, action);
    }

    if (action == "added") {
      // We only have one SitePermsAddon per origin, handling multiple permissions.
      if (this.wrappersMapByOrigin.has(siteOriginNoSuffix)) {
        return;
      }

      const addonWrapper = new SitePermsAddonWrapper(siteOriginNoSuffix, [
        permission,
      ]);
      this.wrappersMapByOrigin.set(siteOriginNoSuffix, addonWrapper);
      return;
    }

    if (action == "deleted") {
      if (!this.wrappersMapByOrigin.has(siteOriginNoSuffix)) {
        return;
      }
      // Only remove the addon if it doesn't have any permissions left.
      if (!this.wrappersMapByOrigin.get(siteOriginNoSuffix).isUninstalled) {
        return;
      }
      this.wrappersMapByOrigin.delete(siteOriginNoSuffix);
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
    if (this._initPromise) {
      Services.obs.removeObserver(this, "perm-changed");
    }
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

  /**
   * Create and return a SitePermsAddonInstall instance for a permission on a given principal
   *
   * @param {nsIPrincipal} installingPrincipal
   * @param {String} sitePerm
   * @returns {SitePermsAddonInstall}
   */
  getSitePermsAddonInstallForWebpage(installingPrincipal, sitePerm) {
    return new SitePermsAddonInstall(installingPrincipal, sitePerm);
  },

  get isEnabled() {
    return lazy.SITEPERMS_ADDON_PROVIDER_ENABLED;
  },

  observe(subject, topic, data) {
    if (!this.isEnabled) {
      return;
    }

    if (topic == FIRST_CONTENT_PROCESS_TOPIC) {
      Services.obs.removeObserver(this, FIRST_CONTENT_PROCESS_TOPIC);

      lazy.AddonManagerPrivate.registerProvider(SitePermsAddonProvider, [
        SITEPERMS_ADDON_TYPE,
      ]);
      Services.obs.notifyObservers(null, "sitepermsaddon-provider-registered");
    } else if (topic === "perm-changed") {
      if (data === "cleared") {
        // In such case, `subject` is null, but we can simply uninstall all existing addons.
        for (const addon of this.wrappersMapByOrigin.values()) {
          addon.uninstall();
        }
        this.wrappersMapByOrigin.clear();
        return;
      }

      const perm = subject.QueryInterface(Ci.nsIPermission);
      this.handlePermissionChange(perm, data);
    }
  },

  addFirstContentProcessObserver() {
    Services.obs.addObserver(this, FIRST_CONTENT_PROCESS_TOPIC);
  },
};

// We want to register the SitePermsAddonProvider once the first content process gets created
// (and only if the feature is also enabled through the "dom.sitepermsaddon-provider.enabled"
// about:config pref).
SitePermsAddonProvider.addFirstContentProcessObserver();
