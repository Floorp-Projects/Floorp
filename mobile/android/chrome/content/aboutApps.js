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
Cu.import("resource://gre/modules/Webapps.jsm");

let gStrings = Services.strings.createBundle("chrome://browser/locale/aboutApps.properties");

XPCOMUtils.defineLazyGetter(window, "gChromeWin", function()
  window.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIWebNavigation)
    .QueryInterface(Ci.nsIDocShellTreeItem)
    .rootTreeItem
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindow)
    .QueryInterface(Ci.nsIDOMChromeWindow));

var AppsUI = {
  uninstall: null,
  shortcut: null
};

function onLoad(aEvent) {
  try {
    let formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].getService(Ci.nsIURLFormatter);
    let link = document.getElementById("marketplaceURL");
    let url = formatter.formatURLPref(link.getAttribute("pref"));
    link.setAttribute("href", url);
  } catch (e) {}

  navigator.mozApps.mgmt.oninstall = onInstall;
  navigator.mozApps.mgmt.onuninstall = onUninstall;
  updateList();

  let contextmenus = gChromeWin.NativeWindow.contextmenus;
  AppsUI.shortcut = contextmenus.add(gStrings.GetStringFromName("appsContext.shortcut"), contextmenus.SelectorContext("div[mozApp]"),
    function(aTarget) {
      let manifest = aTarget.manifest;
      createShortcut(manifest.name, manifest.fullLaunchPath(), manifest.iconURLForSize("64"), "webapp");
    });
  AppsUI.uninstall = contextmenus.add(gStrings.GetStringFromName("appsContext.uninstall"), contextmenus.SelectorContext("div[mozApp]"),
    function(aTarget) {
      aTarget.app.uninstall();
    });
}

function onUnload(aEvent) {
  let contextmenus = gChromeWin.NativeWindow.contextmenus;
  if (AppsUI.shortcut)
    contextmenus.remove(AppsUI.shortcut);
  if (AppsUI.uninstall)
    contextmenus.remove(AppsUI.uninstall);
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
    if (!request.result.length)
      document.getElementById("noapps").className = "";
  }
}

function addApplication(aApp) {
  let list = document.getElementById("appgrid");
  let manifest = new DOMApplicationManifest(aApp.manifest, aApp.origin);

  let container = document.createElement("div");
  container.className = "app";
  container.setAttribute("id", "app-" + aApp.origin);
  container.setAttribute("mozApp", aApp.origin);
  container.setAttribute("title", manifest.name);

  let img = document.createElement("img");
  img.src = manifest.iconURLForSize("64");
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
  let node = document.getElementById("app-" + aEvent.application.origin);
  if (node)
    return;

  addApplication(aEvent.application);
  document.getElementById("noapps").className = "hidden";
}

function onUninstall(aEvent) {
  let node = document.getElementById("app-" + aEvent.application.origin);
  if (node) {
    let parent = node.parentNode;
    parent.removeChild(node);
    if (!parent.firstChild)
      document.getElementById("noapps").className = "";
  }
}

function createShortcut(aTitle, aURL, aIconURL, aType) {
  // The images are 64px, but Android will resize as needed.
  // Bigger is better than too small.
  const kIconSize = 64;

  let canvas = document.createElement("canvas");

  function _createShortcut() {
    let icon = canvas.toDataURL("image/png", "");
    canvas = null;
    try {
      let shell = Cc["@mozilla.org/browser/shell-service;1"].createInstance(Ci.nsIShellService);
      shell.createShortcut(aTitle, aURL, icon, aType);
    } catch(e) {
      Cu.reportError(e);
    }
  }

  canvas.width = canvas.height = kIconSize;
  let ctx = canvas.getContext("2d");

  let favicon = new Image();
  favicon.onload = function() {
    ctx.drawImage(favicon, 0, 0, kIconSize, kIconSize);
    _createShortcut();
  }

  favicon.onerror = function() {
    Cu.reportError("CreateShortcut: favicon image load error");
  }

  favicon.src = aIconURL;
}
