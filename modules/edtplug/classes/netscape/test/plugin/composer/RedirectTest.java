/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
