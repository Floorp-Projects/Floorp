function closeWindow(aClose)
{
  var windowCount = 0;
   var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                      .getService(Components.interfaces.nsIWindowMediator);
  var e = wm.getEnumerator(null);
  
  while (e.hasMoreElements()) {
    var w = e.getNext();
    if (++windowCount == 2) 
      break;
  }

# Closing the last window doesn't quit the application on OS X.
#ifndef XP_MACOSX
  // If we're down to the last window and someone tries to shut down, check to make sure we can!
  if (windowCount == 1 && !canQuitApplication())
    return false;
#endif

  if (aClose)    
    window.close();
  
  return true;
}

function canQuitApplication()
{
  var os = Components.classes["@mozilla.org/observer-service;1"]
                     .getService(Components.interfaces.nsIObserverService);
  if (!os) return true;
  
  try {
    var cancelQuit = Components.classes["@mozilla.org/supports-PRBool;1"]
                              .createInstance(Components.interfaces.nsISupportsPRBool);
    os.notifyObservers(cancelQuit, "quit-application-requested", null);
    
    // Something aborted the quit process. 
    if (cancelQuit.data)
      return false;
  }
  catch (ex) { }
  os.notifyObservers(null, "quit-application-granted", null);
  return true;
}

function goQuitApplication()
{
  if (!canQuitApplication())
    return false;

  var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
  var windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
  var enumerator = windowManagerInterface.getEnumerator( null );
  var appStartup = Components.classes['@mozilla.org/toolkit/app-startup;1'].
                     getService(Components.interfaces.nsIAppStartup);

  while ( enumerator.hasMoreElements()  )
  {
     var domWindow = enumerator.getNext();
     if (("tryToClose" in domWindow) && !domWindow.tryToClose())
       return false;
     domWindow.close();
  };
  appStartup.quit(Components.interfaces.nsIAppStartup.eAttemptQuit);
  return true;
}

//
// Command Updater functions
//
function goUpdateCommand(aCommand)
{
  try {
    var controller = top.document.commandDispatcher
                        .getControllerForCommand(aCommand);

    var enabled = false;
    if (controller)
      enabled = controller.isCommandEnabled(aCommand);

    goSetCommandEnabled(aCommand, enabled);
  }
  catch (e) {
    dump("An error occurred updating the " + aCommand + " command\n");
  }
}

function goDoCommand(aCommand)
{
  try {
    var controller = top.document.commandDispatcher
                        .getControllerForCommand(aCommand);
    if (controller && controller.isCommandEnabled(aCommand))
      controller.doCommand(aCommand);
  }
  catch (e) {
    dump("An error occurred executing the " + aCommand + " command\n" + e + "\n");
  }
}


function goSetCommandEnabled(aID, aEnabled)
{
  var node = document.getElementById(aID);

  if (node) {
    if (aEnabled)
      node.removeAttribute("disabled");
    else
      node.setAttribute("disabled", "true");
  }
}

function goSetMenuValue(aCommand, aLabelAttribute)
{
  var commandNode = top.document.getElementById(aCommand);
  if (commandNode) {
    var label = commandNode.getAttribute(aLabelAttribute);
    if (label)
      commandNode.setAttribute("label", label);
  }
}

function goSetAccessKey(aCommand, aValueAttribute)
{
  var commandNode = top.document.getElementById(aCommand);
  if (commandNode) {
    var value = commandNode.getAttribute(aValueAttribute);
    if (value)
      commandNode.setAttribute("accesskey", value);
  }
}

// this function is used to inform all the controllers attached to a node that an event has occurred
// (e.g. the tree controllers need to be informed of blur events so that they can change some of the
// menu items back to their default values)
function goOnEvent(node, aEvent)
{
  var numControllers = aNode.controllers.getControllerCount();
  var controller;

  for (var controllerIndex = 0; controllerIndex < numControllers; controllerIndex++) {
    controller = aNode.controllers.getControllerAt(controllerIndex);
    if (controller)
      controller.onEvent(aEvent);
  }
}

function visitLink(aEvent) {
  var node = aEvent.target;
  while (node.nodeType != Node.ELEMENT_NODE)
    node = node.parentNode;
  var url = node.getAttribute("link");
  if (!url)
    return;

  var protocolSvc = Components.classes["@mozilla.org/uriloader/external-protocol-service;1"]
                              .getService(Components.interfaces.nsIExternalProtocolService);
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);
  var uri = ioService.newURI(url, null, null);

  // if the scheme is not an exposed protocol, then opening this link
  // should be deferred to the system's external protocol handler
  if (protocolSvc.isExposedProtocol(uri.scheme)) {
    var win = window.top;
    if (win instanceof Components.interfaces.nsIDOMChromeWindow) {
      while (win.opener && !win.opener.closed)
        win = win.opener;
    }
    win.open(uri.spec);
  }
  else
    protocolSvc.loadUrl(uri);
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

#include debug.js
