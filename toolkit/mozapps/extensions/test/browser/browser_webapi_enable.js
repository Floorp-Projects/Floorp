const TESTPAGE = `${SECURE_TESTROOT}webapi_addon_listener.html`;

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webapi.testing", true]],
  });
});

async function getListenerEvents(browser) {
  let result = await SpecialPowers.spawn(browser, [], async function() {
    return content.document.getElementById("result").textContent;
  });

  return result.split("\n").map(JSON.parse);
}

const ID = "test@tests.mozilla.org";

let provider = new MockProvider();
provider.createAddons([
  {
    id: ID,
    name: "Test add-on",
    operationsRequiringRestart: AddonManager.OP_NEED_RESTART_NONE,
  },
]);

// Test disable and enable from content
add_task(async function() {
  await BrowserTestUtils.withNewTab(TESTPAGE, async function(browser) {
    let addon = await promiseAddonByID(ID);
    isnot(addon, null, "Test addon exists");
    is(addon.userDisabled, false, "addon is enabled");

    // Disable the addon from content.
    await SpecialPowers.spawn(browser, [], async function() {
      return content.navigator.mozAddonManager
        .getAddonByID("test@tests.mozilla.org")
        .then(addon => addon.setEnabled(false));
    });

    let events = await getListenerEvents(browser);
    let expected = [
      { id: ID, event: "onDisabling" },
      { id: ID, event: "onDisabled" },
    ];
    Assert.deepEqual(events, expected, "Got expected disable events");

    // Enable the addon from content.
    await SpecialPowers.spawn(browser, [], async function() {
      return content.navigator.mozAddonManager
        .getAddonByID("test@tests.mozilla.org")
        .then(addon => addon.setEnabled(true));
    });

    events = await getListenerEvents(browser);
    expected = expected.concat([
      { id: ID, event: "onEnabling" },
      { id: ID, event: "onEnabled" },
    ]);
    Assert.deepEqual(events, expected, "Got expected enable events");
  });
});
