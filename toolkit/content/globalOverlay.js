/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function closeWindow(aClose, aPromptFunction)
{
# Closing the last window doesn't quit the application on OS X.
#ifndef XP_MACOSX
  var windowCount = 0;
  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator);
  var e = wm.getEnumerator(null);
  
  while (e.hasMoreElements()) {
    var w = e.getNext();
    if (++windowCount == 2) 
      break;
  }

  var inPrivateBrowsing = false;
  try {
    if (["@mozilla.org/privatebrowsing;1"] in Components.classes) {
      var pbSvc = Components.classes["@mozilla.org/privatebrowsing;1"]
                            .getService(Components.interfaces.nsIPrivateBrowsingService);
      inPrivateBrowsing = pbSvc.privateBrowsingEnabled;
    }
  } catch(e) {
    // safe to ignore
  }

  // If we're down to the last window and someone tries to shut down, check to make sure we can!
  if (windowCount == 1 && !canQuitApplication("lastwindow"))
    return false;
  else if (windowCount != 1 || inPrivateBrowsing)
#endif
    if (typeof(aPromptFunction) == "function" && !aPromptFunction())
      return false;

  if (aClose)    
    window.close();
  
  return true;
}

function canQuitApplication(aData)
{
  var os = Components.classes["@mozilla.org/observer-service;1"]
                     .getService(Components.interfaces.nsIObserverService);
  if (!os) return true;
  
  try {
    var cancelQuit = Components.classes["@mozilla.org/supports-PRBool;1"]
                              .createInstance(Components.interfaces.nsISupportsPRBool);
    os.notifyObservers(cancelQuit, "quit-application-requested", aData || null);
    
    // Something aborted the quit process. 
    if (cancelQuit.data)
      return false;
  }
  catch (ex) { }
  return true;
}

function goQuitApplication()
{
  if (!canQuitApplication())
    return false;

  var appStartup = Components.classes['@mozilla.org/toolkit/app-startup;1'].
                     getService(Components.interfaces.nsIAppStartup);

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
    Components.utils.reportError("An error occurred updating the " +
                                 aCommand + " command: " + e);
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
    Components.utils.reportError("An error occurred executing the " +
                                 aCommand + " command: " + e);
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
function goOnEvent(aNode, aEvent)
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

function setTooltipText(aID, aTooltipText)
{
  var element = document.getElementById(aID);
  if (element)
    element.setAttribute("tooltiptext", aTooltipText);
}

__defineGetter__("NS_ASSERT", function() {
  delete this.NS_ASSERT;
  var tmpScope = {};
  Components.utils.import("resource://gre/modules/debug.js", tmpScope);
  return this.NS_ASSERT = tmpScope.NS_ASSERT;
});
