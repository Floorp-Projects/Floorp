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
        //dump("_Observe(" + subject + ", " + topic + ", " + data + ")\n");

        subject = subject.QueryInterface(Components.interfaces.nsIDOMWindow);
        //dump("subject = " + subject + "\n");
        //dump("ContentWindow = " + ContentWindow + "\n");

        // We can't use '==' until the DOM is converted to XPIDL.
        if (subject.Equals(ContentWindow)) {
            Handler.URL = data;
        }
    }
}

function Init() {
    // Initialize the Related Links panel

    // Create a Related Links handler, and install it in the tree
    var Tree = document.getElementById("Tree");
    Tree.database.AddDataSource(Handler.QueryInterface(Components.interfaces.nsIRDFDataSource));

    Handler.URL = ContentWindow.location;

    // Install the observer so we'll be notified when new content is loaded.
    var ObserverService = Components.classes["component://netscape/observer-service"].getService();
    ObserverService = ObserverService.QueryInterface(Components.interfaces.nsIObserverService);
    dump("got observer service\n");

    ObserverService.AddObserver(Observer, "EndDocumentLoad");
    dump("added observer\n");
}


function Boot() {
    // Kludge to deal with no onload in XUL.
    if (document.getElementById("Tree")) {
        Init();
    }
    else {
        setTimeout("Boot();", 0);
    }
}

Boot();
