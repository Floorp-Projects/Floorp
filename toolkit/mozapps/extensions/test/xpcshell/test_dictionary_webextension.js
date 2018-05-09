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

add_task(async function test_registration() {
  const WORD = "Flehgragh";

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
      "en-US.dic": `1\n${WORD}\n`,
      "en-US.aff": "",
    },
  });

  ok(spellCheck.check(WORD), "Word should pass check while add-on load is loaded");

  addon.uninstall();

  await new Promise(executeSoon);

  ok(!spellCheck.check(WORD), "Word should not pass check after add-on unloads");
});
