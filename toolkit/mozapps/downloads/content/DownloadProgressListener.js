# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is Mozilla.org Code.
# 
# The Initial Developer of the Original Code is
# Doron Rosenberg.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Blake Ross <blakeross@telocity.com> (Original Author) 
#   Ben Goodger <ben@bengoodger.com> (v2.0) 
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK *****

var gStrings = [];
const interval = 500; // Update every 500 milliseconds.

function DownloadProgressListener (aDocument, aStringBundle) 
{
  this.doc = aDocument;

  this._statusFormat = aStringBundle.getString("statusFormat");
  this._transferSameUnits = aStringBundle.getString("transferSameUnits");
  this._transferDiffUnits = aStringBundle.getString("transferDiffUnits");
  this._transferNoTotal = aStringBundle.getString("transferNoTotal");
  this._timeLeft = aStringBundle.getString("timeLeft");
  this._timeLessMinute = aStringBundle.getString("timeLessMinute");
  this._timeUnknown = aStringBundle.getString("timeUnknown");
  this._units = [aStringBundle.getString("bytes"),
                 aStringBundle.getString("kilobyte"),
                 aStringBundle.getString("megabyte"),
                 aStringBundle.getString("gigabyte")];
}

DownloadProgressListener.prototype = 
{
  doc: null,

  onDownloadStateChange: function dlPL_onDownloadStateChange(aState, aDownload)
  {
    var downloadID = "dl" + aDownload.id;
    var download = this.doc.getElementById(downloadID);
    if (download)
      download.setAttribute("state", aDownload.state);
  },

  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus, aDownload)
  {
    if (aStateFlags & Components.interfaces.nsIWebProgressListener.STATE_STOP) {
      var downloadID = "dl" + aDownload.id;
      var download = this.doc.getElementById(downloadID);
      if (download)
        download.setAttribute("status", "");
    }
  },

  onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress,
                              aCurTotalProgress, aMaxTotalProgress, aDownload)
  {
    var overallProgress = aCurTotalProgress;

    var downloadID = "dl" + aDownload.id;
    var download = this.doc.getElementById(downloadID);

    // Calculate percentage.
    var percent;
    if (aMaxTotalProgress > 0) {
      percent = Math.floor((overallProgress*100.0)/aMaxTotalProgress);
      if (percent > 100)
        percent = 100;

      // Advance progress meter.
      if (download) {
        download.setAttribute("progress", percent);

        download.setAttribute("progressmode", "normal");
        
        onUpdateProgress();
      }
    } else {
      percent = -1;

      // Progress meter should be barber-pole in this case.
      download.setAttribute("progressmode", "undetermined");
    }

    // Now that we've set the progress and the time, update the UI with 
    // all the the pertinent information (bytes transferred, bytes total,
    // download rate, time remaining). 
    let status = this._statusFormat;

    // Update the bytes transferred and bytes total
    let ([progress, progressUnits] = this._convertByteUnits(aCurTotalProgress),
         [total, totalUnits] = this._convertByteUnits(aMaxTotalProgress),
         transfer) {
      if (total <= 0)
        transfer = this._transferNoTotal;
      else if (progressUnits == totalUnits)
        transfer = this._transferSameUnits;
      else
        transfer = this._transferDiffUnits;

      transfer = this._replaceInsert(transfer, 1, progress);
      transfer = this._replaceInsert(transfer, 2, progressUnits);
      transfer = this._replaceInsert(transfer, 3, total);
      transfer = this._replaceInsert(transfer, 4, totalUnits);

      // Insert 1 is the download progress
      status = this._replaceInsert(status, 1, transfer);

      if (download)
        download.setAttribute("status-internal", transfer);
    }

    // Update the download rate
    let ([rate, unit] = this._convertByteUnits(aDownload.speed)) {
      // Insert 2 is the download rate
      status = this._replaceInsert(status, 2, rate);
      // Insert 3 is the |unit|/sec
      status = this._replaceInsert(status, 3, unit);
    }

    // Update time remaining.
    let (remain) {
      if ((aDownload.speed > 0) && (aMaxTotalProgress > 0)) {
        let minutes = Math.ceil((aMaxTotalProgress - aCurTotalProgress) /
                                aDownload.speed / 60);
        if (minutes > 1)
          remain = this._replaceInsert(this._timeLeft, 1, minutes);
        else
          remain = this._timeLessMinute;
      } else {
        remain = this._timeUnknown;
      }

      // Insert 4 is the time remaining
      status = this._replaceInsert(status, 4, remain);
    }
    
    if (download)
      download.setAttribute("status", status);
  },
  onLocationChange: function(aWebProgress, aRequest, aLocation, aDownload)
  {
  },
  onStatusChange: function(aWebProgress, aRequest, aStatus, aMessage, aDownload)
  {
  },
  onSecurityChange: function(aWebProgress, aRequest, state, aDownload)
  {
  },
  QueryInterface : function(iid)
  {
    if (iid.equals(Components.interfaces.nsIDownloadProgressListener) ||
        iid.equals(Components.interfaces.nsISupports))
    return this;

    throw Components.results.NS_NOINTERFACE;
  },

  _replaceInsert: function ( text, index, value ) 
  {
    var result = text;
    var regExp = new RegExp( "#"+index );
    result = result.replace( regExp, value );
    return result;
  },
  
  // converts a number of bytes to the appropriate unit that results in a
  // number that needs fewer than 4 digits
  // returns a pair: [new value with 3 sig. figs., its unit]
  _convertByteUnits: function(aBytes)
  {
    let unitIndex = 0;

    // convert to next unit if it needs 4 digits (after rounding), but only if
    // we know the name of the next unit
    while ((aBytes >= 999.5) && (unitIndex < this._units.length - 1)) {
      aBytes /= 1024;
      unitIndex++;
    }

    // Get rid of insignificant bits by truncating to 1 or 0 decimal points
    // 0 -> 0; 1.2 -> 1.2; 12.3 -> 12.3; 123.4 -> 123; 234.5 -> 235
    aBytes = aBytes.toFixed((aBytes > 0) && (aBytes < 100) ? 1 : 0);

    return [aBytes, this._units[unitIndex]];
  }
};
