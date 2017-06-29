/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** ****************************************************************
 * window.talosDebug provides some statistical functions
 * (sum, average, median, stddev) and a tpRecordTime method which
 * reports some statistics about the data set, including detected
 * stability point of the data (detecting first few noisy elements).
 * If tpRecordTime doesn't exist globally (i.e. running outside of
 * talos, e.g. in a browser), then it points it to window.talosDebug.tpRecordTime
 * Can be controlled by few properties (disable display, hardcoded
 * stability point, etc)
 *
 * talos-debug.js: Bug 849558
 *****************************************************************/
window.talosDebug = {
  // Optional config properties
  disabled: false,
  ignore: -1, // Number of items to ignore at the begining of the set. -1 for auto-detect.
  displayData: false, // If true, will also display all the data points.
  fixed: 2, // default floating point digits for display.
  // End of config

  sum(values) {
    return values.reduce(function(a, b) { return a + b; });
  },

  average(values) {
    var d = window.talosDebug;
    return values.length ? d.sum(values) / values.length : 999999;
  },

  median(values) {
    var clone = values.slice(0);
    // eslint-disable-next-line no-nested-ternary
    var sorted = clone.sort(function(a, b) { return (a > b) ? 1 : ((a < b) ? -1 : 0); });
    var len = values.length;
    if (!len)
      return 999999;
    if (len % 2) // We have a middle number
      return sorted[(len - 1) / 2];
    return (sorted[len / 2] + sorted[len / 2 - 1]) / 2; // Average value of the two middle items.
  },

  stddev(values, avg) {
    if (values.length <= 1) {
      return 0;
    }

    return Math.sqrt(
      values.map(function(v) { return Math.pow(v - avg, 2); })
            .reduce(function(a, b) { return a + b; }) / (values.length - 1));
  },

  // Estimate the number of warmup iterations of this data set (in a completely unscrientific way).
  // returns -1 if could not find a stability point (supposedly meaning that the values
  // are still in the process of convergence).
  // Algorithm (not based on a scientific method which I know of):
  // Target: Find a locally noisy value after the values show stability
  //         (stddev of a moving window stops decreasing), while trying to make sure
  //         that it wasn't a glitch and the next 2 floating window stddev are, indeed, not
  //         still decreasing.
  //         Do the above for few different window widths (3-7), get the maximum result of those.
  //         Now we should have an index which is at least the 2nd value within the stable part.
  //         We get stddev for that index..end (baseStd), and then go backwards as long as stddev is
  //         decreasing or within ~1% of baseStd, and return the earliest index for which it is.
  detectWarmup(values) {
    var MIN_WIDTH = 3;
    var MAX_WIDTH = 7;
    var d = window.talosDebug;

    function windowStd(from, winSize) {
      var win = values.slice(from, from + winSize);
      return d.stddev(win, d.median(values));
    }

    var stableFrom = -1;
    var overallAverage = d.median(values);
    // var overallStd = d.stddev(values, overallAverage);
    for (var winWidth = MIN_WIDTH; winWidth < (MAX_WIDTH + 1); winWidth++) {
      var prevStd = windowStd(0, winWidth);
      for (var i = 1; i < values.length - winWidth - 3; i++) {
        var w0 = windowStd(i + 0, winWidth);
        var w1 = windowStd(i + 1, winWidth);
        var w2 = windowStd(i + 2, winWidth);
        // var currWindow = values.slice(i, i + winWidth);
        if (w0 >= prevStd && !(w1 < w0 && w2 < w1)) {
          if (i > stableFrom)
            stableFrom = i;
          break;
        }
        prevStd = w0;
      }
    }

    function withinPercentage(base, value, percentage) {
      return Math.abs(base - value) < base * percentage / 100;
    }
    // Now go backwards as long as stddev decreases or doesn't increase in more than 1%.
    var baseStd = d.stddev(values.slice(stableFrom), overallAverage);
    var len = values.length;
    while (true) {
      var current = d.stddev(values.slice(stableFrom - 1), overallAverage);
      // 100/len : the more items we have, the more sensitively we compare:
      //   for 100 items, we're allowing 2% over baseStd.
      if (stableFrom > 0 && (current < baseStd || withinPercentage(baseStd, current, 200 / (len ? len : 100)))) {
        stableFrom--;
      } else {
        break;
      }
    }

    return stableFrom;
  },

  statsDisplay(collection) {
    var d = window.talosDebug;
    var std = d.stddev(collection, d.average(collection));
    var avg = d.average(collection);
    var med = d.median(collection);
    return "Count: " + collection.length +
           "\nAverage: " + avg.toFixed(d.fixed) +
           "\nMedian: " + med.toFixed(d.fixed) +
           "\nStdDev: " + std.toFixed(d.fixed) + " (" + (100 * std / (avg ? avg : 1)).toFixed(d.fixed) + "% of average)";
  },

  tpRecordTime(dataCSV) {
    var d = window.talosDebug;
    if (d.disabled)
      return;

    var collection = ("" + dataCSV).split(",").map(function(item) { return parseFloat(item); });
    var res = d.statsDisplay(collection);

    var warmup = (d.ignore >= 0) ? d.ignore : d.detectWarmup(collection);
    if (warmup >= 0) {
      res += "\n\nWarmup " + ((d.ignore >= 0) ? "requested: " : "auto-detected: ") + warmup;
      var warmedUp = collection.slice(warmup);
      if (warmup) {
        res += "\nAfter ignoring first " + (warmup > 1 ? (warmup + " items") : "item") + ":\n";
        res += d.statsDisplay(warmedUp);
      }
    } else {
      res += "\n\nWarmup auto-detection: Failed.";
    }

    if (d.displayData) {
      var disp = collection.map(function(item) {
                   return item.toFixed(d.fixed);
                 });
      if (warmup >= 0)
        disp.splice(warmup, 0, "[warmed-up:]");
      res += "\n\nRecorded:\n" + disp.join(", ");

      res += "\n\nStddev from item NN to last:\n";
      disp = collection.map(function(value, index) {
               return d.stddev(collection.slice(index), d.average(collection.slice(index))).toFixed(d.fixed);
             });
      if (warmup >= 0)
        disp.splice(warmup, 0, "[warmed-up:]");
      res += disp.join(", ");
    } else {
      res += "\n\n[set window.talosDebug.displayData=true for data]";
    }

    alert(res);
  }
}

// Enable testing outside of talos by providing an alternative report function.
if (typeof (tpRecordTime) === "undefined") {
  tpRecordTime = window.talosDebug.tpRecordTime;
}
