var gStrings = [];
const interval = 500; // Update every 500 milliseconds.

function DownloadProgressListener (aDocument, aStringBundle) 
{
  this.doc = aDocument;
  
  this._statusFormat = aStringBundle.getString("statusFormat");
  this._statusFormatKBMB = aStringBundle.getString("statusFormatKBMB");
  this._statusFormatKBKB = aStringBundle.getString("statusFormatKBKB");
  this._statusFormatMBMB = aStringBundle.getString("statusFormatMBMB");
  this._longTimeFormat = aStringBundle.getString("longTimeFormat");
  this._shortTimeFormat = aStringBundle.getString("shortTimeFormat");
  
}

DownloadProgressListener.prototype = 
{
  elapsed: 0,
  rateChanges: 0,
  rateChangeLimit: 0,
  priorRate: 0,
  lastUpdate: -500,
  doc: null,
  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus, aDownload)
  {
    if (aStateFlags & Components.interfaces.nsIWebProgressListener.STATE_STOP) {
      var aDownloadID = aDownload.targetFile.path;
      var download = this.doc.getElementById(aDownloadID);
      if (download)
        download.setAttribute("status", "");
    }
  },

  onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress,
                              aCurTotalProgress, aMaxTotalProgress, aDownload)
  {
    var overallProgress = aCurTotalProgress;
    // Get current time.
    var now = (new Date()).getTime();
    
    // If interval hasn't elapsed, ignore it.
    if (now - this.lastUpdate < interval && aMaxTotalProgress != "-1" &&  
        parseInt(aCurTotalProgress) < parseInt(aMaxTotalProgress)) {
      return;
    }

    // Update this time.
    this.lastUpdate = now;

    // Update download rate.
    this.elapsed = now - (aDownload.startTime / 1000);
    var rate; // aCurTotalProgress/sec
    if (this.elapsed)
      rate = (aCurTotalProgress * 1000) / this.elapsed;
    else
      rate = 0;

    var aDownloadID = aDownload.targetFile.path;
    var download = this.doc.getElementById(aDownloadID);

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
    }
    else {
      percent = -1;

      // Progress meter should be barber-pole in this case.
      download.setAttribute("progressmode", "undetermined");
    }

    // Now that we've set the progress and the time, update the UI with 
    // all the the pertinent information (bytes transferred, bytes total,
    // download rate, time remaining). 
    var status = this._statusFormat;

    // Insert the progress so far using the formatting routine. 
    var KBProgress = parseInt(overallProgress/1024 + .5);
    var KBTotal = parseInt(aMaxTotalProgress/1024 + .5);
    var kbProgress = this._formatKBytes(KBProgress, KBTotal);
    status = this._replaceInsert(status, 1, kbProgress);
    if (download)
      download.setAttribute("status-internal", kbProgress);

    if (rate) {
      // rate is bytes/sec
      var kRate = rate / 1024; // K bytes/sec;
      kRate = parseInt( kRate * 10 + .5 ); // xxx (3 digits)
      // Don't update too often!
      if (kRate != this.priorRate) {
        if (this.rateChanges++ == this.rateChangeLimit) {
            // Time to update download rate.
            this.priorRate = kRate;
            this.rateChanges = 0;
        }
        else {
          // Stick with old rate for a bit longer.
          kRate = this.priorRate;
        }
      }
      else
        this.rateChanges = 0;

        var fraction = kRate % 10;
        kRate = parseInt((kRate - fraction) / 10);

        // Insert 3 is the download rate (in kilobytes/sec).
        if (kRate < 100)
          kRate += "." + fraction;
        status = this._replaceInsert(status, 2, kRate);
    }
    else
      status = this._replaceInsert(status, 2, "??.?");

    // Update time remaining.
    if (rate && (aMaxTotalProgress > 0)) {
      var rem = (aMaxTotalProgress - aCurTotalProgress) / rate;
      rem = parseInt(rem + .5);
      
      status = this._replaceInsert(status, 3, this._formatSeconds(rem, this.doc));
    }
    else
      status = this._replaceInsert(status, 3, "???");
    
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
  
  // aBytes     aTotalKBytes    returns:
  // x, < 1MB   y < 1MB         x of y KB
  // x, < 1MB   y >= 1MB        x KB of y MB
  // x, >= 1MB  y >= 1MB        x of y MB
  _formatKBytes: function (aKBytes, aTotalKBytes)
  {
    var progressHasMB = parseInt(aKBytes/1000) > 0;
    var totalHasMB = parseInt(aTotalKBytes/1000) > 0;
    
    var format = "";
    if (!progressHasMB && !totalHasMB) {
      format = this._statusFormatKBKB;
      format = this._replaceInsert(format, 1, aKBytes);
      format = this._replaceInsert(format, 2, aTotalKBytes);
    }
    else if (progressHasMB && totalHasMB) {
      format = this._statusFormatMBMB;
      format = this._replaceInsert(format, 1, (aKBytes / 1000).toFixed(1));
      format = this._replaceInsert(format, 2, (aTotalKBytes / 1000).toFixed(1));
    }
    else if (totalHasMB && !progressHasMB) {
      format = this._statusFormatKBMB;
      format = this._replaceInsert(format, 1, aKBytes);
      format = this._replaceInsert(format, 2, (aTotalKBytes / 1000).toFixed(1));
    }
    else {
      // This is an undefined state!
      dump("*** huh?!\n");
    }
    
    return format;  
  },

  _formatSeconds: function (secs, doc)
  {
    // Round the number of seconds to remove fractions.
    secs = parseInt(secs + .5);
    var hours = parseInt(secs/3600);
    secs -= hours * 3600;
    
    var mins = parseInt(secs/60);
    secs -= mins * 60;
    var result = hours ? this._longTimeFormat : this._shortTimeFormat;

    if (hours < 10)
      hours = "0" + hours;
    if (mins < 10)
      mins = "0" + mins;
    if (secs < 10)
      secs = "0" + secs;

    // Insert hours, minutes, and seconds into result string.
    result = this._replaceInsert(result, 1, hours);
    result = this._replaceInsert(result, 2, mins);
    result = this._replaceInsert(result, 3, secs);

    return result;
  },
  
};

# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

 
