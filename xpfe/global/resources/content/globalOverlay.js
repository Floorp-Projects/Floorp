var goPrefWindow = 0;

function goPageSetup()
{
}


function goQuitApplication()
{
	// since RDF in the task menu is broken we seem to crash (occasionally) when closing windows. So just call appShell shutdown
	// until the RDF is back working. I sure hope that it  fixes this problem
	
//	var editorShell = Components.classes["component://netscape/editor/editorshell"].createInstance();
//	editorShell = editorShell.QueryInterface(Components.interfaces.nsIEditorShell);
//	
//	if ( editorShell )
//		editorShell.Exit();
//	var windowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();

//	var	windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
//	do
//	{
		
//		var topWindow = windowManagerInterface.GetMostRecentWindow( null );
//		if ( topWindow == null )
//			break;
//		dump("closing window: "+ topWindow.title+"\n"); 
	
	
	//	if ( topWindow.tryToClose == null )
	//	{
//			topWindow.close();
	//	}
	//	else
	//	{
	//		// Make sure topMostWindow is visibile
	//		// stop closing windows if tryToClose returns false
	//		topWindow.focus();
	//		if ( !topWindow.tryToClose() )
	//			break;
	//	}
//	} while( 1 );
	
	// call appshell exit
	var appShell = Components.classes['component://netscape/appshell/appShellService'].getService();
	appShell = appShell.QueryInterface( Components.interfaces.nsIAppShellService );
	appShell.Shutdown();
}


function goNewCardDialog(selectedAB)
{
	window.openDialog("chrome://addressbook/content/abNewCardDialog.xul",
					  "",
					  "chrome,resizeable=no,modal",
					  {selectedAB:selectedAB});
}


function goEditCardDialog(abURI, card, okCallback)
{
	window.openDialog("chrome://addressbook/content/abEditCardDialog.xul",
					  "",
					  "chrome,resizeable=no,modal",
					  {abURI:abURI, card:card, okCallback:okCallback});
}


function goPreferences(id, pane)
{
	if (!top.goPrefWindow)
		top.goPrefWindow = Components.classes['component://netscape/prefwindow'].createInstance(Components.interfaces.nsIPrefWindow);
	
	if ( top.goPrefWindow )
		top.goPrefWindow.showWindow(id, window, pane);
}

function toggleToolbar( id )
{
	dump( "toggling toolbar "+id+"\n");
	var toolbar = document.getElementById( id );
	if ( toolbar )
	{
	 	var attribValue = toolbar.getAttribute("hidden") ;
	
		//dump("set hidden to "+!attribValue+"\n");
		if ( attribValue != false )
		{
		//	dump( "Show \n");
			toolbar.setAttribute("hidden", "" );
		}
		else
		{
		//	dump("hide \n");
			toolbar.setAttribute("hidden", true );
		}
		document.persist(id, 'hidden')
	}
}