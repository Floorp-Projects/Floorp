/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

"use strict";

const { TelemetryUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryUtils.sys.mjs"
);
const { TelemetryArchiveTesting } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryArchiveTesting.sys.mjs"
);

add_task(async function test_updatePing() {
  const TEST_VERSION = "37.85";
  const TEST_BUILDID = "20150711123724";
  const XML_UPDATE = `<?xml version="1.0"?>
    <updates xmlns="http://www.mozilla.org/2005/app-update">
      <update appVersion="${Services.appinfo.version}" buildID="20080811053724"
              channel="nightly" displayVersion="Version 1.0"
              installDate="1238441400314" isCompleteUpdate="true" type="minor"
              name="Update Test 1.0" detailsURL="http://example.com/"
              previousAppVersion="${TEST_VERSION}"
              serviceURL="https://example.com/" foregroundDownload="true"
              statusText="The Update was successfully installed">
        <patch type="complete" URL="http://example.com/" size="775"
               selected="true" state="succeeded"/>
      </update>
    </updates>`;

  // Set the preferences needed for the test: they will be cleared up
  // after it runs.
  await SpecialPowers.pushPrefEnv({
    set: [
      [TelemetryUtils.Preferences.UpdatePing, true],
      ["browser.startup.homepage_override.mstone", TEST_VERSION],
      ["browser.startup.homepage_override.buildID", TEST_BUILDID],
      ["toolkit.telemetry.log.level", "Trace"],
    ],
  });

  registerCleanupFunction(async () => {
    let activeUpdateFile = getActiveUpdateFile();
    activeUpdateFile.remove(false);
    reloadUpdateManagerData(true);
  });
  writeFile(XML_UPDATE, getActiveUpdateFile());
  writeSuccessUpdateStatusFile();
  reloadUpdateManagerData(false);

  // Start monitoring the ping archive.
  let archiveChecker = new TelemetryArchiveTesting.Checker();
  await archiveChecker.promiseInit();

  // Manually call the BrowserContentHandler: this automatically gets called when
  // the browser is started and an update was applied successfully in order to
  // display the "update" info page.
  Cc["@mozilla.org/browser/clh;1"].getService(Ci.nsIBrowserHandler).defaultArgs;

  // We cannot control when the ping will be generated/archived after we trigger
  // an update, so let's make sure to have one before moving on with validation.
  let updatePing;
  await BrowserTestUtils.waitForCondition(
    async function () {
      // Check that the ping made it into the Telemetry archive.
      // The test data is defined in ../data/sharedUpdateXML.js
      updatePing = await archiveChecker.promiseFindPing("update", [
        [["payload", "reason"], "success"],
        [["payload", "previousBuildId"], TEST_BUILDID],
        [["payload", "previousVersion"], TEST_VERSION],
      ]);
      return !!updatePing;
    },
    "Make sure the ping is generated before trying to validate it.",
    500,
    100
  );

  ok(updatePing, "The 'update' ping must be correctly sent.");

  // We have no easy way to simulate a previously applied update from toolkit/telemetry.
  // Instead of moving this test to mozapps/update as well, just test that the
  // "previousChannel" field is present and either a string or null.
  ok(
    "previousChannel" in updatePing.payload,
    "The payload must contain the 'previousChannel' field"
  );
  const channelField = updatePing.payload.previousChannel;
  if (channelField != null) {
    Assert.equal(
      typeof channelField,
      "string",
      "'previousChannel' must be a string, if available."
    );
  }

  // Also make sure that the ping contains both a client id and an
  // environment section.
  ok("clientId" in updatePing, "The update ping must report a client id.");
  ok(
    "environment" in updatePing,
    "The update ping must report the environment."
  );
});

/**
 * Removes the updates.xml file and returns the nsIFile for the
 * active-update.xml file.
 *
 * @return  The nsIFile for the active-update.xml file.
 */
function getActiveUpdateFile() {
  let updateRootDir = Services.dirsvc.get("UpdRootD", Ci.nsIFile);
  let updatesFile = updateRootDir.clone();
  updatesFile.append("updates.xml");
  if (updatesFile.exists()) {
    // The following is non-fatal.
    try {
      updatesFile.remove(false);
    } catch (e) {}
  }
  let activeUpdateFile = updateRootDir.clone();
  activeUpdateFile.append("active-update.xml");
  return activeUpdateFile;
}

/**
 * Reloads the update xml files.
 *
 * @param  skipFiles (optional)
 *         If true, the update xml files will not be read and the metadata will
 *         be reset. If false (the default), the update xml files will be read
 *         to populate the update metadata.
 */
function reloadUpdateManagerData(skipFiles = false) {
  Cc["@mozilla.org/updates/update-manager;1"]
    .getService(Ci.nsIUpdateManager)
    .internal.reload(skipFiles);
}

/**
 * Writes the specified text to the specified file.
 *
 * @param {string} aText
 *   The string to write to the file.
 * @param {nsIFile} aFile
 *   The file to write to.
 */
function writeFile(aText, aFile) {
  const PERMS_FILE = 0o644;

  const MODE_WRONLY = 0x02;
  const MODE_CREATE = 0x08;
  const MODE_TRUNCATE = 0x20;

  if (!aFile.exists()) {
    aFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
  }
  let fos = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
    Ci.nsIFileOutputStream
  );
  let flags = MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE;
  fos.init(aFile, flags, PERMS_FILE, 0);
  fos.write(aText, aText.length);
  fos.close();
}

/**
 * If we want the update system to treat the update we wrote out as one that it
 * just installed, we need to make it think that the update state is
 * "succeeded".
 */
function writeSuccessUpdateStatusFile() {
  const statusFile = Services.dirsvc.get("UpdRootD", Ci.nsIFile);
  statusFile.append("updates");
  statusFile.append("0");
  statusFile.append("update.status");
  writeFile("succeeded", statusFile);
}
