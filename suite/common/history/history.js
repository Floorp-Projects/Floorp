/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *  Alec Flett <alecf@netscape.com>
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
// The history window uses JavaScript in bookmarks.js too.

var gHistoryOutliner;
var gLastHostname;
var gLastDomain;
var gGlobalHistory;
var gPrefService;
var gDeleteByHostname;
var gDeleteByDomain;
var gHistoryBundle;
var gHistoryStatus;
var gHistoryGrouping = "";
var gWindowManager = null;

function HistoryInit()
{
    gHistoryOutliner =  document.getElementById("historyOutliner");
    gDeleteByHostname = document.getElementById("menu_deleteByHostname");
    gDeleteByDomain =   document.getElementById("menu_deleteByDomain");
    gHistoryBundle =    document.getElementById("historyBundle");
    gHistoryStatus =    document.getElementById("statusbar-display");

    var outlinerController = new nsOutlinerController(gHistoryOutliner);
    var historyController = new nsHistoryController;
    gHistoryOutliner.controllers.appendController(historyController);

    if ("arguments" in window && window.arguments[0] && window.arguments.length >= 1) {
        // We have been supplied a resource URI to root the tree on
        var uri = window.arguments[0];
        gHistoryOutliner.setAttribute("ref", uri);
        if (uri.substring(0,5) == "find:" &&
            !(window.arguments.length > 1 && window.arguments[1] == "newWindow")) {
            // Update the windowtype so that future searches are directed 
            // there and the window is not re-used for bookmarks. 
            var windowNode = document.getElementById("history-window");
            windowNode.setAttribute("windowtype", "history:searchresults");
            windowNode.setAttribute("title", gHistoryBundle.getString("search_results_title"));

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
        GroupBy(gHistoryGrouping);
        if (gHistoryStatus) {  // must be the window
            switch(gHistoryGrouping) {
            case "none":
                document.getElementById("groupByNone").setAttribute("checked", "true");
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
    gHistoryOutliner.focus();
    gHistoryOutliner.outlinerBoxObject.view.selection.select(0);
}

function updateHistoryCommands()
{
    goUpdateCommand("cmd_deleteByHostname");
    goUpdateCommand("cmd_deleteByDomain");
}

function historyOnSelect()
{
    if (!gHistoryStatus) {
        OpenURL(false);
        return;
    }

    // every time selection changes, save the last hostname
    gLastHostname = "";
    gLastDomain = "";
    var match;
    var currentIndex = gHistoryOutliner.currentIndex;
    var rowIsContainer = gHistoryGrouping == "day" ? isContainer(gHistoryOutliner, currentIndex) : false;
    var url = gHistoryOutliner.outlinerBoxObject.view.getCellText(currentIndex, "URL");

    if (url && !rowIsContainer) {
        // matches scheme://(hostname)...
        match = url.match(/^.*?:\/\/(?:([^\/:]*)(?::([^\/:]*))?@)?([^\/:]*)(?::([^\/:]*))?(.*)$/);

        if (match && match.length>1)
            gLastHostname = match[3];
      
        gHistoryStatus.label = url;
    }
    else {
        gHistoryStatus.label = "";
    }

    if (gLastHostname) {
        // matches the last foo.bar in foo.bar or baz.foo.bar
        match = gLastHostname.match(/([^.]+\.[^.]+$)/);
        if (match)
            gLastDomain = match[1];
    }
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
                gGlobalHistory = Components.classes["@mozilla.org/browser/global-history;1"].getService(Components.interfaces.nsIBrowserHistory);
            gGlobalHistory.removePagesFromHost(gLastHostname, false)
            gHistoryOutliner.builder.rebuild();
            return true;

        case "cmd_deleteByDomain":
            if (!gGlobalHistory)
                gGlobalHistory = Components.classes["@mozilla.org/browser/global-history;1"].getService(Components.interfaces.nsIBrowserHistory);
            gGlobalHistory.removePagesFromHost(gLastDomain, true)
            gHistoryOutliner.builder.rebuild();
            return true;

        default:
            return false;
        }
    }
}

var historyDNDObserver = {
    onDragStart: function (aEvent, aXferData, aDragAction)
    {
        var currentIndex = gHistoryOutliner.currentIndex;
        if (isContainer(gHistoryOutliner, currentIndex))
            return false;
        var url = gHistoryOutliner.outlinerBoxObject.view.getCellText(currentIndex, "URL");
        var title = gHistoryOutliner.outlinerBoxObject.view.getCellText(currentIndex, "Name");

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
  var currentIndex = gHistoryOutliner.currentIndex;
  if (currentIndex == -1) return false;
  var container = isContainer(gHistoryOutliner, currentIndex);
  return (event.button == 0 &&
          event.originalTarget.localName == 'outlinerchildren' &&
          !container && gHistoryStatus);
}

function collapseExpand()
{
    var currentIndex = gHistoryOutliner.currentIndex;
    gHistoryOutliner.outlinerBoxObject.view.toggleOpenState(currentIndex);
}

function OpenURL(aInNewWindow)
{
    var currentIndex = gHistoryOutliner.currentIndex;     
    var builder = gHistoryOutliner.builder.QueryInterface(Components.interfaces.nsIXULOutlinerBuilder);
    var url = builder.getResourceAtIndex(currentIndex).Value;

    if (aInNewWindow) {
      var count = gHistoryOutliner.outlinerBoxObject.view.selection.count;
      if (count == 1) {
        if (isContainer(gHistoryOutliner, currentIndex))
          openDialog("chrome://communicator/content/history/history.xul", 
                     "", "chrome,all,dialog=no", url, "newWindow");
        else      
          openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url );
      }
      else {
        var min = new Object(); 
        var max = new Object();
        var rangeCount = gHistoryOutliner.outlinerBoxObject.view.selection.getRangeCount();
        for (var i = 0; i < rangeCount; ++i) {
          gHistoryOutliner.outlinerBoxObject.view.selection.getRangeAt(i, min, max);
          for (var k = max.value; k >= min.value; --k) {
            url = gHistoryOutliner.outlinerBoxObject.view.getCellText(k, "URL");
            window.openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no", url);
          }
        }
      }
    }        
    else
        openTopWin(url);
    return true;
}

function GroupBy(groupingType)
{
    var outliner = document.getElementById("historyOutliner");
    switch(groupingType) {
    case "none":
        outliner.setAttribute("ref", "NC:HistoryRoot");
        break;
    case "site":
        // xxx for now
        outliner.setAttribute("ref", "NC:HistoryByDate");
        break;
    case "day":
    default:
        outliner.setAttribute("ref", "NC:HistoryByDate");
        break;
    }
    gPrefService.setCharPref("browser.history.grouping", groupingType);
}

var groupObserver = {
  observe: function(aPrefBranch, aTopic, aPrefName) {
    try {
      GroupBy(aPrefBranch.QueryInterface(Components.interfaces.nsIPrefBranch).getCharPref(aPrefName));
    }
    catch(ex) {
    }
  }
}

function historyAddBookmarks()
{
  var count = gHistoryOutliner.outlinerBoxObject.view.selection.count;
  var url;
  var title;
  if (count == 1) {
    var currentIndex = gHistoryOutliner.currentIndex;
    url = gHistoryOutliner.outlinerBoxObject.view.getCellText(currentIndex, "URL");
    title = gHistoryOutliner.outlinerBoxObject.view.getCellText(currentIndex, "Name");
    BookmarksUtils.addBookmark(url, title, undefined, true);
  }
  else if (count > 1) {
    var min = new Object(); 
    var max = new Object();
    var rangeCount = gHistoryOutliner.outlinerBoxObject.view.selection.getRangeCount();
    for (var i = 0; i < rangeCount; ++i) {
      gHistoryOutliner.outlinerBoxObject.view.selection.getRangeAt(i, min, max);
      for (var k = max.value; k >= min.value; --k) {
        url = gHistoryOutliner.outlinerBoxObject.view.getCellText(k, "URL");
        title = gHistoryOutliner.outlinerBoxObject.view.getCellText(k, "Name");
        BookmarksUtils.addBookmark(url, title, undefined, false);
      }
    }
  }
}


function updateItems()
{
  var count = gHistoryOutliner.outlinerBoxObject.view.selection.count;
  var openItem = document.getElementById("miOpen");
  var bookmarkItem = document.getElementById("miAddBookmark");
  var copyLocationItem = document.getElementById("miCopyLinkLocation");
  var sep1 = document.getElementById("pre-bookmarks-separator");
  var openItemInNewWindow = document.getElementById("miOpenInNewWindow");
  var collapseExpandItem = document.getElementById("miCollapseExpand");
  if (count > 1) {
    var hasContainer = false;
    if (gHistoryGrouping == "day") {
      var min = new Object(); 
      var max = new Object();
      var rangeCount = gHistoryOutliner.outlinerBoxObject.view.selection.getRangeCount();
      for (var i = 0; i < rangeCount; ++i) {
        gHistoryOutliner.outlinerBoxObject.view.selection.getRangeAt(i, min, max);
        for (var k = max.value; k >= min.value; --k) {
          if (isContainer(gHistoryOutliner, k)) {
            hasContainer = true;
            break;
          }
        }
      }
    }
    if (hasContainer) {
      bookmarkItem.setAttribute("hidden", "true");
      copyLocationItem.setAttribute("hidden", "true");
      sep1.setAttribute("hidden", "true");
      document.getElementById("post-bookmarks-separator").setAttribute("hidden", "true");
      openItem.setAttribute("hidden", "true");
      openItemInNewWindow.setAttribute("hidden", "true");
      collapseExpandItem.setAttribute("hidden", "true");
    }
    else {
      bookmarkItem.removeAttribute("hidden");
      copyLocationItem.removeAttribute("hidden");
      sep1.removeAttribute("hidden");
      bookmarkItem.setAttribute("label", document.getElementById('multipleBookmarks').getAttribute("label"));
      openItem.setAttribute("hidden", "true");
      openItem.removeAttribute("default");
      openItemInNewWindow.setAttribute("default", "true");
    }
  }
  else {
    bookmarkItem.setAttribute("label", document.getElementById('oneBookmark').getAttribute("label"));
    var currentIndex = gHistoryOutliner.currentIndex;
    if (isContainer(gHistoryOutliner, currentIndex)) {
        openItem.setAttribute("hidden", "true");
        openItem.removeAttribute("default");
        collapseExpandItem.removeAttribute("hidden");
        collapseExpandItem.setAttribute("default", "true");
        bookmarkItem.setAttribute("hidden", "true");
        copyLocationItem.setAttribute("hidden", "true");
        sep1.setAttribute("hidden", "true");
        if (isContainerOpen(gHistoryOutliner, currentIndex))
          collapseExpandItem.setAttribute("label", gHistoryBundle.getString("collapseLabel"));
        else
          collapseExpandItem.setAttribute("label", gHistoryBundle.getString("expandLabel"));
        return true;
    }
    collapseExpandItem.setAttribute("hidden", "true");
    bookmarkItem.removeAttribute("hidden");
    copyLocationItem.removeAttribute("hidden");
    sep1.removeAttribute("hidden");
    if (!gWindowManager) {
      gWindowManager = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService();
      gWindowManager = gWindowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
    }
    var topWindowOfType = gWindowManager.getMostRecentWindow("navigator:browser");
    if (!topWindowOfType) {
        openItem.setAttribute("hidden", "true");
        openItem.removeAttribute("default");
        openItemInNewWindow.setAttribute("default", "true");
    }
    else {
      openItem.removeAttribute("hidden");
      if (!openItem.getAttribute("default"))
        openItem.setAttribute("default", "true");
      openItemInNewWindow.removeAttribute("default");
    }
  }
  return true;
}



 