/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {utils: Cu} = Components;

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/services/datareporting/policy.jsm");
Cu.import("resource://testing-common/services/datareporting/mocks.jsm");
Cu.import("resource://gre/modules/UpdateUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");

function getPolicy(name,
                   aCurrentPolicyVersion = 1,
                   aMinimumPolicyVersion = 1,
                   aBranchMinimumVersionOverride) {
  let branch = "testing.datareporting." + name;

  // The version prefs should not be removed on reset, so set them in the
  // default branch.
  let defaultPolicyPrefs = new Preferences({ branch: branch + ".policy."
                                           , defaultBranch: true });
  defaultPolicyPrefs.set("currentPolicyVersion", aCurrentPolicyVersion);
  defaultPolicyPrefs.set("minimumPolicyVersion", aMinimumPolicyVersion);
  let branchOverridePrefName = "minimumPolicyVersion.channel-" + UpdateUtils.getUpdateChannel(false);
  if (aBranchMinimumVersionOverride !== undefined)
    defaultPolicyPrefs.set(branchOverridePrefName, aBranchMinimumVersionOverride);
  else
    defaultPolicyPrefs.reset(branchOverridePrefName);

  let policyPrefs = new Preferences(branch + ".policy.");
  let healthReportPrefs = new Preferences(branch + ".healthreport.");

  let listener = new MockPolicyListener();
  let policy = new DataReportingPolicy(policyPrefs, healthReportPrefs, listener);

  return [policy, policyPrefs, healthReportPrefs, listener];
}

/**
 * Ensure that the notification has been displayed to the user therefore having
 * policy.ensureUserNotified() === true, which will allow for a successful
 * data upload and afterwards does a call to policy.checkStateAndTrigger()
 * @param  {Policy} policy
 * @return {Promise}
 */
function ensureUserNotifiedAndTrigger(policy) {
  return Task.spawn(function* ensureUserNotifiedAndTrigger () {
    policy.ensureUserNotified();
    yield policy._listener.lastNotifyRequest.deferred.promise;
    do_check_true(policy.userNotifiedOfCurrentPolicy);
    policy.checkStateAndTrigger();
  });
}

function defineNow(policy, now) {
  print("Adjusting fake system clock to " + now);
  Object.defineProperty(policy, "now", {
    value: function customNow() {
      return now;
    },
    writable: true,
  });
}

function run_test() {
  run_next_test();
}

add_test(function test_constructor() {
  let policyPrefs = new Preferences("foo.bar.policy.");
  let hrPrefs = new Preferences("foo.bar.healthreport.");
  let listener = {
    onRequestDataUpload: function() {},
    onRequestRemoteDelete: function() {},
    onNotifyDataPolicy: function() {},
  };

  let policy = new DataReportingPolicy(policyPrefs, hrPrefs, listener);
  do_check_true(Date.now() - policy.firstRunDate.getTime() < 1000);

  let tomorrow = Date.now() + 24 * 60 * 60 * 1000;
  do_check_true(tomorrow - policy.nextDataSubmissionDate.getTime() < 1000);

  do_check_eq(policy.dataSubmissionPolicyAcceptedVersion, 0);
  do_check_false(policy.userNotifiedOfCurrentPolicy);

  run_next_test();
});

add_test(function test_prefs() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("prefs");

  let now = new Date();
  let nowT = now.getTime();

  policy.firstRunDate = now;
  do_check_eq(policyPrefs.get("firstRunTime"), nowT);
  do_check_eq(policy.firstRunDate.getTime(), nowT);

  policy.dataSubmissionPolicyNotifiedDate = now;
  do_check_eq(policyPrefs.get("dataSubmissionPolicyNotifiedTime"), nowT);
  do_check_neq(policy.dataSubmissionPolicyNotifiedDate, null);
  do_check_eq(policy.dataSubmissionPolicyNotifiedDate.getTime(), nowT);

  policy.dataSubmissionEnabled = false;
  do_check_false(policyPrefs.get("dataSubmissionEnabled", true));
  do_check_false(policy.dataSubmissionEnabled);

  let new_version = DATAREPORTING_POLICY_VERSION + 1;
  policy.dataSubmissionPolicyAcceptedVersion = new_version;
  do_check_eq(policyPrefs.get("dataSubmissionPolicyAcceptedVersion"), new_version);

  do_check_false(policy.dataSubmissionPolicyBypassNotification);
  policy.dataSubmissionPolicyBypassNotification = true;
  do_check_true(policy.dataSubmissionPolicyBypassNotification);
  do_check_true(policyPrefs.get("dataSubmissionPolicyBypassNotification"));

  policy.lastDataSubmissionRequestedDate = now;
  do_check_eq(hrPrefs.get("lastDataSubmissionRequestedTime"), nowT);
  do_check_eq(policy.lastDataSubmissionRequestedDate.getTime(), nowT);

  policy.lastDataSubmissionSuccessfulDate = now;
  do_check_eq(hrPrefs.get("lastDataSubmissionSuccessfulTime"), nowT);
  do_check_eq(policy.lastDataSubmissionSuccessfulDate.getTime(), nowT);

  policy.lastDataSubmissionFailureDate = now;
  do_check_eq(hrPrefs.get("lastDataSubmissionFailureTime"), nowT);
  do_check_eq(policy.lastDataSubmissionFailureDate.getTime(), nowT);

  policy.nextDataSubmissionDate = now;
  do_check_eq(hrPrefs.get("nextDataSubmissionTime"), nowT);
  do_check_eq(policy.nextDataSubmissionDate.getTime(), nowT);

  policy.currentDaySubmissionFailureCount = 2;
  do_check_eq(hrPrefs.get("currentDaySubmissionFailureCount", 0), 2);
  do_check_eq(policy.currentDaySubmissionFailureCount, 2);

  policy.pendingDeleteRemoteData = true;
  do_check_true(hrPrefs.get("pendingDeleteRemoteData"));
  do_check_true(policy.pendingDeleteRemoteData);

  policy.healthReportUploadEnabled = false;
  do_check_false(hrPrefs.get("uploadEnabled"));
  do_check_false(policy.healthReportUploadEnabled);

  do_check_false(policy.healthReportUploadLocked);
  hrPrefs.lock("uploadEnabled");
  do_check_true(policy.healthReportUploadLocked);
  hrPrefs.unlock("uploadEnabled");
  do_check_false(policy.healthReportUploadLocked);

  run_next_test();
});

add_task(function test_migratePrefs () {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("migratePrefs");
  let outdated_prefs = {
    dataSubmissionPolicyAccepted: true,
    dataSubmissionPolicyBypassAcceptance: true,
    dataSubmissionPolicyResponseType: "something",
    dataSubmissionPolicyResponseTime: Date.now() + "",
  };

  // Test removal of old prefs.
  for (let name in outdated_prefs) {
    policyPrefs.set(name, outdated_prefs[name]);
  }
  policy._migratePrefs();
  for (let name in outdated_prefs) {
    do_check_false(policyPrefs.has(name));
  }
});

add_task(function test_userNotifiedOfCurrentPolicy () {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("initial_submission_notification");

  do_check_false(policy.userNotifiedOfCurrentPolicy,
                 "The initial state should be unnotified.");
  do_check_eq(policy.dataSubmissionPolicyNotifiedDate.getTime(), 0);

  policy.dataSubmissionPolicyAcceptedVersion = DATAREPORTING_POLICY_VERSION;
  do_check_false(policy.userNotifiedOfCurrentPolicy,
                 "The default state of the date should have a time of 0 and it should therefore fail");
  do_check_eq(policy.dataSubmissionPolicyNotifiedDate.getTime(), 0,
              "Updating the accepted version should not set a notified date.");

  policy._recordDataPolicyNotification(new Date(), DATAREPORTING_POLICY_VERSION);
  do_check_true(policy.userNotifiedOfCurrentPolicy,
                "Using the proper API causes user notification to report as true.");

  // It is assumed that later versions of the policy will incorporate previous
  // ones, therefore this should also return true.
  policy._recordDataPolicyNotification(new Date(), DATAREPORTING_POLICY_VERSION);
  policy.dataSubmissionPolicyAcceptedVersion = DATAREPORTING_POLICY_VERSION + 1;
  do_check_true(policy.userNotifiedOfCurrentPolicy, 'A future version of the policy should pass.');

  policy._recordDataPolicyNotification(new Date(), DATAREPORTING_POLICY_VERSION);
  policy.dataSubmissionPolicyAcceptedVersion = DATAREPORTING_POLICY_VERSION - 1;
  do_check_false(policy.userNotifiedOfCurrentPolicy, 'A previous version of the policy should fail.');
});

add_task(function* test_notification_displayed () {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("notification_accept_displayed");

  do_check_eq(listener.requestDataUploadCount, 0);
  do_check_eq(listener.notifyUserCount, 0);
  do_check_eq(policy.dataSubmissionPolicyNotifiedDate.getTime(), 0);

  // Uploads will trigger user notifications as needed.
  policy.checkStateAndTrigger();
  do_check_eq(listener.notifyUserCount, 1);
  do_check_eq(listener.requestDataUploadCount, 0);

  yield ensureUserNotifiedAndTrigger(policy);

  do_check_eq(listener.notifyUserCount, 1);
  do_check_true(policy.dataSubmissionPolicyNotifiedDate.getTime() > 0);
  do_check_true(policy.userNotifiedOfCurrentPolicy);
});

add_task(function* test_submission_kill_switch() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("submission_kill_switch");
  policy.nextDataSubmissionDate = new Date(Date.now() - 24 * 60 * 60 * 1000);
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 0);
  yield ensureUserNotifiedAndTrigger(policy);
  do_check_eq(listener.requestDataUploadCount, 1);

  defineNow(policy,
    new Date(Date.now() + policy.SUBMISSION_REQUEST_EXPIRE_INTERVAL_MSEC + 100));
  policy.dataSubmissionEnabled = false;
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 1);
});

add_task(function* test_upload_kill_switch() {
   let [policy, policyPrefs, hrPrefs, listener] = getPolicy("upload_kill_switch");

  yield ensureUserNotifiedAndTrigger(policy);
  defineNow(policy, policy.nextDataSubmissionDate);

  // So that we don't trigger deletions, which cause uploads to be delayed.
  hrPrefs.ignore("uploadEnabled", policy.uploadEnabledObserver);

  policy.healthReportUploadEnabled = false;
  yield policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 0);
  policy.healthReportUploadEnabled = true;
  yield ensureUserNotifiedAndTrigger(policy);
  do_check_eq(listener.requestDataUploadCount, 1);
});

add_task(function* test_data_submission_no_data() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("data_submission_no_data");

  let now = new Date(policy.nextDataSubmissionDate.getTime() + 1);
  defineNow(policy, now);
  do_check_eq(listener.requestDataUploadCount, 0);
  yield ensureUserNotifiedAndTrigger(policy);
  do_check_eq(listener.requestDataUploadCount, 1);
  listener.lastDataRequest.onNoDataAvailable();

  // The next trigger should try again.
  defineNow(policy, new Date(now.getTime() + 155 * 60 * 1000));
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 2);
 });

add_task(function* test_data_submission_submit_failure_hard() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("data_submission_submit_failure_hard");

  let nextDataSubmissionDate = policy.nextDataSubmissionDate;
  let now = new Date(policy.nextDataSubmissionDate.getTime() + 1);
  defineNow(policy, now);

  yield ensureUserNotifiedAndTrigger(policy);
  do_check_eq(listener.requestDataUploadCount, 1);
  yield listener.lastDataRequest.onSubmissionFailureHard();
  do_check_eq(listener.lastDataRequest.state,
              listener.lastDataRequest.SUBMISSION_FAILURE_HARD);

  let expected = new Date(now.getTime() + 24 * 60 * 60 * 1000);
  do_check_eq(policy.nextDataSubmissionDate.getTime(), expected.getTime());

  defineNow(policy, new Date(now.getTime() + 10));
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 1);
});

add_task(function* test_data_submission_submit_try_again() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("data_submission_failure_soft");

  let nextDataSubmissionDate = policy.nextDataSubmissionDate;
  let now = new Date(policy.nextDataSubmissionDate.getTime());
  defineNow(policy, now);
  yield ensureUserNotifiedAndTrigger(policy);
  yield listener.lastDataRequest.onSubmissionFailureSoft();
  do_check_eq(policy.nextDataSubmissionDate.getTime(),
              nextDataSubmissionDate.getTime() + 15 * 60 * 1000);
});

add_task(function* test_submission_daily_scheduling() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("submission_daily_scheduling");

  let nextDataSubmissionDate = policy.nextDataSubmissionDate;

  // Skip ahead to next submission date. We should get a submission request.
  let now = new Date(policy.nextDataSubmissionDate.getTime());
  defineNow(policy, now);
  yield ensureUserNotifiedAndTrigger(policy);
  do_check_eq(listener.requestDataUploadCount, 1);
  do_check_eq(policy.lastDataSubmissionRequestedDate.getTime(), now.getTime());

  let finishedDate = new Date(now.getTime() + 250);
  defineNow(policy, new Date(finishedDate.getTime() + 50));
  yield listener.lastDataRequest.onSubmissionSuccess(finishedDate);
  do_check_eq(policy.lastDataSubmissionSuccessfulDate.getTime(), finishedDate.getTime());

  // Next scheduled submission should be exactly 1 day after the reported
  // submission success.

  let nextScheduled = new Date(finishedDate.getTime() + 24 * 60 * 60 * 1000);
  do_check_eq(policy.nextDataSubmissionDate.getTime(), nextScheduled.getTime());

  // Fast forward some arbitrary time. We shouldn't do any work yet.
  defineNow(policy, new Date(now.getTime() + 40000));
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 1);

  defineNow(policy, nextScheduled);
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 2);
  yield listener.lastDataRequest.onSubmissionSuccess(new Date(nextScheduled.getTime() + 200));
  do_check_eq(policy.nextDataSubmissionDate.getTime(),
    new Date(nextScheduled.getTime() + 24 * 60 * 60 * 1000 + 200).getTime());
});

add_task(function* test_submission_far_future_scheduling() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("submission_far_future_scheduling");

  let now = new Date(Date.now() - 24 * 60 * 60 * 1000);
  defineNow(policy, now);
  yield ensureUserNotifiedAndTrigger(policy);

  let nextDate = policy._futureDate(3 * 24 * 60 * 60 * 1000 - 1);
  policy.nextDataSubmissionDate = nextDate;
  policy.checkStateAndTrigger();
  do_check_true(policy.dataSubmissionPolicyAcceptedVersion >= DATAREPORTING_POLICY_VERSION);
  do_check_eq(listener.requestDataUploadCount, 0);
  do_check_eq(policy.nextDataSubmissionDate.getTime(), nextDate.getTime());

  policy.nextDataSubmissionDate = new Date(nextDate.getTime() + 1);
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 0);
  do_check_eq(policy.nextDataSubmissionDate.getTime(),
              policy._futureDate(24 * 60 * 60 * 1000).getTime());
});

add_task(function* test_submission_backoff() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("submission_backoff");

  do_check_eq(policy.FAILURE_BACKOFF_INTERVALS.length, 2);


  let now = new Date(policy.nextDataSubmissionDate.getTime());
  defineNow(policy, now);
  yield ensureUserNotifiedAndTrigger(policy);
  do_check_eq(listener.requestDataUploadCount, 1);
  do_check_eq(policy.currentDaySubmissionFailureCount, 0);

  now = new Date(now.getTime() + 5000);
  defineNow(policy, now);

  // On first soft failure we should back off by scheduled interval.
  yield listener.lastDataRequest.onSubmissionFailureSoft();
  do_check_eq(policy.currentDaySubmissionFailureCount, 1);
  do_check_eq(policy.nextDataSubmissionDate.getTime(),
              new Date(now.getTime() + policy.FAILURE_BACKOFF_INTERVALS[0]).getTime());
  do_check_eq(policy.lastDataSubmissionFailureDate.getTime(), now.getTime());

  // Should not request submission until scheduled.
  now = new Date(policy.nextDataSubmissionDate.getTime() - 1);
  defineNow(policy, now);
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 1);

  // 2nd request for submission.
  now = new Date(policy.nextDataSubmissionDate.getTime());
  defineNow(policy, now);
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 2);

  now = new Date(now.getTime() + 5000);
  defineNow(policy, now);

  // On second failure we should back off by more.
  yield listener.lastDataRequest.onSubmissionFailureSoft();
  do_check_eq(policy.currentDaySubmissionFailureCount, 2);
  do_check_eq(policy.nextDataSubmissionDate.getTime(),
              new Date(now.getTime() + policy.FAILURE_BACKOFF_INTERVALS[1]).getTime());

  now = new Date(policy.nextDataSubmissionDate.getTime());
  defineNow(policy, now);
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 3);

  now = new Date(now.getTime() + 5000);
  defineNow(policy, now);

  // On 3rd failure we should back off by a whole day.
  yield listener.lastDataRequest.onSubmissionFailureSoft();
  do_check_eq(policy.currentDaySubmissionFailureCount, 0);
  do_check_eq(policy.nextDataSubmissionDate.getTime(),
              new Date(now.getTime() + 24 * 60 * 60 * 1000).getTime());
});

// Ensure that only one submission request can be active at a time.
add_task(function* test_submission_expiring() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("submission_expiring");

  let nextDataSubmission = policy.nextDataSubmissionDate;
  let now = new Date(policy.nextDataSubmissionDate.getTime());
  defineNow(policy, now);
  yield ensureUserNotifiedAndTrigger(policy);
  do_check_eq(listener.requestDataUploadCount, 1);
  defineNow(policy, new Date(now.getTime() + 500));
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 1);

  defineNow(policy, new Date(policy.now().getTime() +
                             policy.SUBMISSION_REQUEST_EXPIRE_INTERVAL_MSEC));

  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 2);
});

add_task(function* test_delete_remote_data() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("delete_remote_data");

  do_check_false(policy.pendingDeleteRemoteData);
  let nextSubmissionDate = policy.nextDataSubmissionDate;

  let now = new Date();
  defineNow(policy, now);

  policy.deleteRemoteData();
  do_check_true(policy.pendingDeleteRemoteData);
  do_check_neq(nextSubmissionDate.getTime(),
               policy.nextDataSubmissionDate.getTime());
  do_check_eq(now.getTime(), policy.nextDataSubmissionDate.getTime());

  do_check_eq(listener.requestRemoteDeleteCount, 1);
  do_check_true(listener.lastRemoteDeleteRequest.isDelete);
  defineNow(policy, policy._futureDate(1000));

  yield listener.lastRemoteDeleteRequest.onSubmissionSuccess(policy.now());
  do_check_false(policy.pendingDeleteRemoteData);
});

// Ensure that deletion requests take priority over regular data submission.
add_task(function* test_delete_remote_data_priority() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("delete_remote_data_priority");

  let now = new Date();
  defineNow(policy, new Date(now.getTime() + 3 * 24 * 60 * 60 * 1000));

  yield ensureUserNotifiedAndTrigger(policy);
  do_check_eq(listener.requestDataUploadCount, 1);
  policy._inProgressSubmissionRequest = null;

  policy.deleteRemoteData();
  policy.checkStateAndTrigger();

  do_check_eq(listener.requestRemoteDeleteCount, 1);
  do_check_eq(listener.requestDataUploadCount, 1);
});

add_test(function test_delete_remote_data_backoff() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("delete_remote_data_backoff");

  let now = new Date();
  defineNow(policy, now);
  policy.nextDataSubmissionDate = now;
  policy.deleteRemoteData();

  policy.checkStateAndTrigger();
  do_check_eq(listener.requestRemoteDeleteCount, 1);
  defineNow(policy, policy._futureDate(1000));
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 0);
  do_check_eq(listener.requestRemoteDeleteCount, 1);

  defineNow(policy, policy._futureDate(500));
  listener.lastRemoteDeleteRequest.onSubmissionFailureSoft();
  defineNow(policy, policy._futureDate(50));

  policy.checkStateAndTrigger();
  do_check_eq(listener.requestRemoteDeleteCount, 1);

  defineNow(policy, policy._futureDate(policy.FAILURE_BACKOFF_INTERVALS[0] - 50));
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestRemoteDeleteCount, 2);

  run_next_test();
});

// If we request delete while an upload is in progress, delete should be
// scheduled immediately after upload.
add_task(function* test_delete_remote_data_in_progress_upload() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("delete_remote_data_in_progress_upload");

  defineNow(policy, policy.nextDataSubmissionDate);

  yield ensureUserNotifiedAndTrigger(policy);
  do_check_eq(listener.requestDataUploadCount, 1);
  defineNow(policy, policy._futureDate(50 * 1000));

  // If we request a delete during a pending request, nothing should be done.
  policy.deleteRemoteData();
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 1);
  do_check_eq(listener.requestRemoteDeleteCount, 0);

  // Now wait a little bit and finish the request.
  defineNow(policy, policy._futureDate(10 * 1000));
  yield listener.lastDataRequest.onSubmissionSuccess(policy._futureDate(1000));
  defineNow(policy, policy._futureDate(5000));

  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 1);
  do_check_eq(listener.requestRemoteDeleteCount, 1);
});

add_test(function test_polling() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("polling");
  let intended = 500;
  let acceptable = 250;     // Because nsITimer doesn't guarantee times.

  // Ensure checkStateAndTrigger is called at a regular interval.
  let then = Date.now();
  print("Starting run: " + then);
  Object.defineProperty(policy, "POLL_INTERVAL_MSEC", {
    value: intended,
  });
  let count = 0;

  Object.defineProperty(policy, "checkStateAndTrigger", {
    value: function fakeCheckStateAndTrigger() {
      let now = Date.now();
      let after = now - then;
      count++;

      print("Polled at " + now + " after " + after + "ms, intended " + intended);
      do_check_true(after >= acceptable);
      DataReportingPolicy.prototype.checkStateAndTrigger.call(policy);

      if (count >= 2) {
        policy.stopPolling();

        do_check_eq(listener.requestDataUploadCount, 0);

        run_next_test();
      }

      // "Specified timer period will be at least the time between when
      // processing for last firing the callback completes and when the next
      // firing occurs."
      //
      // That means we should set 'then' at the *end* of our handler, not
      // earlier.
      then = Date.now();
    }
  });
  policy.startPolling();
});

add_task(function* test_record_health_report_upload_enabled() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("record_health_report_upload_enabled");

  // Preconditions.
  do_check_false(policy.pendingDeleteRemoteData);
  do_check_true(policy.healthReportUploadEnabled);
  do_check_eq(listener.requestRemoteDeleteCount, 0);

  // User intent to disable should immediately result in a pending
  // delete request.
  policy.recordHealthReportUploadEnabled(false, "testing 1 2 3");
  do_check_false(policy.healthReportUploadEnabled);
  do_check_true(policy.pendingDeleteRemoteData);
  do_check_eq(listener.requestRemoteDeleteCount, 1);

  // Fulfilling it should make it go away.
  yield listener.lastRemoteDeleteRequest.onNoDataAvailable();
  do_check_false(policy.pendingDeleteRemoteData);

  // User intent to enable should get us back to default state.
  policy.recordHealthReportUploadEnabled(true, "testing 1 2 3");
  do_check_false(policy.pendingDeleteRemoteData);
  do_check_true(policy.healthReportUploadEnabled);
});

add_test(function test_pref_change_initiates_deletion() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("record_health_report_upload_enabled");

  // Preconditions.
  do_check_false(policy.pendingDeleteRemoteData);
  do_check_true(policy.healthReportUploadEnabled);
  do_check_eq(listener.requestRemoteDeleteCount, 0);

  // User intent to disable should indirectly result in a pending
  // delete request, because the policy is watching for the pref
  // to change.
  Object.defineProperty(policy, "deleteRemoteData", {
    value: function deleteRemoteDataProxy() {
      do_check_false(policy.healthReportUploadEnabled);
      do_check_false(policy.pendingDeleteRemoteData);     // Just called.

      run_next_test();
    },
  });

  hrPrefs.set("uploadEnabled", false);
});

add_task(function* test_policy_version() {
  let policy, policyPrefs, hrPrefs, listener, now, firstRunTime;
  function createPolicy(shouldBeNotified = false,
                        currentPolicyVersion = 1, minimumPolicyVersion = 1,
                        branchMinimumVersionOverride) {
    [policy, policyPrefs, hrPrefs, listener] =
      getPolicy("policy_version_test", currentPolicyVersion,
                minimumPolicyVersion, branchMinimumVersionOverride);
    let firstRun = now === undefined;
    if (firstRun) {
      firstRunTime = policy.firstRunDate.getTime();
      do_check_true(firstRunTime > 0);
      now = new Date(policy.firstRunDate.getTime());
    }
    else {
      // The first-run time should not be reset even after policy-version
      // upgrades.
      do_check_eq(policy.firstRunDate.getTime(), firstRunTime);
    }
    defineNow(policy, now);
    do_check_eq(policy.userNotifiedOfCurrentPolicy, shouldBeNotified);
  }

  function* triggerPolicyCheckAndEnsureNotified(notified = true) {
    policy.checkStateAndTrigger();
    do_check_eq(listener.notifyUserCount, Number(notified));
    if (notified) {
      policy.ensureUserNotified();
      yield listener.lastNotifyRequest.deferred.promise;
      do_check_true(policy.userNotifiedOfCurrentPolicy);
      do_check_eq(policyPrefs.get("dataSubmissionPolicyAcceptedVersion"),
                  policyPrefs.get("currentPolicyVersion"));
    }
  }

  createPolicy();
  yield triggerPolicyCheckAndEnsureNotified();

  // We shouldn't be notified again if the current version is still valid;
  createPolicy(true);
  yield triggerPolicyCheckAndEnsureNotified(false);

  // Just increasing the current version isn't enough. The minimum
  // version must be changed.
  let currentPolicyVersion = policyPrefs.get("currentPolicyVersion");
  let minimumPolicyVersion = policyPrefs.get("minimumPolicyVersion");
  createPolicy(false, ++currentPolicyVersion, minimumPolicyVersion);
  yield triggerPolicyCheckAndEnsureNotified(true);
  do_check_eq(policyPrefs.get("dataSubmissionPolicyAcceptedVersion"), currentPolicyVersion);

  // Increase the minimum policy version and check if we're notified.

  createPolicy(true, currentPolicyVersion, ++minimumPolicyVersion);
  do_check_true(policyPrefs.has("dataSubmissionPolicyAcceptedVersion"));
  yield triggerPolicyCheckAndEnsureNotified(false);


  // Test increasing the minimum version just on the current channel.
  createPolicy(true, currentPolicyVersion, minimumPolicyVersion);
  yield triggerPolicyCheckAndEnsureNotified(false);
  createPolicy(false, ++currentPolicyVersion, minimumPolicyVersion, minimumPolicyVersion + 1);
  yield triggerPolicyCheckAndEnsureNotified(true);
});
