/* ***** BEGIN LICENSE BLOCK *****
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ***** END LICENSE BLOCK ***** */

let Ci = Components.interfaces, Cc = Components.classes, Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm")
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "WebappManager", "resource://gre/modules/WebappManager.jsm");

const DEFAULT_ICON = "chrome://browser/skin/images/default-app-icon.png";

let gStrings = Services.strings.createBundle("chrome://browser/locale/aboutApps.properties");

XPCOMUtils.defineLazyGetter(window, "gChromeWin", function()
  window.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIWebNavigation)
    .QueryInterface(Ci.nsIDocShellTreeItem)
    .rootTreeItem
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindow)
    .QueryInterface(Ci.nsIDOMChromeWindow));

document.addEventListener("DOMContentLoaded", onLoad, false);

var AppsUI = {
  uninstall: null,
  shortcut: null
};

function openLink(aEvent) {
  try {
    let formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].getService(Ci.nsIURLFormatter);
    let url = formatter.formatURLPref(aEvent.currentTarget.getAttribute("pref"));
    let BrowserApp = gChromeWin.BrowserApp;
    BrowserApp.addTab(url, { selected: true, parentId: BrowserApp.selectedTab.id });
  } catch (ex) {}
}

function checkForUpdates(aEvent) {
  WebappManager.checkForUpdates(true);
}

let ContextMenus = {
  target: null,

  init: function() {
    document.addEventListener("contextmenu", this, false);
    document.getElementById("uninstallLabel").addEventListener("click", this.uninstall.bind(this), false);
  },

  handleEvent: function(event) {
    // store the target of context menu events so that we know which app to act on
    this.target = event.target;
    while (!this.target.hasAttribute("contextmenu")) {
      this.target = this.target.parentNode;
    }
  },

  uninstall: function() {
    navigator.mozApps.mgmt.uninstall(this.target.app);

    this.target = null;
  }
};

function onLoad(aEvent) {
  let elmts = document.querySelectorAll("[pref]");
  for (let i = 0; i < elmts.length; i++) {
    elmts[i].addEventListener("click",  openLink,  false);
  }

  document.getElementById("update-item").addEventListener("click", checkForUpdates, false);

  navigator.mozApps.mgmt.oninstall = onInstall;
  navigator.mozApps.mgmt.onuninstall = onUninstall;
  updateList();

  ContextMenus.init();

  // XXX - Hack to fix bug 985867 for now
  document.addEventListener("touchstart", function() { });
}

function updateList() {
  let grid = document.getElementById("appgrid");
  while (grid.lastChild) {
    grid.removeChild(grid.lastChild);
  }

  let request = navigator.mozApps.mgmt.getAll();
  request.onsuccess = function() {
    for (let i = 0; i < request.result.length; i++)
      addApplication(request.result[i]);
    if (request.result.length)
      document.getElementById("main-container").classList.remove("hidden");
  }
}

function addApplication(aApp) {
  let list = document.getElementById("appgrid");
  let manifest = new ManifestHelper(aApp.manifest, aApp.origin, aApp.manifestURL);

  let container = document.createElement("div");
  container.className = "app list-item";
  container.setAttribute("contextmenu", "appmenu");
  container.setAttribute("id", "app-" + aApp.manifestURL);
  container.setAttribute("title", manifest.name);

  let img = document.createElement("img");
  img.src = manifest.biggestIconURL || DEFAULT_ICON;
  img.onerror = function() {
    // If the image failed to load, and it was not our default icon, attempt to
    // use our default as a fallback.
    if (img.src != DEFAULT_ICON) {
      img.src = DEFAULT_ICON;
    }
  }
  img.setAttribute("title", manifest.name);

  let title = document.createElement("div");
  title.appendChild(document.createTextNode(manifest.name));

  container.appendChild(img);
  container.appendChild(title);
  list.appendChild(container);

  container.addEventListener("click", function(aEvent) {
    aApp.launch();
  }, false);
  container.app = aApp;
  container.manifest = manifest;
}

function onInstall(aEvent) {
  let node = document.getElementById("app-" + aEvent.application.manifestURL);
  if (node)
    return;

  addApplication(aEvent.application);
  document.getElementById("main-container").classList.remove("hidden");
}

function onUninstall(aEvent) {
  let node = document.getElementById("app-" + aEvent.application.manifestURL);
  if (node) {
    let parent = node.parentNode;
    parent.removeChild(node);
    if (!parent.firstChild)
      document.getElementById("main-container").classList.add("hidden");
  }
}

