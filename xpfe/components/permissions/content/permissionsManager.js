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

var popupManager = null;

var permissions = [];

var listCapability; // the capability of sites on the currently viewed list
// TRUE: current popup policy is BLOCK ALL WITH EXCEPTIONS - sites on
// the whitelist are allowed and have permission.capability = true
// FALSE: current popup policy is ALLOW ALL WITH EXCEPTIONS - sites on
// the blacklist are blocked and have permission.capability = false

const POPUP_TYPE = 2;

var additions = [];
var removals = [];

const SIMPLEURI_CONTRACTID = "@mozilla.org/network/simple-uri;1";

var sortColumn = "host";
var lastSort = false;

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
  popupManager = Components.classes["@mozilla.org/PopupWindowManager;1"]
                           .getService(Components.interfaces.nsIPopupWindowManager);

  permissionsTree = document.getElementById("permissionsTree");

  popupStringBundle = document.getElementById("popupStringBundle");
  var title;

  if (window.arguments[0]) {
    document.getElementById("blockExcept").hidden = false;
    lastSort = (permissionsTree.getAttribute("lastSortWhitelist") == "true");
    title = popupStringBundle.getString("whitelistTitle");
  }
  else {
    document.getElementById("allowExcept").hidden = false;
    lastSort = (permissionsTree.getAttribute("lastSortBlacklist") == "true");
    title = popupStringBundle.getString("blacklistTitle");
  }

  document.getElementById("popupManager").setAttribute("title", title);

  listCapability = window.arguments[0];

  loadPermissions(permissions);
  loadTree();
   
  if (window.arguments[1]) { // dialog opened from statusbar icon
    if (listCapability) {
      document.getElementById("addSiteBox").value = window.arguments[1];
    }
    else {
     // pre-pend '.' so we always match on host boundaries. Otherwise 
     // we might think notfoo.com matches foo.com
      var currentLoc = '.'+window.arguments[1];
      var nextHost;
      var inList;

      var matchIndex;
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
          if (listCapability) // host is already on whitelist, don't prefill
            break;

          if (nextHost.length > matchLength) {
            matchIndex = i;
            matchLength = nextHost.length;
          }
        }        
      }
      
      if (inList) // host is in blacklist, select for removal
        permissionsTree.treeBoxObject.view.selection.select(matchIndex);
    }
  }

  document.documentElement.addEventListener("keypress", onReturnHit, true);

  window.sizeToContent();
}

function onAccept() {
  finalizeChanges();
  
  var unblocked; 

  if (listCapability) {
    unblocked = additions;
    permissionsTree.setAttribute("lastSortWhitelist", !lastSort);
  }
  else {
    unblocked = removals;
    permissionsTree.setAttribute("lastSortBlacklist", !lastSort);
  }

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

  if (window.arguments[2])
    window.opener.setButtons();


  return true;                                           
}

function Permission(host, number) {
  this.host = host;
  this.number = number;
}

function loadPermissions(table) {
  var enumerator = popupManager.getEnumerator();
  var count = 0;
  
  while (enumerator.hasMoreElements()) {
    var permission = enumerator.getNext()
                               .QueryInterface(Components.interfaces.nsIPermission);
    if (permission.capability == listCapability) {
      var host = permission.host;
      table[count] = new Permission(host,count++);
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
  lastSort = 
    SortTree(permissionsTree, permissionsTreeView, permissions,
             sortColumn, sortColumn, lastSort);
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
    }
  }

  var box = permissionsTree.treeBoxObject;
  var firstRow = box.getFirstVisibleRow();
  if (firstRow > (permissions.length - 1) ) {
    firstRow = permissions.length - 1;
  }
  permissionsTreeView.rowCount = permissions.length;
  box.rowCountChanged(0, permissions.length);
  box.scrollToRow(firstRow);

  if (permissions.length) {
    var nextSelection = (selections[0] < permissions.length) ? selections[0] : permissions.length - 1;
    box.view.selection.select(-1); 
    box.view.selection.select(nextSelection);
  } 
  else {
    document.getElementById("removePermission").setAttribute("disabled", "true")
    document.getElementById("removeAllPermissions").setAttribute("disabled","true");

    permissionsTree.treeBoxObject.view.selection.select(-1); 
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
  if (additions[host] != null)
    additions[host] = null;
  else
    removals[host] = host;
}

function clearTree() {
  permissionsTree.treeBoxObject.view.selection.select(-1); 

  permissionsTreeView.rowCount = 0;
  permissionsTree.treeBoxObject.invalidate();

  document.getElementById("removePermission").setAttribute("disabled", "true")
  document.getElementById("removeAllPermissions").setAttribute("disabled","true");
}

function finalizeChanges() {
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);
  
  //note: the scheme will be taken off later, it is being added now only to
  //create the uri for add/remove
  for (var i in additions) {
    var host = additions[i];
    if (host != null) {
      host = "http://" + host;
      var uri = ioService.newURI(host, null, null);
      popupManager.add(uri, listCapability);
    }
  }

  for (var i in removals) {
    var host = removals[i];
    if (host != null) {
      host = "http://" + host;
      var uri = ioService.newURI(host, null, null);
      popupManager.remove(uri);
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
    host = host.replace(/^\s*([-\w]*:\/*)?/, ""); // trim any leading space and scheme    
    
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

      lastSort = !lastSort; //keep same sort direction
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
  openHelp("pop_up_blocking_prefs");
  return true;
}

