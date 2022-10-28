/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// useMLBF=true only supports one type of severity (hard block). The value of
// appDisabled in the extension blocklist is checked in test_blocklist_mlbf.js.
enable_blocklist_v2_instead_of_useMLBF();

const URI_EXTENSION_BLOCKLIST_DIALOG =
  "chrome://mozapps/content/extensions/blocklist.xhtml";

// Workaround for Bug 658720 - URL formatter can leak during xpcshell tests
const PREF_BLOCKLIST_ITEM_URL = "extensions.blocklist.itemURL";
Services.prefs.setCharPref(
  PREF_BLOCKLIST_ITEM_URL,
  "http://example.com/blocklist/%blockID%"
);

async function getAddonBlocklistURL(addon) {
  let entry = await Blocklist.getAddonBlocklistEntry(addon);
  return entry && entry.url;
}

var ADDONS = [
  {
    // Tests how the blocklist affects a disabled add-on
    id: "test_bug455906_1@tests.mozilla.org",
    name: "Bug 455906 Addon Test 1",
    version: "5",
    appVersion: "3",
  },
  {
    // Tests how the blocklist affects an enabled add-on
    id: "test_bug455906_2@tests.mozilla.org",
    name: "Bug 455906 Addon Test 2",
    version: "5",
    appVersion: "3",
  },
  {
    // Tests how the blocklist affects an enabled add-on, to be disabled by the notification
    id: "test_bug455906_3@tests.mozilla.org",
    name: "Bug 455906 Addon Test 3",
    version: "5",
    appVersion: "3",
  },
  {
    // Tests how the blocklist affects a disabled add-on that was already warned about
    id: "test_bug455906_4@tests.mozilla.org",
    name: "Bug 455906 Addon Test 4",
    version: "5",
    appVersion: "3",
  },
  {
    // Tests how the blocklist affects an enabled add-on that was already warned about
    id: "test_bug455906_5@tests.mozilla.org",
    name: "Bug 455906 Addon Test 5",
    version: "5",
    appVersion: "3",
  },
  {
    // Tests how the blocklist affects an already blocked add-on
    id: "test_bug455906_6@tests.mozilla.org",
    name: "Bug 455906 Addon Test 6",
    version: "5",
    appVersion: "3",
  },
  {
    // Tests how the blocklist affects an incompatible add-on
    id: "test_bug455906_7@tests.mozilla.org",
    name: "Bug 455906 Addon Test 7",
    version: "5",
    appVersion: "2",
  },
  {
    // Spare add-on used to ensure we get a notification when switching lists
    id: "dummy_bug455906_1@tests.mozilla.org",
    name: "Dummy Addon 1",
    version: "5",
    appVersion: "3",
  },
  {
    // Spare add-on used to ensure we get a notification when switching lists
    id: "dummy_bug455906_2@tests.mozilla.org",
    name: "Dummy Addon 2",
    version: "5",
    appVersion: "3",
  },
];

var gNotificationCheck = null;

// Don't need the full interface, attempts to call other methods will just
// throw which is just fine
var WindowWatcher = {
  openWindow(parent, url, name, features, windowArguments) {
    // Should be called to list the newly blocklisted items
    equal(url, URI_EXTENSION_BLOCKLIST_DIALOG);

    if (gNotificationCheck) {
      gNotificationCheck(windowArguments.wrappedJSObject);
    }

    // run the code after the blocklist is closed
    Services.obs.notifyObservers(null, "addon-blocklist-closed");
  },

  QueryInterface: ChromeUtils.generateQI(["nsIWindowWatcher"]),
};

MockRegistrar.register(
  "@mozilla.org/embedcomp/window-watcher;1",
  WindowWatcher
);

function createAddon(addon) {
  return promiseInstallWebExtension({
    manifest: {
      name: addon.name,
      version: addon.version,
      browser_specific_settings: {
        gecko: {
          id: addon.id,
          strict_min_version: addon.appVersion,
          strict_max_version: addon.appVersion,
        },
      },
    },
  });
}

const BLOCKLIST_DATA = {
  start: {
    // Block 4-6 and a dummy:
    extensions: [
      {
        guid: "test_bug455906_4@tests.mozilla.org",
        versionRange: [{ severity: "-1" }],
      },
      {
        guid: "test_bug455906_5@tests.mozilla.org",
        versionRange: [{ severity: "1" }],
      },
      {
        guid: "test_bug455906_6@tests.mozilla.org",
        versionRange: [{ severity: "2" }],
      },
      {
        guid: "dummy_bug455906_1@tests.mozilla.org",
        versionRange: [],
      },
    ],
  },
  warn: {
    // warn for all test add-ons:
    extensions: ADDONS.filter(a => a.id.startsWith("test_")).map(a => ({
      guid: a.id,
      versionRange: [{ severity: "-1" }],
    })),
  },
  block: {
    // block all test add-ons:
    extensions: ADDONS.filter(a => a.id.startsWith("test_")).map(a => ({
      guid: a.id,
      blockID: a.id,
      versionRange: [],
    })),
  },
  empty: {
    // Block a dummy so there's a change:
    extensions: [
      {
        guid: "dummy_bug455906_2@tests.mozilla.org",
        versionRange: [],
      },
    ],
  },
};

async function loadBlocklist(id, callback) {
  gNotificationCheck = callback;

  await AddonTestUtils.loadBlocklistRawData(BLOCKLIST_DATA[id]);
}

function create_blocklistURL(blockID) {
  let url = Services.urlFormatter.formatURLPref(PREF_BLOCKLIST_ITEM_URL);
  url = url.replace(/%blockID%/g, blockID);
  return url;
}

// Before every main test this is the state the add-ons are meant to be in
async function checkInitialState() {
  let addons = await AddonManager.getAddonsByIDs(ADDONS.map(a => a.id));

  checkAddonState(addons[0], {
    userDisabled: true,
    softDisabled: false,
    appDisabled: false,
  });
  checkAddonState(addons[1], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: false,
  });
  checkAddonState(addons[2], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: false,
  });
  checkAddonState(addons[3], {
    userDisabled: true,
    softDisabled: true,
    appDisabled: false,
  });
  checkAddonState(addons[4], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: false,
  });
  checkAddonState(addons[5], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: true,
  });
  checkAddonState(addons[6], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: true,
  });
}

function checkAddonState(addon, state) {
  return checkAddon(addon.id, addon, state);
}

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "3", "3");

  await promiseStartupManager();

  // Load the initial blocklist into the profile to check add-ons start in the
  // right state.
  await AddonTestUtils.loadBlocklistRawData(BLOCKLIST_DATA.start);

  for (let addon of ADDONS) {
    await createAddon(addon);
  }
});

add_task(async function test_1() {
  // Tests the add-ons were installed and the initial blocklist applied as expected

  let addons = await AddonManager.getAddonsByIDs(ADDONS.map(a => a.id));
  for (var i = 0; i < ADDONS.length; i++) {
    ok(addons[i], `Addon ${i + 1} should be installed correctly`);
  }

  checkAddonState(addons[0], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: false,
  });
  checkAddonState(addons[1], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: false,
  });
  checkAddonState(addons[2], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: false,
  });

  // Warn add-ons should be soft disabled automatically
  checkAddonState(addons[3], {
    userDisabled: true,
    softDisabled: true,
    appDisabled: false,
  });
  checkAddonState(addons[4], {
    userDisabled: true,
    softDisabled: true,
    appDisabled: false,
  });

  // Blocked and incompatible should be app disabled only
  checkAddonState(addons[5], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: true,
  });
  checkAddonState(addons[6], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: true,
  });

  // Put the add-ons into the base state
  await addons[0].disable();
  await addons[4].enable();

  await promiseRestartManager();
  await checkInitialState();

  await loadBlocklist("warn", args => {
    dump("Checking notification pt 2\n");
    // This test is artificial, we don't notify for add-ons anymore, see
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1257565#c111 . Cleaning this up
    // should happen but this patchset is too huge as it is so I'm deferring it.
    equal(args.list.length, 2);
  });

  await promiseRestartManager();
  dump("Checking results pt 2\n");

  addons = await AddonManager.getAddonsByIDs(ADDONS.map(a => a.id));

  info("Should have disabled this add-on as requested");
  checkAddonState(addons[2], {
    userDisabled: true,
    softDisabled: true,
    appDisabled: false,
  });

  info("The blocked add-on should have changed to soft disabled");
  checkAddonState(addons[5], {
    userDisabled: true,
    softDisabled: true,
    appDisabled: false,
  });
  checkAddonState(addons[6], {
    userDisabled: true,
    softDisabled: true,
    appDisabled: true,
  });

  info("These should have been unchanged");
  checkAddonState(addons[0], {
    userDisabled: true,
    softDisabled: false,
    appDisabled: false,
  });
  // XXXgijs this is supposed to be not user disabled or soft disabled, but because we don't show
  // the dialog, it's disabled anyway. Comment out this assertion for now...
  // checkAddonState(addons[1], {userDisabled: false, softDisabled: false, appDisabled: false});
  checkAddonState(addons[3], {
    userDisabled: true,
    softDisabled: true,
    appDisabled: false,
  });
  checkAddonState(addons[4], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: false,
  });

  // Back to starting state
  await addons[2].enable();
  await addons[5].enable();

  await promiseRestartManager();
  await loadBlocklist("start");
});

add_task(async function test_pt3() {
  await promiseRestartManager();
  await checkInitialState();

  await loadBlocklist("block", args => {
    dump("Checking notification pt 3\n");
    equal(args.list.length, 3);
  });

  await promiseRestartManager();
  dump("Checking results pt 3\n");

  let addons = await AddonManager.getAddonsByIDs(ADDONS.map(a => a.id));

  // All should have gained the blocklist state, user disabled as previously
  checkAddonState(addons[0], {
    userDisabled: true,
    softDisabled: false,
    appDisabled: true,
  });
  checkAddonState(addons[1], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: true,
  });
  checkAddonState(addons[2], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: true,
  });
  checkAddonState(addons[4], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: true,
  });

  // Should have gained the blocklist state but no longer be soft disabled
  checkAddonState(addons[3], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: true,
  });

  // Check blockIDs are correct
  equal(
    await getAddonBlocklistURL(addons[0]),
    create_blocklistURL(addons[0].id)
  );
  equal(
    await getAddonBlocklistURL(addons[1]),
    create_blocklistURL(addons[1].id)
  );
  equal(
    await getAddonBlocklistURL(addons[2]),
    create_blocklistURL(addons[2].id)
  );
  equal(
    await getAddonBlocklistURL(addons[3]),
    create_blocklistURL(addons[3].id)
  );
  equal(
    await getAddonBlocklistURL(addons[4]),
    create_blocklistURL(addons[4].id)
  );

  // Shouldn't be changed
  checkAddonState(addons[5], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: true,
  });
  checkAddonState(addons[6], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: true,
  });

  // Back to starting state
  await loadBlocklist("start");
});

add_task(async function test_pt4() {
  let addon = await AddonManager.getAddonByID(ADDONS[4].id);
  await addon.enable();

  await promiseRestartManager();
  await checkInitialState();

  await loadBlocklist("empty", args => {
    dump("Checking notification pt 4\n");
    // See note in other callback - we no longer notify for non-blocked add-ons.
    ok(false, "Should not get a notification as there are no blocked addons.");
  });

  await promiseRestartManager();
  dump("Checking results pt 4\n");

  let addons = await AddonManager.getAddonsByIDs(ADDONS.map(a => a.id));
  // This should have become unblocked
  checkAddonState(addons[5], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: false,
  });

  // Should get re-enabled
  checkAddonState(addons[3], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: false,
  });

  // No change for anything else
  checkAddonState(addons[0], {
    userDisabled: true,
    softDisabled: false,
    appDisabled: false,
  });
  checkAddonState(addons[1], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: false,
  });
  checkAddonState(addons[2], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: false,
  });
  checkAddonState(addons[4], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: false,
  });
  checkAddonState(addons[6], {
    userDisabled: false,
    softDisabled: false,
    appDisabled: true,
  });
});
