/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *  Peter Annema <disttsc@bart.nl>
 */

var gJVMMgr = Components.classes['@mozilla.org/oji/jvm-mgr;1']
                        .getService(Components.interfaces.nsIJVMManager)

function toNavigator()
{
    CycleWindow('navigator:browser', getBrowserURL());
}

// Set up a lame hack to avoid opening two bookmarks.
// Could otherwise happen with two Ctrl-B's in a row.
var gDisableHistory = false;
function enableHistory() {
  gDisableHistory = false;
}

function toHistory()
{
  // Use a single sidebar history dialog

  var cwindowManager = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService();
  var iwindowManager = Components.interfaces.nsIWindowMediator;
  var windowManager  = cwindowManager.QueryInterface(iwindowManager);

  var historyWindow = windowManager.getMostRecentWindow('history:manager');

  if (historyWindow) {
    //debug("Reuse existing history window");
    historyWindow.focus();
  } else {
    //debug("Open a new history dialog");

    if (true == gDisableHistory) {
      //debug("Recently opened one. Wait a little bit.");
      return;
    }
    gDisableHistory = true;

    window.open( "chrome://communicator/content/history/history.xul", "_blank", "chrome,menubar,resizable,scrollbars" );
    setTimeout(enableHistory, 2000);
  }

}

function toJavaScriptConsole()
{
    toOpenWindowByType("global:console", "chrome://global/content/console.xul");
}

function javaItemEnabling()
{
    var enabled = gJVMMgr.JavaEnabled();
    var element = document.getElementById("java");
    if (enabled)
      element.removeAttribute("disabled");
    else
      element.setAttribute("disabled", "true");
}
            
function toJavaConsole()
{
    gJVMMgr.ShowJavaConsole();
}

function toOpenWindowByType( inType, uri )
{
	var windowManager = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService();

	var	windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);

	var topWindow = windowManagerInterface.getMostRecentWindow( inType );
	
	if ( topWindow )
		topWindow.focus();
	else
		window.open(uri, "_blank", "chrome,menubar,toolbar,resizable,scrollbars");
}


function OpenBrowserWindow()
{
  var charsetArg = new String();
  var handler = Components.classes['@mozilla.org/commandlinehandler/general-startup;1?type=browser'];
  handler = handler.getService();
  handler = handler.QueryInterface(Components.interfaces.nsICmdLineHandler);
  var startpage = handler.defaultArgs;
  var url = handler.chromeUrlForTask;
  var wintype = document.firstChild.getAttribute('windowtype');

  // if and only if the current window is a browser window and it has a document with a character
  // set, then extract the current charset menu setting from the current document and use it to
  // initialize the new browser window...
  if (window && (wintype == "navigator:browser") && window._content && window._content.document)
  {
    var DocCharset = window._content.document.characterSet;
    charsetArg = "charset="+DocCharset;

    //we should "inherit" the charset menu setting in a new window
    window.openDialog(url, "_blank", "chrome,all,dialog=no", startpage, charsetArg);
  }
  else // forget about the charset information.
  {
    window.openDialog(url, "_blank", "chrome,all,dialog=no", startpage);
  }
}

function newWindowOfType( aType )
{
  switch (aType) {
  case "navigator:browser":
    OpenBrowserWindow();
    break;
  case "composer:html":
    NewEditorWindow();
    break;
  default:
    break;
  }
}

function CycleWindow( aType, aChromeURL )
{
  var windowManager = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService();
  var windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);

  var topWindowOfType = windowManagerInterface.getMostRecentWindow( aType );
  var topWindow = windowManagerInterface.getMostRecentWindow( null );

  if ( topWindowOfType == null )
    newWindowOfType( aType );
  else if ( topWindowOfType != topWindow )
    topWindowOfType.focus();
  else {
    var enumerator = windowManagerInterface.getEnumerator( aType );
    var firstWindow = windowManagerInterface.convertISupportsToDOMWindow ( enumerator.getNext() );
    var iWindow = firstWindow; // ;-)
    while ( iWindow != topWindow && enumerator.hasMoreElements() )
      iWindow = windowManagerInterface.convertISupportsToDOMWindow ( enumerator.getNext() );
  
    var desiredWindow = firstWindow;
    if ( enumerator.hasMoreElements() )
      desiredWindow = windowManagerInterface.convertISupportsToDOMWindow ( enumerator.getNext() );
    if ( desiredWindow == topWindow ) // Only one window, open a new one 
      newWindowOfType( aType );
    else
      desiredWindow.focus();
  }
}

function toEditor()
{
	CycleWindow('composer:html', 'chrome://editor/content/editor.xul');
}

function ShowWindowFromResource( node )
{
	var windowManager = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService();
	var	windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
    
    var desiredWindow = null;
    var url = node.getAttribute('id');
	desiredWindow = windowManagerInterface.getWindowForResource( url );
	if ( desiredWindow )
	{
		desiredWindow.focus();
	}
}

function OpenTaskURL( inURL )
{
	
	window.open( inURL );
}

function ShowUpdateFromResource( node )
{
	var url = node.getAttribute('url');
        // hack until I get a new interface on xpiflash to do a 
        // look up on the name/url pair.
	OpenTaskURL( "http://www.mozilla.org/binaries.html");
}


