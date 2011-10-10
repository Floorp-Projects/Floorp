/**
 * Test for bug 588781: ensure that for a MIME parameter with both "filename"
 * and "filename*" we pick the RFC2231/5987 encoded form
 *
 * Note: RFC 5987 drops support for continuations (foo*0, foo*1*, etc.). 
 *
 * See also <http://greenbytes.de/tech/webdav/rfc5987.html#rfc.section.4.2>
 */
const Cr = Components.results;

// Test array:
//  - element 0: "Content-Disposition" header to test for 'filename' parameter
//  - element 1: correct value returned under RFC 2231 (email)
//  - element 2: correct value returned under RFC 5987 (HTTP)

var tests = [
  // No filename parameter: return nothing
  ["attachment;", 
    Cr.NS_ERROR_INVALID_ARG, Cr.NS_ERROR_INVALID_ARG],

  // basic
  ["attachment; filename=basic",
   "basic", "basic"],

  // extended
  ["attachment; filename*=UTF-8''extended",
   "extended", "extended"],

  // prefer extended to basic
  ["attachment; filename=basic; filename*=UTF-8''extended",
   "extended", "extended"],

  // prefer extended to basic
  ["attachment; filename*=UTF-8''extended; filename=basic",
   "extended", "extended"],

  // use first basic value 
  ["attachment; filename=first; filename=wrong",
   "first", "first"],

  // old school bad HTTP servers: missing 'attachment' or 'inline'
  ["filename=old",
   "old", "old"],

  ["attachment; filename*=UTF-8''extended",
   "extended", "extended"],

  ["attachment; filename*0=foo; filename*1=bar",
   "foobar", Cr.NS_ERROR_INVALID_ARG],

  // Return first continuation
  ["attachment; filename*0=first; filename*0=wrong; filename=basic",
   "first", "basic"],

  // Only use correctly ordered continuations
  ["attachment; filename*0=first; filename*1=second; filename*0=wrong",
   "firstsecond", Cr.NS_ERROR_INVALID_ARG],

  // prefer continuation to basic (unless RFC 5987)
  ["attachment; filename=basic; filename*0=foo; filename*1=bar",
   "foobar", "basic"],

  // Prefer extended to basic and/or (broken or not) continuation
  ["attachment; filename=basic; filename*0=first; filename*0=wrong; filename*=UTF-8''extended",
   "extended", "extended"],

  // RFC 2231 not clear on correct outcome: we prefer non-continued extended
  ["attachment; filename=basic; filename*=UTF-8''extended; filename*0=foo; filename*1=bar",
   "extended", "extended"],

  // Gaps should result in returning only value until gap hit
  ["attachment; filename*0=foo; filename*2=bar",
   "foo", Cr.NS_ERROR_INVALID_ARG],

  // Don't allow leading 0's (*01)
  ["attachment; filename*0=foo; filename*01=bar",
   "foo", Cr.NS_ERROR_INVALID_ARG],

  // continuations should prevail over non-extended (unless RFC 5987)
  ["attachment; filename=basic; filename*0*=UTF-8''multi\r\n"
    + " filename*1=line\r\n" 
    + " filename*2*=%20extended",
   "multiline extended", "basic"],

  // Gaps should result in returning only value until gap hit
  ["attachment; filename=basic; filename*0*=UTF-8''multi\r\n"
    + " filename*1=line\r\n" 
    + " filename*3*=%20extended",
   "multiline", "basic"],

  // First series, only please, and don't slurp up higher elements (*2 in this
  // case) from later series into earlier one
  ["attachment; filename=basic; filename*0*=UTF-8''multi\r\n"
    + " filename*1=line\r\n" 
    + " filename*0*=UTF-8''wrong\r\n"
    + " filename*1=bad\r\n"
    + " filename*2=evil",
   "multiline", "basic"],

  // RFC 2231 not clear on correct outcome: we prefer non-continued extended
  ["attachment; filename=basic; filename*0=UTF-8''multi\r\n"
    + " filename*=UTF-8''extended\r\n"
    + " filename*1=line\r\n" 
    + " filename*2*=%20extended",
   "extended", "extended"],

  // sneaky: if unescaped, make sure we leave UTF-8'' in value
  ["attachment; filename*0=UTF-8''unescaped\r\n"
    + " filename*1*=%20so%20includes%20UTF-8''%20in%20value", 
   "UTF-8''unescaped so includes UTF-8'' in value", Cr.NS_ERROR_INVALID_ARG],

  // sneaky: if unescaped, make sure we leave UTF-8'' in value
  ["attachment; filename=basic; filename*0=UTF-8''unescaped\r\n"
    + " filename*1*=%20so%20includes%20UTF-8''%20in%20value", 
   "UTF-8''unescaped so includes UTF-8'' in value", "basic"],

  // Prefer basic over invalid continuation
  ["attachment; filename=basic; filename*1=multi\r\n"
    + " filename*2=line\r\n" 
    + " filename*3*=%20extended",
   "basic", "basic"],

  // support digits over 10
  ["attachment; filename=basic; filename*0*=UTF-8''0\r\n"
    + " filename*1=1; filename*2=2;filename*3=3;filename*4=4;filename*5=5\r\n" 
    + " filename*6=6; filename*7=7;filename*8=8;filename*9=9;filename*10=a\r\n"
    + " filename*11=b; filename*12=c;filename*13=d;filename*14=e;filename*15=f\r\n",
   "0123456789abcdef", "basic"],

  // support digits over 10 (check ordering)
  ["attachment; filename=basic; filename*0*=UTF-8''0\r\n"
    + " filename*1=1; filename*2=2;filename*3=3;filename*4=4;filename*5=5\r\n" 
    + " filename*6=6; filename*7=7;filename*8=8;filename*9=9;filename*10=a\r\n"
    + " filename*11=b; filename*12=c;filename*13=d;filename*15=f;filename*14=e\r\n",
   "0123456789abcd" /* should see the 'f', see bug 588414 */, "basic"],

  // support digits over 10 (detect gaps)
  ["attachment; filename=basic; filename*0*=UTF-8''0\r\n"
    + " filename*1=1; filename*2=2;filename*3=3;filename*4=4;filename*5=5\r\n" 
    + " filename*6=6; filename*7=7;filename*8=8;filename*9=9;filename*10=a\r\n"
    + " filename*11=b; filename*12=c;filename*14=e\r\n",
   "0123456789abc", "basic"],

  // return nothing: invalid
  ["attachment; filename*1=multi\r\n"
    + " filename*2=line\r\n" 
    + " filename*3*=%20extended",
   Cr.NS_ERROR_INVALID_ARG, Cr.NS_ERROR_INVALID_ARG],

];

function do_tests(whichRFC)
{
  var mhp = Components.classes["@mozilla.org/network/mime-hdrparam;1"]
                      .getService(Components.interfaces.nsIMIMEHeaderParam);

  var unused = { value : null };

  for (var i = 0; i < tests.length; ++i) {
    dump("Testing " + tests[i] + "\n");
    try {
      var result;
      if (whichRFC == 1)
        result = mhp.getParameter(tests[i][0], "filename", "UTF-8", true, unused);
      else 
        result = mhp.getParameter5987(tests[i][0], "filename", "UTF-8", true, unused);
      do_check_eq(result, tests[i][whichRFC]);
    } 
    catch (e) {
      // Tests can also succeed by expecting to fail with given error code
      if (e.result) {
        // Allow following tests to run by catching exception from do_check_eq()
        try { 
          do_check_eq(e.result, tests[i][whichRFC]); 
        } catch(e) {}  
      }
      continue;
    }
  }
}

function run_test() {

  // Test RFC 2231
  do_tests(1);

  // Test RFC 5987
  do_tests(2);
}

