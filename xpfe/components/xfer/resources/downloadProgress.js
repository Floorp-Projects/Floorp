var data;   // nsIStreamTransferOperation object
var dialog;

function loadDialog() {
    dialog.location.setAttribute( "value", data.source );
    dialog.fileName.setAttribute( "value", data.target );
}

var progId = "component://netscape/appshell/component/xfer";
var observer = {
    Observe: function( subject, topic, data ) {
        switch ( topic ) {
            case progId+";onProgress":
                var words = data.split( " " );
                onProgress( words[0], words[1] );
                break;
            case progId+";onStatus":
                onStatus( data );
                break;
            case progId+";onCompletion":
                onCompletion( data );
                break;
            default:
                dump( "Unknown topic: " + topic + "\n" );
                break;
        }
        return;
    }
}

function onLoad() {
    // Set global variables.
    data = window.arguments[0];
    if ( !data ) {
        dump( "Invalid argument to downloadProgress.xul\n" );
        window.close()
        return;
    }

    dialog = new Object;
    dialog.location    = document.getElementById("dialog.location");
    dialog.contentType = document.getElementById("dialog.contentType");
    dialog.fileName    = document.getElementById("dialog.fileName");
    dialog.status      = document.getElementById("dialog.status");
    dialog.progress    = document.getElementById("dialog.progress");
    dialog.progressPercent = document.getElementById("dialog.progressPercent");
    dialog.timeLeft    = document.getElementById("dialog.timeLeft");
    dialog.cancel      = document.getElementById("dialog.cancel");

    // Fill dialog.
    loadDialog();

    // Commence transfer.
    data.observer = observer;
    data.Start();
}

function onUnload() {
    // Unhook observer.
    data.observer = null;

    // Terminate transfer.
    data.Stop();
}

var started   = false;
var completed = false;
var startTime;
var elapsed;
var interval = 1000; // Update every 1000 milliseconds.
var lastUpdate = -interval; // Update initially.

function stop() {
    // Stop the transfer.
    data.Stop();

    // Close the window.
    window.close();
}

function onProgress( bytes, max ) {
    // Check for first time.
    if ( !started ) {
        // Initialize download start time.
        started = true;
        startTime = ( new Date() ).getTime();
        // Let the user stop, now.
        dialog.cancel.removeAttribute( "disabled" );
    }

    // Get current time.
    var now = ( new Date() ).getTime();
    // If interval hasn't elapsed, ignore it.
    if ( now - lastUpdate < interval && eval(bytes) < eval(max) ) {
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

    // Calculate percentage.
    var percent = Math.round( (bytes*100)/max );

    // Advance progress meter.
    dialog.progress.setAttribute( "value", percent );
    
    // Check if download complete.
    if ( !completed ) {
        // Update status (nnn of mmm)
        var status = "( ";
        status += Math.round( bytes/1024 );
        status += "K of ";
        status += Math.round( max/1024 );
        status += "K bytes ";
        if ( rate ) {
            status += "at ";
            status += Math.round( (rate*10)/1024 ) / 10;
            status += "K bytes/sec )";
        } else {
            status += ")";
        }
        // Update status msg.
        onStatus( status );
    }

    // Update percentage label on progress meter.
    dialog.progressPercent.childNodes[0].nodeValue = percent + "%";
    
    if ( !completed ) {
        // Update time remaining.
        if ( rate ) {
            var rem = Math.round( ( max - bytes ) / rate ); // In seconds.
            dialog.timeLeft.childNodes[0].nodeValue = formatSeconds( rem );
        }
    } else {
        // Clear time remaining field.
        dialog.timeLeft.childNodes[0].nodeValue = "";
    }
}

function formatSeconds( nSecs ) {
    status = "";
    if ( nSecs >= 3600 ) {
        status += Math.round( nSecs/3600 ) + " hours, ";
        nSecs = nSecs % 3600;
    }
    status += Math.round( nSecs/60 ) + " minutes and ";
    nSecs = nSecs % 60;
    status += nSecs + " seconds";
    return status;
}

function onCompletion( status ) {
    // Note that we're done (and can ignore subsequent progress notifications).
    completed = true;
    // Indicate completion in status area.
    onStatus( "Download completed in " + formatSeconds( elapsed/1000 ) );
    // Close the window in 2 seconds (to ensure user sees we're done).
    window.setTimeout( "window.close();", 2000 );
}

function onStatus( status ) {
    // Update status text in dialog.
    dialog.status.childNodes[0].nodeValue = status;
}
