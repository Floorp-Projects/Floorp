/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *	Stephen Lamm             <slamm@netscape.com>
 *	Robert John Churchill    <rjc@netscape.com>
 */

/*
  Code for the Bookmarks Sidebar Panel
 */

// get handle to the BrowserAppCore in the content area.
var appCore = window._content.appCore;



function clicked(event, target)
{
	if ((event.button != 1) || (event.detail != 2) || (target.nodeName != "treeitem"))
		return(false);

    if (event.altKey)
    {
        // if altKey is down then just open the Bookmark Properties dialog
        BookmarkProperties();
        return(true);
    }

	if (target.getAttribute("container") == "true")
		return(false);

	OpenBookmarkURL(event, target, document.getElementById('bookmarksTree').database);
	return(true);
}



function OpenBookmarkURL(event, node, datasources)
{
	if (node.getAttribute("container") == "true")
	{
		return(false);
	}

	var url = node.getAttribute("id");
	try
	{
		// add support for IE favorites under Win32, and NetPositive URLs under BeOS
		var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
		if (rdf) rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
		if (rdf && datasources) {
			var src = rdf.GetResource(url, true);
			var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
			var target = datasources.GetTarget(src, prop, true);
			if (target)	target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
			if (target)	target = target.Value;
			if (target)	url = target;
		}
	}
	catch(ex)
	{
	}

	// Ignore "NC:" urls.
	if (url.substring(0, 3) == "NC:")
	{
		return(false);
	}
  
	if (!window._content)
	{
        const WM_PROGID = "component://netscape/rdf/datasource?name=window-mediator";
        var wm = nsJSComponentManager.getService(WM_PROGID, "nsIWindowMediator");
        if (wm)
        {
          navWindow = wm.getMostRecentWindow("navigator:browser");
          if (navWindow)
          {
            navWindow.appCore.loadUrl(url);
          }
        }
        else
        {
      		window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url );
        }
	}
	else if (event.metaKey)
	{
	    // if metaKey is down, open in a new browser window
  		window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url ); 
	}
	else
	{
		if(appCore)
		{
		    // support session history (if appCore is available)
            appCore.loadUrl(url);
		}
		else
		{
		    // fallback case (if appCore isn't available)
            window._content.location = url;
		}
  	}
}
