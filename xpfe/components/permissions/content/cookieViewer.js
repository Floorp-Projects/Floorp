/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

var kObserverService;

// interface variables
var cookiemanager         = null;          // cookiemanager interface
var permissionmanager     = null;          // permissionmanager interface
var promptservice         = null;          // promptservice interface
var popupmanager          = null;          // popup manager
var gDateService = null;

// cookies and permissions list
var cookies              = [];
var permissions          = [];
var deletedCookies       = [];
var deletedPermissions   = [];

// differentiate between cookies, images, and popups
const nsIPermissionManager = Components.interfaces.nsIPermissionManager;
const nsICookiePermission = Components.interfaces.nsICookiePermission;
const cookieType = "cookie";
const imageType = "image";
const popupType = "popup";


var dialogType = cookieType;
if (window.arguments[0] == "imageManager")
  dialogType = imageType;
else if (window.arguments[0] == "popupManager")
  dialogType = popupType;

var cookieBundle;
var gUpdatingBatch = "";

function Startup() {

  // arguments passed to this routine:
  //   cookieManager 
  //   cookieManagerFromIcon
  //   imageManager
 
  // xpconnect to cookiemanager/permissionmanager/promptservice/popupmanager interfaces
  cookiemanager = Components.classes["@mozilla.org/cookiemanager;1"].getService();
  cookiemanager = cookiemanager.QueryInterface(Components.interfaces.nsICookieManager);
  permissionmanager = Components.classes["@mozilla.org/permissionmanager;1"].getService();
  permissionmanager = permissionmanager.QueryInterface(Components.interfaces.nsIPermissionManager);
  promptservice = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
  promptservice = promptservice.QueryInterface(Components.interfaces.nsIPromptService);
  popupmanager = Components.classes["@mozilla.org/PopupWindowManager;1"].getService();
  popupmanager = popupmanager.QueryInterface(Components.interfaces.nsIPopupWindowManager);

  // intialize gDateService
  if (!gDateService) {
    const nsScriptableDateFormat_CONTRACTID = "@mozilla.org/intl/scriptabledateformat;1";
    const nsIScriptableDateFormat = Components.interfaces.nsIScriptableDateFormat;
    gDateService = Components.classes[nsScriptableDateFormat_CONTRACTID]
      .getService(nsIScriptableDateFormat);
  }

  // intialize string bundle
  cookieBundle = document.getElementById("cookieBundle");

  // label the close button
  document.documentElement.getButton("accept").label = cookieBundle.getString("close");

  // determine if labelling is for cookies or images
  try {
    var tabBox = document.getElementById("tabbox");
    var element;
    if (dialogType == cookieType) {
      element = document.getElementById("cookiesTab");
      tabBox.selectedTab = element;
    } else if (dialogType == imageType) {
      document.title = cookieBundle.getString("imageTitle");

      element = document.getElementById("permissionsTab");
      element.label = cookieBundle.getString("tabBannedImages");
      tabBox.selectedTab = element;

      element = document.getElementById("permissionsText");
      element.value = cookieBundle.getString("textBannedImages");

      // Hide a dummy vbox inside the real box
      // If the actual box is hidden, the tabbox gets confused.
      // The first tab now controls the second panel.
      // Hiding the first tab doesn't help.
      document.getElementById("dummyContainer").hidden = "true";
      document.getElementById("cookiesTab").hidden = "true";

      element = document.getElementById("btnSession");
      element.hidden = "true";
    } else {
      document.title = cookieBundle.getString("popupTitle");

      element = document.getElementById("permissionsTab");
      element.label = cookieBundle.getString("tabBannedPopups");
      tabBox.selectedTab = element;

      element = document.getElementById("permissionsText");
      element.value = cookieBundle.getString("textBannedPopups");

      document.getElementById("dummyContainer").hidden = "true";
      document.getElementById("cookiesTab").hidden = "true";
    }
  } catch(e) {
  }
  // load in the cookies and permissions
  cookiesTree = document.getElementById("cookiesTree");
  permissionsTree = document.getElementById("permissionsTree");
  if (dialogType == cookieType) {
    loadCookies();
  }
  loadPermissions();

  // be prepared to reload the display if anything changes
  kObserverService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  kObserverService.addObserver(cookieReloadDisplay, "cookieChanged", false);
  kObserverService.addObserver(cookieReloadDisplay, "perm-changed", false);
}

function Shutdown() {
  kObserverService.removeObserver(cookieReloadDisplay, "cookieChanged");
  kObserverService.removeObserver(cookieReloadDisplay, "perm-changed");
}

var cookieReloadDisplay = {
  observe: function(subject, topic, state) {
    if (topic == gUpdatingBatch)
      return;
    if (topic == "cookieChanged") {
      if (state == "cookies") {
        cookies.length = 0;
        if (lastCookieSortColumn == "rawHost") {
          lastCookieSortAscending = !lastCookieSortAscending; // prevents sort from being reversed
        }
        loadCookies();
      }
    } else if (topic == "perm-changed") {
      permissions.length = 0;
      if (lastPermissionSortColumn == "rawHost") {
        lastPermissionSortAscending = !lastPermissionSortAscending; // prevents sort from being reversed
      }
      loadPermissions();
    }
  }
}

/*** =================== COOKIES CODE =================== ***/

const nsICookie = Components.interfaces.nsICookie;

var cookiesTreeView = {
  rowCount : 0,
  setTree : function(tree){},
  getImageSrc : function(row,column) {},
  getProgressMode : function(row,column) {},
  getCellValue : function(row,column) {},
  getCellText : function(row,column){
    var rv="";
    switch (column.id) {
    case "domainCol":
      rv = cookies[row].rawHost;
      break;
    case "nameCol":
      rv = cookies[row].name;
      break;
    case "statusCol":
      rv = cookies[row].status;
      break;
    case "expiresCol":
      rv = cookies[row].expires;
      break;
    }
    return rv;
  },
  isSeparator : function(index) {return false;},
  isSorted: function() { return false; },
  isContainer : function(index) {return false;},
  cycleHeader : function(aCol) {},
  getRowProperties : function(row,prop) {},
  getColumnProperties : function(column,prop) {},
  getCellProperties : function(row,column,prop) {}
 };
var cookiesTree;

function Cookie(number,name,value,isDomain,host,rawHost,path,isSecure,expires,
                status,policy) {
  this.number = number;
  this.name = name;
  this.value = value;
  this.isDomain = isDomain;
  this.host = host;
  this.rawHost = rawHost;
  this.path = path;
  this.isSecure = isSecure;
  this.expires = GetExpiresString(expires);
  this.expiresSortValue = expires;
  this.status = GetStatusString(status);
  this.policy = policy;
}

function loadCookies() {
  // load cookies into a table
  var enumerator = cookiemanager.enumerator;
  var count = 0;
  var showPolicyField = false;
  while (enumerator.hasMoreElements()) {
    var nextCookie = enumerator.getNext();
    if (!nextCookie) break;
    nextCookie = nextCookie.QueryInterface(Components.interfaces.nsICookie);
    var host = nextCookie.host;
    if (nextCookie.policy != nsICookie.POLICY_UNKNOWN) {
      showPolicyField = true;
    }
    cookies[count] =
      new Cookie(count++, nextCookie.name, nextCookie.value, nextCookie.isDomain, host,
                 (host.charAt(0)==".") ? host.substring(1,host.length) : host,
                 nextCookie.path, nextCookie.isSecure, nextCookie.expires,
                 nextCookie.status, nextCookie.policy);
  }
  cookiesTreeView.rowCount = cookies.length;

  // sort and display the table
  cookiesTree.treeBoxObject.view = cookiesTreeView;
  if (window.arguments[0] == "cookieManagerFromIcon") { // came here by clicking on cookie icon

    // turn off the icon
    var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
    observerService.notifyObservers(null, "cookieIcon", "off");

    // unhide the status column and sort by reverse order on that column
    //    Note that the sort routine normally does an ascending sort.  The only
    //    exception is if you sort on the same column more than once in a row.
    //    In that case, the subsequent sorts will be the reverse of the previous ones.
    //    So we are now going to force an initial reverse sort by fooling the sort routine
    //    into thinking that it previously sorted on the status column and in ascending
    //    order.
    document.getElementById("statusCol").removeAttribute("hidden");
    lastCookieSortAscending = true; // force order to have blanks last instead of vice versa
    lastCookieSortColumn = 'status';
    CookieColumnSort('status');

  } else {
    // sort by host column
    CookieColumnSort('rawHost');
  }

  // disable "remove all cookies" button if there are no cookies
  if (cookies.length == 0) {
    document.getElementById("removeAllCookies").setAttribute("disabled","true");
  } else {
    document.getElementById("removeAllCookies").removeAttribute("disabled");
  }

  // show policy field if at least one cookie has a policy
  if (showPolicyField) {
    document.getElementById("policyField").removeAttribute("hidden");
  }
}

function GetExpiresString(expires) {
  if (expires) {
    var date = new Date(1000*expires);

    // if a server manages to set a really long-lived cookie, the dateservice
    // can't cope with it properly, so we'll just return a blank string
    // see bug 238045 for details
    var expiry = "";
    try {
      expiry = gDateService.FormatDateTime("", gDateService.dateFormatLong,
                                           gDateService.timeFormatSeconds, 
                                           date.getFullYear(), date.getMonth()+1, 
                                           date.getDate(), date.getHours(),
                                           date.getMinutes(), date.getSeconds());
    } catch(ex) {
      // do nothing
    }
    return expiry;
  }
  return cookieBundle.getString("AtEndOfSession");
}

function GetStatusString(status) {
  switch (status) {
    case nsICookie.STATUS_ACCEPTED:
      return cookieBundle.getString("accepted");
    case nsICookie.STATUS_FLAGGED:
      return cookieBundle.getString("flagged");
    case nsICookie.STATUS_DOWNGRADED:
      return cookieBundle.getString("downgraded");
  }
  return "";
}

function GetPolicyString(policy) {
  switch (policy) {
    case nsICookie.POLICY_NONE:
      return cookieBundle.getString("policyUnstated");
    case nsICookie.POLICY_NO_CONSENT:
      return cookieBundle.getString("policyNoConsent");
    case nsICookie.POLICY_IMPLICIT_CONSENT:
      return cookieBundle.getString("policyImplicitConsent");
    case nsICookie.POLICY_EXPLICIT_CONSENT:
      return cookieBundle.getString("policyExplicitConsent");
    case nsICookie.POLICY_NO_II:
      return cookieBundle.getString("policyNoIICollected");
  }
  return "";
}

function CookieSelected() {
  var selections = GetTreeSelections(cookiesTree);
  if (selections.length) {
    document.getElementById("removeCookie").removeAttribute("disabled");
  } else {
    document.getElementById("removeCookie").setAttribute("disabled", "true");
    ClearCookieProperties();
    return true;
  }
    
  var idx = selections[0];
  if (idx >= cookies.length) {
    // Something got out of synch.  See bug 119812 for details
    dump("Tree and viewer state are out of sync! " +
         "Help us figure out the problem in bug 119812");
    return false;
  }

  var props = [
    {id: "ifl_name", value: cookies[idx].name},
    {id: "ifl_value", value: cookies[idx].value}, 
    {id: "ifl_isDomain",
     value: cookies[idx].isDomain ?
            cookieBundle.getString("domainColon") : cookieBundle.getString("hostColon")},
    {id: "ifl_host", value: cookies[idx].host},
    {id: "ifl_path", value: cookies[idx].path},
    {id: "ifl_isSecure",
     value: cookies[idx].isSecure ?
            cookieBundle.getString("forSecureOnly") : 
            cookieBundle.getString("forAnyConnection")},
    {id: "ifl_expires", value: cookies[idx].expires},
    {id: "ifl_policy", value: GetPolicyString(cookies[idx].policy)}
  ];

  var value;
  var field;
  for (var i = 0; i < props.length; i++)
  {
    field = document.getElementById(props[i].id);
    if ((selections.length > 1) && (props[i].id != "ifl_isDomain")) {
      value = ""; // clear field if multiple selections
    } else {
      value = props[i].value;
    }
    field.value = value;
  }
  return true;
}

function ClearCookieProperties() {
  var properties = 
    ["ifl_name","ifl_value","ifl_host","ifl_path","ifl_isSecure","ifl_expires","ifl_policy"];
  for (var prop=0; prop<properties.length; prop++) {
    document.getElementById(properties[prop]).value = "";
  }
}

function DeleteCookie() {
  DeleteSelectedItemFromTree(cookiesTree, cookiesTreeView,
                                 cookies, deletedCookies,
                                 "removeCookie", "removeAllCookies");
  if (!cookies.length) {
    ClearCookieProperties();
  }
  FinalizeCookieDeletions();
}

function DeleteAllCookies() {
  var title = cookieBundle.getString("deleteAllCookiesTitle");
  var msg = cookieBundle.getString("deleteAllCookies");
  if (!promptservice.confirm(window, title, msg ))
    return;

  ClearCookieProperties();
  DeleteAllFromTree(cookiesTree, cookiesTreeView,
                        cookies, deletedCookies,
                        "removeCookie", "removeAllCookies");
  FinalizeCookieDeletions();
}

function FinalizeCookieDeletions() {
  gUpdatingBatch = "cookieChanged";
  for (var c=0; c<deletedCookies.length; c++) {
    cookiemanager.remove(deletedCookies[c].host,
                         deletedCookies[c].name,
                         deletedCookies[c].path,
                         document.getElementById("checkbox").checked);
  }
  deletedCookies.length = 0;
  gUpdatingBatch = "";
}

function HandleCookieKeyPress(e) {
  if (e.keyCode == 46) {
    DeleteCookie();
  }
}

var lastCookieSortColumn = "";
var lastCookieSortAscending = false;

function CookieColumnSort(column) {
  lastCookieSortAscending =
      SortTree(cookiesTree, cookiesTreeView, cookies,
               column, lastCookieSortColumn, lastCookieSortAscending);
  lastCookieSortColumn = column;
  // set the sortDirection attribute to get the styling going
  // first we need to get the right element
  var sortedCol;
  switch (column) {
    case "rawHost":
      sortedCol = document.getElementById("domainCol");
      break;
    case "name":
      sortedCol = document.getElementById("nameCol");
      break;
    case "expires":
      sortedCol = document.getElementById("expiresCol");
      break;
    case "status":
      sortedCol = document.getElementById("statusCol");
      break;
  }
  if (lastCookieSortAscending)
    sortedCol.setAttribute("sortDirection", "descending");
  else
    sortedCol.setAttribute("sortDirection", "ascending");

  // clear out the sortDirection attribute on the rest of the columns
  var currentCol = sortedCol.parentNode.firstChild;
  while (currentCol) {
    if (currentCol != sortedCol && currentCol.localName == "treecol")
      currentCol.removeAttribute("sortDirection");
    currentCol = currentCol.nextSibling;
  }
}

/*** =================== PERMISSIONS CODE =================== ***/

var permissionsTreeView = {
  rowCount : 0,
  setTree : function(tree){},
  getImageSrc : function(row,column) {},
  getProgressMode : function(row,column) {},
  getCellValue : function(row,column) {},
  getCellText : function(row,column){
    var rv="";
    if (column.id=="siteCol") {
      rv = permissions[row].rawHost;
    } else if (column.id=="capabilityCol") {
      rv = permissions[row].capability;
    }
    return rv;
  },
  isSeparator : function(index) {return false;},
  isSorted: function() { return false; },
  isContainer : function(index) {return false;},
  cycleHeader : function(aCol) {},
  getRowProperties : function(row,prop) {},
  getColumnProperties : function(column,prop) {},
  getCellProperties : function(row,column,prop) {}
 };
var permissionsTree;

function Permission(number, host, rawHost, type, capability) {
  this.number = number;
  this.host = host;
  this.rawHost = rawHost;
  this.type = type;
  this.capability = capability;
}

function loadPermissions() {
  // load permissions into a table
  var enumerator = permissionmanager.enumerator;
  var count = 0;
  var contentStr;
  var canStr, cannotStr, canSessionStr;
  switch (dialogType) {
    case cookieType:
      canStr="can";
      canSessionStr="canSession";
      cannotStr="cannot";
      break;
    case imageType:
      canStr="canImages";
      cannotStr="cannotImages";
      break;
    case popupType:
      canStr="canPopups";
      cannotStr="cannotPopups";
      break;
    default:
      return;
  }
  while (enumerator.hasMoreElements()) {
    var nextPermission = enumerator.getNext();
    nextPermission = nextPermission.QueryInterface(Components.interfaces.nsIPermission);
    if (nextPermission.type == dialogType) {
      var host = nextPermission.host;
      var capability;
      switch (nextPermission.capability) {
        case nsIPermissionManager.ALLOW_ACTION:
          capability = canStr;
          break;
        case nsIPermissionManager.DENY_ACTION:
          capability = cannotStr;
          break;
        case nsICookiePermission.ACCESS_SESSION:
          // we should only hit this for cookies
          capability = canSessionStr;
          break;
        default:
          return;
      }
      permissions[count] = 
        new Permission(count++, host,
                       (host.charAt(0)==".") ? host.substring(1,host.length) : host,
                       nextPermission.type,
                       cookieBundle.getString(capability));
    }
  }
  permissionsTreeView.rowCount = permissions.length;

  // sort and display the table
  permissionsTree.treeBoxObject.view = permissionsTreeView;
  PermissionColumnSort('rawHost', false);

  // disable "remove all" button if there are no cookies/images
  if (permissions.length == 0) {
    document.getElementById("removeAllPermissions").setAttribute("disabled", "true");
  } else {
    document.getElementById("removeAllPermissions").removeAttribute("disabled");
  }

}

function PermissionSelected() {
  var selections = GetTreeSelections(permissionsTree);
  if (selections.length)
    document.getElementById("removePermission").removeAttribute("disabled");
  else
    document.getElementById("removePermission").setAttribute("disabled", "true");
}

function DeletePermission() {
  DeleteSelectedItemFromTree(permissionsTree, permissionsTreeView,
                                 permissions, deletedPermissions,
                                 "removePermission", "removeAllPermissions");
  FinalizePermissionDeletions();
}

function setCookiePermissions(action) {
  var site = document.getElementById('cookie-site');
  var url = site.value.replace(/^\s*([-\w]*:\/+)?/, "http://");
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);
  var uri = ioService.newURI(url, null, null);
  
  // only set the permission if the permission doesn't already exist
  if (permissionmanager.testPermission(uri, dialogType) != action)
    permissionmanager.add(uri, dialogType, action);

  site.focus();
  site.value = "";
}

function buttonEnabling(textfield) {
  // trim any leading space
  var site = textfield.value.replace(/^\s*([-\w]*:\/+)?/, "");
  var block = document.getElementById("btnBlock");
  var session = document.getElementById("btnSession");
  var allow = document.getElementById("btnAllow");
  block.disabled = !site;
  if (dialogType == cookieType)
    session.disabled = !site;
  allow.disabled = !site;
}

function DeleteAllPermissions() {
  var title = cookieBundle.getString("deleteAllSitesTitle");
  var msgType = dialogType == cookieType ? "deleteAllCookiesSites" : "deleteAllImagesSites";
  var msg = cookieBundle.getString(msgType);
  if (!promptservice.confirm(window, title, msg ))
    return;
    
  DeleteAllFromTree(permissionsTree, permissionsTreeView,
                        permissions, deletedPermissions,
                        "removePermission", "removeAllPermissions");
  FinalizePermissionDeletions();
}

function FinalizePermissionDeletions() {
  if (!deletedPermissions.length)
    return;

  gUpdatingBatch = "perm-changed";
  var p;
  if (deletedPermissions[0].type == popupType) {
    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService);
    for (p = 0; p < deletedPermissions.length; ++p)
      // we lost the URI's original scheme, but this will do because the scheme
      // is stripped later anyway.
      popupmanager.remove(ioService.newURI("http://"+deletedPermissions[p].host, null, null));
  } else {
    for (p = 0; p < deletedPermissions.length; ++p)
      permissionmanager.remove(deletedPermissions[p].host, deletedPermissions[p].type);
  }
  deletedPermissions.length = 0;
  gUpdatingBatch = "";
}

function HandlePermissionKeyPress(e) {
  if (e.keyCode == 46) {
    DeletePermission();
  }
}

var lastPermissionSortColumn = "";
var lastPermissionSortAscending = false;

function PermissionColumnSort(column, updateSelection) {
  lastPermissionSortAscending =
    SortTree(permissionsTree, permissionsTreeView, permissions,
                 column, lastPermissionSortColumn, lastPermissionSortAscending, 
                 updateSelection);
  lastPermissionSortColumn = column;
  
  // make sure sortDirection is set
  var sortedCol;
  switch (column) {
    case "rawHost":
      sortedCol = document.getElementById("siteCol");
      break;
    case "capability":
      sortedCol = document.getElementById("capabilityCol");
      break;
  }
  if (lastPermissionSortAscending)
    sortedCol.setAttribute("sortDirection", "descending");
  else
    sortedCol.setAttribute("sortDirection", "ascending");

  // clear out the sortDirection attribute on the rest of the columns
  var currentCol = sortedCol.parentNode.firstChild;
  while (currentCol) {
    if (currentCol != sortedCol && currentCol.localName == "treecol")
      currentCol.removeAttribute("sortDirection");
    currentCol = currentCol.nextSibling;
  }
}

/*** ============ CODE FOR HELP BUTTON =================== ***/

function getSelectedTab()
{
  var selTab = document.getElementById('tabbox').selectedTab;
  var selTabID = selTab.getAttribute('id');
  if (selTabID == 'cookiesTab') {
    key = "cookies_stored";
  } else {
    key = "cookie_sites";
  }  
  return key;
}

function doHelpButton() {
  if (dialogType == imageType) {
    openHelp("image_mgr");
  } else {
    var uri = getSelectedTab();
    openHelp(uri);
  }
  // XXX missing popup help
}
