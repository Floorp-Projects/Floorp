// target for window.open()
const KID_URL       = "child-window.html";

// formats final results
const SERVER_URL    = "http://jrgm.mcom.com/cgi-bin/window-open-2.0/openreport.pl";

// let system settle between each window.open
const OPENER_DELAY  = 1000;   

// three phases: single open/close; overlapped open/close; open-all/close-all
const PHASE_ONE     = 10; 
const PHASE_TWO     = 0; 
const PHASE_THREE   = 0; 

// keep this many windows concurrently open during overlapped phase
const OVERLAP_COUNT = 3;

// repeat three phases CYCLES times    
const CYCLES        = 1;  

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
    // ignore first open
    for (i = 1; i < MAX_INDEX; i++) {
        avgOpenTime += openingTimes[i];
    }
    avgOpenTime = Math.round(avgOpenTime / (MAX_INDEX - 1));
    dump("averageWindowOpenTime:" + avgOpenTime + "ms\n");
    window.close();
}


function openWindow() {
    startingTimes[currentIndex] = (new Date()).getTime();
    windowList[currentIndex] = window.open(KID_URL, currentIndex);
}


