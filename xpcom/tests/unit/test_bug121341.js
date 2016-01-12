var Ci = Components.interfaces;
var Cu = Components.utils;
Cu.import("resource://gre/modules/NetUtil.jsm");

function run_test() {
  var ios = Components.classes["@mozilla.org/network/io-service;1"].
            getService(Components.interfaces.nsIIOService);
                      
  var dataFile = do_get_file("data/bug121341.properties");
  var channel = NetUtil.newChannel({
    uri: ios.newFileURI(dataFile, null, null),
    loadUsingSystemPrincipal: true
  });
  var inp = channel.open2();

  var properties = Components.classes["@mozilla.org/persistent-properties;1"].
                   createInstance(Components.interfaces.nsIPersistentProperties);
  properties.load(inp);

  var value;

  value = properties.getStringProperty("1");
  do_check_eq(value, "abc");

  value = properties.getStringProperty("2");
  do_check_eq(value, "xy");

  value = properties.getStringProperty("3");
  do_check_eq(value, "\u1234\t\r\n\u00AB\u0001\n");

  value = properties.getStringProperty("4");
  do_check_eq(value, "this is multiline property");

  value = properties.getStringProperty("5");
  do_check_eq(value, "this is another multiline property");

  value = properties.getStringProperty("6");
  do_check_eq(value, "test\u0036");

  value = properties.getStringProperty("7");
  do_check_eq(value, "yet another multiline propery");

  value = properties.getStringProperty("8");
  do_check_eq(value, "\ttest5\u0020");

  value = properties.getStringProperty("9");
  do_check_eq(value, " test6\t");

  value = properties.getStringProperty("10a\u1234b");
  do_check_eq(value, "c\uCDEFd");

  value = properties.getStringProperty("11");
  do_check_eq(value, "\uABCD");

  dataFile = do_get_file("data/bug121341-2.properties");

  var channel = NetUtil.newChannel({
    uri: ios.newFileURI(dataFile, null, null),
    loadUsingSystemPrincipal: true
  });
  inp = channel.open2();

  var properties2 = Components.classes["@mozilla.org/persistent-properties;1"].
                    createInstance(Components.interfaces.nsIPersistentProperties);
  try {
    properties2.load(inp);
    do_throw("load() didn't fail");
  }
  catch (e) {
  }
}
