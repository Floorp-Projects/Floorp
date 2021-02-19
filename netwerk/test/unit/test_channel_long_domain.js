// Tests that domains longer than 253 characters fail to load when pref is true

add_task(async function test_long_domain_fails() {
  Services.prefs.setBoolPref("network.dns.limit_253_chars", true);
  let domain = "http://" + "a".repeat(254);

  let req = await new Promise(resolve => {
    let chan = NetUtil.newChannel({
      uri: domain,
      loadUsingSystemPrincipal: true,
    });
    chan.asyncOpen(new ChannelListener(resolve, null, CL_EXPECT_FAILURE));
  });
  Assert.equal(req.status, Cr.NS_ERROR_UNKNOWN_HOST, "Request should fail");

  Services.prefs.clearUserPref("network.dns.limit_253_chars");
});
