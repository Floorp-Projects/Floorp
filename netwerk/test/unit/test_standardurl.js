const StandardURL = Components.Constructor("@mozilla.org/network/standard-url;1",
                                           "nsIStandardURL",
                                           "init");
const nsIStandardURL = Components.interfaces.nsIStandardURL;

function symmetricEquality(expect, a, b)
{
  /* Use if/else instead of |do_check_eq(expect, a.spec == b.spec)| so
     that we get the specs output on the console if the check fails.
   */
  if (expect) {
    /* Check all the sub-pieces too, since that can help with
       debugging cases when equals() returns something unexpected */
    /* We don't check port in the loop, because it can be defaulted in
       some cases. */
    ["spec", "prePath", "scheme", "userPass", "username", "password",
     "hostPort", "host", "path", "filePath", "param", "query",
     "ref", "directory", "fileName", "fileBaseName", "fileExtension"]
      .map(function(prop) {
	dump("Testing '"+ prop + "'\n");
	do_check_eq(a[prop], b[prop]);
      });  
  } else {
    do_check_neq(a.spec, b.spec);
  }
  do_check_eq(expect, a.equals(b));
  do_check_eq(expect, b.equals(a));
}

function stringToURL(str) {
  return (new StandardURL(nsIStandardURL.URLTYPE_AUTHORITY, 80,
			 str, "UTF-8", null))
         .QueryInterface(Components.interfaces.nsIURL);
}

function pairToURLs(pair) {
  do_check_eq(pair.length, 2);
  return pair.map(stringToURL);
}

function test_setEmptyPath()
{
  var pairs =
    [
     ["http://example.com", "http://example.com/tests/dom/tests"],
     ["http://example.com:80", "http://example.com/tests/dom/tests"],
     ["http://example.com:80/", "http://example.com/tests/dom/test"],
     ["http://example.com/", "http://example.com/tests/dom/tests"],
     ["http://example.com/a", "http://example.com/tests/dom/tests"],
     ["http://example.com:80/a", "http://example.com/tests/dom/tests"],
    ].map(pairToURLs);

  for each (var [provided, target] in pairs)
  {
    symmetricEquality(false, target, provided);

    provided.path = "";
    target.path = "";

    do_check_eq(provided.spec, target.spec);
    symmetricEquality(true, target, provided);
  }
}

function test_setQuery()
{
  var pairs =
    [
     ["http://example.com", "http://example.com/?foo"],
     ["http://example.com/bar", "http://example.com/bar?foo"],
     ["http://example.com#bar", "http://example.com/?foo#bar"],
     ["http://example.com/#bar", "http://example.com/?foo#bar"],
     ["http://example.com/?longerthanfoo#bar", "http://example.com/?foo#bar"],
     ["http://example.com/?longerthanfoo", "http://example.com/?foo"],
     /* And one that's nonempty but shorter than "foo" */
     ["http://example.com/?f#bar", "http://example.com/?foo#bar"],
     ["http://example.com/?f", "http://example.com/?foo"],
    ].map(pairToURLs);

  for each (var [provided, target] in pairs) {
    symmetricEquality(false, provided, target);

    provided.query = "foo";

    do_check_eq(provided.spec, target.spec);
    symmetricEquality(true, provided, target);
  }

  [provided, target] =
    ["http://example.com/#", "http://example.com/?foo#bar"].map(stringToURL);
  symmetricEquality(false, provided, target);
  provided.query = "foo";
  symmetricEquality(false, provided, target);

  var newProvided = Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService)
                              .newURI("#bar", null, provided)
                              .QueryInterface(Components.interfaces.nsIURL);

  do_check_eq(newProvided.spec, target.spec);
  symmetricEquality(true, newProvided, target);

}
		      
function test_setRef()
{
  var tests =
    [
     ["http://example.com",      "", "http://example.com/"],
     ["http://example.com:80",   "", "http://example.com:80/"],
     ["http://example.com:80/",  "", "http://example.com:80/"],
     ["http://example.com/",     "", "http://example.com/"],
     ["http://example.com/a",    "", "http://example.com/a"],
     ["http://example.com:80/a", "", "http://example.com:80/a"],

     ["http://example.com",      "x", "http://example.com/#x"],
     ["http://example.com:80",   "x", "http://example.com:80/#x"],
     ["http://example.com:80/",  "x", "http://example.com:80/#x"],
     ["http://example.com/",     "x", "http://example.com/#x"],
     ["http://example.com/a",    "x", "http://example.com/a#x"],
     ["http://example.com:80/a", "x", "http://example.com:80/a#x"],

     ["http://example.com",      "xx", "http://example.com/#xx"],
     ["http://example.com:80",   "xx", "http://example.com:80/#xx"],
     ["http://example.com:80/",  "xx", "http://example.com:80/#xx"],
     ["http://example.com/",     "xx", "http://example.com/#xx"],
     ["http://example.com/a",    "xx", "http://example.com/a#xx"],
     ["http://example.com:80/a", "xx", "http://example.com:80/a#xx"],

     ["http://example.com",      "xxxxxxxxxxxxxx", "http://example.com/#xxxxxxxxxxxxxx"],
     ["http://example.com:80",   "xxxxxxxxxxxxxx", "http://example.com:80/#xxxxxxxxxxxxxx"],
     ["http://example.com:80/",  "xxxxxxxxxxxxxx", "http://example.com:80/#xxxxxxxxxxxxxx"],
     ["http://example.com/",     "xxxxxxxxxxxxxx", "http://example.com/#xxxxxxxxxxxxxx"],
     ["http://example.com/a",    "xxxxxxxxxxxxxx", "http://example.com/a#xxxxxxxxxxxxxx"],
     ["http://example.com:80/a", "xxxxxxxxxxxxxx", "http://example.com:80/a#xxxxxxxxxxxxxx"],
    ];

  for each (var [before, ref, result] in tests)
  {
    /* Test1: starting with empty ref */
    var a = stringToURL(before);
    a.ref = ref;
    var b = stringToURL(result);

    do_check_eq(a.spec, b.spec);
    do_check_eq(ref, b.ref);
    symmetricEquality(true, a, b);

    /* Test2: starting with non-empty */
    a.ref = "yyyy";
    var c = stringToURL(before);
    c.ref = "yyyy";
    symmetricEquality(true, a, c);

    /* Test3: reset the ref */
    a.ref = "";
    symmetricEquality(true, a, stringToURL(before));

    /* Test4: verify again after reset */
    a.ref = ref;
    symmetricEquality(true, a, b);
  }
}

function run_test()
{
  test_setEmptyPath();
  test_setQuery();
  test_setRef();
}
