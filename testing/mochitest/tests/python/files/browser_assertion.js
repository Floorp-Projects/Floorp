function test() {
  const Cc = SpecialPowers.Cc;
  const Ci = SpecialPowers.Ci;
  let debug = Cc["@mozilla.org/xpcom/debug;1"].getService(Ci.nsIDebug2);
  debug.assertion("failed assertion check", "false", "test_assertion.js", 15);
  ok(true, "Should pass");
}