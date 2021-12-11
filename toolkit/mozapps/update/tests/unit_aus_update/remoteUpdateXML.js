/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

async function run_test() {
  setupTestCommon();
  debugDump("testing remote update xml attributes");
  start_httpserver();
  setUpdateURL(gURLData + gHTTPHandlerPath);
  setUpdateChannel("test_channel");

  debugDump("testing update xml not available");
  await waitForUpdateCheck(false).then(aArgs => {
    Assert.equal(
      aArgs.update.errorCode,
      1500,
      "the update errorCode" + MSG_SHOULD_EQUAL
    );
  });

  debugDump(
    "testing one update available, the update's property values and " +
      "the property values of the update's patches"
  );
  let patchProps = {
    type: "complete",
    url: "http://complete/",
    size: "9856459",
  };
  let patches = getRemotePatchString(patchProps);
  patchProps = { type: "partial", url: "http://partial/", size: "1316138" };
  patches += getRemotePatchString(patchProps);
  let updateProps = {
    type: "minor",
    name: "Minor Test",
    displayVersion: "version 2.1a1pre",
    appVersion: "2.1a1pre",
    buildID: "20080811053724",
    detailsURL: "http://details/",
    promptWaitTime: "345600",
    custom1: 'custom1_attr="custom1 value"',
    custom2: 'custom2_attr="custom2 value"',
  };
  let updates = getRemoteUpdateString(updateProps, patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  await waitForUpdateCheck(true, { updateCount: 1 }).then(aArgs => {
    // XXXrstrong - not specifying a detailsURL will cause a leak due to
    // bug 470244 and until this is fixed this will not test the value for
    // detailsURL when it isn't specified in the update xml.

    let bestUpdate = gAUS
      .selectUpdate(aArgs.updates)
      .QueryInterface(Ci.nsIWritablePropertyBag);
    Assert.equal(
      bestUpdate.type,
      "minor",
      "the update type attribute" + MSG_SHOULD_EQUAL
    );
    Assert.equal(
      bestUpdate.name,
      "Minor Test",
      "the update name attribute" + MSG_SHOULD_EQUAL
    );
    Assert.equal(
      bestUpdate.displayVersion,
      "version 2.1a1pre",
      "the update displayVersion attribute" + MSG_SHOULD_EQUAL
    );
    Assert.equal(
      bestUpdate.appVersion,
      "2.1a1pre",
      "the update appVersion attribute" + MSG_SHOULD_EQUAL
    );
    Assert.equal(
      bestUpdate.buildID,
      "20080811053724",
      "the update buildID attribute" + MSG_SHOULD_EQUAL
    );
    Assert.equal(
      bestUpdate.detailsURL,
      "http://details/",
      "the update detailsURL attribute" + MSG_SHOULD_EQUAL
    );
    Assert.equal(
      bestUpdate.promptWaitTime,
      "345600",
      "the update promptWaitTime attribute" + MSG_SHOULD_EQUAL
    );
    Assert.equal(
      bestUpdate.serviceURL,
      gURLData + gHTTPHandlerPath + "?force=1",
      "the update serviceURL attribute" + MSG_SHOULD_EQUAL
    );
    Assert.equal(
      bestUpdate.channel,
      "test_channel",
      "the update channel attribute" + MSG_SHOULD_EQUAL
    );
    Assert.ok(
      !bestUpdate.isCompleteUpdate,
      "the update isCompleteUpdate attribute" + MSG_SHOULD_EQUAL
    );
    Assert.ok(
      !bestUpdate.isSecurityUpdate,
      "the update isSecurityUpdate attribute" + MSG_SHOULD_EQUAL
    );
    // Check that installDate is within 10 seconds of the current date.
    Assert.ok(
      Date.now() - bestUpdate.installDate < 10000,
      "the update installDate attribute should be within 10 seconds " +
        "of the current time"
    );
    Assert.ok(
      !bestUpdate.statusText,
      "the update statusText attribute" + MSG_SHOULD_EQUAL
    );
    // nsIUpdate:state returns an empty string when no action has been performed
    // on an available update
    Assert.equal(
      bestUpdate.state,
      "",
      "the update state attribute" + MSG_SHOULD_EQUAL
    );
    Assert.equal(
      bestUpdate.errorCode,
      0,
      "the update errorCode attribute" + MSG_SHOULD_EQUAL
    );
    Assert.equal(
      bestUpdate.patchCount,
      2,
      "the update patchCount attribute" + MSG_SHOULD_EQUAL
    );
    // XXX TODO - test nsIUpdate:serialize

    Assert.equal(
      bestUpdate.getProperty("custom1_attr"),
      "custom1 value",
      "the update custom1_attr property" + MSG_SHOULD_EQUAL
    );
    Assert.equal(
      bestUpdate.getProperty("custom2_attr"),
      "custom2 value",
      "the update custom2_attr property" + MSG_SHOULD_EQUAL
    );

    let patch = bestUpdate.getPatchAt(0);
    Assert.equal(
      patch.type,
      "complete",
      "the update patch type attribute" + MSG_SHOULD_EQUAL
    );
    Assert.equal(
      patch.URL,
      "http://complete/",
      "the update patch URL attribute" + MSG_SHOULD_EQUAL
    );
    Assert.equal(
      patch.size,
      9856459,
      "the update patch size attribute" + MSG_SHOULD_EQUAL
    );
    // The value for patch.state can be the string 'null' as a valid value. This
    // is confusing if it returns null which is an invalid value since the test
    // failure output will show a failure for null == null. To lessen the
    // confusion first check that the typeof for patch.state is string.
    Assert.equal(
      typeof patch.state,
      "string",
      "the update patch state typeof value should equal |string|"
    );
    Assert.equal(
      patch.state,
      STATE_NONE,
      "the update patch state attribute" + MSG_SHOULD_EQUAL
    );
    Assert.ok(
      !patch.selected,
      "the update patch selected attribute" + MSG_SHOULD_EQUAL
    );
    // XXX TODO - test nsIUpdatePatch:serialize

    patch = bestUpdate.getPatchAt(1);
    Assert.equal(
      patch.type,
      "partial",
      "the update patch type attribute" + MSG_SHOULD_EQUAL
    );
    Assert.equal(
      patch.URL,
      "http://partial/",
      "the update patch URL attribute" + MSG_SHOULD_EQUAL
    );
    Assert.equal(
      patch.size,
      1316138,
      "the update patch size attribute" + MSG_SHOULD_EQUAL
    );
    Assert.equal(
      patch.state,
      STATE_NONE,
      "the update patch state attribute" + MSG_SHOULD_EQUAL
    );
    Assert.ok(
      !patch.selected,
      "the update patch selected attribute" + MSG_SHOULD_EQUAL
    );
    // XXX TODO - test nsIUpdatePatch:serialize
  });

  debugDump(
    "testing an empty update xml that returns a root node name of " +
      "parsererror"
  );
  gResponseBody = "<parsererror/>";
  await waitForUpdateCheck(false).then(aArgs => {
    Assert.equal(
      aArgs.update.errorCode,
      1200,
      "the update errorCode" + MSG_SHOULD_EQUAL
    );
  });

  debugDump("testing no updates available");
  gResponseBody = getRemoteUpdatesXMLString("");
  await waitForUpdateCheck(true, { updateCount: 0 });

  debugDump("testing one update available with two patches");
  patches = getRemotePatchString({});
  patchProps = { type: "partial" };
  patches += getRemotePatchString(patchProps);
  updates = getRemoteUpdateString({}, patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  await waitForUpdateCheck(true, { updateCount: 1 });

  debugDump("testing three updates available each with two patches");
  patches = getRemotePatchString({});
  patchProps = { type: "partial" };
  patches += getRemotePatchString(patchProps);
  updates = getRemoteUpdateString({}, patches);
  updates += updates + updates;
  gResponseBody = getRemoteUpdatesXMLString(updates);
  await waitForUpdateCheck(true, { updateCount: 3 });

  debugDump(
    "testing one update with complete and partial patches with size " +
      "0 specified in the update xml"
  );
  patchProps = { size: "0" };
  patches = getRemotePatchString(patchProps);
  patchProps = { type: "partial", size: "0" };
  patches += getRemotePatchString(patchProps);
  updates = getRemoteUpdateString({}, patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  await waitForUpdateCheck(true).then(aArgs => {
    Assert.equal(
      aArgs.updates.length,
      0,
      "the update count" + MSG_SHOULD_EQUAL
    );
  });

  debugDump(
    "testing one update with complete patch with size 0 specified in " +
      "the update xml"
  );
  patchProps = { size: "0" };
  patches = getRemotePatchString(patchProps);
  updates = getRemoteUpdateString({}, patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  await waitForUpdateCheck(true, { updateCount: 0 });

  debugDump(
    "testing one update with partial patch with size 0 specified in " +
      "the update xml"
  );
  patchProps = { type: "partial", size: "0" };
  patches = getRemotePatchString(patchProps);
  updates = getRemoteUpdateString({}, patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  await waitForUpdateCheck(true, { updateCount: 0 });

  debugDump(
    "testing that updates for older versions of the application " +
      "aren't selected"
  );
  patches = getRemotePatchString({});
  patchProps = { type: "partial" };
  patches += getRemotePatchString(patchProps);
  updateProps = { appVersion: "1.0pre" };
  updates = getRemoteUpdateString(updateProps, patches);
  updateProps = { appVersion: "1.0a" };
  updates += getRemoteUpdateString(updateProps, patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  await waitForUpdateCheck(true, { updateCount: 2 }).then(aArgs => {
    let bestUpdate = gAUS.selectUpdate(aArgs.updates);
    Assert.ok(!bestUpdate, "there shouldn't be an update available");
  });

  debugDump(
    "testing that updates for the current version of the application " +
      "are selected"
  );
  patches = getRemotePatchString({});
  patchProps = { type: "partial" };
  patches += getRemotePatchString(patchProps);
  updateProps = { appVersion: "1.0" };
  updates = getRemoteUpdateString(updateProps, patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  await waitForUpdateCheck(true, { updateCount: 1 }).then(aArgs => {
    let bestUpdate = gAUS.selectUpdate(aArgs.updates);
    Assert.ok(!!bestUpdate, "there should be one update available");
    Assert.equal(
      bestUpdate.appVersion,
      "1.0",
      "the update appVersion attribute" + MSG_SHOULD_EQUAL
    );
    Assert.equal(
      bestUpdate.displayVersion,
      "1.0",
      "the update displayVersion attribute" + MSG_SHOULD_EQUAL
    );
  });

  stop_httpserver(doTestFinish);
}
