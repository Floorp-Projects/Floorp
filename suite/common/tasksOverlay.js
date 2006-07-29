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

  var cwindowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();
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

function toJavaConsole()
{
	try{
		var cid =
			Components.classes['component://netscape/oji/jvm-mgr'];
		var iid = Components.interfaces.nsIJVMManager;
		var jvmMgr = cid.getService(iid);
		jvmMgr.ShowJavaConsole();
	} catch(e) {
		
	}
}

function toOpenWindowByType( inType, uri )
{
	var windowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();

	var	windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);

	var topWindow = windowManagerInterface.getMostRecentWindow( inType );
	
	if ( topWindow )
		topWindow.focus();
	else
		window.open(uri, "_blank", "chrome,menubar,toolbar,resizable,scrollbars");
}


function OpenBrowserWindow()
{
  dump("In OpenBrowserWindw()...\n");
  var charsetArg = new String();
  var handler = Components.classes['component://netscape/commandlinehandler/general-startup-browser'];
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
    dump("*** Current document charset: " + DocCharset + "\n");

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
    dump( "Unsupported type of window: " + aType + "\n");
    break;
  }
}

function CycleWindow( aType, aChromeURL )
{
  var windowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();
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
	var windowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();
	dump("got window Manager \n");
	var	windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
    dump("got interface \n");
    
    var desiredWindow = null;
    var url = node.getAttribute('id');
    dump( url +" finding \n" );
	desiredWindow = windowManagerInterface.getWindowForResource( url );
	dump( "got window \n");
	if ( desiredWindow )
	{
		dump("focusing \n");
		desiredWindow.focus();
	}
}

function OpenTaskURL( inURL )
{
	dump("loading "+inURL+"\n");
	
	window.open( inURL );
}

function ShowUpdateFromResource( node )
{
	var url = node.getAttribute('url');
    dump( url +" finding \n" );
        // hack until I get a new interface on xpiflash to do a 
        // look up on the name/url pair.
	OpenTaskURL( "http://www.mozilla.org/binaries.html");
}
/** 
 * WALLET submenu
 */
function HideWallet() {
  var element;
  element = document.getElementById("wallet");
  element.setAttribute("style","display: none;" );
  element.setAttribute("disabled","true" );
}

function HideImage() {
  var element;
  element = document.getElementById("image");
  element.setAttribute("style","display: none;" );
  element.setAttribute("disabled","true" );
}

function HideEncryptOrObscure() {
  var elementOn, elementOff;
  if (this.pref.GetBoolPref("wallet.crypto")) {
    elementOn = document.getElementById("obscure");
    elementOff = document.getElementById("encrypt");
  } else {
    elementOn = document.getElementById("encrypt");
    elementOff = document.getElementById("obscure");
  }
  elementOn.setAttribute("disabled","false");
  elementOff.setAttribute("disabled","true");
}

function CheckForWalletAndImage()
{
  // remove either encrypt or obscure depending on pref setting
  HideEncryptOrObscure();

  // remove wallet functions if not in browser
  try {
    if (!appCore) {
      HideWallet();
    }
  } catch(e) {
    HideWallet();
  }

  // remove wallet functions (unless overruled by the "wallet.enabled" pref)
  try {
    if (!this.pref.GetBoolPref("wallet.enabled")) {
      HideWallet();
    }
  } catch(e) {
    dump("wallet.enabled pref is missing from all.js\n");
  }

  // remove image functions (unless overruled by the "imageblocker.enabled" pref)
  try {
    if (!this.pref.GetBoolPref("imageblocker.enabled")) {
      HideImage();
    }
  } catch(e) {
    dump("imageblocker.enabled pref is missing from all.js\n");
  }

}

// perform a wallet action
function WalletAction( action ) 
{
  var strings = document.getElementById("personalManagers");
  if (action == "password" || action == "expire" || action == "clear") {
    wallet = Components.classes['component://netscape/wallet/wallet-service'];
    wallet = wallet.getService();
    wallet = wallet.QueryInterface(Components.interfaces.nsIWalletService);

    if (action == "password") {
      if (!wallet.WALLET_ChangePassword()) {
        window.alert(strings.getAttribute("PasswordNotChanged"));
      }
    } else if (action == "expire") {
      if (wallet.WALLET_ExpirePassword()) {
        window.alert(strings.getAttribute("PasswordExpired"));
      } else {
        window.alert(strings.getAttribute("PasswordNotExpired"));
      }
    } else if (action == "clear") {
      if (window.confirm(strings.getAttribute("AllDataWillBeCleared"))) {
        wallet.WALLET_DeleteAll();
      }
    }
    return;
  }

  if (action == "encrypt" || action == "obscure") {
    wallet = Components.classes['component://netscape/wallet/wallet-service'];
    wallet = wallet.getService();
    wallet = wallet.QueryInterface(Components.interfaces.nsIWalletService);
    wallet.WALLET_InitReencryptCallback(window._content);
    if (action == "encrypt") {
      this.pref.SetBoolPref("wallet.crypto", true);
    } else if (action == "obscure") {
      this.pref.SetBoolPref("wallet.crypto", false);
    }
    return;
  }

  if (action == "cookieAllow" || action == "cookieBlock" ||
      action == "imageAllow" || action == "imageBlock") {

    var cookieViewer =
      Components.classes["component://netscape/cookieviewer/cookieviewer-world"]
        .createInstance(Components.interfaces["nsICookieViewer"]);

    COOKIEPERMISSION = 0;
    IMAGEPERMISSION = 1;

    var element;
    if (action == "cookieAllow") {
      cookieViewer.AddPermission(window._content, true, COOKIEPERMISSION);
      element = document.getElementById("AllowCookies");
      alert(element.getAttribute("msg"));
    } else if (action == "cookieBlock") {
      cookieViewer.AddPermission(window._content, false, COOKIEPERMISSION);
      element = document.getElementById("BlockCookies");
      alert(element.getAttribute("msg"));
    } else if (action == "imageAllow") {
      cookieViewer.AddPermission(window._content, true, IMAGEPERMISSION);
      element = document.getElementById("AllowImages");
      alert(element.getAttribute("msg"));
    } else if (action == "imageBlock") {
      cookieViewer.AddPermission(window._content, false, IMAGEPERMISSION);
      element = document.getElementById("BlockImages");
      alert(element.getAttribute("msg"));
    }
    return;
  }

  if( appCore ) {
    switch( action ) {
      case "safefill":
        appCore.walletPreview(window, window._content);
        break;
//    case "password":
//      appCore.walletChangePassword();
//      break;
      case "quickfill": 
        appCore.walletQuickFillin(window._content);
        break;
      case "capture":
      default:
        status = appCore.walletRequestToCapture(window._content);
        break;
    }
  }
}  

// display a Wallet Dialog
function WalletDialog( which )
{
  switch( which ) {
    case "signon":
      window.openDialog("chrome://communicator/content/wallet/SignonViewer.xul","SSViewer","modal=yes,chrome,resizable=no","S"); 
      break;
    case "cookie":
      window.openDialog("chrome://communicator/content/wallet/CookieViewer.xul","CookieViewer","modal=yes,chrome,resizable=no",0); 
      break;
    case "image":
      window.openDialog("chrome://communicator/content/wallet/CookieViewer.xul","CookieViewer","modal=yes,chrome,resizable=no",2); 
      break;
    case "samples":
      window._content.location.href = 'chrome://communicator/locale/wallet/index.html';
      break;
    case "interview":
      window._content.location.href = 'chrome://communicator/locale/wallet/interview.html';
      break;
    case "walletsites":
      window.openDialog("chrome://communicator/content/wallet/SignonViewer.xul","SSViewer","modal=yes,chrome,resizable=no","W"); 
      break;
    case "tutorial":
      window._content.location.href = 'chrome://communicator/locale/wallet/privacy.html';
      break;
    case "wallet":
    default:
      window.openDialog("chrome://communicator/content/wallet/WalletEditor.xul","walletEditor","modal=yes,chrome,resizable=no"); 
      break;
  }
}


