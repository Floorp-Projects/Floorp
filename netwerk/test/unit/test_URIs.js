Components.utils.import("resource://gre/modules/NetUtil.jsm");

function do_info(text, stack) {
  if (!stack)
    stack = Components.stack.caller;

  dump("TEST-INFO | " + stack.filename + " | [" + stack.name + " : " +
       stack.lineNumber + "] " + text + "\n");
}

function run_test()
{
  var tests = [
    { spec: "x-external:", nsIURL: false, nsINestedURI: false },
    { spec: "http://www.example.com/", nsIURL: true, nsINestedURI: false },
    { spec: "view-source:about:blank", nsIURL: false, nsINestedURI: true },
    { spec: "jar:resource://gre/chrome.toolkit.jar!/", nsIURL: true, nsINestedURI: true }
  ];

  tests.forEach(function(aTest) {
    var URI = NetUtil.newURI(aTest.spec);
    do_info("testing " + aTest.spec + " equals " + aTest.spec);
    do_check_true(URI.equals(URI.clone()));
    do_info("testing " + aTest.spec + " instanceof nsIURL");
    do_check_eq(URI instanceof Components.interfaces.nsIURL, aTest.nsIURL);
    do_info("testing " + aTest.spec + " instanceof nsINestedURI");
    do_check_eq(URI instanceof Components.interfaces.nsINestedURI, aTest.nsINestedURI);
  });
}
