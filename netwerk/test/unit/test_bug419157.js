/**
 * Test for bug 419157: ensure that a MIME parameter with no charset information
 *  fallbacks to Latin-1
 */
const Cc = Components.classes;
const Ci = Components.interfaces;

const header = "Content-Disposition: attachment;filename=IT839\x04\xB5(m8)2.pdf;";
const expected = "IT839\u0004\u00b5(m8)2.pdf";

function run_test()
{
  var mhp = Cc["@mozilla.org/network/mime-hdrparam;1"].
    getService(Ci.nsIMIMEHeaderParam);

  var unused = { value : null };
  var result = null;

  try {
    result = mhp.getParameter(header, "filename", "UTF-8", true, unused);
  } catch (e) {}

  do_check_eq(result, expected);
}
