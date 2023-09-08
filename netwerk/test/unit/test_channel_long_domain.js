// Tests that domains longer than 253 characters fail to load when pref is true

add_task(async function test_long_domain_fails() {
  let domain = "http://" + "a".repeat(254);

  let req = await new Promise(resolve => {
    let chan = NetUtil.newChannel({
      uri: domain,
      loadUsingSystemPrincipal: true,
    });
    chan.asyncOpen(new ChannelListener(resolve, null, CL_EXPECT_FAILURE));
  });
  Assert.equal(req.status, Cr.NS_ERROR_UNKNOWN_HOST, "Request should fail");
});
