/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): Alec Flett       <alecf@netscape.com>
 *                 Bill Law         <law@netscape.com>
 *                 Blake Ross       <blakeross@telocity.com>
 *                 Matt Fisher      <matt@netscape.com>
 *                 Simon Fraser     <sfraser@netscape.com>
 *                 Stuart Parmenter <pavlov@netscape.com>
 */

var finder; // Find component.
var data;   // Search context (passed as argument).
var dialog; // Quick access to document/form elements.

function initDialogObject()
{
  // Create dialog object and initialize.
  dialog = new Object;
  dialog.findKey         = document.getElementById("dialog.findKey");
  dialog.caseSensitive   = document.getElementById("dialog.caseSensitive");
  dialog.wrap            = document.getElementById("dialog.wrap");
  dialog.searchBackwards = document.getElementById("dialog.searchBackwards");
  dialog.find            = document.getElementById("ok");
  dialog.bundle          = null;
}

function fillDialog()
{
  // Set initial dialog field contents.
  dialog.findKey.value = data.searchString;

  dialog.caseSensitive.checked   = data.caseSensitive;
  dialog.wrap.checked            = data.wrapSearch;
  dialog.searchBackwards.checked = data.searchBackwards;
}

function loadData()
{
  // Set data attributes per user input.
  data.searchString    = dialog.findKey.value;
  data.caseSensitive   = dialog.caseSensitive.checked;
  data.wrapSearch      = dialog.wrap.checked;
  data.searchBackwards = dialog.searchBackwards.checked;
}

function onLoad()
{
  initDialogObject();

  // Get find component.
  finder = Components.classes["@mozilla.org/appshell/component/find;1"].getService();
  finder = finder.QueryInterface(Components.interfaces.nsIFindComponent);

  // Change "OK" to "Find".
  dialog.find.value = document.getElementById("fBLT").getAttribute("value");

  // Setup the dialogOverlay.xul button handlers.
  doSetOKCancel(onOK, onCancel);

  // Save search context.
  data = window.arguments[0];

  // Tell search context about this dialog.
  data.findDialog = window;

  fillDialog();

  doEnabling();

  if (dialog.findKey.value)
    dialog.findKey.select();
  else
    dialog.findKey.focus();
}

function onUnload() {
  // Disconnect context from this dialog.
  data.findDialog = null;
}

function onOK()
{
  // Transfer dialog contents to data elements.
  loadData();

  // Search.
  var result = finder.findNext(data);
  if (!result) {
    if (!dialog.bundle)
      dialog.bundle = document.getElementById("findBundle");
    window.alert(dialog.bundle.getString("notFoundWarning"));
  }
}

function onCancel()
{
  // Close the window.
  return true;
}

function doEnabling()
{
  dialog.find.disabled = !dialog.findKey.value;
}
