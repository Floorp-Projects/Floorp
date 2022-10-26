/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BackgroundUpdate } = ChromeUtils.import(
  "resource://gre/modules/BackgroundUpdate.jsm"
);
let reasons = () => BackgroundUpdate._reasonsToNotScheduleUpdates();
let REASON = BackgroundUpdate.REASON;

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { ExtensionTestUtils } = ChromeUtils.import(
  "resource://testing-common/ExtensionXPCShellUtils.jsm"
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
        applications: {
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

    // Now turn off langpack updating.
    Services.prefs.setBoolPref("app.update.langpack.enabled", false);

    result = await reasons();
    Assert.ok(
      !result.includes(REASON.LANGPACK_INSTALLED),
      "Reasons does not include LANGPACK_INSTALLED"
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
