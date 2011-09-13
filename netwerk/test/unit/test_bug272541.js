/**
 * Test for bug 272541: - Empty disposition type treated as "attachment" 
 */

const Cr = Components.results

var tests = [
                 [ /* the actual bug */
                  "; filename=foo.html", 
                  Cr.NS_ERROR_FIRST_HEADER_FIELD_COMPONENT_EMPTY],
                 [ /* regression check, but see bug 671204 */
                  "filename=foo.html", 
                  "filename=foo.html"],
                 [ /* sanity check */
                  "attachment; filename=foo.html", 
                  "attachment"],
                ];

function run_test() {

  var mhp = Components.classes["@mozilla.org/network/mime-hdrparam;1"]
                      .getService(Components.interfaces.nsIMIMEHeaderParam);

  var unused = { value : null };

  for (var i = 0; i < tests.length; ++i) {
    dump("Testing " + tests[i] + "\n");
    try {
      do_check_eq(mhp.getParameter(tests[i][0], "", "UTF-8", true, unused),
                  tests[i][1]);
    }
    catch (e) {
      do_check_eq(e.result, tests[i][1]);
    }
  }
}

