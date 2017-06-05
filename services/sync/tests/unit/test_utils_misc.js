_("Misc tests for utils.js");

add_test(function test_default_device_name() {
  // Note that head_helpers overrides getDefaultDeviceName - this test is
  // really just to ensure the actual implementation is sane - we can't
  // really check the value it uses is correct.
  // We are just hoping to avoid a repeat of bug 1369285.
  let def = Utils._orig_getDefaultDeviceName(); // make sure it doesn't throw.
  _("default value is " + def);
  ok(def.length > 0);

  // This is obviously tied to the implementation, but we want early warning
  // if any of these things fail.
  // We really want one of these 2 to provide a value.
  let hostname = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2).get("device") ||
                 Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService).myHostName;
  _("hostname is " + hostname);
  ok(hostname.length > 0);
  // the hostname should be in the default.
  ok(def.includes(hostname));
  // We expect the following to work as a fallback to the above.
  let fallback = Cc["@mozilla.org/network/protocol;1?name=http"].getService(Ci.nsIHttpProtocolHandler).oscpu;
  _("UA fallback is " + fallback);
  ok(fallback.length > 0);
  // the fallback should not be in the default
  ok(!def.includes(fallback));

  run_next_test();
});
