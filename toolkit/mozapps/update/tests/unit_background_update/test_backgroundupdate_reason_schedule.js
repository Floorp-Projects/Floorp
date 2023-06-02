/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BackgroundUpdate } = ChromeUtils.importESModule(
  "resource://gre/modules/BackgroundUpdate.sys.mjs"
);
let reasons = () => BackgroundUpdate._reasonsToNotScheduleUpdates();
let REASON = BackgroundUpdate.REASON;

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);
const { ExtensionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/ExtensionXPCShellUtils.sys.mjs"
);

setupProfileService();

// Setup that allows to install a langpack.
ExtensionTestUtils.init(this);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

add_setup(function test_setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // We need to initialize it once, otherwise operations will be stuck in the pre-init queue.
  Services.fog.initializeFOG();

  setupProfileService();
});

add_task(
  {
    skip_if: () => !AppConstants.MOZ_BACKGROUNDTASKS,
  },
  async function test_reasons_schedule_langpacks() {
    await AddonTestUtils.promiseStartupManager();

    Services.prefs.setBoolPref("app.update.langpack.enabled", true);

    let result = await reasons();
    Assert.ok(
      !result.includes(REASON.LANGPACK_INSTALLED),
      "Reasons does not include LANGPACK_INSTALLED"
    );

    // Install a langpack.
    let langpack = {
      "manifest.json": {
        name: "test Language Pack",
        version: "1.0",
        manifest_version: 2,
        browser_specific_settings: {
          gecko: {
            id: "@test-langpack",
            strict_min_version: "42.0",
            strict_max_version: "42.0",
          },
        },
        langpack_id: "fr",
        languages: {
          fr: {
            chrome_resources: {
              global: "chrome/fr/locale/fr/global/",
            },
            version: "20171001190118",
          },
        },
        sources: {
          browser: {
            base_path: "browser/",
          },
        },
      },
    };

    await Promise.all([
      TestUtils.topicObserved("webextension-langpack-startup"),
      AddonTestUtils.promiseInstallXPI(langpack),
    ]);

    result = await reasons();
    Assert.ok(
      result.includes(REASON.LANGPACK_INSTALLED),
      "Reasons include LANGPACK_INSTALLED"
    );
    result = await checkGleanPing();
    Assert.ok(
      result.includes(REASON.LANGPACK_INSTALLED),
      "Recognizes a language pack is installed."
    );

    // Now turn off langpack updating.
    Services.prefs.setBoolPref("app.update.langpack.enabled", false);

    result = await reasons();
    Assert.ok(
      !result.includes(REASON.LANGPACK_INSTALLED),
      "Reasons does not include LANGPACK_INSTALLED"
    );
    result = await checkGleanPing();
    Assert.ok(
      !result.includes(REASON.LANGPACK_INSTALLED),
      "No Glean metric when no language pack is installed."
    );
  }
);

add_task(
  {
    skip_if: () => !AppConstants.MOZ_BACKGROUNDTASKS,
  },
  async function test_reasons_schedule_default_profile() {
    // It's difficult to arrange a default profile in a testing environment, so
    // this is not as thorough as we'd like.
    let result = await reasons();

    Assert.ok(result.includes(REASON.NO_DEFAULT_PROFILE_EXISTS));
    Assert.ok(result.includes(REASON.NOT_DEFAULT_PROFILE));
  }
);
