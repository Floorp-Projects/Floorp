Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function run_test() {
  var dataFile = do_get_file("data/bug121341.properties");
  var channel = NetUtil.newChannel({
    uri: Services.io.newFileURI(dataFile, null, null),
    loadUsingSystemPrincipal: true
  });
  var inp = channel.open2();

  var properties = Components.classes["@mozilla.org/persistent-properties;1"].
                   createInstance(Components.interfaces.nsIPersistentProperties);
  properties.load(inp);

  var value;

  value = properties.getStringProperty("1");
  Assert.equal(value, "abc");

  value = properties.getStringProperty("2");
  Assert.equal(value, "xy");

  value = properties.getStringProperty("3");
  Assert.equal(value, "\u1234\t\r\n\u00AB\u0001\n");

  value = properties.getStringProperty("4");
  Assert.equal(value, "this is multiline property");

  value = properties.getStringProperty("5");
  Assert.equal(value, "this is another multiline property");

  value = properties.getStringProperty("6");
  Assert.equal(value, "test\u0036");

  value = properties.getStringProperty("7");
  Assert.equal(value, "yet another multiline propery");

  value = properties.getStringProperty("8");
  Assert.equal(value, "\ttest5\u0020");

  value = properties.getStringProperty("9");
  Assert.equal(value, " test6\t");

  value = properties.getStringProperty("10a\u1234b");
  Assert.equal(value, "c\uCDEFd");

  value = properties.getStringProperty("11");
  Assert.equal(value, "\uABCD");

  dataFile = do_get_file("data/bug121341-2.properties");

  var channel2 = NetUtil.newChannel({
    uri: Services.io.newFileURI(dataFile, null, null),
    loadUsingSystemPrincipal: true
  });
  inp = channel2.open2();

  var properties2 = Components.classes["@mozilla.org/persistent-properties;1"].
                    createInstance(Components.interfaces.nsIPersistentProperties);
  try {
    properties2.load(inp);
    do_throw("load() didn't fail");
  } catch (e) {
  }
}
