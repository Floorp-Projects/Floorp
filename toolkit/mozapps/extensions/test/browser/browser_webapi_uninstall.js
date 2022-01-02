/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TESTPAGE = `${SECURE_TESTROOT}webapi_checkavailable.html`;

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webapi.testing", true]],
  });
});

function testWithAPI(task) {
  return async function() {
    await BrowserTestUtils.withNewTab(TESTPAGE, task);
  };
}

function API_uninstallByID(browser, id) {
  return SpecialPowers.spawn(browser, [id], async function(id) {
    let addon = await content.navigator.mozAddonManager.getAddonByID(id);

    let result = await addon.uninstall();
    return result;
  });
}

add_task(
  testWithAPI(async function(browser) {
    const ID1 = "addon1@tests.mozilla.org";
    const ID2 = "addon2@tests.mozilla.org";
    const ID3 = "addon3@tests.mozilla.org";

    let provider = new MockProvider();

    provider.addAddon(new MockAddon(ID1, "Test add-on 1", "extension", 0));
    provider.addAddon(new MockAddon(ID2, "Test add-on 2", "extension", 0));

    let addon = new MockAddon(ID3, "Test add-on 3", "extension", 0);
    addon.permissions &= ~AddonManager.PERM_CAN_UNINSTALL;
    provider.addAddon(addon);

    let [a1, a2, a3] = await promiseAddonsByIDs([ID1, ID2, ID3]);
    isnot(a1, null, "addon1 is installed");
    isnot(a2, null, "addon2 is installed");
    isnot(a3, null, "addon3 is installed");

    let result = await API_uninstallByID(browser, ID1);
    is(result, true, "uninstall of addon1 succeeded");

    [a1, a2, a3] = await promiseAddonsByIDs([ID1, ID2, ID3]);
    is(a1, null, "addon1 is uninstalled");
    isnot(a2, null, "addon2 is still installed");

    result = await API_uninstallByID(browser, ID2);
    is(result, true, "uninstall of addon2 succeeded");
    [a2] = await promiseAddonsByIDs([ID2]);
    is(a2, null, "addon2 is uninstalled");

    await Assert.rejects(
      API_uninstallByID(browser, ID3),
      /Addon cannot be uninstalled/,
      "Unable to uninstall addon"
    );

    // Cleanup addon3
    a3.permissions |= AddonManager.PERM_CAN_UNINSTALL;
    await a3.uninstall();
    [a3] = await promiseAddonsByIDs([ID3]);
    is(a3, null, "addon3 is uninstalled");
  })
);
