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

var prefContractID             = "@mozilla.org/preferences;1";
var externalProtocolServiceID = "@mozilla.org/uriloader/external-protocol-service;1";

// dialog is just an array we'll use to store various properties from the dialog document...
var dialog;

// the helperAppLoader is a nsIHelperAppLauncher object
var helperAppLoader;

// random global variables...
var completed = false;
var startTime = 0;
var elapsed = 0;
var interval = 500; // Update every 500 milliseconds.
var lastUpdate = -interval; // Update initially.
var keepProgressWindowUpBox;
var targetFile;
var gRestartChecked = false;

// These are to throttle down the updating of the download rate figure.
var priorRate = 0;
var rateChanges = 0;
var rateChangeLimit = 2;

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
        var percentMsg = getString( "percentMsg" );
        percentMsg = replaceInsert( percentMsg, 1, 100 );
        dialog.progressText.setAttribute("label", percentMsg);

        processEndOfDownload();
      }
    },

    onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress)
    {

      if (!gRestartChecked) 
      {
        gRestartChecked = true;
        try 
        {
          // right now, all that supports restarting downloads is ftp (rfc959)    
          ftpChannel = aRequest.QueryInterface(Components.interfaces.nsIFTPChannel);
          if (ftpChannel) {
            dialog.pauseResumeDeck.setAttribute("selectedIndex", "1");
          }
        }
        catch (ex) {}
      }

      // this is so that we don't clobber the status text if 
      // a onProgressChange event happens after we press the pause btn.
      if (dialog.downloadPaused) 
      {
        dialog.status.setAttribute("value", getString("pausedMsg"));
      }

      dialog.request = aRequest;  

      var overallProgress = aCurTotalProgress;

      // Get current time.
      var now = ( new Date() ).getTime();
      // If interval hasn't elapsed, ignore it.
      if ( now - lastUpdate < interval && aMaxTotalProgress != "-1" &&  parseInt(aCurTotalProgress) < parseInt(aMaxTotalProgress) )
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
      if ( aMaxTotalProgress > 0)
      {
        percent = Math.floor((overallProgress*100.0)/aMaxTotalProgress);
        if ( percent > 100 )
          percent = 100;

        // Advance progress meter.
        dialog.progress.setAttribute( "value", percent );
      }
      else
      {
        percent = -1;

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
      if (percent < 0)
        dialog.progressText.setAttribute("value", "");
      else
      {
        var percentMsg = getString( "percentMsg" );      
        percentMsg = replaceInsert( percentMsg, 1, percent );
        dialog.progressText.setAttribute("value", percentMsg);
      }

      // Update time remaining.
      if ( rate && (aMaxTotalProgress > 0) )
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
    },
    onSecurityChange: function(aWebProgress, aRequest, state)
    {
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

function loadDialog()
{
  var sourceUrlValue = {};
  var initialDownloadTimeValue = {};
  // targetFile is global because we are going to want re-use later one...
  targetFile =  helperAppLoader.getDownloadInfo(sourceUrlValue, initialDownloadTimeValue);

  var sourceUrl = sourceUrlValue.value;
  startTime = initialDownloadTimeValue.value / 1000;

  // set the elapsed time on the first pass...
  var now = ( new Date() ).getTime();
  // intialize the elapsed time global variable slot
  elapsed = now - startTime;
  // Update elapsed time display.
  dialog.timeElapsed.setAttribute("value", formatSeconds( elapsed / 1000 ));
  dialog.timeLeft.setAttribute("value", formatSeconds( 0 ));

  dialog.location.setAttribute("value", sourceUrl.spec );
  dialog.fileName.setAttribute( "value", targetFile.unicodePath );

  var prefs = Components.classes[prefContractID].getService(Components.interfaces.nsIPref);
  if (prefs)
    keepProgressWindowUpBox.checked = prefs.GetBoolPref("browser.download.progressDnldDialog.keepAlive");
}

function replaceInsert( text, index, value ) {
   var result = text;
   var regExp = new RegExp( "#"+index );
   result = result.replace( regExp, value );
   return result;
}

function onLoad() {
    // Set global variables.
    helperAppLoader = window.arguments[0].QueryInterface( Components.interfaces.nsIHelperAppLauncher );

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
    dialog.progressText = document.getElementById("dialog.progressText");
    dialog.timeLeft    = document.getElementById("dialog.timeLeft");
    dialog.timeElapsed = document.getElementById("dialog.timeElapsed");
    dialog.cancel      = document.getElementById("cancel");
    dialog.pause       = document.getElementById("pause");
    dialog.resume      = document.getElementById("resume");
    dialog.pauseResumeDeck = document.getElementById("pauseResumeDeck");
    dialog.request     = 0;
    dialog.downloadPaused = false;
    keepProgressWindowUpBox = document.getElementById('keepProgressDialogUp');

    // Set up dialog button callbacks.
    var object = this;
    doSetOKCancel("", function () { return object.onCancel();});

    // Fill dialog.
    loadDialog();

    // set our web progress listener on the helper app launcher
    helperAppLoader.setWebProgressListener(progressListener);

    if ( window.opener ) {
        moveToAlertPosition();
    } else {
        centerWindowOnScreen();
    }
    
    // Set initial focus
    if ( !completed )
    {
        keepProgressWindowUpBox.focus();
    }
}

function onUnload()
{

  // remember the user's decision for the checkbox.

  var prefs = Components.classes[prefContractID].getService(Components.interfaces.nsIPref);
  if (prefs)
    prefs.SetBoolPref("browser.download.progressDnldDialog.keepAlive", keepProgressWindowUpBox.checked);

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
   if (helperAppLoader && !completed)
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

// closeWindow should only be called from processEndOfDownload
function closeWindow()
{
  // while the time out was fired the user may have checked the
  // keep this dialog open box...so we should abort and not actually
  // close the window.

  if (!keepProgressWindowUpBox.checked)
    window.close();
  else
    setupPostProgressUI();
}

function setupPostProgressUI()
{
  //dialog.cancel.childNodes[0].nodeValue = "Close";
  // turn the cancel button into a close button
  var cancelButton = document.getElementById('cancel');
  if (cancelButton)
  {
    cancelButton.label = getString("close");
    cancelButton.focus();
  }

  // enable the open and open folder buttons
  var openFolderButton = document.getElementById('openFolder');
  var openButton = document.getElementById('open');

  openFolderButton.removeAttribute("disabled");
  if ( !targetFile.isExecutable() )
  {
    openButton.removeAttribute("disabled");
  }

  dialog.pause.disabled = true; // setAttribute("disabled", true);
  dialog.resume.disabled = true;
  
}

// when we receive a stop notification we are done reporting progress on the download
// now we have to decide if the window is supposed to go away or if we are supposed to remain open
// and enable the open and open folder buttons on the dialog.
function processEndOfDownload()
{
  if (!keepProgressWindowUpBox.checked)
    closeWindow(); // shut down, we are all done.

  // o.t the user has asked the window to stay open so leave it open and enable the open and open new folder buttons
  setupPostProgressUI();
}

function doOpen()
{
  try {
    var localFile = targetFile.QueryInterface(Components.interfaces.nsILocalFile);
    if (localFile)
      localFile.launch();
    window.close();
  } catch (ex) {}
}

function doOpenFolder()
{
  try {
    var localFile = targetFile.QueryInterface(Components.interfaces.nsILocalFile);
    if (localFile)
      localFile.reveal();
    window.close();
  } catch (ex) {}
}

function doPauseButton() {
    if (dialog.downloadPaused)
    {
        // resume
        dialog.downloadPaused = false;
        dialog.pauseResumeDeck.setAttribute("selectedIndex", "1");
        dialog.request.resume()
    }
    else
    {
        // suspend
        dialog.downloadPaused = true;
        dialog.pauseResumeDeck.setAttribute("selectedIndex", "2");
        dialog.request.suspend()
    }
}
