/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that permissions set in preferences are correctly imported but can
// be removed by the user.

const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

const XPI_MIMETYPE = "application/x-xpinstall";

function newPrincipal(uri) {
  return Services.scriptSecurityManager.createContentPrincipal(
    NetUtil.newURI(uri),
    {}
  );
}

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  Services.prefs.setCharPref(
    "xpinstall.whitelist.add",
    "https://test1.com,https://test2.com"
  );
  Services.prefs.setCharPref(
    "xpinstall.whitelist.add.36",
    "https://test3.com,https://www.test4.com"
  );
  Services.prefs.setCharPref(
    "xpinstall.whitelist.add.test5",
    "https://test5.com"
  );

  PermissionTestUtils.add(
    "https://www.test9.com",
    "install",
    Ci.nsIPermissionManager.ALLOW_ACTION
  );

  await promiseStartupManager();

  Assert.ok(
    !AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("http://test1.com")
    )
  );
  Assert.ok(
    AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://test1.com")
    )
  );
  Assert.ok(
    AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://www.test2.com")
    )
  );
  Assert.ok(
    AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://test3.com")
    )
  );
  Assert.ok(
    !AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://test4.com")
    )
  );
  Assert.ok(
    AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://www.test4.com")
    )
  );
  Assert.ok(
    !AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("http://www.test5.com")
    )
  );
  Assert.ok(
    AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://www.test5.com")
    )
  );

  Assert.ok(
    !AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("http://www.test6.com")
    )
  );
  Assert.ok(
    !AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://www.test6.com")
    )
  );
  Assert.ok(
    !AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://test7.com")
    )
  );
  Assert.ok(
    !AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://www.test8.com")
    )
  );

  // This should remain unaffected
  Assert.ok(
    !AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("http://www.test9.com")
    )
  );
  Assert.ok(
    AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://www.test9.com")
    )
  );

  Services.perms.removeAll();

  Assert.ok(
    !AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://test1.com")
    )
  );
  Assert.ok(
    !AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://www.test2.com")
    )
  );
  Assert.ok(
    !AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://test3.com")
    )
  );
  Assert.ok(
    !AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://www.test4.com")
    )
  );
  Assert.ok(
    !AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://www.test5.com")
    )
  );

  // Upgrade the application and verify that the permissions are still not there
  await promiseRestartManager("2");

  Assert.ok(
    !AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://test1.com")
    )
  );
  Assert.ok(
    !AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://www.test2.com")
    )
  );
  Assert.ok(
    !AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://test3.com")
    )
  );
  Assert.ok(
    !AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://www.test4.com")
    )
  );
  Assert.ok(
    !AddonManager.isInstallAllowed(
      XPI_MIMETYPE,
      newPrincipal("https://www.test5.com")
    )
  );
});
