/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation. All Rights Reserved.
 */

var toolkit;
var browser;
var dialog;

function onLoad() {
	dialog = new Object;
  //dialog.title     = document.getElementByID( "xpi.process" );
  //dialog.current   = document.getElementByID( "xpi.currentlyprocessing");
	dialog.newWindow   = document.getElementById( "dialog.newWindow" );

	toolkit = XPAppCoresManager.Find( "toolkitCore" );
	if ( !toolkit ) {
		toolkit = new ToolkitCore();
		toolkit.Init( "toolkitCore" );
	}

	browser = XPAppCoresManager.Find( window.arguments[0] );
	if ( !browser ) {
		dump( "unable to get browser app core\n" );
		toolkit.CloseWindow( window );
	}

}

function onTyping( key ) {
   // Look for enter key...
   if ( key == 13 ) {
      // If ok button not disabled, go for it.
      if ( !dialog.ok.disabled ) {
         open();
      }
   } else {
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
   if ( dialog.ok.disabled ) {
      return;
   }

	var url = dialog.input.value;

	browser.loadUrl( url );

	/* Close dialog. */
	toolkit.CloseWindow( window );
}

function cancel() {
    toolkit.CloseWindow( window );
}

