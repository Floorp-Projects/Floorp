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
 */
var data;   // nsIStreamTransferOperation object
var dialog;

function loadDialog() {
    dialog.location.setAttribute( "value", data.source.URI.spec );
    dialog.fileName.setAttribute( "value", data.target.unicodePath );
    dialog.error = false;
}

var contractId = "@mozilla.org/appshell/component/xfer;1";
var observer = {
    Observe: function( subject, topic, data ) {
        switch ( topic ) {
            case contractId+";onProgress":
                var words = data.split( " " );
                onProgress( words[0], words[1] );
                break;
            case contractId+";onStatus":
                // Ignore useless "Transferring data..." messages
                // after download has started.
                if ( !started ) {
                   onStatus( data );
                }
                break;
            case contractId+";onCompletion":
                onCompletion( data );
                break;
            case contractId+";onError":
                onError( data );
                break;
            default:
                alert( "Unknown topic: " + topic + "\nData: " + data );
                break;
        }
        return;
    }
}

function getString( stringId ) {
    // Check if we've fetched this string already.
    if ( !( stringId in dialog.strings ) ) {
        // Try to get it.
        var elem = document.getElementById( "dialog.strings."+stringId );
        if ( elem
           &&
           elem.firstChild
           &&
           elem.firstChild.nodeValue ) {
            dialog.strings[ stringId ] = elem.firstChild.nodeValue;
        } else {
            // If unable to fetch string, use an empty string.
            dialog.strings[ stringId ] = "";
        }
    }
    return dialog.strings[ stringId ];
}

function replaceInsert( text, index, value ) {
   var result = text;
   var regExp = new RegExp( "#"+index );
   result = result.replace( regExp, value );
   return result;
}

function onLoad() {
    // Set global variables.
    data = window.arguments[0];

    if ( !data ) {
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

    // Fill dialog.
    loadDialog();

    // Commence transfer.
    data.observer = observer;
    try {
        data.Start();
    } catch( exception ) {
        onError( exception );
    }
}

function onUnload() {
    // Only do this one time...
    if ( data ) {
        // Unhook underlying xfer operation.
        var op = data;
        data = null;

        // Unhook observer.
        op.observer = null;

        // See if we completed normally (i.e., are closing ourself).
        if ( started && !completed ) {
            // Terminate transfer.
            try {
                op.Stop();
            } catch ( exception ) {
                if ( !dialog.error ) {
                    // Show error now.
                    onError( exception );
                }
            }
            op = null;
        }
    }
}

var started   = false;
var completed = false;
var startTime = 0;
var elapsed   = 0;
var interval = 1000; // Update every 1000 milliseconds.
var lastUpdate = -interval; // Update initially.

// These are to throttle down the updating of the download rate figure.
var priorRate = 0;
var rateChanges = 0;
var rateChangeLimit = 2;

var warningLimit = 30000; // Warn on Cancel after 30 sec (30000 milleseconds).

function stop() {
   // If too much time has elapsed, make sure it's OK to quit.
   if ( started && !completed ) {
      // Get current time.
      var now = ( new Date() ).getTime();

      // See if sufficient time has elapsed to warrant dialog.
      if ( now - startTime > warningLimit ) {
         // XXX - Disabled for now since confirm call doesn't work.
         if ( 0 && !window.confirm( getString( "confirmCancel" ) ) ) {
            return;
         }
      }
   }

   // Stop the transfer.
   onUnload();

   // Close the window.
   window.close();
}

function onProgress( bytes, max ) {
    // Check for first time.
    if ( !started ) {
        // Initialize download start time.
        started = true;
        startTime = ( new Date() ).getTime();
    }

    // Get current time.
    var now = ( new Date() ).getTime();
    // If interval hasn't elapsed, ignore it.
    if ( now - lastUpdate < interval
         &&
         max != "-1"
         &&
         parseInt(bytes) < parseInt(max) ) {
        return;
    }

    // Update this time.
    lastUpdate = now;

    // Update download rate.
    elapsed = now - startTime;
    var rate; // bytes/sec
    if ( elapsed ) {
        rate = ( bytes * 1000 ) / elapsed;
    } else {
        rate = 0;
    }

    // Update elapsed time display.
    dialog.timeElapsed.setAttribute("value", formatSeconds( elapsed / 1000 ));

    // Calculate percentage.
    var percent;
    if ( max != "-1" ) {
        percent = parseInt( (bytes*100)/max + .5 );
        if ( percent > 100 ) {
           percent = 100;
        }

        // Advance progress meter.
        dialog.progress.setAttribute( "value", percent );
    } else {
        percent = "??";

        // Progress meter should be barber-pole in this case.
        dialog.progress.setAttribute( "mode", "undetermined" );
    }

    // Check if download complete.
    if ( !completed ) {
        // Update status (nnK of mmK bytes at xx.xK bytes/sec)
        var status = getString( "progressMsg" );

        // Insert 1 is the number of kilobytes downloaded so far.
        status = replaceInsert( status, 1, parseInt( bytes/1024 + .5 ) );

        // Insert 2 is the total number of kilobytes to be downloaded (if known).
        if ( max != "-1" ) {
           status = replaceInsert( status, 2, parseInt( max/1024 + .5 ) );
        } else {
           status = replaceInsert( status, 2, "??" );
        }

        if ( rate ) {
           // rate is bytes/sec
           var kRate = rate / 1024; // K bytes/sec;
           kRate = parseInt( kRate * 10 + .5 ); // xxx (3 digits)
           // Don't update too often!
           if ( kRate != priorRate ) {
              if ( rateChanges++ == rateChangeLimit ) {
                 // Time to update download rate.
                 priorRate = kRate;
                 rateChanges = 0;
              } else {
                 // Stick with old rate for a bit longer.
                 kRate = priorRate;
              }
           } else {
              rateChanges = 0;
           }
           var fraction = kRate % 10;
           kRate = parseInt( ( kRate - fraction ) / 10 );

           // Insert 3 is the download rate (in kilobytes/sec).
           status = replaceInsert( status, 3, kRate + "." + fraction );
        } else {
           status = replaceInsert( status, 3, "??.?" );
        }
        // Update status msg.
        onStatus( status );
    }

    // Update percentage label on progress meter.
    var percentMsg = getString( "percentMsg" );
    percentMsg = replaceInsert( percentMsg, 1, percent );
    dialog.progress.label = percentMsg;

    if ( !completed ) {
        // Update time remaining.
        if ( rate && max != "-1" ) {
            var rem = ( max - bytes ) / rate;
            rem = parseInt( rem + .5 );
            dialog.timeLeft.setAttribute("value", formatSeconds( rem ));
        } else {
            dialog.timeLeft.setAttribute("value", getString( "unknownTime" ));
        }
    } else {
        // Clear time remaining field.
        dialog.timeLeft.setAttribute("value", "");
    }
}

function formatSeconds( secs ) {
    // Round the number of seconds to remove fractions.
    secs = parseInt( secs + .5 );
    var hours = parseInt( secs/3600 );
    secs -= hours*3600;
    var mins = parseInt( secs/60 );
    secs -= mins*60;
    var result;
    if ( hours ) {
      result = getString( "longTimeFormat" );
    } else {
      result = getString( "shortTimeFormat" );
    }
    if ( hours < 10 ) {
       hours = "0" + hours;
    }
    if ( mins < 10 ) {
       mins = "0" + mins;
    }
    if ( secs < 10 ) {
       secs = "0" + secs;
    }
    // Insert hours, minutes, and seconds into result string.
    result = replaceInsert( result, 1, hours );
    result = replaceInsert( result, 2, mins );
    result = replaceInsert( result, 3, secs );

    return result;
}

function onCompletion( status ) {
    // Note that we're done (and can ignore subsequent progress notifications).
    completed = true;
    // Indicate completion in status area.
    var msg = getString( "completeMsg" );
    msg = replaceInsert( msg, 1, formatSeconds( elapsed/1000 ) );
    onStatus( msg );

    // Put progress meter at 100%.
    dialog.progress.setAttribute( "value", 100 );
    dialog.progress.setAttribute( "mode", "normal" );
    window.close();
}

function onStatus( status ) {
   // Update status text in dialog.
   // Ignore if status text is less than 10 characters (we're getting some
   // bogus onStatus notifications.
   if ( status.length > 9 ) {
      dialog.status.setAttribute("value", status);
   }
}

function onError( errorCode ) {
    // Record fact we had an error.
    dialog.error = true;

    // Errorcode has format: "operation rv <string>" where
    //   operation - operation that failed; values are as defined
    //               in nsIStreamTransferOperation.idl
    //   rv        - actual nsresult (in "%X" format)
    //   reason    - reason for failure; values are as defined
    //               in nsIStreamTransferOperation.idl
    var msg;
    switch ( parseInt( errorCode.split(" ")[0] ) ) {
        // Write errors...
        case Components.interfaces.nsIStreamTransferOperation.kOpWrite :
        case Components.interfaces.nsIStreamTransferOperation.kOpAsyncWrite :
        case Components.interfaces.nsIStreamTransferOperation.kOpOpenOutputStream :
        case Components.interfaces.nsIStreamTransferOperation.kOpCreateTransport :
        case Components.interfaces.nsIStreamTransferOperation.kOpOutputClose :
        case Components.interfaces.nsIStreamTransferOperation.kOpOutputCancel :
            // Look for some specific reasons.
            switch ( parseInt( errorCode.split(" ")[2] ) ) {
                case Components.interfaces.nsIStreamTransferOperation.kReasonAccessError :
                    // Access/permission error.
                    msg = getString( "accessErrorMsg" );
                    break;

                case Components.interfaces.nsIStreamTransferOperation.kReasonDiskFull :
                    // Out of space error.
                    msg = getString( "diskFullMsg" );
                    break;

                default :
                    // Generic write error.
                    msg = getString( "writeErrorMsg" );
                    break;
            }
            break;

        // Read errors...
        default :
            // Presume generic read error.
            msg = getString( "readErrorMsg" );
            break;
    }
    // Tell user.
    alert( msg );
    // Dump error code to console.
    dump( "downloadProgress.js onError: " + errorCode + "\n" );
    // Terminate transfer and clean up.
    stop();
}
