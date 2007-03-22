/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   William A. ("PowerGUI") Law <law@netscape.com>
 *   Scott MacGregor <mscott@netscape.com>
 *   jean-Francois Ducarroz <ducarroz@netscape.com>
 *   Rod Spears <rods@netscape.com>
 *   Karsten "Mnyromyr" Düsterloh <mnyromyr@tprac.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// deck index constants
const TITLE_COMPLETE_DECK = 1;
const PROGRESS_METER_DECK = 1;


// global variables
var dialog;                 // associative array with various properties from the dialog document
var percentFormat;          // format string for percent value
var printProgress  = null;  // nsIPrintProgress
var progressParams = null;  // nsIPrintProgressParams
var switchUI       = true;  // switch UI on first progress change


// all progress notifications are done through the nsIWebProgressListener implementation...
var progressListener =
{
  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus)
  {
    if (aStateFlags & Components.interfaces.nsIWebProgressListener.STATE_START)
    {
      // put progress meter in undetermined mode
      setProgressPercentage(-1);
    }
    if (aStateFlags & Components.interfaces.nsIWebProgressListener.STATE_STOP)
    {
      // we are done printing; indicate completion in title area
      dialog.titleDeck.selectedIndex = TITLE_COMPLETE_DECK;
      setProgressPercentage(100);
      window.close();
    }
  },

  onProgressChange: function(aWebProgress,      aRequest,
                             aCurSelfProgress,  aMaxSelfProgress,
                             aCurTotalProgress, aMaxTotalProgress)
  {
    if (switchUI)
    {
      // first progress change: show progress meter
      dialog.progressDeck.selectedIndex = PROGRESS_METER_DECK;
      dialog.cancel.removeAttribute("disabled");
      switchUI = false;
    }
    setProgressTitle();

    // calculate percentage and update progress meter
    if (aMaxTotalProgress > 0)
    {
      var percentage = Math.round(aCurTotalProgress * 100 / aMaxTotalProgress);
      setProgressPercentage(percentage);
    }
    else
    {
      // progress meter should be barber-pole in this case
      setProgressPercentage(-1);
    }
  },

  onLocationChange: function(aWebProgress, aRequest, aLocation)
  {
    // we can ignore this notification
  },

  onStatusChange: function(aWebProgress, aRequest, aStatus, aMessage)
  {
    if (aMessage)
      dialog.title.value = aMessage;
  },

  onSecurityChange: function(aWebProgress, aRequest, aStatus)
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


function setProgressTitle()
{
  if (progressParams)
  {
    dialog.title.crop  = progressParams.docTitle ? "end" : "center";
    dialog.title.value = progressParams.docTitle || progressParams.docURL;
  }
}


function setProgressPercentage(aPercentage)
{
  // set percentage as text if non-negative
  if (aPercentage < 0)
  {
    dialog.progress.mode = "undetermined";
    dialog.progressText.value = "";
  }
  else
  {
    dialog.progress.removeAttribute("mode");
    dialog.progress.value = aPercentage;
    dialog.progressText.value = percentFormat.replace("#1", aPercentage);
  }
}


function onLoad()
{
  // set global variables
  printProgress = window.arguments[0];
  if (!printProgress)
  {
    dump("Invalid argument to printProgress.xul\n");
    window.close();
    return;
  }

  dialog = {
    titleDeck   : document.getElementById("dialog.titleDeck"),
    title       : document.getElementById("dialog.title"),
    progressDeck: document.getElementById("dialog.progressDeck"),
    progress    : document.getElementById("dialog.progress"),
    progressText: document.getElementById("dialog.progressText"),
    cancel      : document.documentElement.getButton("cancel")
  };
  percentFormat = dialog.progressText.getAttribute("basevalue");
  // disable the cancel button until first progress is made
  dialog.cancel.setAttribute("disabled", "true");

  // XXX we could probably get rid of this test, it should never fail
  if (window.arguments.length > 1 && window.arguments[1])
  {
    progressParams = window.arguments[1].QueryInterface(Components.interfaces.nsIPrintProgressParams);
    setProgressTitle();
  }

  // set our web progress listener on the helper app launcher
  printProgress.registerListener(progressListener);
  printProgress.doneIniting();
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
    catch(ex){}
  }
}


// If the user presses cancel, tell the app launcher and close the dialog...
function onCancel()
{
  // cancel app launcher
  try
  {
    printProgress.processCanceledByUser = true;
  }
  catch(ex)
  {
    return true;
  }
  // don't close dialog by returning false, the backend will close the dialog
  // when everything will be aborted.
  return false;
}
