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

let gProvider = new MockProvider();

let addons = gProvider.createAddons([
  {
    id: "addon1@tests.mozilla.org",
    name: "Test add-on 1",
    version: "2.1",
    description: "Short description",
    type: "extension",
    userDisabled: false,
    isActive: true,
  },
  {
    id: "addon2@tests.mozilla.org",
    name: "Test add-on 2",
    version: "5.3.7ab",
    description: null,
    type: "theme",
    userDisabled: false,
    isActive: false,
  },
  {
    id: "addon3@tests.mozilla.org",
    name: "Test add-on 3",
    version: "1",
    description: "Longer description",
    type: "extension",
    userDisabled: true,
    isActive: false,
  },
  {
    id: "addon4@tests.mozilla.org",
    name: "Test add-on 4",
    version: "1",
    description: "Longer description",
    type: "extension",
    userDisabled: false,
    isActive: true,
  },
]);

addons[3].permissions &= ~AddonManager.PERM_CAN_UNINSTALL;

function API_getAddonByID(browser, id) {
  return SpecialPowers.spawn(browser, [id], async function(id) {
    let addon = await content.navigator.mozAddonManager.getAddonByID(id);
    let addonDetails = {};
    for (let prop in addon) {
      addonDetails[prop] = addon[prop];
    }
    // We can't send native objects back so clone its properties.
    return JSON.parse(JSON.stringify(addonDetails));
  });
}

add_task(
  testWithAPI(async function(browser) {
    function compareObjects(web, real) {
      ok(
        !!Object.keys(web).length,
        "Got a valid mozAddonManager addon object dump"
      );

      for (let prop of Object.keys(web)) {
        let webVal = web[prop];
        let realVal = real[prop];

        switch (prop) {
          case "isEnabled":
            realVal = !real.userDisabled;
            break;

          case "canUninstall":
            realVal = Boolean(
              real.permissions & AddonManager.PERM_CAN_UNINSTALL
            );
            break;
        }

        // null and undefined don't compare well so stringify them first
        if (realVal === null || realVal === undefined) {
          realVal = `${realVal}`;
          webVal = `${webVal}`;
        }

        is(
          webVal,
          realVal,
          `Property ${prop} should have the right value in add-on ${real.id}`
        );
      }
    }

    let [a1, a2, a3] = await promiseAddonsByIDs([
      "addon1@tests.mozilla.org",
      "addon2@tests.mozilla.org",
      "addon3@tests.mozilla.org",
    ]);
    let w1 = await API_getAddonByID(browser, "addon1@tests.mozilla.org");
    let w2 = await API_getAddonByID(browser, "addon2@tests.mozilla.org");
    let w3 = await API_getAddonByID(browser, "addon3@tests.mozilla.org");

    compareObjects(w1, a1);
    compareObjects(w2, a2);
    compareObjects(w3, a3);
  })
);
