# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Joe Hewitt <hewitt@netscape.com>
#   Simon Bünzli <zeniko@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

var gConsole, gConsoleBundle, gTextBoxEval, gEvaluator, gCodeToEvaluate;

/* :::::::: Console Initialization ::::::::::::::: */

window.onload = function()
{
  gConsole = document.getElementById("ConsoleBox");
  gConsoleBundle = document.getElementById("ConsoleBundle");
  gTextBoxEval = document.getElementById("TextboxEval")  
  gEvaluator = document.getElementById("Evaluator");
  
  updateSortCommand(gConsole.sortOrder);
  updateModeCommand(gConsole.mode);

  gEvaluator.addEventListener("load", loadOrDisplayResult, true);
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
  updateSortCommand(gConsole.sortOrder = aOrder);
}

function updateSortCommand(aOrder)
{
  var orderString = aOrder == 'reverse' ? "Descend" : "Ascend";
  var bc = document.getElementById("Console:sort"+orderString);
  bc.setAttribute("checked", true);  

  orderString = aOrder == 'reverse' ? "Ascend" : "Descend";
  bc = document.getElementById("Console:sort"+orderString);
  bc.setAttribute("checked", false);
}

function updateModeCommand(aMode)
{
  /* aMode can end up invalid if it set by an extension that replaces */
  /* mode and then it is uninstalled or disabled */
  var bc = document.getElementById("Console:mode" + aMode) ||
           document.getElementById("Console:modeAll");
  bc.setAttribute("checked", true);
}

function onEvalKeyPress(aEvent)
{
  if (aEvent.keyCode == 13)
    evaluateTypein();
}

function evaluateTypein()
{
  gCodeToEvaluate = gTextBoxEval.value;
  // reset the iframe first; the code will be evaluated in loadOrDisplayResult
  // below, once about:blank has completed loading (see bug 385092)
  gEvaluator.contentWindow.location = "about:blank";
}

function loadOrDisplayResult()
{
  if (gCodeToEvaluate) {
    gEvaluator.contentWindow.location = "javascript: " +
                                        gCodeToEvaluate.replace(/%/g, "%25");
    gCodeToEvaluate = "";
    return;
  }

  var resultRange = gEvaluator.contentDocument.createRange();
  resultRange.selectNode(gEvaluator.contentDocument.documentElement);
  var result = resultRange.toString();
  if (result)
    gConsole.mCService.logStringMessage(result);
    // or could use appendMessage which doesn't persist
}

// XXX DEBUG
function debug(aText)
{
  var csClass = Components.classes['@mozilla.org/consoleservice;1'];
  var cs = csClass.getService(Components.interfaces.nsIConsoleService);
  cs.logStringMessage(aText);
}
