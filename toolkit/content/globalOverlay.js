function closeWindow(aClose)
{
  var windowCount = 0;
  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"].getService(Components.interfaces.nsIWindowMediator);
  var e = wm.getEnumerator(null);
  
  while (e.hasMoreElements()) {
    var w = e.getNext();
    ++windowCount;
    if (windowCount == 2) 
      break;
  }

  // If we're down to the last window and someone tries to shut down, check to make sure we can!
  if (windowCount == 1 && !canQuitApplication()) 
    return false;

  if (aClose)    
    window.close();
  
  return true;
}

function canQuitApplication()
{
  var os = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  if (os) {
    try {
      var cancelQuit = Components.classes["@mozilla.org/supports-PRBool;1"].createInstance(Components.interfaces.nsISupportsPRBool);
      os.notifyObservers(cancelQuit, "quit-application-requested", null);
      
      // Something aborted the quit process. 
      if (cancelQuit.data)
        return false;
    }
    catch (ex) {
    }
  }
  return true;
}

function goQuitApplication()
{
  if (!canQuitApplication())
    return;
    
  var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
  var windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
  var enumerator = windowManagerInterface.getEnumerator( null );
  var appShell = Components.classes['@mozilla.org/appshell/appShellService;1'].getService();
  appShell = appShell.QueryInterface( Components.interfaces.nsIAppShellService );

  var nativeAppSupport = null;
  try {
    nativeAppSupport = appShell.nativeAppSupport;
  }
  catch ( ex ) {
  }

  while ( enumerator.hasMoreElements()  )
  {
     var domWindow = enumerator.getNext();
     if (("tryToClose" in domWindow) && !domWindow.tryToClose())
       return false;
     domWindow.close();
  };
  appShell.quit(Components.interfaces.nsIAppShellService.eAttemptQuit);
  return true;
}

//
// Command Updater functions
//
function goUpdateCommand(command)
{
  try {
    var controller = top.document.commandDispatcher.getControllerForCommand(command);

    var enabled = false;

    if ( controller )
      enabled = controller.isCommandEnabled(command);

    goSetCommandEnabled(command, enabled);
  }
  catch (e) {
    dump("An error occurred updating the "+command+" command\n");
  }
}

function goDoCommand(command)
{
  try {
    var controller = top.document.commandDispatcher.getControllerForCommand(command);
    if ( controller && controller.isCommandEnabled(command))
      controller.doCommand(command);
  }
  catch (e) {
    dump("An error occurred executing the " + command + " command\n" + e + "\n");
  }
}


function goSetCommandEnabled(id, enabled)
{
  var node = document.getElementById(id);

  if ( node )
  {
    if ( enabled )
      node.removeAttribute("disabled");
    else
      node.setAttribute('disabled', 'true');
  }
}

function goSetMenuValue(command, labelAttribute)
{
  var commandNode = top.document.getElementById(command);
  if ( commandNode )
  {
    var label = commandNode.getAttribute(labelAttribute);
    if ( label )
      commandNode.setAttribute('label', label);
  }
}

function goSetAccessKey(command, valueAttribute)
{
  var commandNode = top.document.getElementById(command);
  if ( commandNode )
  {
    var value = commandNode.getAttribute(valueAttribute);
    if ( value )
      commandNode.setAttribute('accesskey', value);
  }
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

function visitLink(aEvent) {
  var node = aEvent.target;
  while (node.nodeType != Node.ELEMENT_NODE)
    node = node.parentNode;
  var url = node.getAttribute("link");
  if (url != "")
    top.opener.openNewWindowWith(url, null, false);
}

function isValidLeftClick(aEvent, aName)
{
  return (aEvent.button == 0 && aEvent.originalTarget.localName == aName);
}

function setTooltipText(aID, aTooltipText)
{
  var element = document.getElementById(aID);
  if (element)
    element.setAttribute("tooltiptext", aTooltipText);
}

function FillInTooltip ( tipElement )
{
  var retVal = false;
  var textNode = document.getElementById("TOOLTIP-tooltipText");
  if (textNode) {
    while (textNode.hasChildNodes())
      textNode.removeChild(textNode.firstChild);
    var tipText = tipElement.getAttribute("tooltiptext");
    if (tipText) {
      var node = document.createTextNode(tipText);
      textNode.appendChild(node);
      retVal = true;
    }
  }
  return retVal;
}
