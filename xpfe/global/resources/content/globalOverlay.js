var goPrefWindow = 0;

function goPageSetup()
{
}


function goQuitApplication()
{
	var windowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();
	var	windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
	var enumerator = windowManagerInterface.GetEnumerator( null );
	
	while ( enumerator.HasMoreElements()  )
	{
		var  windowToClose = enumerator.GetNext();
		var domWindow = windowManagerInterface.ConvertISupportsToDOMWindow( windowToClose );
		domWindow.focus();
		if ( domWindow.tryToClose == null )
		{
			// dump(" window.close \n");
			domWindow.close();
		}
		else
		{
			// dump(" try to close \n" );
			if ( !domWindow.tryToClose() )
				return false;
		}
	};
	
	// call appshell exit
	var appShell = Components.classes['component://netscape/appshell/appShellService'].getService();
	appShell = appShell.QueryInterface( Components.interfaces.nsIAppShellService );
	appShell.Quit();
	return true;
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
  //if( !top.goPrefWindow ) // XXXX commenting out for now until we find a way to duplicate this.
    top.goPrefWindow = window.openDialog("chrome://pref/content/pref.xul","PrefWindow", "chrome,modal=no,resizable=yes", pane);
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
		
	goSetCommandEnabled(command, enabled);
}

function goDoCommand(command)
{
	var controller = top.document.commandDispatcher.getControllerForCommand(command);

	if ( controller )
		controller.doCommand(command);
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
	if ( commandNode )
	{
		var value = commandNode.getAttribute(valueAttribute);
		if ( value )
			commandNode.setAttribute('value', value);
	}
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

// this function is used to inform all the controllers attached to a node that an event has occurred
// (e.g. the tree controllers need to be informed of blur events so that they can change some of the
// menu items back to their default values)
function goOnEvent(node, event)
{
	var numControllers = node.controllers.getControllerCount();
	var controller;
	
	for ( var controllerIndex = 0; controllerIndex < numControllers; controllerIndex++ )
	{
		controller = node.controllers.getControllerAt(controllerIndex);
		if ( controller )
			controller.onEvent(event);
	}
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

