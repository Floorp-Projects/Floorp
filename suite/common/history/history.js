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

function HistoryInit() {

    gHistoryTree =      document.getElementById("historyTree");
    gDeleteByHostname = document.getElementById("cmd_deleteByHostname");
    gDeleteByDomain =   document.getElementById("cmd_deleteByDomain");
    gHistoryBundle =    document.getElementById("historyBundle");
    
    var treeController = new nsTreeController(gHistoryTree);
    var historyController = new nsHistoryController;
    gHistoryTree.controllers.appendController(historyController);

    gGlobalHistory = Components.classes["@mozilla.org/browser/global-history;1"].getService(Components.interfaces.nsIGlobalHistory);
    
    var children = document.getElementById('treechildren-bookmarks');
    if (children.firstChild)
        gHistoryTree.selectItem(children.firstChild);
    gHistoryTree.focus();

    dump("Updating sort..\n");
    // do a sort
    RefreshSort();
}

function updateHistoryCommands()
{
    dump("Updating history commands..\n");
    goUpdateCommand("cmd_deleteByHostname");
    goUpdateCommand("cmd_deleteByDomain");
}

function historyOnSelect(event)
{
    dump("History selection has changed..\n");
    // every time selection changes, save the last hostname
    gLastHostname = "";
    gLastDomain = "";
    var selection = gHistoryTree.selectedItems;
    if (selection && selection.length > 0) {
        var url = selection[0].id;
        // matches scheme://(hostname)...
        var match = url.match(/.*:\/\/([^\/:]*)/);
        
        if (match && match.length>1)
            gLastHostname = match[1];
    }

    if (gLastHostname) {
        // matches the last foo.bar in foo.bar or baz.foo.bar
        var match = gLastHostname.match(/([^.]+\.[^.]+$)/);
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
            dump("Updating cmd_deleteByHostname..\n");
            if (gLastHostname) {
                stringId = "deleteHost";
                enabled = true;
            } else {
                stringId = "deleteHostNoSelection";
            }
            text =
                gHistoryBundle.stringBundle.formatStringFromName(stringId,
                                                                 [ gLastHostname ], 1);
            dump("Setting value to " + text + "\n");
            gDeleteByHostname.setAttribute("value", text);
            
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
            gDeleteByDomain.setAttribute("value", text);
        }
        return enabled;
    },

    doCommand: function(command)
    {
        switch(command) {
        case "cmd_deleteByHostname":
            gGlobalHistory.RemovePagesFromHost(gLastHostname, false)
            return true;
            
        case "cmd_deleteByDomain":
            gGlobalHistory.RemovePagesFromHost(gLastDomain, true)
            return true;
            
        default:
            return false;
        }

    }

}

var historyDNDObserver = {
  onDragStart: function (aEvent)
  {
    var title = aEvent.target.getAttribute("value");
    var uri = aEvent.target.parentNode.parentNode.id;
    if (aEvent.target.localName != "treecell")     // make sure we have something to drag
      return null;
      
    var htmlString = "<A HREF='" + uri + "'>" + title + "</A>";
    var flavourList = { };
    flavourList["text/unicode"] = { width: 2, data: uri };
    flavourList["text/html"] = { width: 2, data: htmlString };
    flavourList["text/x-moz-url"] = { width: 2, data: uri + "\n" + title };
    return flavourList;
  }

};

function OpenURL(event, node, root)
{
    if ((event.button != 1) || (event.detail != 2)
        || (node.nodeName != "treeitem"))
        return false;

    if (node.getAttribute("container") == "true")
        return false;

    var url = node.id;
    
    window.openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no", url);
    return true;
}
