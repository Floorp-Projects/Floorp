/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
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
// The history window uses JavaScript in bookmarks.js too.

var gHistoryTree;
var gLastHostname;
var gLastDomain;
var gGlobalHistory;
var gPrefService;
var gIOService;
var gDeleteByHostname;
var gDeleteByDomain;
var gHistoryBundle;
var gHistoryStatus;
var gHistoryGrouping = "";

function HistoryCommonInit()
{
    gHistoryTree =  document.getElementById("historyTree");
    gDeleteByHostname = document.getElementById("menu_deleteByHostname");
    gDeleteByDomain =   document.getElementById("menu_deleteByDomain");
    gHistoryBundle =    document.getElementById("historyBundle");
    gHistoryStatus =    document.getElementById("statusbar-display");

    var treeController = new nsTreeController(gHistoryTree);
    var historyController = new nsHistoryController;
    gHistoryTree.controllers.appendController(historyController);

    if ("arguments" in window && window.arguments[0] && window.arguments.length >= 1) {
        // We have been supplied a resource URI to root the tree on
        var uri = window.arguments[0];
        gHistoryTree.setAttribute("ref", uri);
        if (uri.substring(0,5) == "find:" &&
            !(window.arguments.length > 1 && window.arguments[1] == "newWindow")) {
            // Update the windowtype so that future searches are directed 
            // there and the window is not re-used for bookmarks. 
            var windowNode = document.getElementById("history-window");
            windowNode.setAttribute("windowtype", "history:searchresults");
            document.title = gHistoryBundle.getString("search_results_title");

        }
        document.getElementById("groupingMenu").setAttribute("hidden", "true");
    }
    else {
        gPrefService = Components.classes["@mozilla.org/preferences-service;1"]
                                 .getService(Components.interfaces.nsIPrefBranch);
        try {
            gHistoryGrouping = gPrefService.getCharPref("browser.history.grouping");
        }
        catch(e) {
            gHistoryGrouping = "day";
        }
        UpdateTreeGrouping();
        if (gHistoryStatus) {  // must be the window
            switch(gHistoryGrouping) {
            case "none":
                document.getElementById("groupByNone").setAttribute("checked", "true");
                break;
            case "site":
                document.getElementById("groupBySite").setAttribute("checked", "true");
                break;
            case "day":
            default:
                document.getElementById("groupByDay").setAttribute("checked", "true");
            }        
        }
        else {  // must be the sidebar panel
            var pb = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
            var pbi = pb.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
            pbi.addObserver("browser.history.grouping", groupObserver, false);
        }
    } 

    SortInNewDirection(find_sort_direction(find_sort_column()));

    if (gHistoryStatus)
        gHistoryTree.focus();
    gHistoryTree.view.selection.select(0);
}

function HistoryPanelUnload()
{
  var pb = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
  var pbi = pb.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
  pbi.removeObserver("browser.history.grouping", groupObserver, false);
}

function updateHistoryCommands()
{
    goUpdateCommand("cmd_deleteByHostname");
    goUpdateCommand("cmd_deleteByDomain");
}

function historyOnClick(aEvent)
{
  // This is kind of a hack but matches the currently implemented behaviour. 
  // If a status bar is not present, assume we're in sidebar mode, and thus single clicks on containers
  // will open the container. Single clicks on non-containers are handled below in historyOnSelect.
  if (!gHistoryStatus && aEvent.button == 0) {
    var row = { };
    var col = { };
    var elt = { };
    gHistoryTree.treeBoxObject.getCellAt(aEvent.clientX, aEvent.clientY, row, col, elt);
    if (row.value >= 0 && col.value) {
      if (!isContainer(gHistoryTree, row.value))
        OpenURL("current");
      else if (elt.value != "twisty")
        gHistoryTree.treeBoxObject.view.toggleOpenState(row.value);
    }
  }
}

function historyOnSelect()
{
    // every time selection changes, save the last hostname
    gLastHostname = "";
    gLastDomain = "";
    var match;
    var currentIndex = gHistoryTree.currentIndex;
    var rowIsContainer = currentIndex < 0 || (gHistoryGrouping != "none" && isContainer(gHistoryTree, currentIndex));
    var col = gHistoryTree.columns["URL"];
    var url = rowIsContainer ? "" : gHistoryTree.view.getCellText(currentIndex, col);

    if (url) {
        if (!gIOService)
            gIOService = Components.classes['@mozilla.org/network/io-service;1']
                                   .getService(Components.interfaces.nsIIOService);
        try {
            gLastHostname = gIOService.newURI(url, null, null).host;
            // matches the last foo.bar in foo.bar or baz.foo.bar
            match = gLastHostname.match(/([^.]+\.[^.]+$)/);
            if (match)
                gLastDomain = match[1];
        } catch (e) {}
    }

    if (gHistoryStatus)
        gHistoryStatus.label = url;

    document.commandDispatcher.updateCommands("select");
}

function nsHistoryController()
{
}

nsHistoryController.prototype =
{
    supportsCommand: function(command)
    {
        switch(command) {
        case "cmd_deleteByHostname":
        case "cmd_deleteByDomain":
            return true;
        default:
            return false;
        }
    },

    isCommandEnabled: function(command)
    {
        var enabled = false;
        var stringId;
        var text;
        switch(command) {
        case "cmd_deleteByHostname":
            if (gLastHostname) {
                stringId = "deleteHost";
                enabled = true;
            } else {
                stringId = "deleteHostNoSelection";
            }
            text = gHistoryBundle.getFormattedString(stringId, [ gLastHostname ]);
            gDeleteByHostname.setAttribute("label", text);
            break;
        case "cmd_deleteByDomain":
            if (gLastDomain) {
                stringId = "deleteDomain";
                enabled = true;
            } else {
                stringId = "deleteDomainNoSelection";
            }
            text = gHistoryBundle.getFormattedString(stringId, [ gLastDomain ]);
            gDeleteByDomain.setAttribute("label", text);
        }
        return enabled;
    },

    doCommand: function(command)
    {
        switch(command) {
        case "cmd_deleteByHostname":
            if (!gGlobalHistory)
                gGlobalHistory = Components.classes["@mozilla.org/browser/global-history;2"].getService(Components.interfaces.nsIBrowserHistory);
            gGlobalHistory.removePagesFromHost(gLastHostname, false)
            return true;

        case "cmd_deleteByDomain":
            if (!gGlobalHistory)
                gGlobalHistory = Components.classes["@mozilla.org/browser/global-history;2"].getService(Components.interfaces.nsIBrowserHistory);
            gGlobalHistory.removePagesFromHost(gLastDomain, true)
            return true;

        default:
            return false;
        }
    }
}

var historyDNDObserver = {
    onDragStart: function (aEvent, aXferData, aDragAction)
    {
        var currentIndex = gHistoryTree.currentIndex;
        if (isContainer(gHistoryTree, currentIndex))
            return false;
        var col = gHistoryTree.columns["URL"];
        var url = gHistoryTree.view.getCellText(currentIndex, col);
        col = gHistoryTree.columns["Name"];
        var title = gHistoryTree.view.getCellText(currentIndex, col);

        var htmlString = "<A HREF='" + url + "'>" + title + "</A>";
        aXferData.data = new TransferData();
        aXferData.data.addDataForFlavour("text/unicode", url);
        aXferData.data.addDataForFlavour("text/html", htmlString);
        aXferData.data.addDataForFlavour("text/x-moz-url", url + "\n" + title);
        return true;
    }
};

function validClickConditions(event)
{
  var currentIndex = gHistoryTree.currentIndex;
  if (currentIndex == -1) return false;
  var container = isContainer(gHistoryTree, currentIndex);
  return (event.button == 0 &&
          event.originalTarget.localName == 'treechildren' &&
          !container && gHistoryStatus);
}

function collapseExpand()
{
    var currentIndex = gHistoryTree.currentIndex;
    gHistoryTree.treeBoxObject.view.toggleOpenState(currentIndex);
}

function OpenURL(aTarget)
{
    var currentIndex = gHistoryTree.currentIndex;     
    var builder = gHistoryTree.builder.QueryInterface(Components.interfaces.nsIXULTreeBuilder);
    var url = builder.getResourceAtIndex(currentIndex).ValueUTF8;
    var uri = Components.classes["@mozilla.org/network/standard-url;1"].
                createInstance(Components.interfaces.nsIURI);
    uri.spec = url;
    if (uri.schemeIs("javascript") || uri.schemeIs("data")) {
      var strBundleService = Components.classes["@mozilla.org/intl/stringbundle;1"]
                                       .getService(Components.interfaces.nsIStringBundleService);
      var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                    .getService(Components.interfaces.nsIPromptService);
      var historyBundle = strBundleService.createBundle("chrome://communicator/locale/history/history.properties");
      var brandBundle = strBundleService.createBundle("chrome://global/locale/brand.properties");      
      var brandStr = brandBundle.GetStringFromName("brandShortName");
      var errorStr = historyBundle.GetStringFromName("load-js-data-url-error");
      promptService.alert(window, brandStr, errorStr);
      return false;
    }

    if (aTarget != "current") {
      var count = gHistoryTree.view.selection.count;
      var URLArray = [];
      if (count == 1) {
        if (isContainer(gHistoryTree, currentIndex))
          openDialog("chrome://communicator/content/history/history.xul", 
                     "", "chrome,all,dialog=no", url, "newWindow");
        else
          URLArray.push(url);
      }
      else {
        var col = gHistoryTree.columns["URL"];
        var min = new Object(); 
        var max = new Object();
        var rangeCount = gHistoryTree.view.selection.getRangeCount();
        for (var i = 0; i < rangeCount; ++i) {
          gHistoryTree.view.selection.getRangeAt(i, min, max);
          for (var k = max.value; k >= min.value; --k) {
            url = gHistoryTree.view.getCellText(k, col);
            URLArray.push(url);
          }
        }
      }
      if (aTarget == "window") {
        for (i = 0; i < URLArray.length; i++) {
          openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no", URLArray[i]);
        }
      } else {
        if (URLArray.length > 0)
          OpenURLArrayInTabs(URLArray);
      }
    }        
    else if (!isContainer(gHistoryTree, currentIndex))
      openTopWin(url);
    return true;
}

// This opens the URLs contained in the given array in new tabs
// of the most recent window, creates a new window if necessary.
function OpenURLArrayInTabs(aURLArray)
{
  var browserWin = getTopWin();
  if (browserWin) {
    var browser = browserWin.getBrowser();
    browser.selectedTab = browser.addTab(aURLArray[0]);
    for (var i = 1; i < aURLArray.length; ++i)
      tab = browser.addTab(aURLArray[i]);
  } else {
    openTopWin(aURLArray.join("\n")); // Pretend that we're a home page group
  }
}

/**
 * Root the tree on a given URI (used for displaying search results)
 */
function setRoot(root)
{
  gHistoryTree.ref = root;
}

function GroupBy(aGroupingType)
{
  gHistoryGrouping = aGroupingType;
  UpdateTreeGrouping();
  gPrefService.setCharPref("browser.history.grouping", aGroupingType);
}

function UpdateTreeGrouping()
{
  var tree = document.getElementById("historyTree");
  switch(gHistoryGrouping) {
  case "none":
    tree.setAttribute("ref", "NC:HistoryRoot");
    break;
  case "site":
    tree.setAttribute("ref", "find:datasource=history&groupby=Hostname");
    break;
  case "day":
  default:
    tree.setAttribute("ref", "NC:HistoryByDate");
    break;
  }
}

var groupObserver = {
  observe: function(aPrefBranch, aTopic, aPrefName) {
    try {
      gHistoryGrouping = gPrefService.getCharPref("browser.history.grouping");
    }
    catch(e) {
      gHistoryGrouping = "day";
    }
    UpdateTreeGrouping();
  }
}

function historyAddBookmarks()
{
  var urlCol = gHistoryTree.columns["URL"];
  var titleCol = gHistoryTree.columns["Name"];

  var count = gHistoryTree.view.selection.count;
  var url;
  var title;
  if (count == 1) {
    var currentIndex = gHistoryTree.currentIndex;
    url = gHistoryTree.treeBoxObject.view.getCellText(currentIndex, urlCol);
    title = gHistoryTree.treeBoxObject.view.getCellText(currentIndex, titleCol);
    BookmarksUtils.addBookmark(url, title, null, true);
  }
  else if (count > 1) {
    var min = new Object(); 
    var max = new Object();
    var rangeCount = gHistoryTree.view.selection.getRangeCount();
    if (!BMSVC) {
      initServices();
      initBMService();
    }
    for (var i = 0; i < rangeCount; ++i) {
      gHistoryTree.view.selection.getRangeAt(i, min, max);
      for (var k = max.value; k >= min.value; --k) {
        url = gHistoryTree.view.getCellText(k, urlCol);
        title = gHistoryTree.view.getCellText(k, titleCol);
        BookmarksUtils.addBookmark(url, title, null, false);
      }
    }
  }
}


function updateItems()
{
  var count = gHistoryTree.view.selection.count;
  var openItem = document.getElementById("miOpen");
  var bookmarkItem = document.getElementById("miAddBookmark");
  var copyLocationItem = document.getElementById("miCopyLinkLocation");
  var sep1 = document.getElementById("pre-bookmarks-separator");
  var sep2 = document.getElementById("post-bookmarks-separator");
  var openItemInNewWindow = document.getElementById("miOpenInNewWindow");
  var openItemInNewTab = document.getElementById("miOpenInNewTab");
  var collapseExpandItem = document.getElementById("miCollapseExpand");
  if (count > 1) {
    var hasContainer = false;
    if (gHistoryGrouping != "none") {
      var min = new Object(); 
      var max = new Object();
      var rangeCount = gHistoryTree.view.selection.getRangeCount();
      for (var i = 0; i < rangeCount; ++i) {
        gHistoryTree.view.selection.getRangeAt(i, min, max);
        for (var k = max.value; k >= min.value; --k) {
          if (isContainer(gHistoryTree, k)) {
            hasContainer = true;
            break;
          }
        }
      }
    }
    collapseExpandItem.hidden = true;
    openItem.hidden = true;
    if (hasContainer) {   // several items selected, folder(s) amongst them
      bookmarkItem.hidden = true;
      copyLocationItem.hidden = true;
      sep1.hidden = true;
      sep2.hidden = true;
      openItemInNewWindow.hidden = true;
      openItemInNewTab.hidden = true;
    }
    else {                // several items selected, but no folder
      bookmarkItem.hidden = false;
      copyLocationItem.hidden = false;
      sep1.hidden = false;
      sep2.hidden = false;
      bookmarkItem.setAttribute("label", document.getElementById('multipleBookmarks').getAttribute("label"));
      bookmarkItem.setAttribute("accesskey", document.getElementById('multipleBookmarks').getAttribute("accesskey"));
      openItem.removeAttribute("default");
      openItemInNewWindow.setAttribute("default", "true");
      openItemInNewWindow.hidden = false;
      openItemInNewTab.hidden = false;
    }
  }
  else {
    openItemInNewWindow.hidden = false;
    bookmarkItem.setAttribute("label", document.getElementById('oneBookmark').getAttribute("label"));
    bookmarkItem.setAttribute("accesskey", document.getElementById('oneBookmark').getAttribute("accesskey"));
    sep2.hidden = false;
    var currentIndex = gHistoryTree.currentIndex;
    if (isContainer(gHistoryTree, currentIndex)) {   // one folder selected
        openItem.hidden = true;
        openItem.removeAttribute("default");
        openItemInNewWindow.removeAttribute("default");
        openItemInNewTab.hidden = true;
        collapseExpandItem.hidden = false;
        collapseExpandItem.setAttribute("default", "true");
        bookmarkItem.hidden = true;
        copyLocationItem.hidden = true;
        sep1.hidden = true;
        if (isContainerOpen(gHistoryTree, currentIndex)) {
          collapseExpandItem.setAttribute("label", gHistoryBundle.getString("collapseLabel"));
          collapseExpandItem.setAttribute("accesskey", gHistoryBundle.getString("collapseAccesskey"));
        }
        else {
          collapseExpandItem.setAttribute("label", gHistoryBundle.getString("expandLabel"));
          collapseExpandItem.setAttribute("accesskey", gHistoryBundle.getString("expandAccesskey"));          
        }
        return true;
    }                                                // one entry selected (not a folder)
    openItemInNewTab.hidden = false;
    collapseExpandItem.hidden = true;
    bookmarkItem.hidden = false;
    copyLocationItem.hidden = false;
    sep1.hidden = false;
    if (!getTopWin()) {
        openItem.hidden = true;
        openItem.removeAttribute("default");
        openItemInNewWindow.setAttribute("default", "true");
    }
    else {
      openItem.hidden = false;
      if (!openItem.getAttribute("default"))
        openItem.setAttribute("default", "true");
      openItemInNewWindow.removeAttribute("default");
    }
  }
  return true;
}
