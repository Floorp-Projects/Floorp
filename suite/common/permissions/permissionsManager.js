/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <ben@mozilla.org>
 *   Blake Ross <firefox@blakeross.com>
 *   Ian Neal (iann_bugzilla@arlen.demon.co.uk)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const nsIPermissionManager = Components.interfaces.nsIPermissionManager;
const nsICookiePermission = Components.interfaces.nsICookiePermission;

var permissionManager;

var additions = [];
var removals = [];

var sortColumn;
var sortAscending;

var permissionsTreeView = {
    rowCount: 0,
    setTree: function(tree) {},
    getImageSrc: function(row, column) {},
    getProgressMode: function(row, column) {},
    getCellValue: function(row, column) {},
    getCellText: function(row, column) {
      if (column.id == "siteCol")
        return additions[row].rawHost;
      else if (column.id == "statusCol")
        return additions[row].capability;
      return "";
    },
    isSeparator: function(index) { return false; },
    isSorted: function() { return false; },
    isContainer: function(index) { return false; },
    cycleHeader: function(column) {},
    getRowProperties: function(row, column, prop) {},
    getColumnProperties: function(column, prop) {},
    getCellProperties: function(row, column, prop) {}
  };

var permissionsTree;
var permissionType = "popup";

var permissionsBundle;

function Startup() {
  permissionManager = Components.classes["@mozilla.org/permissionmanager;1"]
                                .getService(nsIPermissionManager);

  permissionsTree = document.getElementById("permissionsTree");

  permissionsBundle = document.getElementById("permissionsBundle");

  sortAscending = (permissionsTree.getAttribute("sortAscending") == "true");
  sortColumn = permissionsTree.getAttribute("sortColumn");

  if (window.arguments && window.arguments[0]) {
    document.getElementById("btnBlock").hidden = !window.arguments[0].blockVisible;
    document.getElementById("btnSession").hidden = !window.arguments[0].sessionVisible;
    document.getElementById("btnAllow").hidden = !window.arguments[0].allowVisible;
    setHost(window.arguments[0].prefilledHost);
    permissionType = window.arguments[0].permissionType;
  }

  var introString = permissionsBundle.getString(permissionType + "permissionstext");
  document.getElementById("permissionsText").textContent = introString;

  document.title = permissionsBundle.getString(permissionType + "permissionstitle");

  var dialogElement = document.getElementById("permissionsManager");
  dialogElement.setAttribute("windowtype", "permissions-" + permissionType);

  if (permissionManager) {
    handleHostInput(document.getElementById("url"));
    loadPermissions();
  }
  else {
    btnDisable(true);
    document.getElementById("removeAllPermissions").disabled = true;
    document.getElementById("url").disabled = true;
    document.documentElement.getButton("accept").disabled = true;
  }
}

function onAccept() {
  finalizeChanges();

  permissionsTree.setAttribute("sortAscending", !sortAscending);
  permissionsTree.setAttribute("sortColumn", sortColumn);

  if (permissionType != "popup")
    return true;

  var unblocked = additions; 
  var windowMediator = Components.classes['@mozilla.org/appshell/window-mediator;1']
                                 .getService(Components.interfaces.nsIWindowMediator);
  var enumerator = windowMediator.getEnumerator("navigator:browser");

  //if a site that is currently open is unblocked, make icon go away
  while (enumerator.hasMoreElements()) {
    var win = enumerator.getNext();

    var browsers = win.getBrowser().browsers;
    for (var i in browsers) {
      var nextLocation;
      try {
        nextLocation = browsers[i].currentURI.hostPort;
      }
      catch(ex) { 
        nextLocation = null; //blank window
      }

      if (nextLocation) {
        nextLocation = '.'+nextLocation;
        for (var j in unblocked) {
          var nextUnblocked = '.'+unblocked[j];

          if (nextUnblocked.length > nextLocation.length)
             continue; // can't be a match

          if (nextUnblocked == 
              nextLocation.substr(nextLocation.length - nextUnblocked.length)) {
            browsers[i].popupDomain = null;
            win.document.getElementById("popupIcon").hidden = true;
          }
        }
      }
    } 
  }

  return true;
}

function setHost(aHost) {
  document.getElementById("url").value = aHost;
}

function Permission(id, host, rawHost, type, capability, perm) {
  this.id = id;
  this.host = host;
  this.rawHost = rawHost;
  this.type = type;
  this.capability = capability;
  this.perm = perm;
}

function handleHostInput(aSiteField) {
  // trim any leading and trailing spaces and scheme
  // and set buttons appropiately
  btnDisable(!trimSpacesAndScheme(aSiteField.value));
}

function trimSpacesAndScheme(aString) {
  if (!aString)
    return "";
  return aString.replace(/(^\s+)|(\s+$)/g, "")
                .replace(/([-\w]*:\/+)?/, "");
}

function btnDisable(aDisabled) {
  document.getElementById("btnSession").disabled = aDisabled;
  document.getElementById("btnBlock").disabled = aDisabled;
  document.getElementById("btnAllow").disabled = aDisabled;
}

function loadPermissions() {
  var enumerator = permissionManager.enumerator;
  var count = 0;
  var permission;

  try {
    while (enumerator.hasMoreElements()) {
      permission = enumerator.getNext().QueryInterface(Components.interfaces.nsIPermission);
      if (permission.type == permissionType)
        permissionPush(count++, permission.host, permission.type,
                       capabilityString(permission.capability), permission.capability);
    }
  } catch(ex) {
  }

  permissionsTreeView.rowCount = additions.length;

  // sort and display the table
  permissionsTree.treeBoxObject.view = permissionsTreeView;
  permissionColumnSort(sortColumn, false);

  // disable "remove all" button if there are none
  document.getElementById("removeAllPermissions").disabled = additions.length == 0;
}

function capabilityString(aCapability) {
  var capability = null;
  switch (aCapability) {
    case nsIPermissionManager.ALLOW_ACTION:
      capability = "can";
      break;
    case nsIPermissionManager.DENY_ACTION:
      capability = "cannot";
      break;
    // we should only ever hit this for cookies
    case nsICookiePermission.ACCESS_SESSION:
      capability = "canSession";
      break;
    default:
      break;
  } 
  return permissionsBundle.getString(capability);
}

function permissionPush(aId, aHost, aType, aString, aCapability) {
  var rawHost = (aHost.charAt(0) == ".") ? aHost.substring(1, aHost.length) : aHost;
  var p = new Permission(aId, aHost, rawHost, aType, aString, aCapability);
  additions.push(p);
}

function permissionColumnSort(aColumn, aUpdateSelection) {
  sortAscending = 
    SortTree(permissionsTree, permissionsTreeView, additions,
             aColumn, sortColumn, sortAscending, aUpdateSelection);
  sortColumn = aColumn;
}

function permissionSelected() {
  if (permissionManager) {
    var selections = GetTreeSelections(permissionsTree);
    document.getElementById("removePermission").disabled = (selections.length < 1);
  }
}

function deletePermissions() {
  DeleteSelectedItemFromTree(permissionsTree, permissionsTreeView, additions, removals,
                             "removePermission", "removeAllPermissions");
}

function deleteAllPermissions() {
  DeleteAllFromTree(permissionsTree, permissionsTreeView, additions, removals,
                    "removePermission", "removeAllPermissions");
}

function finalizeChanges() {
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);
  var i, p;

  for (i in removals) {
    p = removals[i];
    try {
      permissionManager.remove(p.host, p.type);
    } catch(ex) {
    }
  }

  for (i in additions) {
    p = additions[i];
    try {
      var uri = ioService.newURI("http://" + p.host, null, null);
      permissionManager.add(uri, p.type, p.perm);
    } catch(ex) {
    }
  }
}

function handlePermissionKeyPress(e) {
  if (e.keyCode == 46) {
    deletePermissions();
  }
}

function addPermission(aPermission) {
  var textbox = document.getElementById("url");
  // trim any leading and trailing spaces and scheme
  var host = trimSpacesAndScheme(textbox.value);
  try {
    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService);
    var uri = ioService.newURI("http://" + host, null, null);
    host = uri.host;
  } catch(ex) {
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                  .getService(Components.interfaces.nsIPromptService);
    var message = permissionsBundle.getFormattedString("alertInvalid", [host]);
    var title = permissionsBundle.getString("alertInvalidTitle");
    promptService.alert(window, title, message);
    textbox.value = "";
    textbox.focus();
    handleHostInput(textbox);
    return;
  }

  // we need this whether the perm exists or not
  var stringCapability = capabilityString(aPermission);

  // check whether the permission already exists, if not, add it
  var exists = false;
  for (var i in additions) {
    if (additions[i].rawHost == host) {
      exists = true;
      additions[i].capability = stringCapability;
      additions[i].perm = aPermission;
      break;
    }
  }

  if (!exists) {
    permissionPush(additions.length, host, permissionType, stringCapability, aPermission);

    permissionsTreeView.rowCount = additions.length;
    permissionsTree.treeBoxObject.rowCountChanged(additions.length - 1, 1);
    permissionsTree.treeBoxObject.ensureRowIsVisible(additions.length - 1);
  }
  textbox.value = "";
  textbox.focus();

  // covers a case where the site exists already, so the buttons don't disable
  handleHostInput(textbox);

  // enable "remove all" button as needed
  document.getElementById("removeAllPermissions").disabled = additions.length == 0;
}

function doHelpButton() {
  openHelp(permissionsBundle.getString(permissionType + "permissionshelp"));
  return true;
}
