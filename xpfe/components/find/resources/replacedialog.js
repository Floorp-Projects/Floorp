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
 * Contributor(s): Kin Blas <kin@netscape.com>
 *
 */

var gFindComponent;   // Find component.
var gFindReplaceData; // Search context (passed as argument).
var gReplaceDialog;      // Quick access to document/form elements.

function initDialogObject()
{
  // Create gReplaceDialog object and initialize.
  gReplaceDialog = new Object();
  gReplaceDialog.findKey         = document.getElementById("dialog.findKey");
  gReplaceDialog.replaceKey      = document.getElementById("dialog.replaceKey");
  gReplaceDialog.caseSensitive   = document.getElementById("dialog.caseSensitive");
  gReplaceDialog.wrap            = document.getElementById("dialog.wrap");
  gReplaceDialog.searchBackwards = document.getElementById("dialog.searchBackwards");
  gReplaceDialog.findNext        = document.getElementById("findNext");
  gReplaceDialog.replace         = document.getElementById("replace");
  gReplaceDialog.replaceAll      = document.getElementById("replaceAll");
  gReplaceDialog.bundle         = null;
}

function loadDialog()
{
  // Set initial dialog field contents.
  gReplaceDialog.findKey.value = gFindReplaceData.searchString;
  gReplaceDialog.replaceKey.value = gFindReplaceData.replaceString;

  gReplaceDialog.caseSensitive.checked = gFindReplaceData.caseSensitive;
  gReplaceDialog.wrap.checked = gFindReplaceData.wrapSearch;
  gReplaceDialog.searchBackwards.checked = gFindReplaceData.searchBackwards;

  doEnabling();
}

function loadData()
{
  // Set gFindReplaceData attributes per user input.
  gFindReplaceData.searchString = gReplaceDialog.findKey.value;
  gFindReplaceData.replaceString = gReplaceDialog.replaceKey.value;
  gFindReplaceData.caseSensitive = gReplaceDialog.caseSensitive.checked;
  gFindReplaceData.wrapSearch = gReplaceDialog.wrap.checked;
  gFindReplaceData.searchBackwards = gReplaceDialog.searchBackwards.checked;
}

function onLoad()
{
  // Init gReplaceDialog.
  initDialogObject();

  // Get find component.
  gFindComponent = Components.classes[ "@mozilla.org/appshell/component/find;1" ].getService();
  gFindComponent = gFindComponent.QueryInterface( Components.interfaces.nsIFindComponent );

  // Save search context.
  gFindReplaceData = window.arguments[0];

  // Tell search context about this dialog.
  gFindReplaceData.replaceDialog = window;

  // Fill dialog.
  loadDialog();

  if (gReplaceDialog.findKey.value)
    gReplaceDialog.findKey.select();
  else
    gReplaceDialog.findKey.focus();
}

function onUnload() {
  // Disconnect context from this dialog.
  gFindReplaceData.replaceDialog = null;
}

function onFindNext()
{
  // Transfer dialog contents to data elements.
  loadData();

  // Search.
  var result = gFindComponent.findNext(gFindReplaceData);
  if (!result) {
    if (!gReplaceDialog.bundle)
      gReplaceDialog.bundle = document.getElementById("replaceBundle");
    alert(gReplaceDialog.bundle.getString("notFoundWarning"));
  }
  return false;
}

function onReplace()
{
  // Transfer dialog contents to data elements.
  loadData();

  // Replace.
  gFindComponent.replaceNext(gFindReplaceData, false);
}

function onReplaceAll()
{
  // Transfer dialog contents to data elements.
  loadData();

  // Replace.
  gFindComponent.replaceNext(gFindReplaceData, true);
}

function doEnabling()
{
  gReplaceDialog.enabled = gReplaceDialog.findKey.value;
  gReplaceDialog.findNext.disabled = !gReplaceDialog.findKey.value;
  gReplaceDialog.replace.disabled = !gReplaceDialog.enabled;
  gReplaceDialog.replaceAll.disabled = !gReplaceDialog.enabled;
}
