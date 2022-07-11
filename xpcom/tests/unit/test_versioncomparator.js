// Versions to test listed in ascending order, none can be equal
var comparisons = [
  "pre",
  // A number that is too large to be supported should be normalized to 0.
  String(0x1f0000000),
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
  "3.0",
];

// Every version in this list means the same version number
var equality = ["1.1pre", "1.1pre0", "1.0+"];

function run_test() {
  for (var i = 0; i < comparisons.length; i++) {
    for (var j = 0; j < comparisons.length; j++) {
      var result = Services.vc.compare(comparisons[i], comparisons[j]);
      if (i == j) {
        if (result != 0) {
          do_throw(comparisons[i] + " should be the same as itself");
        }
      } else if (i < j) {
        if (!(result < 0)) {
          do_throw(comparisons[i] + " should be less than " + comparisons[j]);
        }
      } else if (!(result > 0)) {
        do_throw(comparisons[i] + " should be greater than " + comparisons[j]);
      }
    }
  }

  for (i = 0; i < equality.length; i++) {
    for (j = 0; j < equality.length; j++) {
      if (Services.vc.compare(equality[i], equality[j]) != 0) {
        do_throw(equality[i] + " should equal " + equality[j]);
      }
    }
  }
}
