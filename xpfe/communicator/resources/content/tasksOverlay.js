
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
		window.open(uri, "_blank", "chrome,menubar,toolbar,resizable");
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

  // if and only if the current window is a browser window and it has a document with a character
  // set, then extract the current charset menu setting from the current document and use it to
  // initialize the new browser window...
  if (window && (window.windowtype == "navigator:browser") && window.content && window.content.document)
  {
    var DocCharset = window.content.document.characterSet;
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


function CycleWindow( inType, inChromeURL )
{
	var windowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();
	dump("got window Manager \n");
	var	windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
    dump("got interface \n");
    
    var desiredWindow = null;
    
	var topWindowOfType = windowManagerInterface.getMostRecentWindow( inType );
	var topWindow = windowManagerInterface.getMostRecentWindow( null );
	dump( "got windows \n");
	
	dump( "topWindowOfType = " + topWindowOfType + "\n");
	if ( topWindowOfType == null )
	{
	  if ( inType == "navigator:browser" )
      OpenBrowserWindow();
    else if ( inType == "composer:html" ) /* open editor window */
      NewEditorWindow();
    else
    {
        /* what to do here? */
    }
    
		return;
	}
	
	if ( topWindowOfType != topWindow )
	{
		dump( "first not top so give focus \n");
		topWindowOfType.focus();
		return;
	}
	
	var enumerator = windowManagerInterface.getEnumerator( inType );
	firstWindow = windowManagerInterface.convertISupportsToDOMWindow ( enumerator.getNext() );
	if ( firstWindow == topWindowOfType )
	{
		dump( "top most window is first window \n");
		firstWindow = null;
	}
	else
	{
		dump("find topmost window \n");
		while ( enumerator.hasMoreElements() )
		{
			var nextWindow = windowManagerInterface.convertISupportsToDOMWindow ( enumerator.getNext() );
			if ( nextWindow == topWindowOfType )
				break;
		}	
	}
	desiredWindow = firstWindow;
	if ( enumerator.hasMoreElements() )
	{
		dump( "Give focus to next window in the list \n");
		desiredWindow = windowManagerInterface.convertISupportsToDOMWindow ( enumerator.getNext() );		
	}
	
	if ( desiredWindow )
	{
		desiredWindow.focus();
		dump("focusing window \n");
	}
	else
	{
		dump("open window \n");
	  if ( inType == "navigator:browser" )
      window.OpenBrowserWindow();
    else if ( inType == "composer:html" ) /* open editor window */
      NewEditorWindow();
    else
    {
        /* what to do here? */
    }
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

  // remove quickfill since it was just for development purposes
  element = document.getElementById("walletQuickFill");
  element.setAttribute("style","display: none;" );
  element.setAttribute("disabled","true" );

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
  if (action == "password" || action == "expire" || action == "clear") {
    wallet = Components.classes['component://netscape/wallet/wallet-service'];
    wallet = wallet.getService();
    wallet = wallet.QueryInterface(Components.interfaces.nsIWalletService);

    if (action == "password") {
      wallet.WALLET_ChangePassword();
    } else if (action == "expire") {
      wallet.WALLET_ExpirePassword();
    } else if (action == "clear") {
      wallet.WALLET_DeleteAll();
    }
    return;
  }

  if (action == "encrypt" || action == "obscure") {
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
      cookieViewer.AddPermission(window.content, true, COOKIEPERMISSION);
      element = document.getElementById("AllowCookies");
      alert(element.getAttribute("msg"));
    } else if (action == "cookieBlock") {
      cookieViewer.AddPermission(window.content, false, COOKIEPERMISSION);
      element = document.getElementById("BlockCookies");
      alert(element.getAttribute("msg"));
    } else if (action == "imageAllow") {
      cookieViewer.AddPermission(window.content, true, IMAGEPERMISSION);
      element = document.getElementById("AllowImages");
      alert(element.getAttribute("msg"));
    } else if (action == "imageBlock") {
      cookieViewer.AddPermission(window.content, false, IMAGEPERMISSION);
      element = document.getElementById("BlockImages");
      alert(element.getAttribute("msg"));
    }
    return;
  }

  if( appCore ) {
    switch( action ) {
      case "safefill":
        appCore.walletPreview(window, window.content);
        break;
//    case "password":
//      appCore.walletChangePassword();
//      break;
      case "quickfill": 
        appCore.walletQuickFillin(window.content);
        break;
      case "capture":
      default:
        appCore.walletRequestToCapture(window.content);
        break;
    }
  }
}  

// display a Wallet Dialog
function WalletDialog( which )
{
  switch( which ) {
    case "signon":
      window.openDialog("chrome://communicator/content/wallet/SignonViewer.xul","SSViewer","modal=yes,chrome,resizable=no"); 
      break;
    case "cookie":
      this.pref.SetBoolPref("cookieviewer.cookieTab", true);
      window.openDialog("chrome://communicator/content/wallet/CookieViewer.xul","CookieViewer","modal=yes,chrome,resizable=no"); 
      break;
    case "image":
      this.pref.SetBoolPref("cookieviewer.cookieTab", false);
      window.openDialog("chrome://communicator/content/wallet/CookieViewer.xul","CookieViewer","modal=yes,chrome,resizable=no"); 
      break;
    case "samples":
      window.content.location.href= 'http://www.mozilla.org/wallet/samples/';
      break;
    case "interview":
      window.content.location.href= 'http://www.mozilla.org/wallet/samples/INTERVIEW.HTML';
      break;
    case "wallet":
    default:
      window.openDialog("chrome://communicator/content/wallet/WalletEditor.xul","walletEditor","modal=yes,chrome,resizable=no"); 
      break;
  }
}


