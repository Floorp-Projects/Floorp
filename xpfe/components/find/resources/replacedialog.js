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

function string2Bool( value )
{
  return value != "false";
}

function bool2String( value )
{
  if ( value )
    return "true";
  else
    return "false";
}

function initDialog()
{
  // Create gReplaceDialog object and initialize.
  gReplaceDialog = new Object;
  gReplaceDialog.findKey         = document.getElementById("dialog.findKey");
  gReplaceDialog.replaceKey      = document.getElementById("dialog.replaceKey");
  gReplaceDialog.caseSensitive   = document.getElementById("dialog.caseSensitive");
  gReplaceDialog.wrap            = document.getElementById("dialog.wrap");
  gReplaceDialog.searchBackwards = document.getElementById("dialog.searchBackwards");
  gReplaceDialog.findNext        = document.getElementById("findNext");
  gReplaceDialog.replace         = document.getElementById("replace");
  gReplaceDialog.replaceAll      = document.getElementById("replaceAll");
  gReplaceDialog.close           = document.getElementById("close");
  gReplaceDialog.enabled         = false;
}

function loadDialog()
{
  // Set initial dialog field contents.
  gReplaceDialog.findKey.setAttribute( "value", gFindReplaceData.searchString );
  gReplaceDialog.replaceKey.setAttribute( "value", gFindReplaceData.replaceString );

  if ( gFindReplaceData.caseSensitive ) {
    gReplaceDialog.caseSensitive.setAttribute( "checked", "" );
  } else {
    gReplaceDialog.caseSensitive.removeAttribute( "checked" );
  }

  if ( gFindReplaceData.wrapSearch ) {
    gReplaceDialog.wrap.setAttribute( "checked", "" );
  } else {
    gReplaceDialog.wrap.removeAttribute( "checked" );
  }

  if ( gFindReplaceData.searchBackwards ) {
    gReplaceDialog.searchBackwards.setAttribute( "checked", "" );
  } else {
    gReplaceDialog.searchBackwards.removeAttribute( "checked" );
  }
            
  // disable the findNext button if no text
  if (gReplaceDialog.findKey.getAttribute("value") == "") {
    gReplaceDialog.findNext.setAttribute("disabled", "true");
  }

  // disable the replace and replaceAll buttons if no
  // find or replace text
  if (gReplaceDialog.findKey.getAttribute("value") == "") {
    gReplaceDialog.replace.setAttribute("disabled", "true");
    gReplaceDialog.replaceAll.setAttribute("disabled", "true");
  }
  gReplaceDialog.findKey.focus();
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
  initDialog();

  // Get find component.
  gFindComponent = Components.classes[ "@mozilla.org/appshell/component/find;1" ].getService();
  gFindComponent = gFindComponent.QueryInterface( Components.interfaces.nsIFindComponent );
  if ( !gFindComponent ) {
    alert( "Error accessing find component\n" );
    window.close();
    return;
  }

  // Save search context.
  gFindReplaceData = window.arguments[0];

  // Tell search context about this dialog.
  gFindReplaceData.replaceDialog = window;

  // Fill dialog.
  loadDialog();

  // Give focus to search text field.
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
  gFindComponent.findNext( gFindReplaceData );

  return true;
}

function onReplace()
{
  // Transfer dialog contents to data elements.
  loadData();

  // Replace.
  gFindComponent.replaceNext( gFindReplaceData, false );

  return true;
}

function onReplaceAll()
{
  // Transfer dialog contents to data elements.
  loadData();

  // Replace.
  gFindComponent.replaceNext( gFindReplaceData, true );

  return true;
}

function onClose()
{
  window.close();
  return true;
}

function onTyping()
{
  if ( gReplaceDialog.enabled ) {
    // Disable findNext, replace, and replaceAll if they delete all the text.
    if ( gReplaceDialog.findKey.value == "" ) {
      gReplaceDialog.enabled = false;
      gReplaceDialog.findNext.setAttribute("disabled", "true");
    }
  } else {
    // Enable OK once the user types something.
    if ( gReplaceDialog.findKey.value != "" ) {
      gReplaceDialog.enabled = true;
      gReplaceDialog.findNext.removeAttribute("disabled");
    }
  }

  if ( gReplaceDialog.enabled ) {
    gReplaceDialog.replace.removeAttribute("disabled");
    gReplaceDialog.replaceAll.removeAttribute("disabled");
  } else {
    gReplaceDialog.replace.setAttribute("disabled", "true");
    gReplaceDialog.replaceAll.setAttribute("disabled", "true");
  }
}
