/* 
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 */

var prefwindow = null;

function StartUp(windowName)
{
	dump("\nDoing " + windowName + " startup...\n");
	dump("Looking up prefwindow object...\n");
	if (prefwindow == null)
	{
		dump("Creating prefwindow object...\n");
    	prefwindow = Components.classes['component://netscape/prefwindow'].createInstance(Components.interfaces.nsIPrefWindow);
	}
	else
	{
		dump("prefwindow has already been created! Hurrah!\n");
	}
	if (prefwindow != null && windowName != "Top" && windowName != "Bottom")
	{
		prefwindow.panelLoaded(window);
	}
}


 function choose() {
    var toolkit;
    var browser;
         /* Use existing browser "open" logic. */
         browser.openWindow();
         toolkit.CloseWindow( window );
 }
 
 function SetHomePageToCurrentPage()
 {
 	dump("SetHomePageToCurrentPage() \n ");
 	var windowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();
	var	windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);

    
	var topWindowOfType = windowManagerInterface.GetMostRecentWindow( "navigator:browser" );
	if ( topWindowOfType )
	{
		var inputfield = document.getElementById("pref:string:browser.startup.homepage");
		dump( "window found "+inputfield+"\n");
		if ( inputfield )
		{
			dump("setting home page to "+topWindowOfType.content.location.href+"\n");
			inputfield.value = topWindowOfType.content.location.href;
		}
	}
	else
	{
		dump(" No browser window. Should be disabling this button \n");
	}
 }

function createInstance( progid, iidName ) {
  var iid = eval( "Components.interfaces." + iidName );
  return Components.classes[ progid ].createInstance( iid );
}

function PrefNavSelectFile() {
    // Get filespecwithui component.            
    var fileSpec = createInstance( "component://netscape/filespecwithui", "nsIFileSpecWithUI" );
    try {
        var url = fileSpec.chooseFile( "" );
        var field = document.getElementById( "pref:string:browser.startup.homepage" );
        field.setAttribute( "value", url );
    }
    catch( exception ) {
        // Just a cancel, probably.
    }
}

function PrefCacheSelectFolder() {
    // Get filespecwithui component.            
    var fileSpec = createInstance( "component://netscape/filespecwithui", "nsIFileSpecWithUI" );
    try {
        var url = fileSpec.chooseDirectory( "" );
        var field = document.getElementById( "pref:string:browser.cache.directory" );
        field.setAttribute( "value", url );
    }
    catch( exception ) {
        // Just a cancel, probably.
    }
}

function OpenProxyManualDialog() {
	var dialog = window.openDialog("chrome://pref/content/pref-proxy-manual.xul", "", "chrome", {});
}
