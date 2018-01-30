// -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");

// dialog is just an array we'll use to store various properties from the dialog document...
var dialog;

// the printProgress is a nsIPrintProgress object
var printProgress = null;

// random global variables...
var targetFile;

var docTitle = "";
var docURL   = "";
var progressParams = null;
var switchUI = true;

function ellipseString(aStr, doFront) {
  if (!aStr) {
    return "";
  }

  if (aStr.length > 3 && (aStr.substr(0, 3) == "..." || aStr.substr(aStr.length - 4, 3) == "...")) {
    return aStr;
  }

  var fixedLen = 64;
  if (aStr.length > fixedLen) {
    if (doFront) {
      var endStr = aStr.substr(aStr.length - fixedLen, fixedLen);
      return "..." + endStr;
    }
    var frontStr = aStr.substr(0, fixedLen);
    return frontStr + "...";
  }
  return aStr;
}

// all progress notifications are done through the nsIWebProgressListener implementation...
var progressListener = {
    onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
      if (aStateFlags & Components.interfaces.nsIWebProgressListener.STATE_START) {
        // Put progress meter in undetermined mode.
        // dialog.progress.setAttribute( "value", 0 );
        dialog.progress.setAttribute( "mode", "undetermined" );
      }

      if (aStateFlags & Components.interfaces.nsIWebProgressListener.STATE_STOP) {
        // we are done printing
        // Indicate completion in title area.
        var msg = getString( "printComplete" );
        dialog.title.setAttribute("value", msg);

        // Put progress meter at 100%.
        dialog.progress.setAttribute( "value", 100 );
        dialog.progress.setAttribute( "mode", "normal" );
        var percentPrint = getString( "progressText" );
        percentPrint = replaceInsert( percentPrint, 1, 100 );
        dialog.progressText.setAttribute("value", percentPrint);

        if (Services.focus.activeWindow == window) {
          // This progress dialog is the currently active window. In
          // this case we need to make sure that some other window
          // gets focus before we close this dialog to work around the
          // buggy Windows XP Fax dialog, which ends up parenting
          // itself to the currently focused window and is unable to
          // survive that window going away. What happens without this
          // opener.focus() call on Windows XP is that the fax dialog
          // is opened only to go away when this dialog actually
          // closes (which can happen asynchronously, so the fax
          // dialog just flashes once and then goes away), so w/o this
          // fix, it's impossible to fax on Windows XP w/o manually
          // switching focus to another window (or holding on to the
          // progress dialog with the mouse long enough).
          opener.focus();
        }

        window.close();
      }
    },

    onProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress) {
      if (switchUI) {
        dialog.tempLabel.setAttribute("hidden", "true");
        dialog.progress.setAttribute("hidden", "false");

        var progressLabel = getString("progress");
        if (progressLabel == "") {
          progressLabel = "Progress:"; // better than nothing
        }
        switchUI = false;
      }

      if (progressParams) {
        var docTitleStr = ellipseString(progressParams.docTitle, false);
        if (docTitleStr != docTitle) {
          docTitle = docTitleStr;
          dialog.title.value = docTitle;
        }
        var docURLStr = progressParams.docURL;
        if (docURLStr != docURL && dialog.title != null) {
          docURL = docURLStr;
          if (docTitle == "") {
            dialog.title.value = ellipseString(docURLStr, true);
          }
        }
      }

      // Calculate percentage.
      var percent;
      if ( aMaxTotalProgress > 0 ) {
        percent = Math.round( (aCurTotalProgress * 100) / aMaxTotalProgress );
        if ( percent > 100 )
          percent = 100;

        dialog.progress.removeAttribute( "mode");

        // Advance progress meter.
        dialog.progress.setAttribute( "value", percent );

        // Update percentage label on progress meter.
        var percentPrint = getString( "progressText" );
        percentPrint = replaceInsert( percentPrint, 1, percent );
        dialog.progressText.setAttribute("value", percentPrint);
      } else {
        // Progress meter should be barber-pole in this case.
        dialog.progress.setAttribute( "mode", "undetermined" );
        // Update percentage label on progress meter.
        dialog.progressText.setAttribute("value", "");
      }
    },

    onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
      // we can ignore this notification
    },

    onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {
      if (aMessage != "")
        dialog.title.setAttribute("value", aMessage);
    },

    onSecurityChange(aWebProgress, aRequest, state) {
      // we can ignore this notification
    },

    QueryInterface(iid) {
     if (iid.equals(Components.interfaces.nsIWebProgressListener) || iid.equals(Components.interfaces.nsISupportsWeakReference))
      return this;

     throw Components.results.NS_NOINTERFACE;
    }
};

function getString( stringId ) {
   // Check if we've fetched this string already.
   if (!(stringId in dialog.strings)) {
      // Try to get it.
      var elem = document.getElementById( "dialog.strings." + stringId );
      try {
        if ( elem
           &&
           elem.childNodes
           &&
           elem.childNodes[0]
           &&
           elem.childNodes[0].nodeValue ) {
         dialog.strings[stringId] = elem.childNodes[0].nodeValue;
        } else {
          // If unable to fetch string, use an empty string.
          dialog.strings[stringId] = "";
        }
      } catch (e) { dialog.strings[stringId] = ""; }
   }
   return dialog.strings[stringId];
}

function loadDialog() {
}

function replaceInsert( text, index, value ) {
   var result = text;
   var regExp = new RegExp( "#" + index );
   result = result.replace( regExp, value );
   return result;
}

function onLoad() {

    // Set global variables.
    printProgress = window.arguments[0];
    if (window.arguments[1]) {
      progressParams = window.arguments[1].QueryInterface(Components.interfaces.nsIPrintProgressParams);
      if (progressParams) {
        docTitle = ellipseString(progressParams.docTitle, false);
        docURL   = ellipseString(progressParams.docURL, true);
      }
    }

    if ( !printProgress ) {
        dump( "Invalid argument to printProgress.xul\n" );
        window.close();
        return;
    }

    dialog = {};
    dialog.strings = [];
    dialog.title        = document.getElementById("dialog.title");
    dialog.titleLabel   = document.getElementById("dialog.titleLabel");
    dialog.progress     = document.getElementById("dialog.progress");
    dialog.progressText = document.getElementById("dialog.progressText");
    dialog.progressLabel = document.getElementById("dialog.progressLabel");
    dialog.tempLabel    = document.getElementById("dialog.tempLabel");

    dialog.progress.setAttribute("hidden", "true");

    var progressLabel = getString("preparing");
    if (progressLabel == "") {
      progressLabel = "Preparing..."; // better than nothing
    }
    dialog.tempLabel.value = progressLabel;

    dialog.title.value = docTitle;

    // Fill dialog.
    loadDialog();

    // set our web progress listener on the helper app launcher
    printProgress.registerListener(progressListener);
    // We need to delay the set title else dom will overwrite it
    window.setTimeout(doneIniting, 500);
}

function onUnload() {
  if (printProgress) {
   try {
     printProgress.unregisterListener(progressListener);
     printProgress = null;
   } catch ( exception ) {}
  }
}

// If the user presses cancel, tell the app launcher and close the dialog...
function onCancel() {
  // Cancel app launcher.
   try {
     printProgress.processCanceledByUser = true;
   } catch ( exception ) { return true; }

  // don't Close up dialog by returning false, the backend will close the dialog when everything will be aborted.
  return false;
}

function doneIniting() {
  printProgress.doneIniting();
}
