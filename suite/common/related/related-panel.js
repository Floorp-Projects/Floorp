/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*

  Code for the Related Links Sidebar Panel

 */

// The content window that we're supposed to be observing. This is
// god-awful fragile. The logic goes something like this. Our parent
// is the sidebar, whose parent is the content frame, whose frame[1]
// is the content area.
var ContentWindow = window.parent.parent.frames[1];

// The related links handler
var Handler = Components.classes["component://netscape/related-links-handler"].createInstance();
Handler = Handler.QueryInterface(Components.interfaces.nsIRelatedLinksHandler);

// Our observer object
var Observer = {
    Observe: function(subject, topic, data) {
        // dump("Observer.Observe(" + subject + ", " + topic + ", " + data + ")\n");
        if (subject != ContentWindow)
            return;

        // Okay, it's a hit. Before we go out and fetch RL data, make sure that
        // the RelatedLinks folder is open.
        var root = document.getElementById('NC:RelatedLinks');

        if (root.getAttribute('open') == 'true') {
            Handler.URL = data;
        }
    }
}

function Init() {
    // Initialize the Related Links panel

    // Create a Related Links handler, and install it in the tree
    var Tree = document.getElementById("Tree");
    Tree.database.AddDataSource(Handler.QueryInterface(Components.interfaces.nsIRDFDataSource));

    // Install the observer so we'll be notified when new content is loaded.
    var ObserverService = Components.classes["component://netscape/observer-service"].getService();
    ObserverService = ObserverService.QueryInterface(Components.interfaces.nsIObserverService);
    dump("got observer service\n");

    ObserverService.AddObserver(Observer, "EndDocumentLoad");
    dump("added observer\n");
}


function OnDblClick(treeitem)
{
    // Deal with a double-click

    // First, see if they're opening the related links node. If so,
    // we'll need to go out and fetch related links _now_.
    if (treeitem.getAttribute('id') == 'NC:RelatedLinks' &&
        treeitem.getAttribute('open') != 'true') {
        Handler.URL = ContentWindow.location;
        return;
    }

    // Next, check to see if it's a container. If so, then just let
    // the tree do its open and close stuff.
    if (treeitem.getAttribute('container') == 'true') {
        return;
    }

    if (treeitem.getAttribute('type') == 'http://home.netscape.com/NC-rdf#BookmarkSeparator') {
        return;
    }

    // Okay, it's not a container. See if it has a URL, and if so, open it.
    var id = treeitem.getAttribute('id');
    if (id) {
        ContentWindow.location = id;
    }
}


