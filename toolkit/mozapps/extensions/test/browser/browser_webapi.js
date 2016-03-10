/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TESTPAGE = `${SECURE_TESTROOT}webapi_checkavailable.html`;

Services.prefs.setBoolPref("extensions.webapi.testing", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("extensions.webapi.testing");
});

function testWithAPI(task) {
  return function*() {
    yield BrowserTestUtils.withNewTab(TESTPAGE, task);
  }
}

let gProvider = new MockProvider();

gProvider.createAddons([{
  id: "addon1@tests.mozilla.org",
  name: "Test add-on 1",
  version: "2.1",
  description: "Short description",
  type: "extension",
  userDisabled: false,
  isActive: true,
}, {
  id: "addon2@tests.mozilla.org",
  name: "Test add-on 2",
  version: "5.3.7ab",
  description: null,
  type: "theme",
  userDisabled: false,
  isActive: false,
}, {
  id: "addon3@tests.mozilla.org",
  name: "Test add-on 3",
  version: "1",
  description: "Longer description",
  type: "extension",
  userDisabled: true,
  isActive: false,
}]);

function API_getAddonByID(browser, id) {
  return ContentTask.spawn(browser, id, function*(id) {
    let addon = yield content.navigator.mozAddonManager.getAddonByID(id);

    // We can't send native objects back so clone its properties.
    let result = {};
    for (let prop in addon) {
      result[prop] = addon[prop];
    }

    return result;
  });
}

add_task(testWithAPI(function*(browser) {
  function compareObjects(web, real) {
    for (let prop of Object.keys(web)) {
      let webVal = web[prop];
      let realVal = real[prop];

      switch (prop) {
        case "isEnabled":
          realVal = !real.userDisabled;
          break;
      }

      // null and undefined don't compare well so stringify them first
      if (realVal === null || realVal === undefined) {
        realVal = `${realVal}`;
        webVal = `${webVal}`;
      }

      is(webVal, realVal, `Property ${prop} should have the right value in add-on ${real.id}`);
    }
  }

  let [a1, a2, a3] = yield promiseAddonsByIDs(["addon1@tests.mozilla.org",
                                               "addon2@tests.mozilla.org",
                                               "addon3@tests.mozilla.org"]);
  let w1 = yield API_getAddonByID(browser, "addon1@tests.mozilla.org");
  let w2 = yield API_getAddonByID(browser, "addon2@tests.mozilla.org");
  let w3 = yield API_getAddonByID(browser, "addon3@tests.mozilla.org");

  compareObjects(w1, a1);
  compareObjects(w2, a2);
  compareObjects(w3, a3);
}));
