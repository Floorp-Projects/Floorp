/**
 * Test for bug 651185: double quotes around 2231/5987 encoded param
 */

const Cr = Components.results

var BS = '\\';
var DQUOTE = '"'; 
 
var tests = [
                 [ /* sanity check */
                  "Content-Disposition: attachment; filename*=utf-8''%41", 
                  "A"],
                 [ /* the actual bug */
                  "Content-Disposition: attachment; filename*=" + DQUOTE + "utf-8''%41" + DQUOTE, 
                  Cr.NS_ERROR_INVALID_ARG],
                ];

function run_test() {

  var mhp = Components.classes["@mozilla.org/network/mime-hdrparam;1"]
                      .getService(Components.interfaces.nsIMIMEHeaderParam);

  var unused = { value : null };

  for (var i = 0; i < tests.length; ++i) {
    dump("Testing " + tests[i] + "\n");
    try {
      do_check_eq(mhp.getParameter(tests[i][0], "filename", "UTF-8", true, unused),
                  tests[i][1]);
    }
    catch (e) {
      do_check_eq(e.result, tests[i][1]);
    }
  }
}

