Cu.import("resource://gre/modules/NetUtil.jsm");

function run_test() {
  var base = NetUtil.newURI("http://www.example.com");
  var about1 = NetUtil.newURI("about:blank");
  var about2 = NetUtil.newURI("about:blank", null, base);

  var chan1 = NetUtil.newChannel({
    uri: about1,
    loadUsingSystemPrincipal: true 
  }).QueryInterface(Components.interfaces.nsIPropertyBag2);

  var chan2 = NetUtil.newChannel({
    uri: about2,
    loadUsingSystemPrincipal: true
  }).QueryInterface(Components.interfaces.nsIPropertyBag2);

  var haveProp = false;
  var propVal = null;
  try {
    propVal = chan1.getPropertyAsInterface("baseURI",
                                           Components.interfaces.nsIURI);
    haveProp = true;
  } catch (e if e.result == Components.results.NS_ERROR_NOT_AVAILABLE) {
    // Property shouldn't be there.
  }
  do_check_eq(propVal, null);
  do_check_eq(haveProp, false);
  do_check_eq(chan2.getPropertyAsInterface("baseURI",
                                           Components.interfaces.nsIURI),
              base);
}
