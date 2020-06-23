/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test() {
  setupTestCommon();

  debugDump(
    "testing addition of a successful update to " +
      FILE_UPDATES_XML +
      " and verification of update properties including the format " +
      "prior to bug 530872"
  );

  setUpdateChannel("test_channel");

  let patchProps = {
    type: "partial",
    url: "http://partial/",
    size: "86",
    selected: "true",
    state: STATE_PENDING,
    custom1: 'custom1_attr="custom1 patch value"',
    custom2: 'custom2_attr="custom2 patch value"',
  };
  let patches = getLocalPatchString(patchProps);
  let updateProps = {
    type: "major",
    name: "New",
    displayVersion: "version 4",
    appVersion: "4.0",
    buildID: "20070811053724",
    detailsURL: "http://details1/",
    serviceURL: "http://service1/",
    installDate: "1238441300314",
    statusText: "test status text",
    isCompleteUpdate: "false",
    channel: "test_channel",
    foregroundDownload: "true",
    promptWaitTime: "345600",
    previousAppVersion: "3.0",
    custom1: 'custom1_attr="custom1 value"',
    custom2: 'custom2_attr="custom2 value"',
  };
  let updates = getLocalUpdateString(updateProps, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_SUCCEEDED);

  patchProps = {
    type: "complete",
    url: "http://complete/",
    size: "75",
    selected: "true",
    state: STATE_FAILED,
    custom1: 'custom3_attr="custom3 patch value"',
    custom2: 'custom4_attr="custom4 patch value"',
  };
  patches = getLocalPatchString(patchProps);
  updateProps = {
    type: "minor",
    name: "Existing",
    appVersion: "3.0",
    detailsURL: "http://details2/",
    serviceURL: "http://service2/",
    statusText: getString("patchApplyFailure"),
    isCompleteUpdate: "true",
    channel: "test_channel",
    foregroundDownload: "false",
    promptWaitTime: "691200",
    custom1: 'custom3_attr="custom3 value"',
    custom2: 'custom4_attr="custom4 value"',
  };
  updates = getLocalUpdateString(updateProps, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), false);

  standardInit();

  Assert.ok(
    !gUpdateManager.activeUpdate,
    "the update manager activeUpdate attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    gUpdateManager.getUpdateCount(),
    2,
    "the update manager updateCount attribute" + MSG_SHOULD_EQUAL
  );

  debugDump("checking the first update properties");
  let update = gUpdateManager
    .getUpdateAt(0)
    .QueryInterface(Ci.nsIWritablePropertyBag);
  Assert.equal(
    update.state,
    STATE_SUCCEEDED,
    "the update state attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.type,
    "major",
    "the update type attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.name,
    "New",
    "the update name attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.displayVersion,
    "version 4",
    "the update displayVersion attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.appVersion,
    "4.0",
    "the update appVersion attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.buildID,
    "20070811053724",
    "the update buildID attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.detailsURL,
    "http://details1/",
    "the update detailsURL attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.serviceURL,
    "http://service1/",
    "the update serviceURL attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.installDate,
    "1238441300314",
    "the update installDate attribute" + MSG_SHOULD_EQUAL
  );
  // statusText is updated
  Assert.equal(
    update.statusText,
    getString("installSuccess"),
    "the update statusText attribute" + MSG_SHOULD_EQUAL
  );
  Assert.ok(
    !update.isCompleteUpdate,
    "the update isCompleteUpdate attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.channel,
    "test_channel",
    "the update channel attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.promptWaitTime,
    "345600",
    "the update promptWaitTime attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.previousAppVersion,
    "3.0",
    "the update previousAppVersion attribute" + MSG_SHOULD_EQUAL
  );
  // Custom attributes
  Assert.equal(
    update.getProperty("custom1_attr"),
    "custom1 value",
    "the update custom1_attr property" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.getProperty("custom2_attr"),
    "custom2 value",
    "the update custom2_attr property" + MSG_SHOULD_EQUAL
  );
  // nsIPropertyBag enumerator
  debugDump("checking the first update enumerator");
  Assert.ok(
    update.enumerator instanceof Ci.nsISimpleEnumerator,
    "update enumerator should be an instance of nsISimpleEnumerator"
  );
  let results = Array.from(update.enumerator);
  Assert.equal(
    results.length,
    3,
    "the length of the array created from the update enumerator" +
      MSG_SHOULD_EQUAL
  );
  Assert.ok(
    results.every(prop => prop instanceof Ci.nsIProperty),
    "the objects in the array created from the update enumerator " +
      "should all be an instance of nsIProperty"
  );
  Assert.equal(
    results[0].name,
    "custom1_attr",
    "the first property name" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    results[0].value,
    "custom1 value",
    "the first property value" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    results[1].name,
    "custom2_attr",
    "the second property name" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    results[1].value,
    "custom2 value",
    "the second property value" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    results[2].name,
    "foregroundDownload",
    "the second property name" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    results[2].value,
    "true",
    "the third property value" + MSG_SHOULD_EQUAL
  );

  debugDump("checking the first update patch properties");
  let patch = update.selectedPatch.QueryInterface(Ci.nsIWritablePropertyBag);
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
    "86",
    "the update patch size attribute" + MSG_SHOULD_EQUAL
  );
  Assert.ok(
    !!patch.selected,
    "the update patch selected attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    patch.state,
    STATE_SUCCEEDED,
    "the update patch state attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    patch.getProperty("custom1_attr"),
    "custom1 patch value",
    "the update patch custom1_attr property" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    patch.getProperty("custom2_attr"),
    "custom2 patch value",
    "the update patch custom2_attr property" + MSG_SHOULD_EQUAL
  );
  // nsIPropertyBag enumerator
  debugDump("checking the first update patch enumerator");
  Assert.ok(
    patch.enumerator instanceof Ci.nsISimpleEnumerator,
    "patch enumerator should be an instance of nsISimpleEnumerator"
  );
  results = Array.from(patch.enumerator);
  Assert.equal(
    results.length,
    2,
    "the length of the array created from the patch enumerator" +
      MSG_SHOULD_EQUAL
  );
  Assert.ok(
    results.every(prop => prop instanceof Ci.nsIProperty),
    "the objects in the array created from the patch enumerator " +
      "should all be an instance of nsIProperty"
  );
  Assert.equal(
    results[0].name,
    "custom1_attr",
    "the first property name" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    results[0].value,
    "custom1 patch value",
    "the first property value" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    results[1].name,
    "custom2_attr",
    "the second property name" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    results[1].value,
    "custom2 patch value",
    "the second property value" + MSG_SHOULD_EQUAL
  );

  debugDump("checking the second update properties");
  update = gUpdateManager
    .getUpdateAt(1)
    .QueryInterface(Ci.nsIWritablePropertyBag);
  Assert.equal(
    update.state,
    STATE_FAILED,
    "the update state attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.name,
    "Existing",
    "the update name attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.type,
    "minor",
    "the update type attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.displayVersion,
    "3.0",
    "the update displayVersion attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.appVersion,
    "3.0",
    "the update appVersion attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.detailsURL,
    "http://details2/",
    "the update detailsURL attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.serviceURL,
    "http://service2/",
    "the update serviceURL attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.installDate,
    "1238441400314",
    "the update installDate attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.statusText,
    getString("patchApplyFailure"),
    "the update statusText attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.buildID,
    "20080811053724",
    "the update buildID attribute" + MSG_SHOULD_EQUAL
  );
  Assert.ok(
    !!update.isCompleteUpdate,
    "the update isCompleteUpdate attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.channel,
    "test_channel",
    "the update channel attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.promptWaitTime,
    "691200",
    "the update promptWaitTime attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.previousAppVersion,
    "1.0",
    "the update previousAppVersion attribute" + MSG_SHOULD_EQUAL
  );
  // Custom attributes
  Assert.equal(
    update.getProperty("custom3_attr"),
    "custom3 value",
    "the update custom3_attr property" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.getProperty("custom4_attr"),
    "custom4 value",
    "the update custom4_attr property" + MSG_SHOULD_EQUAL
  );
  // nsIPropertyBag enumerator
  debugDump("checking the second update enumerator");
  Assert.ok(
    update.enumerator instanceof Ci.nsISimpleEnumerator,
    "update enumerator should be an instance of nsISimpleEnumerator"
  );
  results = Array.from(update.enumerator);
  Assert.equal(
    results.length,
    3,
    "the length of the array created from the update enumerator" +
      MSG_SHOULD_EQUAL
  );
  Assert.ok(
    results.every(prop => prop instanceof Ci.nsIProperty),
    "the objects in the array created from the update enumerator " +
      "should all be an instance of nsIProperty"
  );
  Assert.equal(
    results[0].name,
    "custom3_attr",
    "the first property name" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    results[0].value,
    "custom3 value",
    "the first property value" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    results[1].name,
    "custom4_attr",
    "the second property name" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    results[1].value,
    "custom4 value",
    "the second property value" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    results[2].name,
    "foregroundDownload",
    "the third property name" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    results[2].value,
    "false",
    "the third property value" + MSG_SHOULD_EQUAL
  );

  debugDump("checking the second update patch properties");
  patch = update.selectedPatch.QueryInterface(Ci.nsIWritablePropertyBag);
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
    "75",
    "the update patch size attribute" + MSG_SHOULD_EQUAL
  );
  Assert.ok(
    !!patch.selected,
    "the update patch selected attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    patch.state,
    STATE_FAILED,
    "the update patch state attribute" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    patch.getProperty("custom3_attr"),
    "custom3 patch value",
    "the update patch custom3_attr property" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    patch.getProperty("custom4_attr"),
    "custom4 patch value",
    "the update patch custom4_attr property" + MSG_SHOULD_EQUAL
  );
  // nsIPropertyBag enumerator
  debugDump("checking the second update patch enumerator");
  Assert.ok(
    patch.enumerator instanceof Ci.nsISimpleEnumerator,
    "patch enumerator should be an instance of nsISimpleEnumerator"
  );
  results = Array.from(patch.enumerator);
  Assert.equal(
    results.length,
    2,
    "the length of the array created from the patch enumerator" +
      MSG_SHOULD_EQUAL
  );
  Assert.ok(
    results.every(prop => prop instanceof Ci.nsIProperty),
    "the objects in the array created from the patch enumerator " +
      "should all be an instance of nsIProperty"
  );
  Assert.equal(
    results[0].name,
    "custom3_attr",
    "the first property name" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    results[0].value,
    "custom3 patch value",
    "the first property value" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    results[1].name,
    "custom4_attr",
    "the second property name" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    results[1].value,
    "custom4 patch value",
    "the second property value" + MSG_SHOULD_EQUAL
  );

  let attrNames = [
    "appVersion",
    "buildID",
    "channel",
    "detailsURL",
    "displayVersion",
    "elevationFailure",
    "errorCode",
    "installDate",
    "isCompleteUpdate",
    "name",
    "previousAppVersion",
    "promptWaitTime",
    "serviceURL",
    "state",
    "statusText",
    "type",
    "unsupported",
  ];
  checkIllegalProperties(update, attrNames);

  attrNames = [
    "errorCode",
    "finalURL",
    "selected",
    "size",
    "state",
    "type",
    "URL",
  ];
  checkIllegalProperties(patch, attrNames);

  executeSoon(doTestFinish);
}

function checkIllegalProperties(object, propertyNames) {
  let objectName =
    object instanceof Ci.nsIUpdate ? "nsIUpdate" : "nsIUpdatePatch";
  propertyNames.forEach(function(name) {
    // Check that calling getProperty, setProperty, and deleteProperty on an
    // nsIUpdate attribute throws NS_ERROR_ILLEGAL_VALUE
    let result = 0;
    try {
      object.getProperty(name);
    } catch (e) {
      result = e.result;
    }
    Assert.equal(
      result,
      Cr.NS_ERROR_ILLEGAL_VALUE,
      "calling getProperty using an " +
        objectName +
        " attribute " +
        "name should throw NS_ERROR_ILLEGAL_VALUE"
    );

    result = 0;
    try {
      object.setProperty(name, "value");
    } catch (e) {
      result = e.result;
    }
    Assert.equal(
      result,
      Cr.NS_ERROR_ILLEGAL_VALUE,
      "calling setProperty using an " +
        objectName +
        " attribute " +
        "name should throw NS_ERROR_ILLEGAL_VALUE"
    );

    result = 0;
    try {
      object.deleteProperty(name);
    } catch (e) {
      result = e.result;
    }
    Assert.equal(
      result,
      Cr.NS_ERROR_ILLEGAL_VALUE,
      "calling deleteProperty using an " +
        objectName +
        " attribute " +
        "name should throw NS_ERROR_ILLEGAL_VALUE"
    );
  });
}
