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

/**
 * Command Updater
 */
var CommandUpdater = {
  /**
   * Gets a controller that can handle a particular command. 
   * @param   command
   *          A command to locate a controller for, preferring controllers that
   *          show the command as enabled.
   * @returns In this order of precedence:
   *            - the first controller supporting the specified command 
   *              associated with the focused element that advertises the
   *              command as ENABLED
   *            - the first controller supporting the specified command 
   *              associated with the global window that advertises the
   *              command as ENABLED
   *            - the first controller supporting the specified command
   *              associated with the focused element
   *            - the first controller supporting the specified command
   *              associated with the global window
   */
  _getControllerForCommand: function(command) {
    try {
      var controller = 
          top.document.commandDispatcher.getControllerForCommand(command);
      if (controller && controller.isCommandEnabled(command))
        return controller;
    }
    catch(e) {
    }
    var controllerCount = window.controllers.getControllerCount();
    for (var i = 0; i < controllerCount; ++i) {
      var current = window.controllers.getControllerAt(i);
      try {
        if (current.supportsCommand(command) && current.isCommandEnabled(command))
          return current;
      }
      catch (e) {
      }
    }
    return controller || window.controllers.getControllerForCommand(command);
  },
  
  /**
   * Updates the state of a XUL <command> element for the specified command
   * depending on its state.
   * @param   command
   *          The name of the command to update the XUL <command> element for
   */
  updateCommand: function(command) {
    var controller = this._getControllerForCommand(command);
    if (!controller)
      return;
    try {
      this.enableCommand(command, controller.isCommandEnabled(command));
    }
    catch (e) {
    }
  },
  
  /**
   * Enables or disables a XUL <command> element.
   * @param   command
   *          The name of the command to enable or disable
   * @param   enabled
   *          true if the command should be enabled, false otherwise.
   */
  enableCommand: function(command, enabled) {
    var element = document.getElementById(command);
    if (!element)
      return;
    if (enabled)
      element.removeAttribute("disabled");
    else
      element.setAttribute("disabled", "true");
  },

  /**
   * Performs the action associated with a specified command using the most
   * relevant controller.
   * @param   command
   *          The command to perform.
   */
  doCommand: function(command) {
    var controller = this._getControllerForCommand(command);
    if (!controller)
      return;
    controller.doCommand(command);
  }, 
  
  /**
   * Changes the label attribute for the specified command.
   * @param   command
   *          The command to update.
   * @param   labelAttribute
   *          The label value to use.
   */
  setMenuValue: function(command, labelAttribute) {
    var commandNode = top.document.getElementById(command);
    if (commandNode)
    {
      var label = commandNode.getAttribute(labelAttribute);
      if ( label )
        commandNode.setAttribute('label', label);
    }
  },
  
  /**
   * Changes the accesskey attribute for the specified command.
   * @param   command
   *          The command to update.
   * @param   valueAttribute
   *          The value attribute to use.
   */
  setAccessKey: function(command, valueAttribute) {
    var commandNode = top.document.getElementById(command);
    if (commandNode)
    {
      var value = commandNode.getAttribute(valueAttribute);
      if ( value )
        commandNode.setAttribute('accesskey', value);
    }
  },

  /**
   * Inform all the controllers attached to a node that an event has occurred
   * (e.g. the tree controllers need to be informed of blur events so that they can change some of the
   * menu items back to their default values)
   * @param   node
   *          The node receiving the event
   * @param   event
   *          The event.
   */
  onEvent: function(node, event) {
    var numControllers = node.controllers.getControllerCount();
    var controller;

    for ( var controllerIndex = 0; controllerIndex < numControllers; controllerIndex++ )
    {
      controller = node.controllers.getControllerAt(controllerIndex);
      if ( controller )
        controller.onEvent(event);
    }
  }  
};
// Shim for compatibility with existing code. 
function goDoCommand(command) { CommandUpdater.doCommand(command); }
function goUpdateCommand(command) { CommandUpdater.updateCommand(command); }
function goSetCommandEnabled(command, enabled) { CommandUpdater.enableCommand(command, enabled); }
function goSetMenuValue(command, labelAttribute) { CommandUpdater.setMenuValue(command, labelAttribute); }
function goSetAccessKey(command, valueAttribute) { CommandUpdater.setAccessKey(command, valueAttribute); }
function goOnEvent(node, event) { CommandUpdater.onEvent(node, event); }

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
