/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Contributor(s): Joe Hewitt <hewitt@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var gConsole, gConsoleBundle;

/* :::::::: Console Initialization ::::::::::::::: */

window.onload = function()
{
  gConsole = document.getElementById("ConsoleBox");
  gConsoleBundle = document.getElementById("ConsoleBundle");
  
  top.controllers.insertControllerAt(0, ConsoleController);
  
  updateSortCommand(gConsole.sortOrder);
  updateModeCommand(gConsole.mode);
}

/* :::::::: Console UI Functions ::::::::::::::: */

function changeMode(aMode)
{
  switch (aMode) {
    case "Errors":
    case "Warnings":
    case "Messages":
      gConsole.mode = aMode;
      break;
    case "All":
      gConsole.mode = null;
  }
  
  document.persist("ConsoleBox", "mode");
}

function clearConsole()
{
  gConsole.clear();
}

function changeSortOrder(aOrder)
{
  var order = aOrder == -1 ? -1 : 1; // default to 1
  gConsole.sortOrder = order;
  document.persist("ConsoleBox", "sortOrder");
  updateSortCommand(order);
}

function updateSortCommand(aOrder)
{
  var order = aOrder == -1 ? -1 : 1; // default to 1
  var orderString = order == 1 ? "Ascend" : "Descend";
  var bc = document.getElementById("Console:sort"+orderString);
  bc.setAttribute("checked", true);  

  order *= -1;
  orderString = order == 1 ? "Ascend" : "Descend";
  bc = document.getElementById("Console:sort"+orderString);
  bc.setAttribute("checked", false);
}

function updateModeCommand(aMode)
{
  var bc = document.getElementById("Console:mode" + aMode);
  bc.setAttribute("checked", true);
}

function toggleToolbar(aEl)
{
  var bc = document.getElementById(aEl.getAttribute("observes"));
  var truth = bc.getAttribute("checked");
  bc.setAttribute("checked", truth != "true");
  var toolbar = document.getElementById(bc.getAttribute("_toolbar"));
  toolbar.setAttribute("hidden", truth);

  document.persist(toolbar.id, "hidden");
  document.persist(bc.id, "checked");
}

function copyItemToClipboard()
{
  gConsole.copySelectedItem();
}

function isItemSelected()
{
  return gConsole.selectedItem != null;
}

function UpdateCopyMenu()
{
  goUpdateCommand("cmd_copy");
}

function onEvalKeyPress(aEvent)
{
  if (aEvent.keyCode == 13)
    evaluateTypein();
}

function evaluateTypein()
{
  var code = document.getElementById("TextboxEval").value;
  var iframe = document.getElementById("Evaluator");
  iframe.setAttribute("src", "javascript: " + code);
}

/* :::::::: Command Controller for the Window ::::::::::::::: */

var ConsoleController = 
{
  isCommandEnabled: function (aCommand)
  {
    switch (aCommand) {
      case "cmd_copy":
        return isItemSelected();
      default:
        return false;
    }
  },
  
  supportsCommand: function (aCommand) 
  {
    switch (aCommand) {
      case "cmd_copy":
        return true;
      default:
        return false;
    }
  },
  
  doCommand: function (aCommand)
  {
    switch (aCommand) {
      case "cmd_copy":
        copyItemToClipboard();
        break;
      default:
        break;
    }
  },
  
  onEvent: function (aEvent) 
  {
  }
};

// XXX DEBUG

function debug(aText)
{
  var csClass = Components.classes['@mozilla.org/consoleservice;1'];
  var cs = csClass.getService(Components.interfaces.nsIConsoleService);
  cs.logStringMessage(aText);
}

function getStackTrace()
{
  var frame = Components.stack.caller;
  var str = "";
  while (frame) {
    if (frame.filename)
      str += frame.filename + ", Line " + frame.lineNumber;
    else
      str += "[" + gConsoleBundle.getString("noFile") + "]";
    
    str += " --> ";
    
    if (frame.functionName)
      str += frame.functionName;
    else
      str += "[" + gConsoleBundle.getString("noFunction") + "]";
      
    str += "\n";
    
    frame = frame.caller;
  }
  
  return str;
}
