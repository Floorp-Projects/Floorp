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
var bundle;

function onLoad() 
  {

    bundle = srGetStrBundle( "chrome://navigator/locale/openLocation.properties" );
  
  	dialog                = new Object;
  	dialog.input          = document.getElementById( "dialog.input" );
  	dialog.help           = document.getElementById( "dialog.help" );
    dialog.open           = document.getElementById( "ok" );
    dialog.openAppList    = document.getElementById( "openAppList" );
    dialog.openTopWindow  = document.getElementById( "currentWindow" );
    dialog.openEditWindow = document.getElementById( "editWindow" );
  
  
  	browser = window.arguments[0];
  
    if ( !browser ) 
      {
        // No browser supplied - we are calling from Composer
        dialog.openAppList.selectedItem = dialog.openEditWindow;
        dialog.openTopWindow.disabled = true;
      } 
    else
      dialog.openAppList.selectedItem = dialog.openTopWindow;
  

    // change OK button text to 'open'
    dialog.open.setAttribute("value", bundle.GetStringFromName( "openButtonLabel" ));

  	doSetOKCancel(open, 0, 0, 0);
  
    doEnabling();
    
  	/* Give input field the focus. */
  	dialog.input.focus();
  
  }

function doEnabling() 
  {
    if ( dialog.input.value == "" ) 
      {
        // No input, disable ok button if enabled.
        if ( !dialog.open.getAttribute("disabled") )
          dialog.open.setAttribute("disabled","true");
      }
    else
      {
        if ( dialog.open.getAttribute("disabled") )
          dialog.open.removeAttribute( "disabled" );
      }
  }

function open() 
  {
  	try 
      {
        switch ( dialog.openAppList.data ) 
          {
            case "0":
              browser.loadUrl( dialog.input.value );
              break;
            case "1": 
              dump("*** foopy\n");
              window.opener.delayedOpenWindow( "chrome://navigator/content/navigator.xul", "all,dialog=no", dialog.input.value );
              break;
            case "2":
              window.opener.delayedOpenWindow( "chrome://editor/content", "chrome,all,dialog=no", dialog.input.value );
              break;
          }
      }
    catch( exception ) 
      {
    	}
  
     // Delay closing slightly to avoid timing bug on Linux.
     window.close();
     return false;
  }

function createInstance( progid, iidName ) 
  {
    var iid = Components.interfaces[iidName];
    return Components.classes[ progid ].createInstance( iid );
  }

const nsIFilePicker = Components.interfaces.nsIFilePicker;
function onChooseFile() 
{
  try {
    var fp = Components.classes["component://mozilla/filepicker"].createInstance(nsIFilePicker);
    fp.init(window, bundle.GetStringFromName("chooseFileDialogTitle"), nsIFilePicker.modeOpen);
    fp.setFilters(nsIFilePicker.filterAll);
    fp.show();
    dialog.input.value = fp.file.path;
  } catch(ex) { }
}
