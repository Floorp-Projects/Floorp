const StandardURL = Components.Constructor("@mozilla.org/network/standard-url;1",
                                           "nsIStandardURL",
                                           "init");
const nsIStandardURL = Components.interfaces.nsIStandardURL;

function symmetricEquality(expect, a, b)
{
  do_check_eq(expect, a.equals(b));
  do_check_eq(expect, b.equals(a));
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
    ];

  for (var i = 0; i < pairs.length; i++)
  {
    var provided = new StandardURL(nsIStandardURL.URLTYPE_AUTHORITY, 80,
                                   pairs[i][0],
                                   "UTF-8",
                                   null);

    var target = new StandardURL(nsIStandardURL.URLTYPE_AUTHORITY, 80,
                                 pairs[i][1],
                                 "UTF-8", null);

    provided.QueryInterface(Components.interfaces.nsIURI);
    target.QueryInterface(Components.interfaces.nsIURI);

    symmetricEquality(false, target, provided);

    provided.path = "";
    target.path = "";

    do_check_eq(provided.spec, target.spec);
    symmetricEquality(true, target, provided);
  }
}

function run_test()
{
  test_setEmptyPath();
}
