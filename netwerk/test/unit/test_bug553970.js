const Cc = Components.classes;

function makeURL(spec) {
  return Cc["@mozilla.org/network/io-service;1"].
           getService(Components.interfaces.nsIIOService).
           newURI(spec, null, null).
           QueryInterface(Components.interfaces.nsIURL);
}

// Checks that nsIURL::GetRelativeSpec does what it claims to do.
function run_test() {

  // Elements of tests have the form [this.spec, aURIToCompare.spec, expectedResult].
  let tests = [
    ["http://mozilla.org/",  "http://www.mozilla.org/", "http://www.mozilla.org/"],
    ["http://mozilla.org/",  "http://www.mozilla.org",  "http://www.mozilla.org/"],
    ["http://foo.com/bar/",  "http://foo.com:80/bar/",  ""                       ],
    ["http://foo.com/",      "http://foo.com/a.htm#b",  "a.htm#b"                ],
    ["http://foo.com/a/b/",  "http://foo.com/c",        "../../c"                ],
  ];

  for (var i = 0; i < tests.length; i++) {
    let url1 = makeURL(tests[i][0]);
    let url2 = makeURL(tests[i][1]);
    let expected = tests[i][2];
    do_check_eq(expected, url1.getRelativeSpec(url2));
  }
}
