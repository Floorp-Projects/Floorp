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
