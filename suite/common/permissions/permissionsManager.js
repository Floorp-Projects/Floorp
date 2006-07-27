/* ***** BEGIN LICENSE BLOCK *****
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const nsIPermissionManager = Components.interfaces.nsIPermissionManager;
const popupType = "popup";

var permissionManager = null;

var permissions = [];

var additions = [];
var removals = [];

var sortColumn = "host";
var sortAscending = false;

var permissionsTreeView = {
    rowCount: 0,
    setTree: function(tree) {},
    getImageSrc: function(row, column) {},
    getProgressMode: function(row, column) {},
    getCellValue: function(row, column) {},
    getCellText: function(row, column) {
      var rv = permissions[row].host;
      return rv;
  },
  isSeparator: function(index) { return false; },
  isSorted: function() { return false; },
  isContainer: function(index) { return false; },
  cycleHeader: function(aColId, aElt) {},
  getRowProperties: function(row, column,prop) {},
  getColumnProperties: function(column, columnElement, prop) {},
  getCellProperties: function(row, prop) {}
};

var permissionsTree;
var popupStringBundle;

function Startup() {
  permissionManager = Components.classes["@mozilla.org/permissionmanager;1"]
                                .getService(Components.interfaces.nsIPermissionManager);

  permissionsTree = document.getElementById("permissionsTree");

  popupStringBundle = document.getElementById("popupStringBundle");

  sortAscending = (permissionsTree.getAttribute("sortAscending") == "true");

  loadPermissions(permissions);
  loadTree();

  // window.arguments[0] contains the host to prefill   
  if (window.arguments[0] != "") {
    // fill textbox to unblock/add to whitelist
    var prefill = window.arguments[0];
    if (prefill.indexOf("www.") == 0) 
      prefill = prefill.slice(4);
    document.getElementById("addSiteBox").value = prefill;
  }

  document.documentElement.addEventListener("keypress", onReturnHit, true);
}

function getMatch(host) {
  // pre-pend '.' so we always match on host boundaries. Otherwise 
  // we might think notfoo.com matches foo.com
  var currentLoc = '.'+host;
  var nextHost;
  var inList;

  var matchIndex = null;
  var matchLength = 0;

  for (var i = 0; i < permissionsTreeView.rowCount; i++) {
    nextHost = '.'+permissions[i].host;

    if (currentLoc.length < nextHost.length)
      continue; // can't be a match, list host is more specific

    // look for an early out exact match -- check length first for speed
    if (currentLoc.length == nextHost.length && nextHost == currentLoc) {
      inList = true;
      matchIndex = i;
      break;
    }

    if (nextHost == currentLoc.substr(currentLoc.length - nextHost.length)) { 
      inList = true;
      if (nextHost.length > matchLength) {
        matchIndex = i;
        matchLength = nextHost.length;
      }
    }        
  }
      
  return matchIndex;
}

function onAccept() {
  finalizeChanges();
  
  var unblocked = additions; 

  permissionsTree.setAttribute("sortAscending", !sortAscending);

  var nextLocation;
  var nextUnblocked;

  var windowMediator = Components.classes['@mozilla.org/appshell/window-mediator;1']
                                 .getService(Components.interfaces.nsIWindowMediator);
  var enumerator = windowMediator.getEnumerator("navigator:browser");

  //if a site that is currently open is unblocked, make icon go away
  while(enumerator.hasMoreElements()) {
    var win = enumerator.getNext();
    
    var browsers = win.getBrowser().browsers;
    for (var i = 0; i < browsers.length; i++) {
      try {
        nextLocation = browsers[i].currentURI.hostPort;
      }
      catch(ex) { 
        //blank window
      }

      if (nextLocation) {
        nextLocation = '.'+nextLocation;
        for (var j in unblocked) {
          nextUnblocked = '.'+unblocked[j];

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

function Permission(host, number) {
  this.host = host;
  this.number = number;
}

function loadPermissions(table) {
  var enumerator = permissionManager.enumerator;
  var count = 0;
  
  while (enumerator.hasMoreElements()) {
    var permission = enumerator.getNext();
    if (permission) {
      permission = permission.QueryInterface(Components.interfaces.nsIPermission);
      if ((permission.type == popupType) && (permission.capability == nsIPermissionManager.ALLOW_ACTION)) {
        var host = permission.host;
        table[count] = new Permission(host,count++);
      }
    }
  }
}

function loadTree() {
  var rowCount = permissions.length;
  permissionsTreeView.rowCount = rowCount;
  permissionsTree.treeBoxObject.view = permissionsTreeView;
  permissionColumnSort();
  
  if (permissions.length == 0)
    document.getElementById("removeAllPermissions").setAttribute("disabled","true");
  else
    document.getElementById("removeAllPermissions").removeAttribute("disabled");
}

function permissionColumnSort() {
  sortAscending = 
    SortTree(permissionsTree, permissionsTreeView, permissions,
             sortColumn, sortColumn, sortAscending);
}

function permissionSelected() {
  var selections = GetTreeSelections(permissionsTree);
  if (selections.length) {
    document.getElementById("removePermission").removeAttribute("disabled");
  }
}

function deletePermissions() {
  var selections = GetTreeSelections(permissionsTree);
  
  for (var s = selections.length - 1; s >= 0; s--) {
    var i = selections[s];

    var host = permissions[i].host;
    updatePendingRemovals(host);

    permissions[i] = null;
  }

  for (var j = 0; j < permissions.length; j++) {
    if (permissions[j] == null) {
      var k = j;
      while ((k < permissions.length) && (permissions[k] == null)) {
        k++;
      }
      permissions.splice(j, k-j);
      permissionsTreeView.rowCount -= k - j;
      permissionsTree.treeBoxObject.rowCountChanged(j, j - k);
    }
  }

  if (permissions.length) {
    var nextSelection = (selections[0] < permissions.length) ? selections[0] : permissions.length - 1;
    permissionsTreeView.selection.select(nextSelection);
    permissionsTree.treeBoxObject.ensureRowIsVisible(nextSelection);
  } 
  else {
    document.getElementById("removePermission").setAttribute("disabled", "true")
    document.getElementById("removeAllPermissions").setAttribute("disabled","true");
  }
}

function deleteAllPermissions() {
  for (var i = 0; i < permissions.length; i++) {
    var host = permissions[i].host;
    updatePendingRemovals(host);
  }

  permissions.length = 0;
  clearTree();
}

function updatePendingRemovals(host) {
  if (additions[host])
    additions[host] = null;
  else
    removals[host] = host;
}

function clearTree() {
  var oldCount = permissionsTreeView.rowCount;
  permissionsTreeView.rowCount = 0;
  permissionsTree.treeBoxObject.rowCountChanged(0, -oldCount);

  document.getElementById("removePermission").setAttribute("disabled", "true")
  document.getElementById("removeAllPermissions").setAttribute("disabled","true");
}

function finalizeChanges() {
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);
  
  var uri;
  var host;
  var i;
  
  //note: the scheme will be taken off later, it is being added now only to
  //create the uri for add/remove
  for (i in additions) {
    host = additions[i];
    if (host != null) {
      host = "http://" + host;
      uri = ioService.newURI(host, null, null);
      permissionManager.add(uri, popupType, true);
    }
  }

  for (i in removals) {
    host = removals[i];
    if (host != null) {
      permissionManager.remove(host, popupType);
    }
  }

}

function handlePermissionKeyPress(e) {
  if (e.keyCode == 46) {
    deletePermissions();
  }
}

function addPermission() {
  var addSiteBox = document.getElementById("addSiteBox");
  var host = addSiteBox.value;
  
  if (host != "") {
    host = host.replace(/^\s*([-\w]*:\/+)?/, ""); // trim any leading space and scheme
    
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                  .getService(Components.interfaces.nsIPromptService); 
    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService);
    
    try {
      var uri = ioService.newURI("http://"+host, null, null);
    }
    catch(ex) {
      var msgInvalid = popupStringBundle.getFormattedString("alertInvalid", [host]);
      if (promptService)
        promptService.alert(window, "", msgInvalid);
      addSiteBox.value = "";
      return;
    }

    host = uri.hostPort;

    if (!host) {
      addSiteBox.value = "";
      return;
    }

    var length = permissions.length;   
 
    var isDuplicate = false;
    for (var i = 0; i < length; i++) {
      if (permissions[i].host == host) {
        var msgDuplicate = popupStringBundle.getFormattedString("alertDuplicate", [host]); 
        if (promptService)
          promptService.alert(window, "", msgDuplicate);
        isDuplicate = true;
        break;
      }
    }

    if (!isDuplicate) {
      var newPermission = new Permission(host, length);
      permissions.push(newPermission);

      sortAscending = !sortAscending; //keep same sort direction
      loadTree();
    
      if (removals[host] != null)
        removals[host] = null;
      else
        additions[host] = host;
    }

    addSiteBox.value = "";
  }
}

function onReturnHit(event) {
  var focusedElement = document.commandDispatcher.focusedElement;
  var addSiteBox = document.getElementById("addSiteBox");
  if (event.keyCode == 13) {
    if (focusedElement) {
      if (focusedElement.id == "permissionsTree")
        return;
      else {
        event.preventBubble();
        if (focusedElement == addSiteBox.inputField) {
          var addSiteButton = document.getElementById("addSiteButton");
          addSiteButton.doCommand();
        }
      }
    }
  }
}

function doHelpButton() {
  openHelp("pop_up_blocking");
  return true;
}

