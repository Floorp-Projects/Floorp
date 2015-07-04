// ----------------------------------------------------------------------------
// Tests installing two signed add-ons in the same trigger works.
// This verifies bug 453545
function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Signed XPI": TESTROOT + "signed.xpi",
    "Signed XPI 2": TESTROOT + "signed2.xpi",
    "Signed XPI 3": TESTROOT + "signed-no-o.xpi",
    "Signed XPI 4": TESTROOT + "signed-no-cn.xpi",
    "Signed XPI 5": TESTROOT + "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function get_item(items, url) {
  for (let item of items) {
    if (item.url == url)
      return item;
  }
  ok(false, "Item for " + url + " was not listed");
  return null;
}

function confirm_install(window) {

  var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"].
                       getService(Components.interfaces.nsIStringBundleService);
  var bundle = sbs.createBundle("chrome://mozapps/locale/xpinstall/xpinstallConfirm.properties");

  var expectedIntroString = bundle.formatStringFromName("itemWarnIntroMultiple", ["5"], 1);
  
  var introStringNode = window.document.getElementById("itemWarningIntro");
  is(introStringNode.textContent, expectedIntroString, "Should have the correct intro string");

  var items = window.document.getElementById("itemList").childNodes;
  is(items.length, 5, "Should be 5 items listed in the confirmation dialog");
  let item = get_item(items, TESTROOT + "signed.xpi");
  if (item) {
    is(item.name, "Signed XPI Test", "Should have seen the name from the trigger list");
    is(item.cert, "(Object Signer)", "Should have seen the signer");
    is(item.signed, "true", "Should have listed the item as signed");
  }
  item = get_item(items, TESTROOT + "signed2.xpi");
  if (item) {
    is(item.name, "Signed XPI Test", "Should have seen the name from the trigger list");
    is(item.cert, "(Object Signer)", "Should have seen the signer");
    is(item.signed, "true", "Should have listed the item as signed");
  }
  return true;
}

function install_ended(install, addon) {
  install.cancel();
}

function finish_test(count) {
  is(count, 5, "5 Add-ons should have been successfully installed");

  Services.perms.remove(makeURI("http://example.com"), "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
