/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     William A. ("PowerGUI") Law <law@netscape.com>
 *     Scott MacGregor <mscott@netscape.com>
 *     jean-Francois Ducarroz <ducarroz@netscape.com>
 *     Rod Spears <rods@netscape.com>
 */

// dialog is just an array we'll use to store various properties from the dialog document...
var dialog;

// the printProgress is a nsIPrintProgress object
var printProgress = null; 

// random global variables...
var targetFile;

var docTitle = "";
var docURL   = "";
var progressParams = null;

// all progress notifications are done through the nsIWebProgressListener implementation...
var progressListener = {
    onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus)
    {
      if (aStateFlags & Components.interfaces.nsIWebProgressListener.STATE_START)
      {
        // Put progress meter in undetermined mode.
        // dialog.progress.setAttribute( "value", 0 );
        dialog.progress.setAttribute( "mode", "undetermined" );
      }
      
      if (aStateFlags & Components.interfaces.nsIWebProgressListener.STATE_STOP)
      {
        // we are done sending/saving the message...
        // Indicate completion in status area.
        var msg = getString( "printComplete" );
        dialog.status.setAttribute("value", msg);

        // Put progress meter at 100%.
        dialog.progress.setAttribute( "value", 100 );
        dialog.progress.setAttribute( "mode", "normal" );
        var percentPrint = getString( "progressText" );
        percentPrint = replaceInsert( percentPrint, 1, 100 );
        dialog.progressText.setAttribute("value", percentPrint);

        window.close();
      }
    },
    
    onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress)
    {
      if (progressParams)
      {
        var docTitleStr = progressParams.docTitle;
        if (docTitleStr != docTitle) {
          SetTitle(docTitleStr);
          docTitle = docTitleStr;
        }
        var docURLStr = progressParams.docURL;
        if (docURLStr != docURL && dialog.status != null) {
          dialog.status.value = docURLStr;
          docURL = docURLStr;
        }
      }

      // Calculate percentage.
      var percent;
      if ( aMaxTotalProgress > 0 ) 
      {
        percent = parseInt( (aCurTotalProgress*100)/aMaxTotalProgress + .5 );
        if ( percent > 100 )
          percent = 100;
        
        dialog.progress.removeAttribute( "mode");
        
        // Advance progress meter.
        dialog.progress.setAttribute( "value", percent );

        // Update percentage label on progress meter.
        var percentPrint = getString( "progressText" );
        percentPrint = replaceInsert( percentPrint, 1, percent );
        dialog.progressText.setAttribute("value", percentPrint);
      } 
      else 
      {
        // Progress meter should be barber-pole in this case.
        dialog.progress.setAttribute( "mode", "undetermined" );
        // Update percentage label on progress meter.
        dialog.progressText.setAttribute("value", "");
      }
    },

	  onLocationChange: function(aWebProgress, aRequest, aLocation)
    {
      // we can ignore this notification
    },

    onStatusChange: function(aWebProgress, aRequest, aStatus, aMessage)
    {
      if (aMessage != "")
        dialog.status.setAttribute("value", aMessage);
    },

    onSecurityChange: function(aWebProgress, aRequest, state)
    {
      // we can ignore this notification
    },

    QueryInterface : function(iid)
    {
     if (iid.equals(Components.interfaces.nsIWebProgressListener) || iid.equals(Components.interfaces.nsISupportsWeakReference))
      return this;
     
     throw Components.results.NS_NOINTERFACE;
    }
};

function getString( stringId ) {
   // Check if we've fetched this string already.
   if (!(stringId in dialog.strings)) {
      // Try to get it.
      var elem = document.getElementById( "dialog.strings."+stringId );
      try {
        if ( elem
           &&
           elem.childNodes
           &&
           elem.childNodes[0]
           &&
           elem.childNodes[0].nodeValue ) {
         dialog.strings[ stringId ] = elem.childNodes[0].nodeValue;
        } else {
          // If unable to fetch string, use an empty string.
          dialog.strings[ stringId ] = "";
        }
      } catch (e) { dialog.strings[ stringId ] = ""; }
   }
   return dialog.strings[ stringId ];
}

function loadDialog() 
{
}

function replaceInsert( text, index, value ) {
   var result = text;
   var regExp = new RegExp( "#"+index );
   result = result.replace( regExp, value );
   return result;
}

function onLoad() {
    // Set global variables.
    printProgress = window.arguments[0];
    if (window.arguments[1])
    {
      progressParams = window.arguments[1].QueryInterface(Components.interfaces.nsIPrintProgressParams)
      if (progressParams)
      {
        docTitle = progressParams.docTitle;
        docURL = progressParams.docURL;
      }
    }

    if ( !printProgress ) {
        dump( "Invalid argument to downloadProgress.xul\n" );
        window.close()
        return;
    }

    dialog = new Object;
    dialog.strings = new Array;
    dialog.status       = document.getElementById("dialog.status");
    dialog.progress     = document.getElementById("dialog.progress");
    dialog.progressText = document.getElementById("dialog.progressText");
    dialog.cancel       = document.getElementById("cancel");

    dialog.status.value = docURL;

    // Set up dialog button callbacks.
    var object = this;
    doSetOKCancel("", function () { return object.onCancel();});

    // Fill dialog.
    loadDialog();

    // set our web progress listener on the helper app launcher
    printProgress.registerListener(progressListener);
    moveToAlertPosition();

    //We need to delay the set title else dom will overwrite it
    window.setTimeout(SetTitle, 0, docTitle);
}

function onUnload() 
{
  if (printProgress)
  {
   try 
   {
     printProgress.unregisterListener(progressListener);
     printProgress = null;
   }
    
   catch( exception ) {}
  }
}

function SetTitle(str)
{
  var prefix = getString("titlePrefixPrint");
  window.title = prefix + " " + str;
}

// If the user presses cancel, tell the app launcher and close the dialog...
function onCancel () 
{
  // Cancel app launcher.
   try 
   {
     printProgress.processCanceledByUser = true;
   }
   catch( exception ) {return true;}
    
  // don't Close up dialog by returning false, the backend will close the dialog when everything will be aborted.
  return false;
}
