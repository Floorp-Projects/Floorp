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
 */

// The history window uses JavaScript in bookmarks.js too.

function debug(msg)
{
    // Uncomment for noise
    //dump(msg+"\n");
}

function HistoryInit() {
    var tree = document.getElementById("bookmarksTree");
    debug("adding controller to tree");
    tree.controllers.appendController(HistoryController);
    var children = document.getElementById('treechildren-bookmarks');
    tree.selectItem(children.firstChild);
    tree.focus();
}

var HistoryController = {
    supportsCommand: function(command)
    {
        debug("history in supports with " + command);
        switch(command)
        {
            case "cmd_copy":
            case "cmd_delete":
            case "cmd_selectAll":
                return true;
            default:
                return false;
        }
    },
    isCommandEnabled: function(command)
    {
        debug("history in enabled with " + command);
        switch(command)
        {
            case "cmd_copy":
            case "cmd_delete":
            case "cmd_selectAll":
                return true;
            default:
                return false;
        }
    },
    doCommand: function(command)
    {
        debug("history in do with " + command);
        switch(command)
        {
            case "cmd_copy":
                doCopy();
                break;
            case "cmd_delete":
                doDelete();
                break;
            case "cmd_selectAll":
                doSelectAll();
                break;
        }
    },
};

var historyDNDObserver = {
  onDragStart: function (aEvent)
  {
    var title = aEvent.target.getAttribute("value");
    var uri = aEvent.target.parentNode.parentNode.id;
    dump("*** title = " + title + "; uri = " + uri + "\n");
    if ( title == "" && uri == "" )     // make sure we have something to drag
      return null;
      
    var htmlString = "<A HREF='" + uri + "'>" + title + "</A>";
    var flavourList = { };
    flavourList["text/unicode"] = { width: 2, data: uri };
    flavourList["text/html"] = { width: 2, data: htmlString };
    flavourList["text/x-moz-url"] = { width: 2, data: uri + " " + "[ TEMP TITLE ]" };
    return flavourList;
  },

};