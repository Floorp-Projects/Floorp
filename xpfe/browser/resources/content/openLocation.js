/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Contributor(s): Michael Lowe <michael.lowe@bigfoot.com>
 */

var browser;
var dialog;

function onLoad() {
	dialog = new Object;
	dialog.input     = document.getElementById( "dialog.input" );
	dialog.help      = document.getElementById( "dialog.help" );
	dialog.topWindow = document.getElementById( "dialog.topWindow" );
	dialog.topWindowDiv = document.getElementById( "dialog.topWindowDiv" );
	dialog.newWindow = document.getElementById( "dialog.newWindow" );
	dialog.newWindowDiv = document.getElementById( "dialog.newWindowDiv" );
	dialog.editNewWindow = document.getElementById( "dialog.editNewWindow" );
  dialog.open            = document.getElementById("ok");
	dialog.openWhereBox = document.getElementById( "dialog.openWhereBox" );


	browser = window.arguments[0];

  if ( !browser ) {
    // No browser supplied - we are calling from Composer
    dialog.topWindow.checked = false;
    dialog.editNewWindow.checked = true;

    dialog.topWindowDiv.setAttribute("style","display:none;");
    var pNode = dialog.newWindowDiv.parentNode;
    pNode.removeChild(dialog.newWindowDiv);
    pNode.appendChild(dialog.newWindowDiv);
  } else {
    dialog.topWindow.checked = true;
  }
	doSetOKCancel(open, 0, 0, 0);

	moveToAlertPosition();
	/* Give input field the focus. */
	dialog.input.focus();

  // change OK to load
  dialog.open.setAttribute("value", document.getElementById("openLabel").getAttribute("value"));
}

function onTyping( key ) {
   // Look for enter key...
   if ( key != 13 ) {
      // Check for valid input.
      if ( dialog.input.value == "" ) {
         // No input, disable ok button if enabled.
         if ( !dialog.open.disabled ) {
            dialog.open.setAttribute( "disabled", "" );
         }
      } else {
         // Input, enable ok button if disabled.
         if ( dialog.open.disabled ) {
            dialog.open.removeAttribute( "disabled" );
         }
      }
   }
}

function open() {
	if ( dialog.open.disabled || dialog.input.value == "" ) {
		return false;
	}

	var url = dialog.input.value;
	    
	try {
	    if ( dialog.topWindow.checked ) {
	        // Open the URL.
	        browser.loadUrl( url );
	    } else if ( dialog.newWindow.checked ) {
		    /* User wants new window. */
            window.opener.openDialog( "chrome://navigator/content/navigator.xul", "_blank", "all,dialog=no", url );
	    } else if ( dialog.editNewWindow.checked ) {
            window.opener.openDialog( "chrome://editor/content", "_blank", "chrome,all,dialog=no", url );
        }
    } catch( exception ) {
	    // XXX l10n
	    alert( "Error opening location." );
	    return false;
	}

   // Delay closing slightly to avoid timing bug on Linux.
   window.setTimeout( "window.close()", 10 );
   return false;
}

function createInstance( progid, iidName ) {
  var iid = eval( "Components.interfaces." + iidName );
  return Components.classes[ progid ].createInstance( iid );
}

function onChooseFile() {
    // Get filespecwithui component.            
    var fileSpec = createInstance( "component://netscape/filespecwithui", "nsIFileSpecWithUI" );
    try {
        fileSpec.parentWindow = window;
        var url = fileSpec.chooseFile( document.getElementById("chooseFileTitle").getAttribute("value") );
        fileSpec.parentWindow = null;
        dialog.input.value = fileSpec.URLString;
    }
    catch( exception ) {
        // Just a cancel, probably.
    }
}