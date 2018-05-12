/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals gChromeWin */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/AddonManager.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const AMO_ICON = "chrome://browser/skin/images/amo-logo.png";
const UPDATE_INDICATOR = "chrome://browser/skin/images/extension-update.svg";

var gStringBundle = Services.strings.createBundle("chrome://browser/locale/aboutAddons.properties");

XPCOMUtils.defineLazyGetter(window, "gChromeWin", function() {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
           .getInterface(Ci.nsIWebNavigation)
           .QueryInterface(Ci.nsIDocShellTreeItem)
           .rootTreeItem
           .QueryInterface(Ci.nsIInterfaceRequestor)
           .getInterface(Ci.nsIDOMWindow)
           .QueryInterface(Ci.nsIDOMChromeWindow);
});
ChromeUtils.defineModuleGetter(window, "Preferences",
                               "resource://gre/modules/Preferences.jsm");

var ContextMenus = {
  target: null,

  init: function() {
    document.addEventListener("contextmenu", this);

    document.getElementById("contextmenu-enable").addEventListener("click", ContextMenus.enable.bind(this));
    document.getElementById("contextmenu-disable").addEventListener("click", ContextMenus.disable.bind(this));
    document.getElementById("contextmenu-uninstall").addEventListener("click", ContextMenus.uninstall.bind(this));

    // XXX - Hack to fix bug 985867 for now
    document.addEventListener("touchstart", function() { });
  },

  handleEvent: function(event) {
    // store the target of context menu events so that we know which app to act on
    this.target = event.target;
    while (!this.target.hasAttribute("contextmenu")) {
      this.target = this.target.parentNode;
    }

    if (!this.target) {
      document.getElementById("contextmenu-enable").setAttribute("hidden", "true");
      document.getElementById("contextmenu-disable").setAttribute("hidden", "true");
      document.getElementById("contextmenu-uninstall").setAttribute("hidden", "true");
      return;
    }

    let addon = this.target.addon;
    if (addon.scope == AddonManager.SCOPE_APPLICATION) {
      document.getElementById("contextmenu-uninstall").setAttribute("hidden", "true");
    } else {
      document.getElementById("contextmenu-uninstall").removeAttribute("hidden");
    }

    // Hide the enable/disable context menu items if the add-on was disabled by
    // Firefox (e.g. unsigned or blocklisted add-on).
    if (addon.appDisabled) {
      document.getElementById("contextmenu-enable").setAttribute("hidden", "true");
      document.getElementById("contextmenu-disable").setAttribute("hidden", "true");
      return;
    }

    let enabled = this.target.getAttribute("isDisabled") != "true";
    if (enabled) {
      document.getElementById("contextmenu-enable").setAttribute("hidden", "true");
      document.getElementById("contextmenu-disable").removeAttribute("hidden");
    } else {
      document.getElementById("contextmenu-enable").removeAttribute("hidden");
      document.getElementById("contextmenu-disable").setAttribute("hidden", "true");
    }
  },

  enable: function(event) {
    Addons.setEnabled(true, this.target.addon);
    this.target = null;
  },

  disable: function(event) {
    Addons.setEnabled(false, this.target.addon);
    this.target = null;
  },

  uninstall: function(event) {
    Addons.uninstall(this.target.addon);
    this.target = null;
  }
};

function sendEMPong() {
  Services.obs.notifyObservers(window, "EM-pong");
}

async function init() {
  window.addEventListener("popstate", onPopState);

  AddonManager.addInstallListener(Addons);
  AddonManager.addAddonListener(Addons);

  await Addons.init();
  showAddons();
  ContextMenus.init();

  Services.obs.addObserver(sendEMPong, "EM-ping");

  // The addons list has been loaded and rendered, send a notification
  // if the openOptionsPage is waiting to be able to select an addon details page.
  Services.obs.notifyObservers(window, "EM-loaded");
}

function uninit() {
  AddonManager.removeInstallListener(Addons);
  AddonManager.removeAddonListener(Addons);

  Services.obs.removeObserver(sendEMPong, "EM-ping");
}

function openLink(url) {
  let BrowserApp = gChromeWin.BrowserApp;
  BrowserApp.addTab(url, { selected: true, parentId: BrowserApp.selectedTab.id });
}

function openOptionsInTab(url) {
  let BrowserApp = gChromeWin.BrowserApp;
  BrowserApp.selectOrAddTab(url, {
    startsWith: true,
    selected: true,
    parentId: BrowserApp.selectedTab.id
  });
}

function onPopState(aEvent) {
  // Called when back/forward is used to change the state of the page
  if (aEvent.state) {
    // Show the detail page for an addon
    const listItem = Addons._getElementForAddon(aEvent.state.id);
    if (listItem) {
      Addons.showDetails(listItem);
    } else {
      // If the addon doesn't exist anymore, go back in the history.
      history.back();
    }
  } else {
    // Clear any previous detail addon
    let detailItem = document.querySelector("#addons-details > .addon-item");
    detailItem.addon = null;

    showAddons();
  }
}

function showAddonDetails(addonId) {
  const listItem = Addons._getElementForAddon(addonId);
  if (listItem) {
    Addons.showDetails(listItem);
    history.pushState({ id: addonId }, document.title);
  } else {
    throw new Error(`Addon not found: ${addonId}`);
  }
}

function showAddons() {
  // Hide the addon options and show the addons list
  let details = document.querySelector("#addons-details");
  details.classList.add("hidden");
  let list = document.querySelector("#addons-list");
  list.classList.remove("hidden");
  document.documentElement.removeAttribute("details");

  // Clean the optionsBox content when switching to the add-ons list view.
  let optionsBox = document.querySelector("#addons-details > .addon-item .options-box");
  optionsBox.innerHTML = "";
}

function showAddonOptions() {
  // Hide the addon list and show the addon options
  let list = document.querySelector("#addons-list");
  list.classList.add("hidden");
  let details = document.querySelector("#addons-details");
  details.classList.remove("hidden");
  document.documentElement.setAttribute("details", "true");
}

var Addons = {
  _restartCount: 0,

  _createItem: function _createItem(aAddon) {
    let outer = document.createElement("div");
    outer.setAttribute("addonID", aAddon.id);
    outer.className = "addon-item list-item";
    outer.setAttribute("role", "button");
    outer.setAttribute("contextmenu", "addonmenu");
    outer.addEventListener("click", () => {
      this.showDetails(outer);
      history.pushState({ id: aAddon.id }, document.title);
    }, true);

    let img = document.createElement("img");
    img.className = "icon";
    img.setAttribute("src", aAddon.iconURL || AMO_ICON);
    outer.appendChild(img);

    let inner = document.createElement("div");
    inner.className = "inner";

    let details = document.createElement("div");
    details.className = "details";
    inner.appendChild(details);

    let titlePart = document.createElement("div");
    titlePart.textContent = aAddon.name;
    titlePart.className = "title";
    details.appendChild(titlePart);

    let versionPart = document.createElement("div");
    versionPart.textContent = aAddon.version;
    versionPart.className = "version";
    details.appendChild(versionPart);

    if ("description" in aAddon) {
      let descPart = document.createElement("div");
      descPart.textContent = aAddon.description;
      descPart.className = "description";
      inner.appendChild(descPart);
    }

    outer.appendChild(inner);

    let update = document.createElement("img");
    update.className = "update-indicator";
    update.setAttribute("src", UPDATE_INDICATOR);
    outer.appendChild(update);

    return outer;
  },

  _createBrowseItem: function _createBrowseItem() {
    let outer = document.createElement("div");
    outer.className = "addon-item list-item";
    outer.setAttribute("role", "button");
    outer.addEventListener("click", function(event) {
      try {
        openLink(Services.urlFormatter.formatURLPref("extensions.getAddons.browseAddons"));
      } catch (e) {
        Cu.reportError(e);
      }
    }, true);

    let img = document.createElement("img");
    img.className = "icon";
    img.setAttribute("src", AMO_ICON);
    outer.appendChild(img);

    let inner = document.createElement("div");
    inner.className = "inner";

    let title = document.createElement("div");
    title.id = "browse-title";
    title.className = "title";
    title.textContent = gStringBundle.GetStringFromName("addons.browseAll");
    inner.appendChild(title);

    outer.appendChild(inner);
    return outer;
  },

  _createItemForAddon: function _createItemForAddon(aAddon) {
    let opType = this._getOpTypeForOperations(aAddon.pendingOperations);
    let hasUpdate = this._addonHasUpdate(aAddon);

    let optionsURL = aAddon.optionsURL || "";

    let blocked = "";
    switch (aAddon.blocklistState) {
      case Ci.nsIBlocklistService.STATE_BLOCKED:
        blocked = "blocked";
        break;
      case Ci.nsIBlocklistService.STATE_SOFTBLOCKED:
        blocked = "softBlocked";
        break;
      case Ci.nsIBlocklistService.STATE_OUTDATED:
        blocked = "outdated";
        break;
    }

    let item = this._createItem(aAddon);
    item.setAttribute("isDisabled", !aAddon.isActive);
    item.setAttribute("isUnsigned", aAddon.signedState <= AddonManager.SIGNEDSTATE_MISSING);
    item.setAttribute("opType", opType);
    if (blocked)
      item.setAttribute("blockedStatus", blocked);
    item.setAttribute("optionsURL", optionsURL);
    item.setAttribute("hasUpdate", hasUpdate);
    item.addon = aAddon;

    return item;
  },

  _getElementForAddon: function(aKey) {
    let list = document.getElementById("addons-list");
    let element = list.querySelector("div[addonID=\"" + CSS.escape(aKey) + "\"]");
    return element;
  },

  _addonHasUpdate(addon) {
    return gChromeWin.ExtensionPermissions.updates.has(addon.id);
  },

  init: async function init() {
    const aAddons = await AddonManager.getAllAddons();

    // Clear all content before filling the addons
    let list = document.getElementById("addons-list");
    list.innerHTML = "";

    aAddons.sort(function(a, b) {
      return a.name.localeCompare(b.name);
    });

    for (let i = 0; i < aAddons.length; i++) {
      // Don't create item for system add-ons.
      if (aAddons[i].isSystem)
        continue;

      let item = this._createItemForAddon(aAddons[i]);
      list.appendChild(item);
    }

    // Add a "Browse all Firefox Add-ons" item to the bottom of the list.
    let browseItem = this._createBrowseItem();
    list.appendChild(browseItem);

    document.getElementById("update-btn").addEventListener("click", Addons.updateCurrent.bind(this));
    document.getElementById("uninstall-btn").addEventListener("click", Addons.uninstallCurrent.bind(this));
    document.getElementById("cancel-btn").addEventListener("click", Addons.cancelUninstall.bind(this));
    document.getElementById("disable-btn").addEventListener("click", Addons.disable.bind(this));
    document.getElementById("enable-btn").addEventListener("click", Addons.enable.bind(this));

    document.getElementById("unsigned-learn-more").addEventListener("click", function() {
      openLink(Services.urlFormatter.formatURLPref("app.support.baseURL") + "unsigned-addons");
    });
  },

  _getOpTypeForOperations: function _getOpTypeForOperations(aOperations) {
    if (aOperations & AddonManager.PENDING_UNINSTALL)
      return "needs-uninstall";
    if (aOperations & AddonManager.PENDING_ENABLE)
      return "needs-enable";
    if (aOperations & AddonManager.PENDING_DISABLE)
      return "needs-disable";
    return "";
  },

  showDetails: function showDetails(aListItem) {
    let detailItem = document.querySelector("#addons-details > .addon-item");
    detailItem.setAttribute("isDisabled", aListItem.getAttribute("isDisabled"));
    detailItem.setAttribute("isUnsigned", aListItem.getAttribute("isUnsigned"));
    detailItem.setAttribute("opType", aListItem.getAttribute("opType"));
    detailItem.setAttribute("optionsURL", aListItem.getAttribute("optionsURL"));
    let addon = detailItem.addon = aListItem.addon;

    let favicon = document.querySelector("#addons-details > .addon-item .icon");
    favicon.setAttribute("src", addon.iconURL || AMO_ICON);

    detailItem.querySelector(".title").textContent = addon.name;
    detailItem.querySelector(".version").textContent = addon.version;
    detailItem.querySelector(".description-full").textContent = addon.description;
    detailItem.querySelector(".status-uninstalled").textContent =
      gStringBundle.formatStringFromName("addonStatus.uninstalled", [addon.name], 1);

    let updateBtn = document.getElementById("update-btn");
    if (this._addonHasUpdate(addon)) {
      updateBtn.removeAttribute("hidden");
    } else {
      updateBtn.setAttribute("hidden", true);
    }

    let enableBtn = document.getElementById("enable-btn");
    if (addon.appDisabled) {
      enableBtn.setAttribute("disabled", "true");
    } else {
      enableBtn.removeAttribute("disabled");
    }

    let uninstallBtn = document.getElementById("uninstall-btn");
    if (addon.scope == AddonManager.SCOPE_APPLICATION) {
      uninstallBtn.setAttribute("disabled", "true");
    } else {
      uninstallBtn.removeAttribute("disabled");
    }

    let addonItem = document.querySelector("#addons-details > .addon-item");
    let optionsBox = addonItem.querySelector(".options-box");
    let optionsURL = aListItem.getAttribute("optionsURL");

    // Always clean the options content before rendering the options of the
    // newly selected extension.
    optionsBox.innerHTML = "";

    switch (parseInt(addon.optionsType)) {
      case AddonManager.OPTIONS_TYPE_INLINE_BROWSER:
        // Allow the options to use all the available width space.
        optionsBox.classList.remove("inner");

        this.createWebExtensionOptions(optionsBox, addon, addonItem);
        break;
      case AddonManager.OPTIONS_TYPE_TAB:
        // Keep the usual layout for any options related the legacy (or system) add-ons
        // when the options are opened in a new tab from a single button in the addon
        // details page.
        optionsBox.classList.add("inner");

        this.createOptionsInTabButton(optionsBox, addon, addonItem);
        break;
    }

    showAddonOptions();
  },

  createOptionsInTabButton: function(destination, addon, detailItem) {
    let frame = destination.querySelector("iframe#addon-options");
    let button = destination.querySelector("button#open-addon-options");

    if (frame) {
      // Remove any existent options frame (e.g. when the addon updates
      // contains the open_in_tab options for the first time).

      frame.remove();
    }

    if (!button) {
      button = document.createElement("button");
      button.setAttribute("id", "open-addon-options");
      button.textContent = gStringBundle.GetStringFromName("addon.options");
      destination.appendChild(button);
    }

    button.onclick = async () => {
      if (addon.isWebExtension) {
        // WebExtensions are loaded asynchronously and the optionsURL
        // may not be available until the addon has been started.
        await addon.startupPromise;
      }

      const {optionsURL} = addon;
      openOptionsInTab(optionsURL);
    };

    // Ensure that the Addon Options are visible (the options box will be hidden if the optionsURL
    // attribute is an empty string, which happens when a WebExtensions is still loading).
    detailItem.removeAttribute("optionsURL");
  },

  createWebExtensionOptions: async function(destination, addon, detailItem) {
    // WebExtensions are loaded asynchronously and the optionsURL
    // may not be available until the addon has been started.
    await addon.startupPromise;

    const {optionsURL, optionsBrowserStyle} = addon;
    let frame = destination.querySelector("iframe#addon-options");

    if (!frame) {
      let originalHeight;
      frame = document.createElement("iframe");
      frame.setAttribute("id", "addon-options");
      frame.setAttribute("mozbrowser", "true");
      frame.setAttribute("style", "width: 100%; overflow: hidden;");

      // Adjust iframe height to the iframe content (also between navigation of multiple options
      // files).
      frame.onload = (evt) => {
        if (evt.target !== frame) {
          return;
        }

        const {document} = frame.contentWindow;
        const bodyScrollHeight = document.body && document.body.scrollHeight;
        const documentScrollHeight = document.documentElement.scrollHeight;

        // Set the iframe height to the maximum between the body and the document
        // scrollHeight values.
        frame.style.height = Math.max(bodyScrollHeight, documentScrollHeight) + "px";

        // Restore the original iframe height between option page loads,
        // so that we don't force the new document to have the same size
        // of the previosuly loaded option page.
        frame.contentWindow.addEventListener("unload", () => {
          frame.style.height = originalHeight + "px";
        }, {once: true});
      };

      destination.appendChild(frame);
      originalHeight = frame.getBoundingClientRect().height;
    }

    // Loading the URL this way prevents the native back
    // button from applying to the iframe.
    frame.contentWindow.location.replace(optionsURL);

    // Ensure that the Addon Options are visible (the options box will be hidden if the optionsURL
    // attribute is an empty string, which happens when a WebExtensions is still loading).
    detailItem.removeAttribute("optionsURL");
  },

  setEnabled: function setEnabled(aValue, aAddon) {
    let detailItem = document.querySelector("#addons-details > .addon-item");
    let addon = aAddon || detailItem.addon;
    if (!addon)
      return;

    let listItem = this._getElementForAddon(addon.id);

    function setDisabled(addon, value) {
      if (value) {
        return addon.enable();
      }
      return addon.disable();
    }

    let opType;
    if (addon.type == "theme") {
      if (aValue) {
        // We can have only one theme enabled, so disable the current one if any
        let list = document.getElementById("addons-list");
        let item = list.firstElementChild;
        while (item) {
          if (item.addon && (item.addon.type == "theme") && (item.addon.isActive)) {
            item.addon.disable();
            item.setAttribute("isDisabled", true);
            break;
          }
          item = item.nextSibling;
        }
      }
      setDisabled(addon, !aValue);
    } else if (addon.type == "locale") {
      setDisabled(addon, !aValue);
    } else {
      setDisabled(addon, !aValue);
      opType = this._getOpTypeForOperations(addon.pendingOperations);

      if ((addon.pendingOperations & AddonManager.PENDING_ENABLE) ||
          (addon.pendingOperations & AddonManager.PENDING_DISABLE)) {
        this.showRestart();
      } else if (listItem && /needs-(enable|disable)/.test(listItem.getAttribute("opType"))) {
        this.hideRestart();
      }
    }

    if (addon == detailItem.addon) {
      detailItem.setAttribute("isDisabled", !aValue);
      if (opType)
        detailItem.setAttribute("opType", opType);
      else
        detailItem.removeAttribute("opType");

      // Remove any addon options iframe if the currently selected addon has been disabled.
      if (!aValue) {
        const addonOptionsIframe = document.querySelector("#addon-options");
        if (addonOptionsIframe) {
          addonOptionsIframe.remove();
        }
      }
    }

    // Sync to the list item
    if (listItem) {
      listItem.setAttribute("isDisabled", !aValue);
      if (opType)
        listItem.setAttribute("opType", opType);
      else
        listItem.removeAttribute("opType");
    }
  },

  enable: function enable() {
    this.setEnabled(true);
  },

  disable: function disable() {
    this.setEnabled(false);
  },

  updateCurrent() {
    let detailItem = document.querySelector("#addons-details > .addon-item");

    let addon = detailItem.addon;
    if (!addon)
      return;

    gChromeWin.ExtensionPermissions.applyUpdate(addon.id);
  },

  uninstallCurrent: function uninstallCurrent() {
    let detailItem = document.querySelector("#addons-details > .addon-item");

    let addon = detailItem.addon;
    if (!addon)
      return;

    this.uninstall(addon);
  },

  uninstall: function uninstall(aAddon) {
    if (!aAddon) {
      return;
    }

    let listItem = this._getElementForAddon(aAddon.id);
    aAddon.uninstall();

    if (aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL) {
      this.showRestart();

      // A disabled addon doesn't need a restart so it has no pending ops and
      // can't be cancelled
      let opType = this._getOpTypeForOperations(aAddon.pendingOperations);
      if (!aAddon.isActive && opType == "")
        opType = "needs-uninstall";

      detailItem.setAttribute("opType", opType);
      listItem.setAttribute("opType", opType);
    }
  },

  cancelUninstall: function ev_cancelUninstall() {
    let detailItem = document.querySelector("#addons-details > .addon-item");
    let addon = detailItem.addon;
    if (!addon)
      return;

    addon.cancelUninstall();
    this.hideRestart();

    let opType = this._getOpTypeForOperations(addon.pendingOperations);
    detailItem.setAttribute("opType", opType);

    let listItem = this._getElementForAddon(addon.id);
    listItem.setAttribute("opType", opType);
  },

  showRestart: function showRestart() {
    this._restartCount++;
    gChromeWin.XPInstallObserver.showRestartPrompt();
  },

  hideRestart: function hideRestart() {
    this._restartCount--;
    if (this._restartCount == 0)
      gChromeWin.XPInstallObserver.hideRestartPrompt();
  },

  onEnabled: function(aAddon) {
    let listItem = this._getElementForAddon(aAddon.id);
    if (!listItem)
      return;

    // Reload the details to pick up any options now that it's enabled.
    listItem.setAttribute("optionsURL", aAddon.optionsURL || "");
    let detailItem = document.querySelector("#addons-details > .addon-item");
    if (aAddon == detailItem.addon)
      this.showDetails(listItem);
  },

  onInstallEnded: function(aInstall, aAddon) {
    let needsRestart = false;
    if (aInstall.existingAddon && (aInstall.existingAddon.pendingOperations & AddonManager.PENDING_UPGRADE))
      needsRestart = true;
    else if (aAddon.pendingOperations & AddonManager.PENDING_INSTALL)
      needsRestart = true;

    let list = document.getElementById("addons-list");
    let element = this._getElementForAddon(aAddon.id);
    if (!element) {
      element = this._createItemForAddon(aAddon);
      list.insertBefore(element, list.firstElementChild);
    }

    if (needsRestart)
      element.setAttribute("opType", "needs-restart");
  },

  onInstalled: function(aAddon) {
    let list = document.getElementById("addons-list");
    let element = this._getElementForAddon(aAddon.id);
    if (element) {
      // Upgrade of an existing addon, update version and description in
      // list item and detail view, plus indicators about a pending update.
      element.querySelector(".version").textContent = aAddon.version;

      let desc = element.querySelector(".description");
      if (desc) {
        desc.textContent = aAddon.description;
      }

      element.setAttribute("hasUpdate", false);
      document.getElementById("update-btn").setAttribute("hidden", true);

      element = document.querySelector("#addons-details > .addon-item");
      if (element.addon && element.addon.id == aAddon.id) {
        element.querySelector(".version").textContent = aAddon.version;
        element.querySelector(".description-full").textContent = aAddon.description;
      }
    } else {
      element = this._createItemForAddon(aAddon);

      // Themes aren't considered active on install, so set existing as disabled, and new one enabled.
      if (aAddon.type == "theme") {
        let item = list.firstElementChild;
        while (item) {
          if (item.addon && (item.addon.type == "theme")) {
            item.setAttribute("isDisabled", true);
          }
          item = item.nextSibling;
        }
        element.setAttribute("isDisabled", false);
      }

      list.insertBefore(element, list.firstElementChild);
    }
  },

  onUninstalled: function(aAddon) {
    let list = document.getElementById("addons-list");
    let element = this._getElementForAddon(aAddon.id);
    list.removeChild(element);

    // Go back if we're in the detail view of the add-on that was uninstalled.
    let detailItem = document.querySelector("#addons-details > .addon-item");
    if (detailItem.addon.id == aAddon.id) {
      history.back();
    }
  },

  onInstallFailed: function(aInstall) {
  },

  onDownloadProgress: function xpidm_onDownloadProgress(aInstall) {
  },

  onDownloadFailed: function(aInstall) {
  },

  onDownloadCancelled: function(aInstall) {
  }
};

window.addEventListener("load", init);
window.addEventListener("unload", uninit);
