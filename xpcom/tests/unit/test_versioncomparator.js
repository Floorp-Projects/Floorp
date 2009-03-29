// Versions to test listed in ascending order, none can be equal
var comparisons = [
  "0.9",
  "0.9.1",
  "1.0pre1",
  "1.0pre2",
  "1.0",
  "1.1pre",
  "1.1pre1a",
  "1.1pre1",
  "1.1pre10a",
  "1.1pre10",
  "1.1",
  "1.1.0.1",
  "1.1.1",
  "1.1.*",
  "1.*",
  "2.0",
  "2.1",
  "3.0.-1",
  "3.0"
];

// Every version in this list means the same version number
var equality = [
  "1.1pre",
  "1.1pre0",
  "1.0+"
];

function run_test()
{
  var vc = Components.classes["@mozilla.org/xpcom/version-comparator;1"]
                     .getService(Components.interfaces.nsIVersionComparator);

  for (var i = 0; i < comparisons.length; i++) {
    for (var j = 0; j < comparisons.length; j++) {
      var result = vc.compare(comparisons[i], comparisons[j]);
      if (i == j) {
        if (result != 0)
          do_throw(comparisons[i] + " should be the same as itself");
      }
      else if (i < j) {
        if (!(result < 0))
          do_throw(comparisons[i] + " should be less than " + comparisons[j]);
      }
      else if (!(result > 0)) {
        do_throw(comparisons[i] + " should be greater than " + comparisons[j]);
      }
    }
  }

  for (i = 0; i < equality.length; i++) {
    for (j = 0; j < equality.length; j++) {
      if (vc.compare(equality[i], equality[j]) != 0)
        do_throw(equality[i] + " should equal " + equality[j]);
    }
  }
}
