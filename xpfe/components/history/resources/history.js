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

function HistoryInit() {
    var tree = document.getElementById("bookmarksTree");
    debug("adding controller to tree\n");
    var historyController = new nsTreeController(tree);

    var children = document.getElementById('treechildren-bookmarks');
    if (children.firstChild)
        tree.selectItem(children.firstChild);
    tree.focus();
}

var historyDNDObserver = {
  onDragStart: function (aEvent)
  {
    var title = aEvent.target.getAttribute("value");
    var uri = aEvent.target.parentNode.parentNode.id;
    dump("*** title = " + title + "; uri = " + uri + "\n");
    if ( aEvent.target.localName != "treecell" )     // make sure we have something to drag
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
}
