
function toNavigator()
{
	CycleWindow('navigator:browser', 'chrome://navigator/content/');
}

function toMessengerWindow()
{
	toOpenWindowByType("mail:3pane", "chrome://messenger/content/");
}


function toAddressBook() 
{
	toOpenWindowByType("mail:addressbook", "chrome://addressbook/content/addressbook.xul");
}

function toNewsgroups() 
{

        dump ("Sorry, command not implemented.\n");

}

function toIRC() 
{

        window.open ("resource:///irc/tests/index.html");

}

function toHistory()
{
    window.open( "chrome://history/content/", "_blank", "chrome,dependent=yes,all" );
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

	var topWindow = windowManagerInterface.GetMostRecentWindow( inType );
	
	if ( topWindow )
		topWindow.focus();
	else
		window.open(uri, "_blank", "chrome,menubar,toolbar,resizable");
}


function OpenBrowserWindow()
{
  dump("In OpenBrowserWindw()...\n");
  pref = Components.classes['component://netscape/preferences'];

  // if all else fails, use trusty "about:blank" as the start page
  var startpage = "about:blank";  
  if (pref) {
    pref = pref.getService(); 
    pref = pref.QueryInterface(Components.interfaces.nsIPref);
  }



  if (pref) {
    // from mozilla/modules/libpref/src/init/all.js
    // 0 = blank 
    // 1 = home (browser.startup.homepage)
    // 2 = last 
		choice = 1;
	  try {
      choice = pref.GetIntPref("browser.startup.page");
	  }
	  catch (ex) {
		  dump("failed to get the browser.startup.page pref\n");
	  }

    switch (choice) {
      case 0:
        startpage = "about:blank";
        break;
      case 1:
        try {
          startpage = pref.CopyCharPref("browser.startup.homepage");
        }
        catch (ex) {
	        dump("failed to get the browser.startup.homepage pref\n");
	        startpage = "about:blank";
        }
                break;
      case 2:
        try {
	        var history = Components.classes["component://netscape/browser/global-history"].getService();
                        history = history.QueryInterface(Components.interfaces.nsIGlobalHistory);
	        startpage = history.GetLastPageVisted();
        }
        catch (ex) {
	        dump(ex +"\n");
        }
                break;
      default:
                startpage = "about:blank";
    }
  }
//	window.open(startpage); // This doesn't size the window properly.
  window.openDialog( "chrome://navigator/content/navigator.xul", "_blank", "chrome,all,dialog=no", startpage );
}


function CycleWindow( inType, inChromeURL )
{
	var windowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();
	dump("got window Manager \n");
	var	windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
    dump("got interface \n");
    
    var desiredWindow = null;
    
	var topWindowOfType = windowManagerInterface.GetMostRecentWindow( inType );
	var topWindow = windowManagerInterface.GetMostRecentWindow( null );
	dump( "got windows \n");
	
	dump( "topWindowOfType = " + topWindowOfType + "\n");
	if ( topWindowOfType == null )
	{
		OpenBrowserWindow();
		return;
	}
	
	if ( topWindowOfType != topWindow )
	{
		dump( "first not top so give focus \n");
		topWindowOfType.focus();
		return;
	}
	
	var enumerator = windowManagerInterface.GetEnumerator( inType );
	firstWindow = windowManagerInterface.ConvertISupportsToDOMWindow ( enumerator.GetNext() );
	if ( firstWindow == topWindowOfType )
	{
		dump( "top most window is first window \n");
		firstWindow = null;
	}
	else
	{
		dump("find topmost window \n");
		while ( enumerator.HasMoreElements() )
		{
			var nextWindow = windowManagerInterface.ConvertISupportsToDOMWindow ( enumerator.GetNext() );
			if ( nextWindow == topWindowOfType )
				break;
		}	
	}
	desiredWindow = firstWindow;
	if ( enumerator.HasMoreElements() )
	{
		dump( "Give focus to next window in the list \n");
		desiredWindow = windowManagerInterface.ConvertISupportsToDOMWindow ( enumerator.GetNext() );		
	}
	
	if ( desiredWindow )
	{
		desiredWindow.focus();
		dump("focusing window \n");
	}
	else
	{
		dump("open window \n");
		window.OpenBrowserWindow();
	}
}

function toEditor()
{
  //TODO: Find and existing editor window using CycleWindow()
  // For now, just start a new blank document
  window.openDialog( "chrome://editor/content", "_blank", "chrome,all,dialog=no", "about:blank");
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
	desiredWindow = windowManagerInterface.GetWindowForResource( url );
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
