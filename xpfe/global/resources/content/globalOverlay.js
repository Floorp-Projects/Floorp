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
	appShell.Quit();
}


function goOpenNewMessage()
{
	var msgComposeService = Components.classes["component://netscape/messengercompose"].getService(); 
	msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService); 

	msgComposeService.OpenComposeWindowWithValues(null, 0, null, null, null, null, null, null, null); 
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
		top.goPrefWindow = Components.classes['component://netscape/prefwindow'].getService(Components.interfaces.nsIPrefWindow);
	
	if ( top.goPrefWindow )
		top.goPrefWindow.showWindow(id, window, pane);
}


function goToggleToolbar( id, elementID )
{
	dump( "toggling toolbar "+id+"\n");
	var toolbar = document.getElementById( id );
	var element = document.getElementById( elementID );
	if ( toolbar )
	{
	 	var attribValue = toolbar.getAttribute("hidden") ;
	
		//dump("set hidden to "+!attribValue+"\n");
		if ( attribValue != false )
		{
		//	dump( "Show \n");
			toolbar.setAttribute("hidden", "" );
			if ( element )
				element.setAttribute("checked","true")
		}
		else
		{
		//	dump("hide \n");
			toolbar.setAttribute("hidden", true );
			if ( element )
				element.setAttribute("checked","false")
		}
		document.persist(id, 'hidden');
		document.persist(elementID, 'checked');
	}
}


//
// Command Updater functions
//
function goUpdateCommand(command)
{
	var controller = top.document.commandDispatcher.getControllerForCommand(command);
	
	var enabled = false;
	
	if ( controller )
		enabled = controller.isCommandEnabled(command);
	else
		goSetMenuValue(command, 'valueDefault');
		
	goSetCommandEnabled(command, enabled);
}

function goDoCommand(command)
{
	var controller = top.document.commandDispatcher.getControllerForCommand(command);

	if ( controller )
		controller.doCommand(command);
}


function goSetDefaultController(controller)
{
	top.controllers.appendController(controller);
}

function goSetCommandEnabled(id, enabled)
{
	var node = top.document.getElementById(id);

	if ( node )
	{
		if ( enabled )
			node.removeAttribute("disabled");
		else
			node.setAttribute('disabled', 'true');
	}
}

function goSetMenuValue(command, valueAttribute)
{
	var commandNode = top.document.getElementById(command);
	var value = commandNode.getAttribute(valueAttribute);
	if ( commandNode && value )
		commandNode.setAttribute('value', value);
}

function goUpdateGlobalEditMenuItems()
{
	goUpdateCommand('cmd_undo');
	goUpdateCommand('cmd_redo');
	goUpdateCommand('cmd_cut');
	goUpdateCommand('cmd_copy');
	goUpdateCommand('cmd_paste');
	goUpdateCommand('cmd_selectAll');
	goUpdateCommand('cmd_delete');
}

// This used to be BrowserNewEditorWindow in navigator.js
function NewEditorWindow()
{
  dump("In NewEditorWindow()...\n");
  // Open editor window with blank page
  // Kludge to leverage openDialog non-modal!
  window.openDialog( "chrome://editor/content", "_blank", "chrome,all,dialog=no", "about:blank");
}

function NewEditorFromTemplate()
{
  dump("NOT IMPLEMENTED: Write NewEditorFromTemplate()!\n")
}

function NewEditorFromDraft()
{
  dump("NOT IMPLEMENTED: Write NewEditorFromDraft()!\n")
}

