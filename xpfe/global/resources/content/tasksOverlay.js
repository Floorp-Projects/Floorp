
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

function toHistory()
{
	var toolkitCore = XPAppCoresManager.Find("toolkitCore");
    if (!toolkitCore) {
      toolkitCore = new ToolkitCore();
      if (toolkitCore) {
        toolkitCore.Init("toolkitCore");
      }
    }
    if (toolkitCore) {
      toolkitCore.ShowWindow("chrome://history/content/",window);
    }
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
		window.open(uri, "", "chrome,menubar,toolbar,resizable");
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
		dump( " no windows of this type so create a new one \n");
		window.open( inChromeURL, "","chrome,menubar,toolbar,resizable" );
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
		window.open( inChromeURL, "","chrome,menubar,toolbar,resizable" );
	}
}

function toEditor()
{
	var toolkitCore = XPAppCoresManager.Find("ToolkitCore");
	if (!toolkitCore) {
	  toolkitCore = new ToolkitCore();
	  if (toolkitCore) {
	    toolkitCore.Init("ToolkitCore");
	  }
	}
	if (toolkitCore) {
	  toolkitCore.ShowWindowWithArgs("chrome://editor/content/EditorAppShell.xul",window,"chrome://editor/content/EditorInitPage.html");
	}
}

function toNewTextEditorWindow()
{
	core = XPAppCoresManager.Find("toolkitCore");
	if ( !core ) {
	    core = new ToolkitCore();
	    if ( core ) {
	        core.Init("toolkitCore");
	    }
	}
	if ( core ) {
	    core.ShowWindowWithArgs( "chrome://editor/content/TextEditorAppShell.xul", window, "chrome://editor/content/EditorInitPagePlain.html" );
	} else {
	    dump("Error; can't create toolkitCore\n");
	}
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
	
	window.content.location.href= inURL;
	dump(window.content.location.href+"\n");
    RefreshUrlbar();
}
