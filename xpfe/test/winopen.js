// target for window.open()
const KID_URL       = "child-window.html";

// formats final results
const SERVER_URL    = "http://jrgm.mcom.com/cgi-bin/window-open-2.0/openreport.pl";

// let system settle between each window.open
const OPENER_DELAY  = 1000;   

// three phases: single open/close; overlapped open/close; open-all/close-all
var PHASE_ONE     = 10; 
var PHASE_TWO     = 0; 
var PHASE_THREE   = 0; 

// keep this many windows concurrently open during overlapped phase
var OVERLAP_COUNT = 3;

// repeat three phases CYCLES times    
var CYCLES        = 1;  

// autoclose flag
var AUTOCLOSE = 1;

// Chrome url for child windows.
var KID_CHROME   = null;
var SAVED_CHROME = null;

// URL options and correspnding vars.
const options = [ [ "phase1", "PHASE_ONE", false ],
                  [ "phase2", "PHASE_TWO", false ],
                  [ "phase3", "PHASE_THREE", false ],
                  [ "overlap", "OVERLAP_COUNT", false ],
                  [ "cycles", "CYCLES", false ],
                  [ "chrome", "KID_CHROME", true ],
                  [ "close", "AUTOCLOSE", false ] ];

// Note: You can attach search options to the url for this file to control
// any of the options in the array above.  E.g., specifying
// mozilla -chrome "file:///D|/mozilla/xpfe/test/winopen.xul?phase1=16&close=0"
// will run this script with PHASE_ONE=16 and AUTOCLOSE=0.
//
// On Win32, you must enclose the -chrome option in quotes in order pass funny Win32 shell
// characters such as '&' or '|'!

var opts = window.location.search.substring(1).split( '&' );
for ( opt in opts ) {
    for ( var i in options ) {
        if ( opts[opt].indexOf( options[i][0]+"=" ) == 0 ) {
            var newVal = opts[opt].split( '=' )[ 1 ];
            // wrap with quotes, if required.
            if ( options[i][2] ) {
                newVal = '"' + newVal + '"';
            }
            eval( options[i][1] + "=" + newVal + ";" );
        }
    }
}

var prefs = null;

if ( KID_CHROME ) {
    // Reset browser.chromeURL so it points to KID_CHROME.
    // This will cause window.open in openWindow to open that chrome.
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
    prefs = Components.classes["@mozilla.org/preferences-service;1"]
              .getService( Components.interfaces.nsIPrefBranch );
    SAVED_CHROME = prefs.getCharPref( "browser.chromeURL" );
    prefs.setCharPref( "browser.chromeURL", KID_CHROME );
}

const CYCLE_SIZE    = PHASE_ONE + PHASE_TWO + PHASE_THREE;
const MAX_INDEX     = CYCLE_SIZE * CYCLES;  // total number of windows to open

var windowList      = [];   // handles to opened windows
var startingTimes   = [];   // time that window.open is called
var openingTimes    = [];   // time that child window took to fire onload
var closingTimes    = [];   // collect stats for case of closing >1 windows
var currentIndex    = 0;       


function childIsOpen(aTime) {
    openingTimes[currentIndex] = aTime - startingTimes[currentIndex];
    updateDisplay(currentIndex, openingTimes[currentIndex]);
    reapWindows(currentIndex);
    currentIndex++;
    if (currentIndex < MAX_INDEX)
        scheduleNextWindow();
    else
        window.setTimeout(reportResults, OPENER_DELAY);
}


function updateDisplay(index, time) {
    var formIndex = document.getElementById("formIndex");
    if (formIndex) 
        formIndex.setAttribute("value", index+1);
    var formTime  = document.getElementById("formTime");
    if (formTime) 
        formTime.setAttribute("value", time);
}


function scheduleNextWindow() {
    window.setTimeout(openWindow, OPENER_DELAY);
}


function closeOneWindow(aIndex) {
    var win = windowList[aIndex];
    // no-op if window is already closed
    if (win && !win.closed) {
	win.close();
	windowList[aIndex] = null;
    }
}    


function closeAllWindows(aRecordTimeToClose) {
    var timeToClose = (new Date()).getTime();
    var count = 0;
    for (var i = 0; i < windowList.length; i++) {
	if (windowList[i])
	    count++;
	closeOneWindow(i);
    }
    if (aRecordTimeToClose && count > 0) {
        timeToClose = (new Date()).getTime() - timeToClose;
        closingTimes.push(parseInt(timeToClose/count));
    }
}    


// close some, none, or all open windows in the list
function reapWindows() {
    var modIndex = currentIndex % CYCLE_SIZE;
    if (modIndex < PHASE_ONE-1) {
	// first phase in each "cycle", are single open/close sequences
	closeOneWindow(currentIndex);
    } 
    else if (PHASE_ONE-1 <= modIndex && modIndex < PHASE_ONE+PHASE_TWO-1) {
	// next phase in each "cycle", keep N windows concurrently open
	closeOneWindow(currentIndex - OVERLAP_COUNT);
    }
    else if (modIndex == PHASE_ONE+PHASE_TWO-1) {
	// end overlapping windows cycle; close all windows
	closeAllWindows(false);
    }
    else if (PHASE_ONE+PHASE_TWO <= modIndex && modIndex < CYCLE_SIZE-1) {
	// do nothing; keep adding windows
    }
    else if (modIndex == CYCLE_SIZE-1) {
	// end open-all/close-all phase; close windows, recording time to close
	closeAllWindows(true);
    }
}

function calcMedian( numbers ) {
    if ( numbers.length == 0 ) {
        return 0;
    } else if ( numbers.length == 1 ) {
        return numbers[0];
    } else if ( numbers.length == 2 ) {
        return ( numbers[0] + numbers[1] ) / 2;
    } else {
        numbers.sort( function (a,b){ return a-b; } );
        var n = Math.floor( numbers.length / 2 );
        return numbers.length % 2 ? numbers[n] : ( numbers[n-1] + numbers[n] ) / 2;
    }
}

function reportResults() {
    //XXX need to create a client-side method to do this?
    var opening = openingTimes.join(':'); // times for each window open
    var closing = closingTimes.join(':'); // these are for >1 url, as a group
    //var ua = escape(navigator.userAgent).replace(/\+/g, "%2B"); // + == ' ', on servers
    //var reportURL = SERVER_URL + 
    //    "?opening="    + opening + 
    //    "&closing="    + closing + 
    //    "&maxIndex="   + MAX_INDEX + 
    //    "&cycleSize="  + CYCLE_SIZE + 
	//"&ua="         + ua;
    //window.open(reportURL, "test-results");
    var avgOpenTime = 0;
    var minOpenTime = 99999;
    var maxOpenTime = 0;
    var medOpenTime = calcMedian( openingTimes.slice(1) );
    // ignore first open
    for (i = 1; i < MAX_INDEX; i++) {
        avgOpenTime += openingTimes[i];
        if ( minOpenTime > openingTimes[i] ) {
            minOpenTime = openingTimes[i];
        }
        if ( maxOpenTime < openingTimes[i] ) {
            maxOpenTime = openingTimes[i];
        }
    }
    avgOpenTime = Math.round(avgOpenTime / (MAX_INDEX - 1));
    dump("openingTimes="+openingTimes.slice(1)+"\n");
    dump("avgOpenTime:" + avgOpenTime + "\n" );
    dump("minOpenTime:" + minOpenTime + "\n" );
    dump("maxOpenTime:" + maxOpenTime + "\n" );
    dump("medOpenTime:" + medOpenTime + "\n" );
    dump("__xulWinOpenTime:" + medOpenTime + "\n");
    // Close the root window, if required.
    if ( AUTOCLOSE ) {
        window.close();
    } else {
        document.getElementById("formTimes").value = openingTimes.slice(1);
        document.getElementById("formAvg").value   = avgOpenTime;
        document.getElementById("formMin").value   = minOpenTime;
        document.getElementById("formMax").value   = maxOpenTime;
        document.getElementById("formMed").value   = medOpenTime;
        document.getElementById("formAgain").setAttribute( "disabled", "false" );
    }
}

function tryAgain() {
    document.getElementById("formAgain").setAttribute( "disabled", "true" );
    windowList      = [];
    startingTimes   = [];
    openingTimes    = [];
    closingTimes    = [];
    currentIndex    = 0;       
    openWindow();
}

function restoreChromeURL() {
    // Restore browser.chromeURL pref.
    if ( KID_CHROME && SAVED_CHROME.length ) {
        netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
        prefs.setCharPref( "browser.chromeURL", SAVED_CHROME );
    }
}

function openWindow() {
    startingTimes[currentIndex] = (new Date()).getTime();
    var path   = window.location.pathname.substring( 0, window.location.pathname.lastIndexOf('/') );
    var url    = window.location.protocol + "//" + 
                 window.location.hostname + path + "/" +
                 KID_URL;
    windowList[currentIndex] = window.open(url, currentIndex);
}


