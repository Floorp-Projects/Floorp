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
 */

// dialog is just an array we'll use to store various properties from the dialog document...
var dialog;

// the helperAppLoader is a nsIHelperAppLauncher object
var helperAppLoader; 

// random global variables...
var started   = false;
var completed = false;
var startTime;
// since this progress dialog is brought up after we've already started downloading, we need to record the # bytes already 
// downloaded before we started showing progress. This is used to make sure our time remaining calculation works correctly...
var initialByteOffset;
var elapsed;
var interval = 1000; // Update every 1000 milliseconds.
var lastUpdate = -interval; // Update initially.

// These are to throttle down the updating of the download rate figure.
var priorRate = 0;
var rateChanges = 0;
var rateChangeLimit = 2;

var warningLimit = 30000; // Warn on Cancel after 30 sec (30000 milleseconds).

// all progress notifications are done through our nsIWebProgressListener implementation...
var progressListener = {
    onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus)
    {
      if (aStateFlags & Components.interfaces.nsIWebProgressListener.STATE_STOP)
      {
        // we are done downloading...
        completed = true;
        // Indicate completion in status area.
        var msg = getString( "completeMsg" );
        msg = replaceInsert( msg, 1, formatSeconds( elapsed/1000 ) );
        dialog.status.setAttribute("value", msg);

        // Put progress meter at 100%.
        dialog.progress.setAttribute( "value", 100 );
        dialog.progress.setAttribute( "mode", "normal" );
        try {
          // Close the window in 2 seconds (to ensure user sees we're done).
          window.setTimeout( "window.close();", 2000 );
        } 
        catch ( exception ) 
        {
          dump( "Error setting close timeout\n" );
          // OK, try to just close the window immediately.
          window.close();
          // If that's not working either, change button text to give user a clue.
          dialog.cancel.childNodes[0].nodeValue = "Close";
        }
      }
    },
    
    onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress)
    {
      // Check for first time.
      if ( !started ) 
      {
        // Initialize download start time.
        started = true;
        startTime = ( new Date() ).getTime();
        initialByteOffset = aCurTotalProgress;
      }

      var overallProgress = aCurTotalProgress;

      aCurTotalProgress -= initialByteOffset;

      // Get current time.
      var now = ( new Date() ).getTime();
      // If interval hasn't elapsed, ignore it.
      if ( now - lastUpdate < interval && aMaxTotalProgress != "-1" &&  eval(aCurTotalProgress) < eval(aMaxTotalProgress) ) 
        return;

      // Update this time.
      lastUpdate = now;

      // Update download rate.
      elapsed = now - startTime;
      var rate; // aCurTotalProgress/sec
      if ( elapsed )
        rate = ( aCurTotalProgress * 1000 ) / elapsed;
      else 
        rate = 0;

      // Update elapsed time display.
      dialog.timeElapsed.setAttribute("value", formatSeconds( elapsed / 1000 ));

      // Calculate percentage.
      var percent;
      if ( aMaxTotalProgress != "-1" ) 
      {
        percent = parseInt( (overallProgress*100)/aMaxTotalProgress + .5 );
        if ( percent > 100 )
          percent = 100;
        
        // Advance progress meter.
        dialog.progress.setAttribute( "value", percent );
      } 
      else 
      {
        percent = "??";

        // Progress meter should be barber-pole in this case.
        dialog.progress.setAttribute( "mode", "undetermined" );
      }

      // now that we've set the progress and the time, update # bytes downloaded...
      // Update status (nnK of mmK bytes at xx.xK aCurTotalProgress/sec)
      var status = getString( "progressMsg" );

      // Insert 1 is the number of kilobytes downloaded so far.
      status = replaceInsert( status, 1, parseInt( overallProgress/1024 + .5 ) );

      // Insert 2 is the total number of kilobytes to be downloaded (if known).
      if ( aMaxTotalProgress != "-1" )
         status = replaceInsert( status, 2, parseInt( aMaxTotalProgress/1024 + .5 ) );
      else 
         status = replaceInsert( status, 2, "??" );

      if ( rate ) 
      {
        // rate is bytes/sec
        var kRate = rate / 1024; // K bytes/sec;
        kRate = parseInt( kRate * 10 + .5 ); // xxx (3 digits)
        // Don't update too often!
        if ( kRate != priorRate ) 
        {
          if ( rateChanges++ == rateChangeLimit ) 
          {
             // Time to update download rate.
             priorRate = kRate;
             rateChanges = 0;
          } 
          else 
          {
            // Stick with old rate for a bit longer.
            kRate = priorRate;
          }
        }
        else
          rateChanges = 0;

         var fraction = kRate % 10;
         kRate = parseInt( ( kRate - fraction ) / 10 );

         // Insert 3 is the download rate (in kilobytes/sec).
         status = replaceInsert( status, 3, kRate + "." + fraction );
      } 
      else
       status = replaceInsert( status, 3, "??.?" );

      // Update status msg.
      dialog.status.setAttribute("value", status);

      // Update percentage label on progress meter.
      var percentMsg = getString( "percentMsg" );
      percentMsg = replaceInsert( percentMsg, 1, percent );
      dialog.progress.progresstext = percentMsg;

      // Update time remaining.
      if ( rate && aMaxTotalProgress != "-1" ) 
      {
        var rem = ( aMaxTotalProgress - aCurTotalProgress ) / rate;
            rem = parseInt( rem + .5 );
            dialog.timeLeft.setAttribute("value", formatSeconds( rem ));
      }
      else
            dialog.timeLeft.setAttribute("value", getString( "unknownTime" ));
    },
	  onLocationChange: function(aWebProgress, aRequest, aLocation)
    {
      // we can ignore this notification
    },
    onStatusChange: function(aWebProgress, aRequest, aStatus, aMessage)
    {
      if (aMessage)
        dialog.status.setAttribute("value", status);
    },
    onSecurityChange: function(aWebProgress, aRequest, state)
    {
    }
};

function getString( stringId ) {
   // Check if we've fetched this string already.
   if ( !dialog.strings[ stringId ] ) {
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

function formatSeconds( secs ) 
{
  // Round the number of seconds to remove fractions.
  secs = parseInt( secs + .5 );
  var hours = parseInt( secs/3600 );
  secs -= hours*3600;
  var mins = parseInt( secs/60 );
  secs -= mins*60;
  var result;
  if ( hours ) 
    result = getString( "longTimeFormat" );
  else 
    result = getString( "shortTimeFormat" );
  
  if ( hours < 10 ) 
     hours = "0" + hours;
  if ( mins < 10 )
     mins = "0" + mins;
  if ( secs < 10 )
     secs = "0" + secs;
 
  // Insert hours, minutes, and seconds into result string.
  result = replaceInsert( result, 1, hours );
  result = replaceInsert( result, 2, mins );
  result = replaceInsert( result, 3, secs );

  return result;
}

function loadDialog() {
  var sourceUrlValue = {};
  var targetFile =  helperAppLoader.getDownloadInfo(sourceUrlValue);
  var sourceUrl = sourceUrlValue.value;

  dialog.location.setAttribute("value", sourceUrlValue.value.spec );
  dialog.fileName.setAttribute( "value", targetFile.unicodePath );
}

function replaceInsert( text, index, value ) {
   var result = text;
   var regExp = eval( "/#"+index+"/" );
   result = result.replace( regExp, value );
   return result;
}

function onLoad() {
    // Set global variables.
    helperAppLoader = window.arguments[0];

    if ( !helperAppLoader ) {
        dump( "Invalid argument to downloadProgress.xul\n" );
        window.close()
        return;
    }

    dialog = new Object;
    dialog.strings = new Array;
    dialog.location    = document.getElementById("dialog.location");
    dialog.contentType = document.getElementById("dialog.contentType");
    dialog.fileName    = document.getElementById("dialog.fileName");
    dialog.status      = document.getElementById("dialog.status");
    dialog.progress    = document.getElementById("dialog.progress");
    dialog.timeLeft    = document.getElementById("dialog.timeLeft");
    dialog.timeElapsed = document.getElementById("dialog.timeElapsed");
    dialog.cancel      = document.getElementById("dialog.cancel");

    // Set up dialog button callbacks.
    var object = this;
    doSetOKCancel("", function () { return object.onCancel();});

    // Fill dialog.
    loadDialog();

    // set our web progress listener on the helper app launcher
    helperAppLoader.setWebProgressListener(progressListener);
    moveToAlertPosition();
}

function onUnload() 
{
   // Cancel app launcher.
   if (helperAppLoader)
   {
     try 
     {
       helperAppLoader.closeProgressWindow();
       helperAppLoader = null;
     }
      
     catch( exception ) {}
   }
}

// If the user presses cancel, tell the app launcher and close the dialog...
function onCancel () 
{
   // Cancel app launcher.
   if (helperAppLoader)
   {
     try 
     {
       helperAppLoader.Cancel();
     }
      
     catch( exception ) {}
   }
    
  // Close up dialog by returning true.
  return true;
}
