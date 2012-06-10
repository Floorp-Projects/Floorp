# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
