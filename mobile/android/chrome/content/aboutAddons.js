/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Ci = Components.interfaces, Cc = Components.classes, Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm")
Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const AMO_ICON = "chrome://browser/skin/images/amo-logo.png";

let gStringBundle = Services.strings.createBundle("chrome://browser/locale/aboutAddons.properties");

XPCOMUtils.defineLazyGetter(window, "gChromeWin", function()
  window.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIWebNavigation)
    .QueryInterface(Ci.nsIDocShellTreeItem)
    .rootTreeItem
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindow)
    .QueryInterface(Ci.nsIDOMChromeWindow));

var ContextMenus = {
  target: null,

  init: function() {
    document.addEventListener("contextmenu", this, false);

    document.getElementById("contextmenu-enable").addEventListener("click", ContextMenus.enable.bind(this), false);
    document.getElementById("contextmenu-disable").addEventListener("click", ContextMenus.disable.bind(this), false);
    document.getElementById("contextmenu-uninstall").addEventListener("click", ContextMenus.uninstall.bind(this), false);

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
  
  disable: function (event) {
    Addons.setEnabled(false, this.target.addon);
    this.target = null;
  },
  
  uninstall: function (event) {
    Addons.uninstall(this.target.addon);
    this.target = null;
  }
}

function init() {
  window.addEventListener("popstate", onPopState, false);

  AddonManager.addInstallListener(Addons);
  AddonManager.addAddonListener(Addons);
  Addons.init();
  showList();
  ContextMenus.init();

}


function uninit() {
  AddonManager.removeInstallListener(Addons);
  AddonManager.removeAddonListener(Addons);
}

function openLink(aEvent) {
  try {
    let formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].getService(Ci.nsIURLFormatter);

    let url = formatter.formatURLPref(aEvent.currentTarget.getAttribute("pref"));
    let BrowserApp = gChromeWin.BrowserApp;
    BrowserApp.addTab(url, { selected: true, parentId: BrowserApp.selectedTab.id });
  } catch (ex) {}
}

function onPopState(aEvent) {
  // Called when back/forward is used to change the state of the page
  if (aEvent.state) {
    // Show the detail page for an addon
    Addons.showDetails(Addons._getElementForAddon(aEvent.state.id));
  } else {
    // Clear any previous detail addon
    let detailItem = document.querySelector("#addons-details > .addon-item");
    detailItem.addon = null;

    showList();
  }
}

function showList() {
  // Hide the detail page and show the list
  let details = document.querySelector("#addons-details");
  details.style.display = "none";
  let list = document.querySelector("#addons-list");
  list.style.display = "block";
}

var Addons = {
  _restartCount: 0,

  _createItem: function _createItem(aAddon) {
    let outer = document.createElement("div");
    outer.setAttribute("addonID", aAddon.id);
    outer.className = "addon-item list-item";
    outer.setAttribute("role", "button");
    outer.setAttribute("contextmenu", "addonmenu");
    outer.addEventListener("click", function() {
      this.showDetails(outer);
      history.pushState({ id: aAddon.id }, document.title);
    }.bind(this), true);

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
    return outer;
  },

  _createBrowseItem: function _createBrowseItem() {
    let outer = document.createElement("div");
    outer.className = "addon-item list-item";
    outer.setAttribute("role", "button");
    outer.setAttribute("pref", "extensions.getAddons.browseAddons");
    outer.addEventListener("click", openLink, true);

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
    let appManaged = (aAddon.scope == AddonManager.SCOPE_APPLICATION);
    let opType = this._getOpTypeForOperations(aAddon.pendingOperations);
    let updateable = (aAddon.permissions & AddonManager.PERM_CAN_UPGRADE) > 0;
    let uninstallable = (aAddon.permissions & AddonManager.PERM_CAN_UNINSTALL) > 0;

    let blocked = "";
    switch(aAddon.blocklistState) {
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
    item.setAttribute("opType", opType);
    item.setAttribute("updateable", updateable);
    if (blocked)
      item.setAttribute("blockedStatus", blocked);
    item.setAttribute("optionsURL", aAddon.optionsURL || "");
    item.addon = aAddon;

    return item;
  },

  _getElementForAddon: function(aKey) {
    let list = document.getElementById("addons-list");
    let element = list.querySelector("div[addonID=\"" + CSS.escape(aKey) + "\"]");
    return element;
  },

  init: function init() {
    let self = this;
    AddonManager.getAllAddons(function(aAddons) {
      // Clear all content before filling the addons
      let list = document.getElementById("addons-list");
      list.innerHTML = "";

      aAddons.sort(function(a,b) {
        return a.name.localeCompare(b.name);
      });
      for (let i=0; i<aAddons.length; i++) {
        let item = self._createItemForAddon(aAddons[i]);
        list.appendChild(item);
      }

      // Add a "Browse all Firefox Add-ons" item to the bottom of the list.
      let browseItem = self._createBrowseItem();
      list.appendChild(browseItem);
    });

    document.getElementById("uninstall-btn").addEventListener("click", Addons.uninstallCurrent.bind(this), false);
    document.getElementById("cancel-btn").addEventListener("click", Addons.cancelUninstall.bind(this), false);
    document.getElementById("disable-btn").addEventListener("click", Addons.disable.bind(this), false);
    document.getElementById("enable-btn").addEventListener("click", Addons.enable.bind(this), false);
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
    // This function removes and returns the text content of aNode without
    // removing any child elements. Removing the text nodes ensures any XBL
    // bindings apply properly.
    function stripTextNodes(aNode) {
      var text = "";
      for (var i = 0; i < aNode.childNodes.length; i++) {
        if (aNode.childNodes[i].nodeType != document.ELEMENT_NODE) {
          text += aNode.childNodes[i].textContent;
          aNode.removeChild(aNode.childNodes[i--]);
        } else {
          text += stripTextNodes(aNode.childNodes[i]);
        }
      }
      return text;
    }

    let detailItem = document.querySelector("#addons-details > .addon-item");
    detailItem.setAttribute("isDisabled", aListItem.getAttribute("isDisabled"));
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

    let enableBtn = document.getElementById("enable-btn");
    if (addon.appDisabled)
      enableBtn.setAttribute("disabled", "true");
    else
      enableBtn.removeAttribute("disabled");

    let uninstallBtn = document.getElementById("uninstall-btn");
    if (addon.scope == AddonManager.SCOPE_APPLICATION)
      uninstallBtn.setAttribute("disabled", "true");
    else
      uninstallBtn.removeAttribute("disabled");

    let box = document.querySelector("#addons-details > .addon-item .options-box");
    box.innerHTML = "";

    // Retrieve the extensions preferences
    try {
      let optionsURL = aListItem.getAttribute("optionsURL");
      let xhr = new XMLHttpRequest();
      xhr.open("GET", optionsURL, true);
      xhr.onload = function(e) {
        if (xhr.responseXML) {
          // Only allow <setting> for now
          let settings = xhr.responseXML.querySelectorAll(":root > setting");
          if (settings.length > 0) {
            for (let i = 0; i < settings.length; i++) {
              var setting = settings[i];
              var desc = stripTextNodes(setting).trim();
              if (!setting.hasAttribute("desc")) {
                setting.setAttribute("desc", desc);
              }
              box.appendChild(setting);
            }
            // Send an event so add-ons can prepopulate any non-preference based
            // settings
            let event = document.createEvent("Events");
            event.initEvent("AddonOptionsLoad", true, false);
            window.dispatchEvent(event);
          } else {
            // Reset the options URL to hide the options header if there are no
            // valid settings to show.
            detailItem.setAttribute("optionsURL", "");
          }

          // Also send a notification to match the behavior of desktop Firefox
          let id = aListItem.getAttribute("addonID");
          Services.obs.notifyObservers(document, AddonManager.OPTIONS_NOTIFICATION_DISPLAYED, id);
        }
      }
      xhr.send(null);
    } catch (e) { }

    let list = document.querySelector("#addons-list");
    list.style.display = "none";
    let details = document.querySelector("#addons-details");
    details.style.display = "block";
  },

  setEnabled: function setEnabled(aValue, aAddon) {
    let detailItem = document.querySelector("#addons-details > .addon-item");
    let addon = aAddon || detailItem.addon;
    if (!addon)
      return;

    let listItem = this._getElementForAddon(addon.id);

    let opType;
    if (addon.type == "theme") {
      if (aValue) {
        // We can have only one theme enabled, so disable the current one if any
        let list = document.getElementById("addons-list");
        let item = list.firstElementChild;
        while (item) {
          if (item.addon && (item.addon.type == "theme") && (item.addon.isActive)) {
            item.addon.userDisabled = true;
            item.setAttribute("isDisabled", true);
            break;
          }
          item = item.nextSibling;
        }
      }
      addon.userDisabled = !aValue;
    } else if (addon.type == "locale") {
      addon.userDisabled = !aValue;
    } else {
      addon.userDisabled = !aValue;
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
    if (!element) {
      element = this._createItemForAddon(aAddon);
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
}

window.addEventListener("load", init, false);
window.addEventListener("unload", uninit, false);
