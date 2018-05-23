/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

XPCOMUtils.defineLazyServiceGetter(this, "spellCheck",
                                   "@mozilla.org/spellchecker/engine;1", "mozISpellCheckingEngine");

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "61", "61");

  await promiseStartupManager();
});

add_task(async function test_validation() {
  await Assert.rejects(
    promiseInstallWebExtension({
      manifest: {
        applications: {gecko: {id: "en-US-no-dic@dictionaries.mozilla.org"}},
        "dictionaries": {
          "en-US": "en-US.dic",
        },
      },
    })
  );

  await Assert.rejects(
    promiseInstallWebExtension({
      manifest: {
        applications: {gecko: {id: "en-US-no-aff@dictionaries.mozilla.org"}},
        "dictionaries": {
          "en-US": "en-US.dic",
        },
      },

      files: {
        "en-US.dic": "",
      },
    })
  );

  let addon = await promiseInstallWebExtension({
    manifest: {
      applications: {gecko: {id: "en-US-1@dictionaries.mozilla.org"}},
      "dictionaries": {
        "en-US": "en-US.dic",
      },
    },

    files: {
      "en-US.dic": "",
      "en-US.aff": "",
    },
  });

  let addon2 = await promiseInstallWebExtension({
    manifest: {
      applications: {gecko: {id: "en-US-2@dictionaries.mozilla.org"}},
      "dictionaries": {
        "en-US": "dictionaries/en-US.dic",
      },
    },

    files: {
      "dictionaries/en-US.dic": "",
      "dictionaries/en-US.aff": "",
    },
  });

  addon.uninstall();
  addon2.uninstall();
});

const WORD = "Flehgragh";

add_task(async function test_registration() {
  spellCheck.dictionary = "en-US";

  ok(!spellCheck.check(WORD), "Word should not pass check before add-on loads");

  let addon = await promiseInstallWebExtension({
    manifest: {
      applications: {gecko: {id: "en-US@dictionaries.mozilla.org"}},
      "dictionaries": {
        "en-US": "en-US.dic",
      },
    },

    files: {
      "en-US.dic": `2\n${WORD}\nnativ/A\n`,
      "en-US.aff": `
SFX A Y 1
SFX A   0       en         [^elr]
      `,
    },
  });

  ok(spellCheck.check(WORD), "Word should pass check while add-on load is loaded");
  ok(spellCheck.check("nativen"), "Words should have correct affixes");

  addon.uninstall();

  await new Promise(executeSoon);

  ok(!spellCheck.check(WORD), "Word should not pass check after add-on unloads");
});

// Tests that existing unpacked dictionaries are migrated to
// WebExtension dictionaries on schema bump.
add_task(async function test_migration() {
  let profileDir = gProfD.clone();
  profileDir.append("extensions");

  await promiseShutdownManager();

  const ID = "en-US@dictionaries.mozilla.org";
  await promiseWriteInstallRDFToDir({
    id: ID,
    type: "64",
    version: "1.0",
    targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "61",
        maxVersion: "61.*"}],
  }, profileDir, ID, {
    "dictionaries/en-US.dic": `1\n${WORD}\n`,
    "dictionaries/en-US.aff": "",
  });

  await promiseStartupManager();

  ok(spellCheck.check(WORD), "Word should pass check after initial load");

  var {XPIDatabase, XPIProvider, XPIStates} = ChromeUtils.import("resource://gre/modules/addons/XPIProvider.jsm", null);

  let addon = await XPIDatabase.getVisibleAddonForID(ID);
  let state = XPIStates.findAddon(ID);

  // Mangle the add-on state to match what an unpacked dictionary looked
  // like in older versions.
  XPIDatabase.setAddonProperties(addon, {
    startupData: null,
    type: "dictionary",
  });

  state.type = "dictionary";
  state.startupData = null;
  XPIStates.save();

  // Dictionary add-ons usually do not unregister dictionaries at app
  // shutdown, so force them to here.
  XPIProvider.activeAddons.get(ID).scope.shutdown(0);
  await promiseShutdownManager();

  ok(!spellCheck.check(WORD), "Word should not pass check while add-on manager is shut down");

  // Drop the schema version to the last one that supported legacy
  // dictionaries.
  Services.prefs.setIntPref("extensions.databaseSchema", 25);
  await promiseStartupManager();

  ok(spellCheck.check(WORD), "Word should pass check while add-on load is loaded");
});
