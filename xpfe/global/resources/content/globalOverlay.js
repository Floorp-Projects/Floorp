var goPrefWindow = 0;

function getBrowserURL() {

  try {
    var prefs = Components.classes["component://netscape/preferences"];
    if (prefs) {
      prefs = prefs.getService();
      if (prefs)
        prefs = prefs.QueryInterface(Components.interfaces.nsIPref);
    }
    if (prefs) {
      var url = prefs.CopyCharPref("browser.chromeURL");
      if (url)
        return url;
    }
  } catch(e) {
  }
  return "chrome://navigator/content/navigator.xul";
}

function goPageSetup()
{
}


function goQuitApplication()
{
	var windowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();
	var	windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
	var enumerator = windowManagerInterface.getEnumerator( null );
	
	while ( enumerator.HasMoreElements()  )
	{
		var  windowToClose = enumerator.GetNext();
		var domWindow = windowManagerInterface.convertISupportsToDOMWindow( windowToClose );
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

	msgComposeService.OpenComposeWindow(null,
										null,
										Components.interfaces.nsIMsgCompType.New,
										Components.interfaces.nsIMsgCompFormat.Default,
										null); 
}  

function goNewCardDialog(selectedAB)
{
	window.openDialog("chrome://messenger/content/addressbook/abNewCardDialog.xul",
					  "",
					  "chrome,resizeable=no,modal",
					  {selectedAB:selectedAB});
}


function goEditCardDialog(abURI, card, okCallback)
{
	window.openDialog("chrome://messenger/content/addressbook/abEditCardDialog.xul",
					  "",
					  "chrome,resizeable=no,modal",
					  {abURI:abURI, card:card, okCallback:okCallback});
}


function goPreferences(id, paneURL, paneID)
{
  var prefWindowModalityPref;
  try {
    var pref = Components.classes["component://netscape/preferences"].getService();
    if( pref ) 
      pref = pref.QueryInterface( Components.interfaces.nsIPref );
    if( pref )
      prefWindowModalityPref = pref.GetBoolPref( "browser.prefWindowModal");
  }
  catch(e) {
    prefWindowModalityPref = true;
  }
  var modality = prefWindowModalityPref ? "yes" : "no";
  
  var prefWindow = window.openDialog("chrome://communicator/content/pref/pref.xul","PrefWindow", "chrome,modal=" + modality+ ",resizable=yes", paneURL, paneID);
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


function goClickThrobber( urlPref )
{
	var url;
	try {
		var pref = Components.classes["component://netscape/preferences"].getService();
		if( pref )
		pref = pref.QueryInterface( Components.interfaces.nsIPref );
		url = pref.CopyCharPref(urlPref);
	}

	catch(e) {
		url = null;
	}

	if ( url )
		openTopWin(url);
}


//No longer needed.  Rip this out since we are using openTopWin
function goHelpMenu( url )
{
  /* note that this chrome url should probably change to not have all of the navigator controls */
  /* also, do we want to limit the number of help windows that can be spawned? */
  window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url );
}


function openTopWin( url )
{
  /* note that this chrome url should probably change to not have all of the navigator controls,
     but if we do this we need to have the option for chrome controls because goClickThrobber()
     needs to use this function with chrome controls */
  /* also, do we want to limit the number of help windows that can be spawned? */
    dump("SetPrefToCurrentPage("+ url +") \n ");
    if ((url == null) || (url == "")) return;
     
    var windowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();
    var windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
 
    var topWindowOfType = windowManagerInterface.getMostRecentWindow( "navigator:browser" );
    if ( topWindowOfType )
    {
        dump("setting page: " + topWindowOfType.content.location.href + "\n");
        topWindowOfType.focus();
        topWindowOfType.content.location.href = url;
    }
    else
    {
        dump(" No browser window. Should be disabling this button \n");
        window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url );
    }
}

function goAboutDialog()
{
  var defaultAboutState = false;
  try {
    var pref = Components.classes["component://netscape/preferences"].getService();
    if( pref )
      pref = pref.QueryInterface( Components.interfaces.nsIPref );
    defaultAboutState = pref.GetBoolPref("browser.show_about_as_stupid_modal_window");
  }
  catch(e) {
    defaultAboutState = false;
  }
  if( defaultAboutState )
  	window.openDialog("chrome:global/content/about.xul", "About", "modal,chrome,resizable=yes,height=450,width=550");
  else 
    window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", 'chrome://global/content/about.html' );
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
	//dump("Updating edit menu items\n");
	goUpdateCommand('cmd_undo');
	goUpdateCommand('cmd_redo');
	goUpdateCommand('cmd_cut');
	goUpdateCommand('cmd_copy');
	goUpdateCommand('cmd_paste');
	goUpdateCommand('cmd_pasteQuote');
	goUpdateCommand('cmd_selectAll');
	goUpdateCommand('cmd_delete');
}

// update menu items that rely on the current selection
function goUpdateSelectEditMenuItems()
{
	//dump("Updating select menu items\n");
	goUpdateCommand('cmd_cut');
	goUpdateCommand('cmd_copy');
	goUpdateCommand('cmd_delete');
}

// update menu items that relate to undo/redo
function goUpdateUndoEditMenuItems()
{
	//dump("Updating undo/redo menu items\n");
	goUpdateCommand('cmd_undo');
	goUpdateCommand('cmd_redo');

	// we shouldn't really do this here, but we don't get the right notifications now
	goUpdateCommand('cmd_paste');
	goUpdateCommand('cmd_pasteQuote');
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


function helpMenuCreate()
{
    //adding the brand string to the about
	var BrandBundle = srGetStrBundle("chrome://global/locale/brand.properties");
	var aboutStrName = BrandBundle.GetStringFromName("aboutStrName");
	var aboutItem = document.getElementById( "releaseName" );
	aboutItem.setAttribute("value", aboutStrName);
	
	//Adding the release url since it will change based on brand
	var BrandRelUrl = BrandBundle.GetStringFromName("releaseUrl");	
	var relCommand =  "openTopWin(\'" + BrandRelUrl + "\')";
	var relItem = document.getElementById( "releaseUrl" );
	relItem.setAttribute("oncommand", relCommand);
}

function SendPage()
{
  var pageUrl = window.content.location.href;
  var pageTitle =  window.content.document.title;
  window.openDialog( "chrome://messenger/content/messengercompose/messengercompose.xul", "_blank", 
                     "chrome,all,dialog=no", 
                     "attachment='" + pageUrl + "',body='" + pageUrl +
                     "',subject='" + pageTitle + "',bodyislink=true");
}

function FillInTooltip ( tipElement )
{
  var retVal = false;
  var button = document.getElementById('replaceMe');
  if ( button ) {  
    var tipText = tipElement.getAttribute('tooltiptext');
    if ( tipText != "" ) {
      button.setAttribute('value', tipText);
      retVal = true;
    }
  }
  
  return retVal;
}
