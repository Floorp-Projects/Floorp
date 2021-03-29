function test() {
  const Cc = SpecialPowers.Cc;
  const Ci = SpecialPowers.Ci;
  let debug = Cc["@mozilla.org/xpcom/debug;1"].getService(Ci.nsIDebug2);
  debug.abort("test_crash.js", 5);
  ok(false, "Should pass");
}