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
 * Contributor(s): 
 */

var browser;
var dialog;

function onLoad() {
	dialog = new Object;
	dialog.input     = document.getElementById( "dialog.input" );
	dialog.ok        = document.getElementById( "ok" );
	dialog.help      = document.getElementById( "dialog.help" );
	dialog.newWindow = document.getElementById( "dialog.newWindow" );

	browser = window.arguments[0];

	if ( !browser ) {
		dump( "No browser instance provided!\n" );
		dialog.newWindow.checked = true;
		dialog.newWindow.disabled = true;
	//	window.close();
    //    return;
	}
	doSetOKCancel(open, 0, 0, 0);

	moveToAlertPosition();
	/* Give input field the focus. */
	dialog.input.focus();
}

function onTyping( key ) {
   // Look for enter key...
   if ( key != 13 ) {
      // Check for valid input.
      if ( dialog.input.value == "" ) {
         // No input, disable ok button if enabled.
         if ( !dialog.ok.disabled ) {
            dialog.ok.setAttribute( "disabled", "" );
         }
      } else {
         // Input, enable ok button if disabled.
         if ( dialog.ok.disabled ) {
            dialog.ok.removeAttribute( "disabled" );
         }
      }
   }
}

function open() {
	if ( dialog.ok.disabled || dialog.input.value == "" ) {
		return false;
	}

	var url = dialog.input.value;

	if ( !dialog.newWindow.checked ) {
		/* Load url in opener. */
		browser.loadUrl( url );
	} else {
		/* User wants new window. */
        window.opener.openDialog( "chrome://navigator/content/navigator.xul", "_blank", "all,dialog=no", url );
	}

	return true;
}
