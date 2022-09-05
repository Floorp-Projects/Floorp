// test that things that are expected to be in gre-resources are still there

"use strict";

function wrapInputStream(input) {
  var nsIScriptableInputStream = Ci.nsIScriptableInputStream;
  var factory = Cc["@mozilla.org/scriptableinputstream;1"];
  var wrapper = factory.createInstance(nsIScriptableInputStream);
  wrapper.init(input);
  return wrapper;
}

function check_file(file) {
  var channel = NetUtil.newChannel({
    uri: "resource://gre-resources/" + file,
    loadUsingSystemPrincipal: true,
  });
  try {
    let instr = wrapInputStream(channel.open());
    Assert.ok(!!instr.read(1024).length);
  } catch (e) {
    do_throw("Failed to read " + file + " from gre-resources:" + e);
  }
}

function run_test() {
  for (let file of ["ua.css"]) {
    check_file(file);
  }
}
