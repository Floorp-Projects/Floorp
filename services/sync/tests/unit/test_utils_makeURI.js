_("Make sure uri strings are converted to nsIURIs");
Cu.import("resource://services-sync/util.js");

// both the makeURI and makeURL functions are tested
// in this file

function run_test() {
  _test_makeURI();
  _test_makeURL();
}

function _test_makeURI() {
  _("Check http uris");
  let uri1 = "http://mozillalabs.com/";
  do_check_eq(Utils.makeURI(uri1).spec, uri1);
  let uri2 = "http://www.mozillalabs.com/";
  do_check_eq(Utils.makeURI(uri2).spec, uri2);
  let uri3 = "http://mozillalabs.com/path";
  do_check_eq(Utils.makeURI(uri3).spec, uri3);
  let uri4 = "http://mozillalabs.com/multi/path";
  do_check_eq(Utils.makeURI(uri4).spec, uri4);
  let uri5 = "http://mozillalabs.com/?query";
  do_check_eq(Utils.makeURI(uri5).spec, uri5);
  let uri6 = "http://mozillalabs.com/#hash";
  do_check_eq(Utils.makeURI(uri6).spec, uri6);

  _("Check https uris");
  let uris1 = "https://mozillalabs.com/";
  do_check_eq(Utils.makeURI(uris1).spec, uris1);
  let uris2 = "https://www.mozillalabs.com/";
  do_check_eq(Utils.makeURI(uris2).spec, uris2);
  let uris3 = "https://mozillalabs.com/path";
  do_check_eq(Utils.makeURI(uris3).spec, uris3);
  let uris4 = "https://mozillalabs.com/multi/path";
  do_check_eq(Utils.makeURI(uris4).spec, uris4);
  let uris5 = "https://mozillalabs.com/?query";
  do_check_eq(Utils.makeURI(uris5).spec, uris5);
  let uris6 = "https://mozillalabs.com/#hash";
  do_check_eq(Utils.makeURI(uris6).spec, uris6);

  _("Check chrome uris");
  let uric1 = "chrome://browser/content/browser.xul";
  do_check_eq(Utils.makeURI(uric1).spec, uric1);
  let uric2 = "chrome://browser/skin/browser.css";
  do_check_eq(Utils.makeURI(uric2).spec, uric2);
  let uric3 = "chrome://browser/locale/browser.dtd";
  do_check_eq(Utils.makeURI(uric3).spec, uric3);

  _("Check about uris");
  let uria1 = "about:weave";
  do_check_eq(Utils.makeURI(uria1).spec, uria1);
  let uria2 = "about:weave/";
  do_check_eq(Utils.makeURI(uria2).spec, uria2);
  let uria3 = "about:weave/path";
  do_check_eq(Utils.makeURI(uria3).spec, uria3);
  let uria4 = "about:weave/multi/path";
  do_check_eq(Utils.makeURI(uria4).spec, uria4);
  let uria5 = "about:weave/?query";
  do_check_eq(Utils.makeURI(uria5).spec, uria5);
  let uria6 = "about:weave/#hash";
  do_check_eq(Utils.makeURI(uria6).spec, uria6);

  _("Invalid uris are undefined");
  do_check_eq(Utils.makeURI("mozillalabs.com"), undefined);
  do_check_eq(Utils.makeURI("chrome://badstuff"), undefined);
  do_check_eq(Utils.makeURI("this is a test"), undefined);
}
