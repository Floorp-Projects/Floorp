/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(function test_getInstallSourceFromHost_helpers() {
  const sourceHostTestCases = [
    {
      host: "addons.allizom.org",
      installSourceFromHost: "test-host",
    },
    {
      host: "addons.mozilla.org",
      installSourceFromHost: "amo",
    },
    {
      host: "discovery.addons.mozilla.org",
      installSourceFromHost: "disco",
    },
    {
      host: "about:blank",
      installSourceFromHost: "unknown",
    },
    {
      host: "fake-extension-uuid",
      installSourceFromHost: "unknown",
    },
    {
      host: null,
      installSourceFromHost: "unknown",
    },
  ];

  for (let testCase of sourceHostTestCases) {
    let { host, installSourceFromHost } = testCase;

    equal(
      AddonManager.getInstallSourceFromHost(host),
      installSourceFromHost,
      `Got the expected result from getInstallFromHost for host ${host}`
    );
  }
});
