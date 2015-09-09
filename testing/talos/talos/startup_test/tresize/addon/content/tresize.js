/*
 * tresize-test - A purer form of paint measurement than tpaint. This
 * test opens a single window positioned at 10,10 and sized to 425,425,
 * then resizes the window outward |max| times measuring the amount of
 * time it takes to repaint each resize. Dumps the resulting dataset
 * and average to stdout or logfile.
 */

var dataSet = new Array();
var windowSize = 425;
var resizeIncrement = 2;
var count = 0;
var max = 300;

var dumpDataSet = false;
var doneCallback = null;

function painted() {
  window.gBrowser.removeEventListener("MozAfterPaint", painted, true);
  var paintTime = window.performance.now();
  Profiler.pause("resize " + count);
  dataSet[count].end = paintTime;
  setTimeout(resizeCompleted, 20);
}

function resizeTest() {
  try {
    windowSize += resizeIncrement;
    window.gBrowser.addEventListener("MozAfterPaint", painted, true);
    Profiler.resume("resize " + count);
    dataSet[count] = {'start': window.performance.now()};
    window.resizeTo(windowSize,windowSize);
  } catch(ex) { finish([ex + '\n']); }
}

function testCompleted() {
  try {
    Profiler.finishTest();
    var total = 0;
    var diffs = [];
    for (var idx = 0; idx < count; idx++) {
      var diff = dataSet[idx].end - dataSet[idx].start;
      total += diff;
      diffs.push(diff);
    }
    var average = (total/count);
    var retVal = [];
    if (dumpDataSet) {
      retVal.push('__start_reporttresize-test.html,' + diffs + '__end_report\n');
    } else {
      retVal.push('__start_report' + average + '__end_report\n');
    }
    retVal.push('__startTimestamp' + Date.now() + '__endTimestamp\n');
    finish(retVal);
  } catch(ex) { finish([ex + '\n']); }
}

function resizeCompleted() {
  count++;
  if (count >= max) {
    testCompleted();
  } else {
    resizeTest();
  }
}

function runTest(callback, locationSearch) {
  doneCallback = callback;
  window.moveTo(10,10);
  window.resizeTo(windowSize,windowSize);
  Profiler.initFromURLQueryParams(locationSearch);
  Profiler.beginTest("tresize");
  resizeTest();
}

function finish(msgs) {
  if (doneCallback) {
    doneCallback(msgs);
  }
}

