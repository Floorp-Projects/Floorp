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
      let origin = Services.io.newURI(app.origin, null, null);
      gChromeWin.WebappsUI.createShortcut(manifest.name, manifest.fullLaunchPath(), gChromeWin.WebappsUI.getBiggestIcon(manifest.icons, origin), "webapp");
    });
  AppsUI.uninstall = contextmenus.add(gStrings.GetStringFromName("appsContext.uninstall"), contextmenus.SelectorContext("div[mozApp]"),
    function(aTarget) {
      aTarget.app.uninstall();

      let manifest = aTarget.manifest;
      gChromeWin.sendMessageToJava({
        gecko: {
          type: "Shortcut:Remove",
          title: manifest.name,
          url: manifest.fullLaunchPath(),
          origin: aTarget.app.origin,
          shortcutType: "webapp"
        }
      });
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
  let origin = Services.io.newURI(aApp.origin, null, null);
  img.src = gChromeWin.WebappsUI.getBiggestIcon(manifest.icons, origin);
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
