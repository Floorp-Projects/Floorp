/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Blake Ross <blaker@netscape.com> (Original Author)
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
 
var gStrings = new Array;
const interval = 500; // Update every 500 milliseconds.

function nsDownloadProgressListener() {
}

nsDownloadProgressListener.prototype = {
    rateChanges: 0,
    rateChangeLimit: 0,
    priorRate: "",
    lastUpdate: -500,
    doc: null,
    get document() {
      return this.doc;
    },
    set document(newval) {
      return this.doc = newval;
    },
    onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus, aDownload)
    {
      if (aStateFlags & Components.interfaces.nsIWebProgressListener.STATE_STOP)
      {
        var aDownloadID = aDownload.targetFile.path;
        var elt = this.doc.getElementById(aDownloadID).firstChild.firstChild;

        var timeRemainingCol = elt.nextSibling.nextSibling.nextSibling;
        timeRemainingCol.setAttribute("label", "");
        
        var speedCol = timeRemainingCol.nextSibling.nextSibling;
        speedCol.setAttribute("label", "");

        var elapsedCol = speedCol.nextSibling;
        elapsedCol.setAttribute("label", "");

        // Fire an onselect event for the downloadView element
        // to update menu status
        var event = this.doc.createEvent('Events');
        event.initEvent('select', false, true);
        this.doc.getElementById("downloadView").dispatchEvent(event);
      }
    },

    onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress,
                               aCurTotalProgress, aMaxTotalProgress, aDownload)
    {
      var overallProgress = aCurTotalProgress;
      // Get current time.
      var now = ( new Date() ).getTime();
      // If interval hasn't elapsed, ignore it.
      if ( now - this.lastUpdate < interval && aMaxTotalProgress != "-1" &&  parseInt(aCurTotalProgress) < parseInt(aMaxTotalProgress) ) {
        return;
      }

      // Update this time.
      this.lastUpdate = now;

      var aDownloadID = aDownload.targetFile.path
      var elt = this.doc.getElementById(aDownloadID).firstChild.firstChild;
      if (this.doc.getElementById("TimeElapsed").getAttribute("hidden") != "true") {
        elapsedCol = elt.nextSibling.nextSibling.nextSibling.nextSibling.nextSibling.nextSibling;
        // Update elapsed time display.
        elapsedCol.setAttribute("label", formatSeconds( now / 1000 - aDownload.startTime / 1000000, this.doc ));
      }
      // Calculate percentage.
      var percent;
      var progressCol = elt.nextSibling;
      if ( aMaxTotalProgress > 0)
      {
        percent = Math.floor((overallProgress*100.0)/aMaxTotalProgress);
        if ( percent > 100 )
          percent = 100;

        // Advance progress meter.
        progressCol.setAttribute( "value", percent );

        progressCol.setAttribute("mode", "normal");
      }
      else
      {
        percent = -1;

        // Progress meter should be barber-pole in this case.
        progressCol.setAttribute( "mode", "undetermined" );
      }

      // now that we've set the progress and the time, update # bytes downloaded...
      // Update status (nnK of mmK bytes at xx.xK aCurTotalProgress/sec)
      var status = getString( "progressMsgNoRate", this.doc );

      // Insert 1 is the number of kilobytes downloaded so far.
      status = replaceInsert( status, 1, parseInt( overallProgress/1024 + .5 ) );

      // Insert 2 is the total number of kilobytes to be downloaded (if known).
      if ( aMaxTotalProgress != "-1" )
         status = replaceInsert( status, 2, parseInt( aMaxTotalProgress/1024 + .5 ) );
      else
         status = replaceInsert( status, 2, "??" );
      
      var rateMsg = getString( "rateMsg", this.doc );
      var rate = aDownload.speed;
      if ( rate )
      {
        // rate is bytes/sec
        var kRate = (rate / 1024).toFixed(1); // kilobytes/sec
		
        // Don't update too often!
        if ( kRate != this.priorRate )
        {
          if ( this.rateChanges++ == this.rateChangeLimit )
          {
             // Time to update download rate.
             this.priorRate = kRate;
             this.rateChanges = 0;
          }
          else
          {
            // Stick with old rate for a bit longer.
            kRate = this.priorRate;
          }
        }
        else
          this.rateChanges = 0;

        // Insert 3 is the download rate (in kilobytes/sec).
        rateMsg = replaceInsert( rateMsg, 1, kRate );
      }
      else
        rateMsg = replaceInsert( rateMsg, 1, "??.?" );

      var timeRemainingCol = elt.nextSibling.nextSibling.nextSibling;

      // Update status msg.
      var statusCol = timeRemainingCol.nextSibling;
      statusCol.setAttribute("label", status);

      var speedCol = statusCol.nextSibling;
      speedCol.setAttribute("label", rateMsg);
      // Update percentage label on progress meter.      
      if (this.doc.getElementById("ProgressPercent").getAttribute("hidden") != "true") {
        var progressText = elt.nextSibling.nextSibling;
        if (percent < 0)
          progressText.setAttribute("label", "");
        else {
          var percentMsg = getString( "percentMsg", this.doc );      
          percentMsg = replaceInsert( percentMsg, 1, percent );
          progressText.setAttribute("label", percentMsg);
        }
      }
      // Update time remaining.
      if ( rate && (aMaxTotalProgress > 0) )
      {
        var rem = ( aMaxTotalProgress - aCurTotalProgress ) / rate;
        rem = parseInt( rem + .5 );
        timeRemainingCol.setAttribute("label", formatSeconds( rem, this.doc ));
      }
      else
        timeRemainingCol.setAttribute("label", getString( "unknownTime", this.doc ));
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
    }
};

var nsDownloadProgressListenerFactory = {
  createInstance: function (outer, iid) {
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    return (new nsDownloadProgressListener()).QueryInterface(iid);
  }
};

var nsDownloadProgressListenerModule = {

  registerSelf: function (compMgr, fileSpec, location, type)
  { 
    var compReg = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
    compReg.registerFactoryLocation(Components.ID("{09cddbea-1dd2-11b2-aa15-c41ffea19d79}"),
                                    "Download Progress Listener",
                                    "@mozilla.org/download-manager/listener;1",
                                    fileSpec, location, type);
  },
  canUnload: function(compMgr)
  {
    return true;
  },

  getClassObject: function (compMgr, cid, iid) {
    if (!cid.equals(Components.ID("{09cddbea-1dd2-11b2-aa15-c41ffea19d79}")))
        throw Components.results.NS_ERROR_NO_INTERFACE;
    
    if (!iid.equals(Components.interfaces.nsIFactory))
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    return nsDownloadProgressListenerFactory;
  }
};

function NSGetModule(compMgr, fileSpec) {
    return nsDownloadProgressListenerModule;
}

function replaceInsert( text, index, value ) {
   var result = text;
   var regExp = new RegExp( "#"+index );
   result = result.replace( regExp, value );
   return result;
}

function getString( stringId, doc ) {
   // Check if we've fetched this string already.
   if ( !gStrings[ stringId ] ) {
      // Try to get it.
      var elem = doc.getElementById( "strings."+stringId );
      try {
        if ( elem
           &&
           elem.childNodes
           &&
           elem.childNodes[0]
           &&
           elem.childNodes[0].nodeValue ) {
         gStrings[ stringId ] = elem.childNodes[0].nodeValue;
        } else {
          // If unable to fetch string, use an empty string.
          gStrings[ stringId ] = "";
        }
      } catch (e) { gStrings[ stringId ] = ""; }
   }
   return gStrings[ stringId ];
}

function formatSeconds( secs, doc )
{
  // Round the number of seconds to remove fractions.
  secs = parseInt( secs + .5 );
  var hours = parseInt( secs/3600 );
  secs -= hours*3600;
  var mins = parseInt( secs/60 );
  secs -= mins*60;
  var result;
  if ( hours )
    result = getString( "longTimeFormat", doc );
  else
    result = getString( "shortTimeFormat", doc );

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


