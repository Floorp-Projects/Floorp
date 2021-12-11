/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

_("Make sure uri strings are converted to nsIURIs");

function run_test() {
  _test_makeURI();
}

function _test_makeURI() {
  _("Check http uris");
  let uri1 = "http://mozillalabs.com/";
  Assert.equal(CommonUtils.makeURI(uri1).spec, uri1);
  let uri2 = "http://www.mozillalabs.com/";
  Assert.equal(CommonUtils.makeURI(uri2).spec, uri2);
  let uri3 = "http://mozillalabs.com/path";
  Assert.equal(CommonUtils.makeURI(uri3).spec, uri3);
  let uri4 = "http://mozillalabs.com/multi/path";
  Assert.equal(CommonUtils.makeURI(uri4).spec, uri4);
  let uri5 = "http://mozillalabs.com/?query";
  Assert.equal(CommonUtils.makeURI(uri5).spec, uri5);
  let uri6 = "http://mozillalabs.com/#hash";
  Assert.equal(CommonUtils.makeURI(uri6).spec, uri6);

  _("Check https uris");
  let uris1 = "https://mozillalabs.com/";
  Assert.equal(CommonUtils.makeURI(uris1).spec, uris1);
  let uris2 = "https://www.mozillalabs.com/";
  Assert.equal(CommonUtils.makeURI(uris2).spec, uris2);
  let uris3 = "https://mozillalabs.com/path";
  Assert.equal(CommonUtils.makeURI(uris3).spec, uris3);
  let uris4 = "https://mozillalabs.com/multi/path";
  Assert.equal(CommonUtils.makeURI(uris4).spec, uris4);
  let uris5 = "https://mozillalabs.com/?query";
  Assert.equal(CommonUtils.makeURI(uris5).spec, uris5);
  let uris6 = "https://mozillalabs.com/#hash";
  Assert.equal(CommonUtils.makeURI(uris6).spec, uris6);

  _("Check chrome uris");
  let uric1 = "chrome://browser/content/browser.xhtml";
  Assert.equal(CommonUtils.makeURI(uric1).spec, uric1);
  let uric2 = "chrome://browser/skin/browser.css";
  Assert.equal(CommonUtils.makeURI(uric2).spec, uric2);

  _("Check about uris");
  let uria1 = "about:weave";
  Assert.equal(CommonUtils.makeURI(uria1).spec, uria1);
  let uria2 = "about:weave/";
  Assert.equal(CommonUtils.makeURI(uria2).spec, uria2);
  let uria3 = "about:weave/path";
  Assert.equal(CommonUtils.makeURI(uria3).spec, uria3);
  let uria4 = "about:weave/multi/path";
  Assert.equal(CommonUtils.makeURI(uria4).spec, uria4);
  let uria5 = "about:weave/?query";
  Assert.equal(CommonUtils.makeURI(uria5).spec, uria5);
  let uria6 = "about:weave/#hash";
  Assert.equal(CommonUtils.makeURI(uria6).spec, uria6);

  _("Invalid uris are undefined");
  Assert.equal(CommonUtils.makeURI("mozillalabs.com"), undefined);
  Assert.equal(CommonUtils.makeURI("chrome://badstuff"), undefined);
  Assert.equal(CommonUtils.makeURI("this is a test"), undefined);
}
