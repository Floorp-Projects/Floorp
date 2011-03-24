/**
 * Test for bug 588781: ensure that for a MIME parameter with both "filename"
 * and "filename*" we pick the RFC2231/5987 encoded form
 *
 * Note: RFC 5987 drops support for continuations (foo*0, foo*1*, etc.). 
 *
 * See also <http://greenbytes.de/tech/webdav/rfc5987.html#rfc.section.4.2>
 */

var succeed = [
  ["Content-Disposition: attachment; filename=basic; filename*=UTF-8''extended",
  "extended"],
  ["Content-Disposition: attachment; filename*=UTF-8''extended; filename=basic",
  "extended"],
  ["Content-Disposition: attachment; filename=basic",
  "basic"],
  ["Content-Disposition: attachment; filename*=UTF-8''extended",
  "extended"],
  ["Content-Disposition: attachment; filename*0=foo; filename*1=bar",
  "foobar"],
/* BROKEN: we prepend 'basic' to result
    ["Content-Disposition: attachment; filename=basic; filename*0=foo; filename*1=bar",
    "foobar"],
*/
  // RFC 2231 not clear on correct outcome: we prefer non-continued extended
  ["Content-Disposition: attachment; filename=basic; filename*=UTF-8''extended; filename*0=foo; filename*1=bar",
  "extended"],
/* BROKEN: not checking order yet 
  // Gaps should result in returning only value until gap hit
  ["Content-Disposition: attachment; filename*0=foo; filename*2=bar",
  "foo"],
*/
/* BROKEN: don't check for leading 0s yet
  // Don't handle leading 0's (*01)
  ["Content-Disposition: attachment; filename*0=foo; filename*01=bar",
  "foo"],
*/
  ["Content-Disposition: attachment; filename=basic; filename*0*=UTF-8''multi\r\n"
    + " filename*1=line\r\n" 
    + " filename*2*=%20extended",
  "multiline extended"],
/* BROKEN: not checking order yet 
  // Gaps should result in returning only value until gap hit
  ["Content-Disposition: attachment; filename=basic; filename*0*=UTF-8''multi\r\n"
    + " filename*1=line\r\n" 
    + " filename*3*=%20extended",
  "multiline"],
*/
  // RFC 2231 not clear on correct outcome: we prefer non-continued extended
  ["Content-Disposition: attachment; filename=basic; filename*0=UTF-8''multi\r\n"
    + " filename*=UTF-8''extended\r\n"
    + " filename*1=line\r\n" 
    + " filename*2*=%20extended",
  "extended"],
  // sneaky: if unescaped, make sure we leave UTF-8'' in value
  ["Content-Disposition: attachment; filename*0=UTF-8''unescaped\r\n"
    + " filename*1*=%20so%20includes%20UTF-8''%20in%20value", 
  "UTF-8''unescaped so includes UTF-8'' in value"],
/* BROKEN: we prepend 'basic' to result
  // sneaky: if unescaped, make sure we leave UTF-8'' in value
  ["Content-Disposition: attachment; filename=basic; filename*0=UTF-8''unescaped\r\n"
    + " filename*1*=%20so%20includes%20UTF-8''%20in%20value", 
  "UTF-8''unescaped so includes UTF-8'' in value"],
*/
/*  BROKEN: we append filename*1 to 'basic'
  // Also not sure if this is the spec'd behavior here:
  ["Content-Disposition: attachment; filename=basic; filename*1=multi\r\n"
    + " filename*2=line\r\n" 
    + " filename*3*=%20extended",
  "basic"],
*/
];

var broken = [
  ["Content-Disposition: attachment; filename*1=multi\r\n"
    + " filename*2=line\r\n" 
    + " filename*3*=%20extended",
  "param continuation must start from 0: should fail"],
];


function run_test() {

  var mhp = Components.classes["@mozilla.org/network/mime-hdrparam;1"]
                      .getService(Components.interfaces.nsIMIMEHeaderParam);

  var unused = { value : null };

  for (var i = 0; i < succeed.length; ++i) {
    dump("Testing " + succeed[i] + "\n");
    try {
      do_check_eq(mhp.getParameter(succeed[i][0], "filename", "UTF-8", true, unused),
                  succeed[i][1]);
    } catch (e) {}
  }

  // Check failure cases
  for (var i = 0; i < broken.length; ++i) {
    dump("Testing " + broken[i] + "\n");
    try {
      var result = mhp.getParameter(broken[i][0], "filename", "UTF-8", true, unused);
      // No exception?  Error. 
      do_check_eq(broken[i][1], "instead got: " + result);
    } catch (e) {
      // .result set if getParameter failed: check for correct error code
      if (e.result)
        do_check_eq(e.result, Components.results.NS_ERROR_OUT_OF_MEMORY);
    }
  }
}

