/**
 * Print some debug message to the console. All arguments will be printed,
 * separated by spaces.
 *
 * @param [arg0, arg1, arg2, ...]
 *        Any number of arguments to print out
 * @usage _("Hello World") -> prints "Hello World"
 * @usage _(1, 2, 3) -> prints "1 2 3"
 */
let _ = function(some, debug, text, to) print(Array.slice(arguments).join(" "));

_("Make sure Utils.deepEquals correctly finds items that are deeply equal");
Components.utils.import("resource://weave/util.js");

function run_test() {
  let data = '[NaN, undefined, null, true, false, Infinity, 0, 1, "a", "b", {a: 1}, {a: "a"}, [{a: 1}], [{a: true}], {a: 1, b: 2}, [1, 2], [1, 2, 3]]';
  _("Generating two copies of data:", data);
  let d1 = eval(data);
  let d2 = eval(data);

  d1.forEach(function(a) {
    _("Testing", a, typeof a, JSON.stringify([a]));
    let numMatch = 0;

    d2.forEach(function(b) {
      if (Utils.deepEquals(a, b)) {
        numMatch++;
        _("Found a match", b, typeof b, JSON.stringify([b]));
      }
    });

    let expect = 1;
    if (isNaN(a) && typeof a == "number") {
      expect = 0;
      _("Checking NaN should result in no matches");
    }

    _("Making sure we found the correct # match:", expect);
    _("Actual matches:", numMatch);
    do_check_eq(numMatch, expect);
  });
}
