/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// given an array of strings, finds the longest common prefix
function findCommonPrefixLength(strs) {
  if (strs.length < 2) {
    // only one page in the manifest
    // i.e. http://localhost/tests/perf-reftest/bloom-basic.html
    var place = strs[0].lastIndexOf("/");
    if (place < 0) place = 0;
    return place;
  }

  var len = 0;
  do {
    var newlen = len + 1;
    var newprefix = null;
    var failed = false;
    for (var i = 0; i < strs.length; i++) {
      if (newlen > strs[i].length) {
	failed = true;
	break;
      }

      var s = strs[i].substr(0, newlen);
      if (newprefix == null) {
	newprefix = s;
      } else if (newprefix != s) {
	failed = true;
	break;
      }
    }

    if (failed)
      break;

    len++;
  } while (true);
  return len;
}

// Constructor
function Report() {
  this.timeVals = {};
  this.totalCCTime = 0;
  this.showTotalCCTime = false;
}

Report.prototype.pageNames = function() {
  var retval = [];
  for (var page in this.timeVals) {
    retval.push(page);
  }
  return retval;
}

Report.prototype.getReport = function() {

  var report;
  var pages = this.pageNames();
  var prefixLen = findCommonPrefixLength(pages);

  report = "__start_tp_report\n";
  report += "_x_x_mozilla_page_load\n";
  report += "_x_x_mozilla_page_load_details\n";
  report += "|i|pagename|runs|\n";

  for (var i = 0; i < pages.length; i++) {
    report += "|" +
      i + ";" +
      pages[i].substr(prefixLen) + ";" +
      this.timeVals[pages[i]].join(";") +
      "\n";
  }
  report += "__end_tp_report\n";

  if (this.showTotalCCTime) {
    report += "__start_cc_report\n";
    report += "_x_x_mozilla_cycle_collect," + this.totalCCTime + "\n";
    report += "__end_cc_report\n";
  }
  var now = (new Date()).getTime();
  report += "__startTimestamp" + now + "__endTimestamp\n"; // timestamp for determning shutdown time, used by talos

  return report;
}

Report.prototype.getReportSummary = function() {

  function average(arr) {
    var sum = 0;
    for (var i in arr)
      sum += arr[i];
    return sum / (arr.length || 1);
  }

  function median(arr) {
    if (!arr.length)
      return 0; // As good indication for "not available" as any other value.

    var sorted = arr.slice(0).sort();
    var mid = Math.floor(arr.length / 2);

    if (sorted.length % 2)
      return sorted[mid];

    return average(sorted.slice(mid, mid + 2));
  }

  // We use sample stddev and not population stddev because
  // well.. it's a sample and we can't collect all/infinite number of values.
  function stddev(arr) {
    if (arr.length <= 1) {
      return 0;
    }
    var avg = average(arr);

    var squareDiffArr = arr.map( function(v) { return Math.pow(v - avg, 2); } );
    var sum = squareDiffArr.reduce( function(a, b) { return a + b; } );
    var rv = Math.sqrt(sum / (arr.length - 1));
    return rv;
  }

  var report = "";
  var pages = this.pageNames();
  var prefixLen = findCommonPrefixLength(pages);

  report += "------- Summary: start -------\n";
  report += "Number of tests: " + pages.length + "\n";

  for (var i = 0; i < pages.length; i++) {
    var results = this.timeVals[pages[i]].map(function(v) {
                                                return Number(v);
                                              });

    report += "\n[#" + i + "] " + pages[i].substr(prefixLen)
            + "  Cycles:" + results.length
            + "  Average:" + average(results).toFixed(2)
            + "  Median:" + median(results).toFixed(2)
            + "  stddev:" + stddev(results).toFixed(2)
            + " (" + (100 * stddev(results) / median(results)).toFixed(1) + "%)"
            + (results.length < 5 ? "" : ("  stddev-sans-first:" + stddev(results.slice(1)).toFixed(2)))
            + "\nValues: " + results.map(function(v) {
                                           return v.toFixed(1);
                                         }).join("  ")
            + "\n";
  }
  report += "-------- Summary: end --------\n";

  return report;
}

Report.prototype.recordTime = function(pageName, ms) {
  if (this.timeVals[pageName] == undefined) {
    this.timeVals[pageName] = [];
  }
  this.timeVals[pageName].push(ms);
}

Report.prototype.recordCCTime = function(ms) {
  this.totalCCTime += ms;
  this.showTotalCCTime = true;
}
