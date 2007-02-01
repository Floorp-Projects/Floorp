// Constructor
function Report(pages) {
  this.pages = pages;
  this.timeVals = new Array(pages.length);  // matrix of times
  for (var i = 0; i < this.timeVals.length; ++i) {
    this.timeVals[i] = new Array;
  }
}

// returns an object with the following properties:
//   min  : min value of array elements
//   max  : max value of array elements
//   mean : mean value of array elements
//   vari : variance computation
//   stdd : standard deviation, sqrt(vari)
//   indexOfMax : index of max element (the element that is
//                removed from the mean computation)
Report.prototype.getArrayStats = function(ary) {
  var r = {};
  r.min = ary[0];
  r.max = ary[0];
  r.indexOfMax = 0;
  var sum = 0;
  for (var i = 0; i < ary.length; ++i) {
    if (ary[i] < r.min) {
      r.min = ary[i];
    } else if (ary[i] > r.max) {
      r.max = ary[i];
      r.indexOfMax = i;
    }
    sum = sum + ary[i];
  }

  // median
  sorted_ary = ary.concat();
  sorted_ary.sort();
  // remove longest run
  sorted_ary.pop();
  if (sorted_ary.length%2) {
    r.median = sorted_ary[(sorted_ary.length-1)/2]; 
  }else{
    var n = Math.floor(sorted_ary.length / 2);
    r.median = (sorted_ary[n] + sorted_ary[n + 1]) / 2;
  }

  // ignore max value when computing mean and stddev
  r.mean = (sum - r.max) / (ary.length - 1);

  r.vari = 0;
  for (var i = 0; i < ary.length; ++i) {
    if (i == r.indexOfMax)
      continue;
    var d = r.mean - ary[i];
    r.vari = r.vari + d * d;
  }

  r.vari = r.vari / (ary.length - 1);
  r.stdd = Math.sqrt(r.vari);
  return r;
}

Report.prototype.getReport = function() {
  var all = new Array();
  var counter = 0;
  
  for (var i = 0; i < this.timeVals.length; ++i) {
    for (var j = 0; j < this.timeVals[i].length; ++j) {
      all[counter] = this.timeVals[i][j];
      ++counter;
    }
  }

  // avg and avg median are cumulative for all the pages
  var avgs = new Array();
  var medians = new Array();
  for (var i = 0; i < this.timeVals.length; ++i) {
     avgs[i] = this.getArrayStats(this.timeVals[i]).mean;
     medians[i] = this.getArrayStats(this.timeVals[i]).median;
  }
  var avg = this.getArrayStats(avgs).mean;
  var avgmed = this.getArrayStats(medians).mean;

  var r = this.getArrayStats(all);

  var report = '';

  report +=
    "(tinderbox dropping follows)\n"+
    "_x_x_mozilla_page_load,"+avgmed+","+r.max+","+r.min+"\n"+
    "_x_x_mozilla_page_load_details,avgmedian|"+avgmed+"|average|"+avg.toFixed(2)+"|minimum|"+r.min+"|maximum|"+r.max+"|stddev|"+r.stdd.toFixed(2)+":"

  for (var i = 0; i < this.timeVals.length; ++i) {
    r = this.getArrayStats(this.timeVals[i]);
    report +=
      '|'+
      i+';'+
      pages[i]+';'+
      r.median+';'+
      r.mean+';'+
      r.min+';'+
      r.max
    for (var j = 0; j < this.timeVals[i].length; ++j) {
      report += 
        ';'+this.timeVals[i][j]
    }
  } 
  return report;
}

Report.prototype.recordTime = function(pageIndex, ms) {
  this.timeVals[pageIndex].push(ms);
}
