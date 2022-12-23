/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);
const { TelemetryUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryUtils.sys.mjs"
);

const HANG_TIME = 1000; // ms
const TEST_USER_INTERACTION_ID = "testing.interaction";
const TEST_CLOBBERED_USER_INTERACTION_ID = `${TEST_USER_INTERACTION_ID} (clobbered)`;
const TEST_VALUE_1 = "some value";
const TEST_VALUE_2 = "some other value";
const TEST_ADDITIONAL_TEXT_1 = "some additional text";
const TEST_ADDITIONAL_TEXT_2 = "some other additional text";

/**
 * Intentionally hangs the main thread in the parent process for
 * HANG_TIME, and then returns the BHR hang report generated for
 * that hang.
 *
 * @returns {Promise}
 * @resolves {nsIHangDetails}
 *   The hang report that was created.
 */
async function hangAndWaitForReport(expectTestAnnotation) {
  let hangPromise = TestUtils.topicObserved("bhr-thread-hang", subject => {
    let hang = subject.QueryInterface(Ci.nsIHangDetails);
    if (hang.thread != "Gecko") {
      return false;
    }

    if (expectTestAnnotation) {
      return hang.annotations.some(annotation =>
        annotation[0].startsWith(TEST_USER_INTERACTION_ID)
      );
    }

    return hang.annotations.every(
      annotation => annotation[0] != TEST_USER_INTERACTION_ID
    );
  });

  executeSoon(() => {
    let startTime = Date.now();
    // eslint-disable-next-line no-empty
    while (Date.now() - startTime < HANG_TIME) {}
  });

  let [report] = await hangPromise;
  return report;
}

/**
 * Makes sure that the profiler is initialized. This has the added side-effect
 * of making sure that BHR is initialized as well.
 */
function ensureProfilerInitialized() {
  startProfiler();
  stopProfiler();
}

function stopProfiler() {
  Services.profiler.StopProfiler();
}

function startProfiler() {
  // Starting and stopping the profiler with the "stackwalk" flag will cause the
  // profiler's stackwalking features to be synchronously initialized. This
  // should prevent us from not initializing BHR quickly enough.
  Services.profiler.StartProfiler(1000, 10, ["stackwalk"]);
}

/**
 * Given a performance profile object, returns a count of how many
 * markers matched the value (and optional additionalText) that
 * the UserInteraction backend added. This function only checks
 * markers on thread 0.
 *
 * @param {Object} profile
 *   A profile returned from Services.profiler.getProfileData();
 * @param {String} value
 *   The value that the marker is expected to have.
 * @param {String} additionalText
 *   (Optional) If additionalText was provided when finishing the
 *   UserInteraction, then markerCount will check for a marker with
 *   text in the form of "value,additionalText".
 * @returns {Number}
 *   A count of how many markers appear that match the criteria.
 */
function markerCount(profile, value, additionalText) {
  let expectedName = value;
  if (additionalText) {
    expectedName = [value, additionalText].join(",");
  }

  let thread0 = profile.threads[0];
  let stringTable = thread0.stringTable;
  let markerStringIndex = stringTable.indexOf(TEST_USER_INTERACTION_ID);

  let markers = thread0.markers.data.filter(markerData => {
    return (
      markerData[0] == markerStringIndex && markerData[5].name == expectedName
    );
  });

  return markers.length;
}

/**
 * Given an nsIHangReport, returns true if there are one or more annotations
 * with the TEST_USER_INTERACTION_ID name, and the passed value.
 *
 * @param {nsIHangReport} report
 *   The hang report to check the annotations of.
 * @param {String} value
 *   The value that the annotation should have.
 * @returns {boolean}
 *   True if the annotation was found.
 */
function hasHangAnnotation(report, value) {
  return report.annotations.some(annotation => {
    return annotation[0] == TEST_USER_INTERACTION_ID && annotation[1] == value;
  });
}

/**
 * Given an nsIHangReport, returns true if there are one or more annotations
 * with the TEST_CLOBBERED_USER_INTERACTION_ID name, and the passed value.
 *
 * This check should be used when we expect a pre-existing UserInteraction to
 * have been clobbered by a new UserInteraction.
 *
 * @param {nsIHangReport} report
 *   The hang report to check the annotations of.
 * @param {String} value
 *   The value that the annotation should have.
 * @returns {boolean}
 *   True if the annotation was found.
 */
function hasClobberedHangAnnotation(report, value) {
  return report.annotations.some(annotation => {
    return (
      annotation[0] == TEST_CLOBBERED_USER_INTERACTION_ID &&
      annotation[1] == value
    );
  });
}

/**
 * Tests that UserInteractions cause BHR annotations and profiler
 * markers to be written.
 */
add_task(async function test_recording_annotations_and_markers() {
  if (!Services.telemetry.canRecordExtended) {
    Assert.ok("Hang reporting not enabled.");
    return;
  }

  ensureProfilerInitialized();

  Services.prefs.setBoolPref(
    TelemetryUtils.Preferences.OverridePreRelease,
    true
  );

  // First, we'll check to see if we can get a single annotation and
  // profiler marker to be set.
  startProfiler();

  UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_1);
  let report = await hangAndWaitForReport(true);
  UserInteraction.finish(TEST_USER_INTERACTION_ID);
  let profile = Services.profiler.getProfileData();
  stopProfiler();
  Assert.equal(
    markerCount(profile, TEST_VALUE_1),
    1,
    "Should have found the marker in the profile."
  );

  Assert.ok(
    hasHangAnnotation(report, TEST_VALUE_1),
    "Should have the BHR annotation set."
  );

  // Next, we'll make sure that when we're not running a UserInteraction,
  // no marker or annotation is set.
  startProfiler();

  report = await hangAndWaitForReport(false);
  profile = Services.profiler.getProfileData();

  stopProfiler();

  Assert.equal(
    markerCount(profile, TEST_VALUE_1),
    0,
    "Should not find the marker in the profile."
  );
  Assert.ok(
    !hasHangAnnotation(report),
    "Should not have the BHR annotation set."
  );

  // Next, we'll ensure that we can set multiple markers and annotations
  // by using the optional object argument to start() and finish().
  startProfiler();

  let obj1 = {};
  let obj2 = {};
  UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_1, obj1);
  UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_2, obj2);
  report = await hangAndWaitForReport(true);
  UserInteraction.finish(
    TEST_USER_INTERACTION_ID,
    obj1,
    TEST_ADDITIONAL_TEXT_1
  );
  UserInteraction.finish(
    TEST_USER_INTERACTION_ID,
    obj2,
    TEST_ADDITIONAL_TEXT_2
  );
  profile = Services.profiler.getProfileData();

  stopProfiler();

  Assert.equal(
    markerCount(profile, TEST_VALUE_1, TEST_ADDITIONAL_TEXT_1),
    1,
    "Should have found first marker in the profile."
  );

  Assert.equal(
    markerCount(profile, TEST_VALUE_2, TEST_ADDITIONAL_TEXT_2),
    1,
    "Should have found second marker in the profile."
  );

  Assert.ok(
    hasHangAnnotation(report, TEST_VALUE_1),
    "Should have the first BHR annotation set."
  );

  Assert.ok(
    hasHangAnnotation(report, TEST_VALUE_2),
    "Should have the second BHR annotation set."
  );
});

/**
 * Tests that UserInteractions can be updated, resulting in their BHR
 * annotations and profiler markers to also be updated.
 */
add_task(async function test_updating_annotations_and_markers() {
  if (!Services.telemetry.canRecordExtended) {
    Assert.ok("Hang reporting not enabled.");
    return;
  }

  ensureProfilerInitialized();

  // First, we'll check to see if we can get a single annotation and
  // profiler marker to be set.
  startProfiler();

  UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_1);
  // Updating the UserInteraction means that a new value will overwrite
  // the old.
  UserInteraction.update(TEST_USER_INTERACTION_ID, TEST_VALUE_2);
  let report = await hangAndWaitForReport(true);
  UserInteraction.finish(TEST_USER_INTERACTION_ID);
  let profile = Services.profiler.getProfileData();

  stopProfiler();

  Assert.equal(
    markerCount(profile, TEST_VALUE_1),
    0,
    "Should not have found the original marker in the profile."
  );

  Assert.equal(
    markerCount(profile, TEST_VALUE_2),
    1,
    "Should have found the updated marker in the profile."
  );

  Assert.ok(
    !hasHangAnnotation(report, TEST_VALUE_1),
    "Should not have the original BHR annotation set."
  );

  Assert.ok(
    hasHangAnnotation(report, TEST_VALUE_2),
    "Should have the updated BHR annotation set."
  );

  // Next, we'll ensure that we can update multiple markers and annotations
  // by using the optional object argument to start() and finish().
  startProfiler();

  let obj1 = {};
  let obj2 = {};
  UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_1, obj1);
  UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_2, obj2);

  // Now swap the values between the two UserInteractions
  UserInteraction.update(TEST_USER_INTERACTION_ID, TEST_VALUE_2, obj1);
  UserInteraction.update(TEST_USER_INTERACTION_ID, TEST_VALUE_1, obj2);

  report = await hangAndWaitForReport(true);
  UserInteraction.finish(
    TEST_USER_INTERACTION_ID,
    obj1,
    TEST_ADDITIONAL_TEXT_1
  );
  UserInteraction.finish(
    TEST_USER_INTERACTION_ID,
    obj2,
    TEST_ADDITIONAL_TEXT_2
  );
  profile = Services.profiler.getProfileData();

  stopProfiler();

  Assert.equal(
    markerCount(profile, TEST_VALUE_2, TEST_ADDITIONAL_TEXT_1),
    1,
    "Should have found first marker in the profile."
  );

  Assert.equal(
    markerCount(profile, TEST_VALUE_1, TEST_ADDITIONAL_TEXT_2),
    1,
    "Should have found second marker in the profile."
  );

  Assert.ok(
    hasHangAnnotation(report, TEST_VALUE_1),
    "Should have the first BHR annotation set."
  );

  Assert.ok(
    hasHangAnnotation(report, TEST_VALUE_2),
    "Should have the second BHR annotation set."
  );
});

/**
 * Tests that UserInteractions can be cancelled, resulting in no BHR
 * annotations and profiler markers being recorded.
 */
add_task(async function test_cancelling_annotations_and_markers() {
  if (!Services.telemetry.canRecordExtended) {
    Assert.ok("Hang reporting not enabled.");
    return;
  }

  ensureProfilerInitialized();

  // First, we'll check to see if we can get a single annotation and
  // profiler marker to be set.
  startProfiler();

  UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_1);
  UserInteraction.cancel(TEST_USER_INTERACTION_ID);
  let report = await hangAndWaitForReport(false);

  let profile = Services.profiler.getProfileData();

  stopProfiler();

  Assert.equal(
    markerCount(profile, TEST_VALUE_1),
    0,
    "Should not have found the marker in the profile."
  );

  Assert.ok(
    !hasHangAnnotation(report, TEST_VALUE_1),
    "Should not have the BHR annotation set."
  );

  // Next, we'll ensure that we can cancel multiple markers and annotations
  // by using the optional object argument to start() and finish().
  startProfiler();

  let obj1 = {};
  let obj2 = {};
  UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_1, obj1);
  UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_2, obj2);

  UserInteraction.cancel(TEST_USER_INTERACTION_ID, obj1);
  UserInteraction.cancel(TEST_USER_INTERACTION_ID, obj2);

  report = await hangAndWaitForReport(false);

  Assert.ok(
    !UserInteraction.finish(TEST_USER_INTERACTION_ID, obj1),
    "Finishing a canceled UserInteraction should return false."
  );

  Assert.ok(
    !UserInteraction.finish(TEST_USER_INTERACTION_ID, obj2),
    "Finishing a canceled UserInteraction should return false."
  );

  profile = Services.profiler.getProfileData();

  stopProfiler();

  Assert.equal(
    markerCount(profile, TEST_VALUE_1),
    0,
    "Should not have found the first marker in the profile."
  );

  Assert.equal(
    markerCount(profile, TEST_VALUE_2),
    0,
    "Should not have found the second marker in the profile."
  );

  Assert.ok(
    !hasHangAnnotation(report, TEST_VALUE_1),
    "Should not have the first BHR annotation set."
  );

  Assert.ok(
    !hasHangAnnotation(report, TEST_VALUE_2),
    "Should not have the second BHR annotation set."
  );
});

/**
 * Tests that starting UserInteractions with the same ID and object
 * creates a clobber annotation.
 */
add_task(async function test_clobbered_annotations() {
  if (!Services.telemetry.canRecordExtended) {
    Assert.ok("Hang reporting not enabled.");
    return;
  }

  UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_1);
  // Now clobber the original UserInteraction
  UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_2);

  let report = await hangAndWaitForReport(true);
  Assert.ok(
    UserInteraction.finish(TEST_USER_INTERACTION_ID),
    "Should have been able to finish the UserInteraction."
  );

  Assert.ok(
    !hasHangAnnotation(report, TEST_VALUE_1),
    "Should not have the original BHR annotation set."
  );

  Assert.ok(
    hasClobberedHangAnnotation(report, TEST_VALUE_2),
    "Should have the clobber BHR annotation set."
  );
});
