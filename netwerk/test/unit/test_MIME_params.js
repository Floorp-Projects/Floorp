/**
 * Tests for parsing header fields using the syntax used in
 * Content-Disposition and Content-Type
 *
 * See also https://bugzilla.mozilla.org/show_bug.cgi?id=609667
 */
const Cr = Components.results;

var BS = '\\';
var DQUOTE = '"'; 

// Test array:
//  - element 0: "Content-Disposition" header to test
//  under RFC 2231 (email):
//  - element 1: correct value returned for disposition-type (empty param name)
//  - element 2: correct value for filename returned
//  under RFC 5987 (HTTP):
//  (note: 5987-mode not yet in use, see bug 601933)
//  - element 3: correct value returned for disposition-type (empty param name)
//  - element 4: correct value for filename returned 
//  
// 3 and 4 may be left out if they are identical

var tests = [
  // No filename parameter: return nothing
  ["attachment;",
   "attachment", Cr.NS_ERROR_INVALID_ARG],

  // basic
  ["attachment; filename=basic",
   "attachment", "basic"],

  // extended
  ["attachment; filename*=UTF-8''extended",
   "attachment", "extended"],

  // prefer extended to basic (bug 588781)
  ["attachment; filename=basic; filename*=UTF-8''extended",
   "attachment", "extended"],

  // prefer extended to basic (bug 588781)
  ["attachment; filename*=UTF-8''extended; filename=basic",
   "attachment", "extended"],

  // use first basic value (invalid; error recovery)
  ["attachment; filename=first; filename=wrong",
   "attachment", "first"],

  // old school bad HTTP servers: missing 'attachment' or 'inline'
  // (invalid; error recovery)
  ["filename=old",
   "filename=old", "old"],

  ["attachment; filename*=UTF-8''extended",
   "attachment", "extended"],

  // continuations not part of RFC 5987 (bug 610054)
  ["attachment; filename*0=foo; filename*1=bar",
   "attachment", "foobar",
   "attachment", Cr.NS_ERROR_INVALID_ARG],

  // Return first continuation (invalid; error recovery)
  ["attachment; filename*0=first; filename*0=wrong; filename=basic",
   "attachment", "first",
   "attachment", "basic"],

  // Only use correctly ordered continuations  (invalid; error recovery)
  ["attachment; filename*0=first; filename*1=second; filename*0=wrong",
   "attachment", "firstsecond",
   "attachment", Cr.NS_ERROR_INVALID_ARG],

  // prefer continuation to basic (unless RFC 5987)
  ["attachment; filename=basic; filename*0=foo; filename*1=bar",
   "attachment", "foobar",
   "attachment", "basic"],

  // Prefer extended to basic and/or (broken or not) continuation
  // (invalid; error recovery)
  ["attachment; filename=basic; filename*0=first; filename*0=wrong; filename*=UTF-8''extended",
   "attachment", "extended"],

  // RFC 2231 not clear on correct outcome: we prefer non-continued extended
  // (invalid; error recovery)
  ["attachment; filename=basic; filename*=UTF-8''extended; filename*0=foo; filename*1=bar",
   "attachment", "extended"],

  // Gaps should result in returning only value until gap hit
  // (invalid; error recovery)
  ["attachment; filename*0=foo; filename*2=bar",
   "attachment", "foo",
   "attachment", Cr.NS_ERROR_INVALID_ARG],

  // Don't allow leading 0's (*01) (invalid; error recovery)
  ["attachment; filename*0=foo; filename*01=bar",
   "attachment", "foo",
   "attachment", Cr.NS_ERROR_INVALID_ARG],

  // continuations should prevail over non-extended (unless RFC 5987)
  ["attachment; filename=basic; filename*0*=UTF-8''multi\r\n"
    + " filename*1=line\r\n" 
    + " filename*2*=%20extended",
   "attachment", "multiline extended",
   "attachment", "basic"],

  // Gaps should result in returning only value until gap hit
  // (invalid; error recovery)
  ["attachment; filename=basic; filename*0*=UTF-8''multi\r\n"
    + " filename*1=line\r\n" 
    + " filename*3*=%20extended",
   "attachment", "multiline",
   "attachment", "basic"],

  // First series, only please, and don't slurp up higher elements (*2 in this
  // case) from later series into earlier one (invalid; error recovery)
  ["attachment; filename=basic; filename*0*=UTF-8''multi\r\n"
    + " filename*1=line\r\n" 
    + " filename*0*=UTF-8''wrong\r\n"
    + " filename*1=bad\r\n"
    + " filename*2=evil",
   "attachment", "multiline",
   "attachment", "basic"],

  // RFC 2231 not clear on correct outcome: we prefer non-continued extended
  // (invalid; error recovery)
  ["attachment; filename=basic; filename*0=UTF-8''multi\r\n"
    + " filename*=UTF-8''extended\r\n"
    + " filename*1=line\r\n" 
    + " filename*2*=%20extended",
   "attachment", "extended"],

  // sneaky: if unescaped, make sure we leave UTF-8'' in value
  ["attachment; filename*0=UTF-8''unescaped\r\n"
    + " filename*1*=%20so%20includes%20UTF-8''%20in%20value", 
   "attachment", "UTF-8''unescaped so includes UTF-8'' in value",
   "attachment", Cr.NS_ERROR_INVALID_ARG],

  // sneaky: if unescaped, make sure we leave UTF-8'' in value
  ["attachment; filename=basic; filename*0=UTF-8''unescaped\r\n"
    + " filename*1*=%20so%20includes%20UTF-8''%20in%20value", 
   "attachment", "UTF-8''unescaped so includes UTF-8'' in value",
   "attachment", "basic"],

  // Prefer basic over invalid continuation
  // (invalid; error recovery)
  ["attachment; filename=basic; filename*1=multi\r\n"
    + " filename*2=line\r\n" 
    + " filename*3*=%20extended",
   "attachment", "basic"],

  // support digits over 10
  ["attachment; filename=basic; filename*0*=UTF-8''0\r\n"
    + " filename*1=1; filename*2=2;filename*3=3;filename*4=4;filename*5=5\r\n" 
    + " filename*6=6; filename*7=7;filename*8=8;filename*9=9;filename*10=a\r\n"
    + " filename*11=b; filename*12=c;filename*13=d;filename*14=e;filename*15=f\r\n",
   "attachment", "0123456789abcdef",
   "attachment", "basic"],

  // support digits over 10 (check ordering)
  ["attachment; filename=basic; filename*0*=UTF-8''0\r\n"
    + " filename*1=1; filename*2=2;filename*3=3;filename*4=4;filename*5=5\r\n" 
    + " filename*6=6; filename*7=7;filename*8=8;filename*9=9;filename*10=a\r\n"
    + " filename*11=b; filename*12=c;filename*13=d;filename*15=f;filename*14=e\r\n",
   "attachment", "0123456789abcd" /* should see the 'f', see bug 588414 */,
   "attachment", "basic"],

  // support digits over 10 (detect gaps)
  ["attachment; filename=basic; filename*0*=UTF-8''0\r\n"
    + " filename*1=1; filename*2=2;filename*3=3;filename*4=4;filename*5=5\r\n" 
    + " filename*6=6; filename*7=7;filename*8=8;filename*9=9;filename*10=a\r\n"
    + " filename*11=b; filename*12=c;filename*14=e\r\n",
   "attachment", "0123456789abc",
   "attachment", "basic"],

  // return nothing: invalid
  // (invalid; error recovery)
  ["attachment; filename*1=multi\r\n"
    + " filename*2=line\r\n" 
    + " filename*3*=%20extended",
   "attachment", Cr.NS_ERROR_INVALID_ARG],
   
  // Bug 272541: Empty disposition type treated as "attachment" 

  // sanity check
  ["attachment; filename=foo.html", 
   "attachment", "foo.html",
   "attachment", "foo.html"],

  // the actual bug
  ["; filename=foo.html", 
   Cr.NS_ERROR_FIRST_HEADER_FIELD_COMPONENT_EMPTY, "foo.html",
   Cr.NS_ERROR_FIRST_HEADER_FIELD_COMPONENT_EMPTY, "foo.html"],
  
  // regression check, but see bug 671204
  ["filename=foo.html", 
   "filename=foo.html", "foo.html",
   "filename=foo.html", "foo.html"],
   
  // Bug 419157: ensure that a MIME parameter with no charset information
  // fallbacks to Latin-1

  ["attachment;filename=IT839\x04\xB5(m8)2.pdf;",
   "attachment", "IT839\u0004\u00b5(m8)2.pdf"],

  // Bug 588389: unescaping backslashes in quoted string parameters
   
  // '\"', should be parsed as '"'  
  ["attachment; filename=" + DQUOTE + (BS + DQUOTE) + DQUOTE, 
   "attachment", DQUOTE],
  
  // 'a\"b', should be parsed as 'a"b'
  ["attachment; filename=" + DQUOTE + 'a' + (BS + DQUOTE) + 'b' + DQUOTE, 
   "attachment", "a" + DQUOTE + "b"],
  
  // '\x', should be parsed as 'x'
  ["attachment; filename=" + DQUOTE + (BS + "x") + DQUOTE, 
   "attachment", "x"],
   
  // test empty param (quoted-string)
  ["attachment; filename=" + DQUOTE + DQUOTE, 
   "attachment", ""],
  
  // test empty param 
  ["attachment; filename=", 
   "attachment", ""],    
  
  // Bug 651185: double quotes around 2231/5987 encoded param
  // Change reverted to backwards compat issues with various web services,
  // such as OWA (Bug 703015), plus similar problems in Thunderbird. If this
  // is tried again in the future, email probably needs to be special-cased.
  
  // sanity check
  ["attachment; filename*=utf-8''%41", 
   "attachment", "A"],

  // the actual bug   
  ["attachment; filename*=" + DQUOTE + "utf-8''%41" + DQUOTE, 
   "attachment", "A"],
  // previously with the fix for 651185:
  // "attachment", Cr.NS_ERROR_INVALID_ARG],

  // Bug 670333: Content-Disposition parser does not require presence of "="
  // in params
  
  // sanity check
  ["attachment; filename*=UTF-8''foo-%41.html", 
   "attachment", "foo-A.html"],

  // the actual bug
  ["attachment; filename *=UTF-8''foo-%41.html", 
   "attachment", Cr.NS_ERROR_INVALID_ARG],
   
  // the actual bug, without 2231/5987 encoding
  ["attachment; filename X", 
   "attachment", Cr.NS_ERROR_INVALID_ARG],

  // sanity check with WS on both sides
  ["attachment; filename = foo-A.html", 
   "attachment", "foo-A.html"],   

  // Bug 685192: in RFC2231/5987 encoding, a missing charset field should be
  // treated as error 

  // the actual bug
  ["attachment; filename*=''foo", 
   "attachment", "foo"],      
  // previously with the fix for 692574:
  // "attachment", Cr.NS_ERROR_INVALID_ARG],      

  // sanity check
  ["attachment; filename*=a''foo", 
   "attachment", "foo"],      

  // Bug 692574: RFC2231/5987 decoding should not tolerate missing single
  // quotes

  // one missing
  ["attachment; filename*=UTF-8'foo-%41.html", 
   "attachment", "foo-A.html"],
  // previously with the fix for 692574:
  // "attachment", Cr.NS_ERROR_INVALID_ARG],

  // both missing
  ["attachment; filename*=foo-%41.html", 
   "attachment","foo-A.html"],
  // previously with the fix for 692574:
  // "attachment", Cr.NS_ERROR_INVALID_ARG],

  // make sure fallback works
  ["attachment; filename*=UTF-8'foo-%41.html; filename=bar.html", 
   "attachment", "foo-A.html"],
  // previously with the fix for 692574:
  // "attachment", "bar.html"],

  // Bug 704989: add workaround for broken Outlook Web App (OWA)
  // attachment handling

  ["attachment; filename*=\"a%20b\"", 
   "attachment", "a b"],

  // Bug 717121: crash nsMIMEHeaderParamImpl::DoParameterInternal

  ["attachment; filename=\"", 
   "attachment", ""], 
];

function do_tests(whichRFC)
{
  var mhp = Components.classes["@mozilla.org/network/mime-hdrparam;1"]
                      .getService(Components.interfaces.nsIMIMEHeaderParam);

  var unused = { value : null };

  for (var i = 0; i < tests.length; ++i) {
    dump("Testing #" + i + ": " + tests[i] + "\n");

    // check disposition type
    var expectedDt = tests[i].length == 3 || whichRFC == 0 ? tests[i][1] : tests[i][3];

    try {
      var result;
      
      if (whichRFC == 0)
        result = mhp.getParameter(tests[i][0], "", "UTF-8", true, unused);
      else 
        result = mhp.getParameter5987(tests[i][0], "", "UTF-8", true, unused);

      do_check_eq(result, expectedDt);
    } 
    catch (e) {
      // Tests can also succeed by expecting to fail with given error code
      if (e.result) {
        // Allow following tests to run by catching exception from do_check_eq()
        try { 
          do_check_eq(e.result, expectedDt); 
        } catch(e) {}  
      }
      continue;
    }

    // check filename parameter
    var expectedFn = tests[i].length == 3 || whichRFC == 0 ? tests[i][2] : tests[i][4];

    try {
      var result;
      
      if (whichRFC == 0)
        result = mhp.getParameter(tests[i][0], "filename", "UTF-8", true, unused);
      else 
        result = mhp.getParameter5987(tests[i][0], "filename", "UTF-8", true, unused);

      do_check_eq(result, expectedFn);
    } 
    catch (e) {
      // Tests can also succeed by expecting to fail with given error code
      if (e.result) {
        // Allow following tests to run by catching exception from do_check_eq()
        try { 
          do_check_eq(e.result, expectedFn); 
        } catch(e) {}  
      }
      continue;
    }
  }
}

function run_test() {

  // Test RFC 2231
  do_tests(0);

  // Test RFC 5987
  do_tests(1);
}

