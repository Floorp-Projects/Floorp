/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// useMLBF=true case is covered by test_blocklist_mlbf.js
Services.prefs.setBoolPref("extensions.blocklist.useMLBF", false);

const BLOCKLIST_DATA = [
  {
    id: "foo",
    guid: "myfoo",
    versionRange: [
      {
        severity: "3",
      },
    ],
  },
  {
    blockID: "bar",
    // we'll get a uuid as an `id` property from loadBlocklistRawData
    guid: "mybar",
    versionRange: [
      {
        severity: "3",
      },
    ],
  },
];

const BASE_BLOCKLIST_INFOURL = Services.prefs.getStringPref(
  "extensions.blocklist.detailsURL"
);

/*
 * Check that add-on blocklist URLs are correctly exposed
 * based on either blockID or id properties on the entries
 * in remote settings.
 */
add_task(async function blocklistURL_check() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  await promiseStartupManager();
  await AddonTestUtils.loadBlocklistRawData({ extensions: BLOCKLIST_DATA });

  let entry = await Blocklist.getAddonBlocklistEntry({
    id: "myfoo",
    version: "1.0",
  });
  Assert.equal(entry.url, BASE_BLOCKLIST_INFOURL + "foo.html");

  entry = await Blocklist.getAddonBlocklistEntry({
    id: "mybar",
    version: "1.0",
  });
  Assert.equal(entry.url, BASE_BLOCKLIST_INFOURL + "bar.html");
});
