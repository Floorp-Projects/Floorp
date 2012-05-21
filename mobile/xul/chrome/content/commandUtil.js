/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


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
      var controller = top.document.commandDispatcher.getControllerForCommand(command);
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
    var enabled = false;
    try {
      var controller = this._getControllerForCommand(command);
      if (controller) {
        enabled = controller.isCommandEnabled(command);
      }
    }
    catch(ex) { }

    this.enableCommand(command, enabled);
  },

  /**
   * Updates the state of a XUL <command> element for the specified command
   * depending on its state.
   * @param   command
   *          The name of the command to update the XUL <command> element for
   */
  updateCommands: function(_commands) {
    var commands = _commands.split(",");
    for (var command in commands) {
      this.updateCommand(commands[command]);
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
