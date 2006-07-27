/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Alec Flett <alecf@netscape.com>
 */

// The history window uses JavaScript in bookmarks.js too.

function debug(msg)
{
    // Uncomment for noise
    //dump(msg+"\n");
}

var gHistoryTree;
var gLastHostname;
var gLastDomain;
var gGlobalHistory;

var gDeleteByHostname;
var gDeleteByDomain;
var gHistoryBundle;
var gHistoryStatus;

function HistoryInit() {

    gHistoryTree =      document.getElementById("historyTree");
    gDeleteByHostname = document.getElementById("menu_deleteByHostname");
    gDeleteByDomain =   document.getElementById("menu_deleteByDomain");
    gHistoryBundle =    document.getElementById("historyBundle");
    gHistoryStatus =    document.getElementById("statusbar-display");

    var treeController = new nsTreeController(gHistoryTree);
    var historyController = new nsHistoryController;
    gHistoryTree.controllers.appendController(historyController);

    gGlobalHistory = Components.classes["@mozilla.org/browser/global-history;1"].getService(Components.interfaces.nsIBrowserHistory);

    if ("arguments" in window && window.arguments[0] && window.arguments.length >= 1) {
      // We have been supplied a resource URI to root the tree on
      var uri = window.arguments[0];
      setRoot(uri);
      if (uri.substring(0,5) == "find:") {
        // Update the windowtype so that future searches are directed 
        // there and the window is not re-used for bookmarks. 
        var windowNode = document.getElementById("history-window");
        windowNode.setAttribute("windowtype", "history:searchresults");
      }
    }

    var children = document.getElementById('treechildren-bookmarks');
    if (children.firstChild)
        gHistoryTree.selectItem(children.firstChild);
    gHistoryTree.focus();

    // do a sort
    RefreshSort();
}

function updateHistoryCommands()
{
    goUpdateCommand("cmd_deleteByHostname");
    goUpdateCommand("cmd_deleteByDomain");
}

function historyOnSelect(event)
{
    // every time selection changes, save the last hostname
    gLastHostname = "";
    gLastDomain = "";
    var match;
    var selection = gHistoryTree.selectedItems;
    if (selection && selection.length > 0) {
        var url = selection[0].id;
        // matches scheme://(hostname)...
        match = url.match(/.*:\/\/([^\/:]*)/);

        if (match && match.length>1)
            gLastHostname = match[1];
        
        if (gHistoryStatus)
            gHistoryStatus.label = url;
    } else {
        if (gHistoryStatus)
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
            text =
                gHistoryBundle.stringBundle.formatStringFromName(stringId,
                                                                 [ gLastHostname ], 1);
            gDeleteByHostname.setAttribute("label", text);

            break;
        case "cmd_deleteByDomain":
            if (gLastDomain) {
                stringId = "deleteDomain";
                enabled = true;
            } else {
                stringId = "deleteDomainNoSelection";
            }
            text = gHistoryBundle.stringBundle.formatStringFromName(stringId,
                                                                    [ gLastDomain ], 1);
            gDeleteByDomain.setAttribute("label", text);
        }
        return enabled;
    },

    doCommand: function(command)
    {
        switch(command) {
        case "cmd_deleteByHostname":
            gGlobalHistory.removePagesFromHost(gLastHostname, false)
            return true;

        case "cmd_deleteByDomain":
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
    var title = aEvent.target.getAttribute("label");
    var uri = aEvent.target.parentNode.parentNode.id;
    if (aEvent.target.localName != "treecell")     // make sure we have something to drag
      return null;

    var htmlString = "<A HREF='" + uri + "'>" + title + "</A>";
    aXferData.data = new TransferData();
    aXferData.data.addDataForFlavour("text/unicode", uri);
    aXferData.data.addDataForFlavour("text/html", htmlString);
    aXferData.data.addDataForFlavour("text/x-moz-url", url + "\n" + title);
  }
};

function OpenURL(event, node, root)
{
    if ((event.button != 0) || (event.detail != 2)
        || (node.nodeName != "treeitem"))
        return false;

    if (node.getAttribute("container") == "true")
        return false;

    var url = node.id;
    if (event.ctrlKey)
        // if metaKey is down, open in a new browser window
        window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", id );
    else
        openTopWin(url);
    return true;
}

/**
 * Root the tree on a given URI (used for displaying search results)
 */
function setRoot(root)
{
  var windowNode = document.getElementById("history-window");
  windowNode.setAttribute("title", gHistoryBundle.getString("search_results_title"));
  gHistoryTree.setAttribute("ref", root);
}

