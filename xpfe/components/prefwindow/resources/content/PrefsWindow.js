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
    	prefwindow = Components.classes['component://netscape/prefwindow'].getService(Components.interfaces.nsIPrefWindow);
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

function OnLoadPref()
{
	doSetOKCancel(handleOKButton, handleCancelButton);
}

function handleOKButton()
{
	if ( prefwindow )
	{
		prefwindow.savePrefs();
		return false;
	}
	return true;
}

function handleCancelButton()
{
	if ( prefwindow )
	{
		prefwindow.cancelPrefs();
		return false;
	}
	return true;
}

//function to open and close the manual prefs
function openit()  {

   var manualProxy = document.getElementById('manual-proxy');
   var formStyle = manualProxy.getAttribute('style');
   if (!(formStyle == "display: block; "))	
    {
	manualProxy.setAttribute("style", "display: block;");
	}
   else  {
    manualProxy.setAttribute("style", "display: none;");
	}   
   }

 function choose() {
    var toolkit;
    var browser;
         /* Use existing browser "open" logic. */
         browser.openWindow();
         toolkit.CloseWindow( window );
 }
 
 function SetPrefToCurrentPage(prefID)
 {
 	dump("SetPrefToCurrentPage("+ prefID +") \n ");
	if ((prefID == null) || (prefID == "")) return;
	
 	var windowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();
	var	windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);

    
	var topWindowOfType = windowManagerInterface.getMostRecentWindow( "navigator:browser" );
	if ( topWindowOfType )
	{
		var inputfield = document.getElementById(prefID);
		dump( "window found "+inputfield+"\n");
		if ( inputfield )
		{
			dump("setting home page to "+topWindowOfType._content.location.href+"\n");
			inputfield.value = topWindowOfType._content.location.href;
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
const nsIFilePicker = Components.interfaces.nsIFilePicker;
function PrefNavSelectFile(prefID) {

  try {
    var fp = Components.classes["component://mozilla/filepicker"].createInstance(nsIFilePicker);
    /* XXX    no title here */
    fp.init(window, "", nsIFilePicker.modeOpen);
    fp.appendFilters(nsIFilePicker.filterAll);
    fp.show();
    var field = document.getElementById(prefID);
    field.setAttribute("value", fp.file.unicodePath);
  } catch(ex) { }
}

function PrefCacheSelectFolder() {
    // Get filespecwithui component.     
  try {
    var fp = Components.classes["component://mozilla/filepicker"].createInstance(nsIFilePicker);
    /* XXX    no title here */
    fp.init(window, "", nsIFilePicker.modeGetFolder);
    fp.appendFilters(nsIFilePicker.filterAll);
    fp.show();
    var field = document.getElementById("pref:string:browser.cache.directory");
    field.setAttribute("value", fp.file.unicodePath);
  } catch(ex) { }
}
//This function is unused, and therefore the files are no longer included by MANIFESTs
function OpenProxyManualDialog() {
	var dialog = window.openDialog("chrome://communicator/content/pref/pref-proxy-manual.xul", "", "chrome", {});
}



//dialog function that is not being called yet
function onOK()
{
	//dump("\nDoing " + windowName + " startup...\n");
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
	if (prefwindow)
	{
		prefwindow.changePanel("chrome://communicator/content/prefs/pref-proxies.xul");
	}
	return true
}

function startUpProxy()
{
}

function manualSelected()  {
	manualSelect = document.getElementById('pref:1:int:network.proxy.type');
	manualProxy = document.getElementById('manual-proxy');
	dump("\n" + manualSelect);
	dump("\n" + manualSelect.checked);
	if (manualSelect.checked)
	  manualProxy.setAttribute("style", "display: block");

}

function setColorWell(menu,otherId,setbackground)
{
    // Find the colorWell and colorPicker in the hierarchy.
    var colorWell = menu.firstChild.nextSibling;
    var colorPicker = menu.firstChild.nextSibling.nextSibling.firstChild;

    // Extract color from colorPicker and assign to colorWell.
    var color = colorPicker.getAttribute('color');
    setColorFromPicker(colorWell,color,otherId,setbackground);
}

function setColorFromPicker(colorWell,color,otherId,setbackground)
{
    if (colorWell) {
      colorWell.style.backgroundColor = color;
    }
    if (otherId) {
	    otherElement=document.getElementById(otherId);
	    if (setbackground) {
		    var basestyle = otherElement.getAttribute('basestyle');
		    var newstyle = basestyle + "background-color:" + color + ";";
		    otherElement.setAttribute('style',newstyle);
	    }
	    else {
		    otherElement.style.color = color;
	    }
    }
}

