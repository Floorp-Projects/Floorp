/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

package netscape.test.plugin.composer;

import netscape.plugin.composer.*;
import java.net.URL;

/** Sample Plugin that shows how to use Document.redirectDocumentOpen.
 *
 * I wrote this for my own use. I have a public
 * web site that is updated via ftp rather than http.
 * This means that the browsing URL is different than the
 * editing URL. For example, my main page has browsing URL
 *
 *     http://www.palevich.com/index.html
 *
 * and an editing URL of:
 *
 *     ftp://palevich@shell3.ba.best.com/users/u1/palevich/public_html/index.html
 *
 * The plug-in works by changing request to edit pages from my web site
 * into requests to edit pages from my ftp site.
 *
 * The plug-in is registered as an event handler. It handles the "edit" event.
 *
 * Note: The URLs given in this example may not work in the future,
 * since I may change ISPs, or my current ISP may change its machine
 * names.
 *
 * In order to be useful, you would substitute your own URLs.
 * (Since you don't have my password, you won't be able to save
 * your changes to my web pages, anyway.)
 *
 */

public class RedirectTest extends Plugin {

    static public void main(String[] args) {
        Test.perform(args, new RedirectTest());
    }

    // Since this is an event handler, we don't need to override the
    // UI methods GetName, GetCategory, or GetHint.

    public boolean perform(Document document){
        String event = document.getEventName();
        URL url = document.getDocumentURL();
        System.err.println("RedirectTest: event " + event + " url:" + url);
        // Only do something if this is an edit event.
        // and we have a non-empty document URL.

        if ( event != null && "edit".equals(event) && url != null ){
            // Does the URL start with the old prefix?
            String urlAsString = url.toString();
            if ( urlAsString.startsWith(oldPrefix) ){

                // Create a new URL string that has the new prefix and
                // the remainder of the old URL.

                String newURL = newPrefix + url.toString().substring(oldPrefix.length());

                // My ISP's http: server will automatically
                // send the index.html file if the URL ends in a "/".
                // However, the Navigator FTP client will display a directory for
                // an FTP URL that ends in a "/". So we explicitly
                // check for this case and append and index.html here.

                if ( newURL.endsWith("/") ){
                    newURL = newURL + "index.html";
                }
                System.err.println("Redirecting open to " + newURL);
                document.redirectDocumentOpen(newURL);
            }
        }
        return true;
    }

    // The prefix of the browsable version of the web pages

    private String oldPrefix = "http://www.palevich.com/";

    // The prefix of the editable version of the web pages

    private String newPrefix = "ftp://palevich@shell3.ba.best.com/users/u1/palevich/public_html/";
}
