/**
 * Test for bug 588389: unescaping backslashes in quoted string parameters
 */

var BS = '\\';
var DQUOTE = '"'; 
 
var reference = [
                 [ // '\"', should be parsed as '"'
                  "Content-Disposition: attachment; foobar=" + DQUOTE + (BS + DQUOTE) + DQUOTE, 
                  DQUOTE],
                 [ // 'a\"b', should be parsed as 'a"b'
                  "Content-Disposition: attachment; foobar=" + DQUOTE + 'a' + (BS + DQUOTE) + 'b' + DQUOTE, 
                  'a' + DQUOTE + 'b'],
                 [ // '\x', should be parsed as 'x'
                  "Content-Disposition: attachment; foobar=" + DQUOTE + (BS + "x") + DQUOTE, 
                  "x"],
                 [ // test empty param (quoted-string)
                  "Content-Disposition: attachment; foobar=" + DQUOTE + DQUOTE, 
                  ""],
                 [ // test empty param
                  "Content-Disposition: attachment; foobar=", 
                  ""],
                ];

function run_test() {

  var mhp = Components.classes["@mozilla.org/network/mime-hdrparam;1"]
                      .getService(Components.interfaces.nsIMIMEHeaderParam);

  var unused = { value : null };

  for (var i = 0; i < reference.length; ++i) {
    dump("Testing " + reference[i] + "\n");
    do_check_eq(mhp.getParameter(reference[i][0], "foobar", "UTF-8", true, unused),
                reference[i][1]);
  }
}

