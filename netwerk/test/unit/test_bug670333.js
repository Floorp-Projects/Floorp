/**
 * Test for bug 670333: Content-Disposition parser does not require presence of "=" in params
 */

const Cr = Components.results

var BS = '\\';
var DQUOTE = '"'; 
 
var tests = [
                 [ /* sanity check */
                  "Content-Disposition: attachment; filename*=UTF-8''foo-%41.html", 
                  "foo-A.html"],
                 [ /* the actual bug */
                  "Content-Disposition: attachment; filename *=UTF-8''foo-%41.html", 
                  Cr.NS_ERROR_INVALID_ARG],
                 [ /* the actual bug, without 2231/5987 encoding */
                  "Content-Disposition: attachment; filename X", 
                  Cr.NS_ERROR_INVALID_ARG],
                 [ /* sanity check with WS on both sides */
                  "Content-Disposition: attachment; filename = foo-A.html", 
                  "foo-A.html"],
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

