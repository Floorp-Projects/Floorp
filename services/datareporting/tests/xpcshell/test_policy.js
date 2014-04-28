/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/services/datareporting/policy.jsm");
Cu.import("resource://testing-common/services/datareporting/mocks.jsm");
Cu.import("resource://gre/modules/UpdateChannel.jsm");

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
  let branchOverridePrefName = "minimumPolicyVersion.channel-" + UpdateChannel.get(false);
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

  do_check_eq(policy.notifyState, policy.STATE_NOTIFY_UNNOTIFIED);

  run_next_test();
});

add_test(function test_prefs() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("prefs");

  let now = new Date();
  let nowT = now.getTime();

  policy.firstRunDate = now;
  do_check_eq(policyPrefs.get("firstRunTime"), nowT);
  do_check_eq(policy.firstRunDate.getTime(), nowT);

  policy.dataSubmissionPolicyNotifiedDate= now;
  do_check_eq(policyPrefs.get("dataSubmissionPolicyNotifiedTime"), nowT);
  do_check_eq(policy.dataSubmissionPolicyNotifiedDate.getTime(), nowT);

  policy.dataSubmissionPolicyResponseDate = now;
  do_check_eq(policyPrefs.get("dataSubmissionPolicyResponseTime"), nowT);
  do_check_eq(policy.dataSubmissionPolicyResponseDate.getTime(), nowT);

  policy.dataSubmissionPolicyResponseType = "type-1";
  do_check_eq(policyPrefs.get("dataSubmissionPolicyResponseType"), "type-1");
  do_check_eq(policy.dataSubmissionPolicyResponseType, "type-1");

  policy.dataSubmissionEnabled = false;
  do_check_false(policyPrefs.get("dataSubmissionEnabled", true));
  do_check_false(policy.dataSubmissionEnabled);

  policy.dataSubmissionPolicyAccepted = false;
  do_check_false(policyPrefs.get("dataSubmissionPolicyAccepted", true));
  do_check_false(policy.dataSubmissionPolicyAccepted);

  do_check_false(policy.dataSubmissionPolicyBypassAcceptance);
  policyPrefs.set("dataSubmissionPolicyBypassAcceptance", true);
  do_check_true(policy.dataSubmissionPolicyBypassAcceptance);

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

add_test(function test_notify_state_prefs() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("notify_state_prefs");

  do_check_eq(policy.notifyState, policy.STATE_NOTIFY_UNNOTIFIED);

  policy._dataSubmissionPolicyNotifiedDate = new Date();
  do_check_eq(policy.notifyState, policy.STATE_NOTIFY_WAIT);

  policy.dataSubmissionPolicyResponseDate = new Date();
  policy._dataSubmissionPolicyNotifiedDate = null;
  do_check_eq(policy.notifyState, policy.STATE_NOTIFY_COMPLETE);

  run_next_test();
});

add_task(function test_initial_submission_notification() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("initial_submission_notification");

  do_check_eq(listener.notifyUserCount, 0);

  // Fresh instances should not do anything initially.
  policy.checkStateAndTrigger();
  do_check_eq(listener.notifyUserCount, 0);

  // We still shouldn't notify up to the millisecond before the barrier.
  defineNow(policy, new Date(policy.firstRunDate.getTime() +
                             policy.SUBMISSION_NOTIFY_INTERVAL_MSEC - 1));
  policy.checkStateAndTrigger();
  do_check_eq(listener.notifyUserCount, 0);
  do_check_null(policy._dataSubmissionPolicyNotifiedDate);
  do_check_eq(policy.dataSubmissionPolicyNotifiedDate.getTime(), 0);

  // We have crossed the threshold. We should see notification.
  defineNow(policy, new Date(policy.firstRunDate.getTime() +
                             policy.SUBMISSION_NOTIFY_INTERVAL_MSEC));
  policy.checkStateAndTrigger();
  do_check_eq(listener.notifyUserCount, 1);
  yield listener.lastNotifyRequest.onUserNotifyComplete();
  do_check_true(policy._dataSubmissionPolicyNotifiedDate instanceof Date);
  do_check_true(policy.dataSubmissionPolicyNotifiedDate.getTime() > 0);
  do_check_eq(policy.dataSubmissionPolicyNotifiedDate.getTime(),
              policy._dataSubmissionPolicyNotifiedDate.getTime());
  do_check_eq(policy.notifyState, policy.STATE_NOTIFY_WAIT);
});

add_test(function test_bypass_acceptance() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("bypass_acceptance");

  policyPrefs.set("dataSubmissionPolicyBypassAcceptance", true);
  do_check_false(policy.dataSubmissionPolicyAccepted);
  do_check_true(policy.dataSubmissionPolicyBypassAcceptance);
  defineNow(policy, new Date(policy.nextDataSubmissionDate.getTime()));
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 1);

  run_next_test();
});

add_task(function test_notification_implicit_acceptance() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("notification_implicit_acceptance");

  let now = new Date(policy.nextDataSubmissionDate.getTime() -
                     policy.SUBMISSION_NOTIFY_INTERVAL_MSEC + 1);
  defineNow(policy, now);
  policy.checkStateAndTrigger();
  do_check_eq(listener.notifyUserCount, 1);
  yield listener.lastNotifyRequest.onUserNotifyComplete();
  do_check_eq(policy.dataSubmissionPolicyResponseType, "none-recorded");

  do_check_true(5000 < policy.IMPLICIT_ACCEPTANCE_INTERVAL_MSEC);
  defineNow(policy, new Date(now.getTime() + 5000));
  policy.checkStateAndTrigger();
  do_check_eq(listener.notifyUserCount, 1);
  do_check_eq(policy.notifyState, policy.STATE_NOTIFY_WAIT);
  do_check_eq(policy.dataSubmissionPolicyResponseDate.getTime(), 0);
  do_check_eq(policy.dataSubmissionPolicyResponseType, "none-recorded");

  defineNow(policy, new Date(now.getTime() + policy.IMPLICIT_ACCEPTANCE_INTERVAL_MSEC + 1));
  policy.checkStateAndTrigger();
  do_check_eq(listener.notifyUserCount, 1);
  do_check_eq(policy.notifyState, policy.STATE_NOTIFY_COMPLETE);
  do_check_eq(policy.dataSubmissionPolicyResponseDate.getTime(), policy.now().getTime());
  do_check_eq(policy.dataSubmissionPolicyResponseType, "accepted-implicit-time-elapsed");
});

add_task(function test_notification_rejected() {
  // User notification failed. We should not record it as being presented.
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("notification_failed");

  let now = new Date(policy.nextDataSubmissionDate.getTime() -
                     policy.SUBMISSION_NOTIFY_INTERVAL_MSEC + 1);
  defineNow(policy, now);
  policy.checkStateAndTrigger();
  do_check_eq(listener.notifyUserCount, 1);
  yield listener.lastNotifyRequest.onUserNotifyFailed(new Error("testing failed."));
  do_check_null(policy._dataSubmissionPolicyNotifiedDate);
  do_check_eq(policy.dataSubmissionPolicyNotifiedDate.getTime(), 0);
  do_check_eq(policy.notifyState, policy.STATE_NOTIFY_UNNOTIFIED);
});

add_task(function test_notification_accepted() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("notification_accepted");

  let now = new Date(policy.nextDataSubmissionDate.getTime() -
                     policy.SUBMISSION_NOTIFY_INTERVAL_MSEC + 1);
  defineNow(policy, now);
  policy.checkStateAndTrigger();
  yield listener.lastNotifyRequest.onUserNotifyComplete();
  do_check_eq(policy.notifyState, policy.STATE_NOTIFY_WAIT);
  do_check_false(policy.dataSubmissionPolicyAccepted);
  listener.lastNotifyRequest.onUserNotifyComplete();
  listener.lastNotifyRequest.onUserAccept("foo-bar");
  do_check_eq(policy.notifyState, policy.STATE_NOTIFY_COMPLETE);
  do_check_eq(policy.dataSubmissionPolicyResponseType, "accepted-foo-bar");
  do_check_true(policy.dataSubmissionPolicyAccepted);
  do_check_eq(policy.dataSubmissionPolicyResponseDate.getTime(), now.getTime());
});

add_task(function test_notification_rejected() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("notification_rejected");

  let now = new Date(policy.nextDataSubmissionDate.getTime() -
                     policy.SUBMISSION_NOTIFY_INTERVAL_MSEC + 1);
  defineNow(policy, now);
  policy.checkStateAndTrigger();
  yield listener.lastNotifyRequest.onUserNotifyComplete();
  do_check_eq(policy.notifyState, policy.STATE_NOTIFY_WAIT);
  do_check_false(policy.dataSubmissionPolicyAccepted);
  listener.lastNotifyRequest.onUserReject();
  do_check_eq(policy.notifyState, policy.STATE_NOTIFY_COMPLETE);
  do_check_eq(policy.dataSubmissionPolicyResponseType, "rejected-no-reason");
  do_check_false(policy.dataSubmissionPolicyAccepted);

  // No requests for submission should occur if user has rejected.
  defineNow(policy, new Date(policy.nextDataSubmissionDate.getTime() + 10000));
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 0);
});

add_test(function test_submission_kill_switch() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("submission_kill_switch");

  policy.firstRunDate = new Date(Date.now() - 3 * 24 * 60 * 60 * 1000);
  policy.nextDataSubmissionDate = new Date(Date.now() - 24 * 60 * 60 * 1000);
  policy.recordUserAcceptance("accept-old-ack");
  do_check_true(policyPrefs.has("dataSubmissionPolicyAcceptedVersion"));
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 1);

  defineNow(policy,
    new Date(Date.now() + policy.SUBMISSION_REQUEST_EXPIRE_INTERVAL_MSEC + 100));
  policy.dataSubmissionEnabled = false;
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 1);

  run_next_test();
});

add_test(function test_upload_kill_switch() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("upload_kill_switch");

  defineNow(policy, policy._futureDate(-24 * 60 * 60 * 1000));
  policy.recordUserAcceptance();
  defineNow(policy, policy.nextDataSubmissionDate);

  // So that we don't trigger deletions, which cause uploads to be delayed.
  hrPrefs.ignore("uploadEnabled", policy.uploadEnabledObserver);

  policy.healthReportUploadEnabled = false;
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 0);
  policy.healthReportUploadEnabled = true;
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 1);

  run_next_test();
});

add_test(function test_data_submission_no_data() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("data_submission_no_data");

  policy.dataSubmissionPolicyResponseDate = new Date(Date.now() - 24 * 60 * 60 * 1000);
  policy.dataSubmissionPolicyAccepted = true;
  let now = new Date(policy.nextDataSubmissionDate.getTime() + 1);
  defineNow(policy, now);
  do_check_eq(listener.requestDataUploadCount, 0);
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 1);
  listener.lastDataRequest.onNoDataAvailable();

  // The next trigger should try again.
  defineNow(policy, new Date(now.getTime() + 155 * 60 * 1000));
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 2);

  run_next_test();
});

add_task(function test_data_submission_submit_failure_hard() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("data_submission_submit_failure_hard");

  policy.dataSubmissionPolicyResponseDate = new Date(Date.now() - 24 * 60 * 60 * 1000);
  policy.dataSubmissionPolicyAccepted = true;
  let nextDataSubmissionDate = policy.nextDataSubmissionDate;
  let now = new Date(policy.nextDataSubmissionDate.getTime() + 1);
  defineNow(policy, now);

  policy.checkStateAndTrigger();
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

add_task(function test_data_submission_submit_try_again() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("data_submission_failure_soft");

  policy.recordUserAcceptance();
  let nextDataSubmissionDate = policy.nextDataSubmissionDate;
  let now = new Date(policy.nextDataSubmissionDate.getTime());
  defineNow(policy, now);
  policy.checkStateAndTrigger();
  yield listener.lastDataRequest.onSubmissionFailureSoft();
  do_check_eq(policy.nextDataSubmissionDate.getTime(),
              nextDataSubmissionDate.getTime() + 15 * 60 * 1000);
});

add_task(function test_submission_daily_scheduling() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("submission_daily_scheduling");

  policy.dataSubmissionPolicyResponseDate = new Date(Date.now() - 24 * 60 * 60 * 1000);
  policy.dataSubmissionPolicyAccepted = true;
  let nextDataSubmissionDate = policy.nextDataSubmissionDate;

  // Skip ahead to next submission date. We should get a submission request.
  let now = new Date(policy.nextDataSubmissionDate.getTime());
  defineNow(policy, now);
  policy.checkStateAndTrigger();
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

add_test(function test_submission_far_future_scheduling() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("submission_far_future_scheduling");

  let now = new Date(Date.now() - 24 * 60 * 60 * 1000);
  defineNow(policy, now);
  policy.recordUserAcceptance();
  now = new Date();
  defineNow(policy, now);

  let nextDate = policy._futureDate(3 * 24 * 60 * 60 * 1000 - 1);
  policy.nextDataSubmissionDate = nextDate;
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 0);
  do_check_eq(policy.nextDataSubmissionDate.getTime(), nextDate.getTime());

  policy.nextDataSubmissionDate = new Date(nextDate.getTime() + 1);
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 0);
  do_check_eq(policy.nextDataSubmissionDate.getTime(),
              policy._futureDate(24 * 60 * 60 * 1000).getTime());

  run_next_test();
});

add_task(function test_submission_backoff() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("submission_backoff");

  do_check_eq(policy.FAILURE_BACKOFF_INTERVALS.length, 2);

  policy.dataSubmissionPolicyResponseDate = new Date(Date.now() - 24 * 60 * 60 * 1000);
  policy.dataSubmissionPolicyAccepted = true;

  let now = new Date(policy.nextDataSubmissionDate.getTime());
  defineNow(policy, now);
  policy.checkStateAndTrigger();
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
add_test(function test_submission_expiring() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("submission_expiring");

  policy.dataSubmissionPolicyResponseDate = new Date(Date.now() - 24 * 60 * 60 * 1000);
  policy.dataSubmissionPolicyAccepted = true;
  let nextDataSubmission = policy.nextDataSubmissionDate;
  let now = new Date(policy.nextDataSubmissionDate.getTime());
  defineNow(policy, now);
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 1);
  defineNow(policy, new Date(now.getTime() + 500));
  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 1);

  defineNow(policy, new Date(policy.now().getTime() +
                             policy.SUBMISSION_REQUEST_EXPIRE_INTERVAL_MSEC));

  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 2);

  run_next_test();
});

add_task(function test_delete_remote_data() {
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
add_test(function test_delete_remote_data_priority() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("delete_remote_data_priority");

  let now = new Date();
  defineNow(policy, policy._futureDate(-24 * 60 * 60 * 1000));
  policy.recordUserAcceptance();
  defineNow(policy, new Date(now.getTime() + 3 * 24 * 60 * 60 * 1000));

  policy.checkStateAndTrigger();
  do_check_eq(listener.requestDataUploadCount, 1);
  policy._inProgressSubmissionRequest = null;

  policy.deleteRemoteData();
  policy.checkStateAndTrigger();

  do_check_eq(listener.requestRemoteDeleteCount, 1);
  do_check_eq(listener.requestDataUploadCount, 1);

  run_next_test();
});

add_test(function test_delete_remote_data_backoff() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("delete_remote_data_backoff");

  let now = new Date();
  defineNow(policy, policy._futureDate(-24 * 60 * 60 * 1000));
  policy.recordUserAcceptance();
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
add_task(function test_delete_remote_data_in_progress_upload() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("delete_remote_data_in_progress_upload");

  let now = new Date();
  defineNow(policy, policy._futureDate(-24 * 60 * 60 * 1000));
  policy.recordUserAcceptance();
  defineNow(policy, policy.nextDataSubmissionDate);

  policy.checkStateAndTrigger();
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

        do_check_eq(listener.notifyUserCount, 0);
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

// Ensure that implicit acceptance of policy is resolved through polling.
//
// This is probably covered by other tests. But, it's best to have explicit
// coverage from a higher-level.
add_test(function test_polling_implicit_acceptance() {
  let [policy, policyPrefs, hrPrefs, listener] = getPolicy("polling_implicit_acceptance");

  // Redefine intervals with shorter, test-friendly values.
  Object.defineProperty(policy, "POLL_INTERVAL_MSEC", {
    value: 250,
  });

  Object.defineProperty(policy, "IMPLICIT_ACCEPTANCE_INTERVAL_MSEC", {
    value: 700,
  });

  let count = 0;

  // Track JS elapsed time, so we can decide if we've waited for enough ticks.
  let start;
  Object.defineProperty(policy, "checkStateAndTrigger", {
    value: function CheckStateAndTriggerProxy() {
      count++;
      let now = Date.now();
      let delta = now - start;
      print("checkStateAndTrigger count: " + count + ", now " + now +
            ", delta " + delta);

      // Account for some slack.
      DataReportingPolicy.prototype.checkStateAndTrigger.call(policy);

      // What should happen on different invocations:
      //
      //   1) We are inside the prompt interval so user gets prompted.
      //   2) still ~300ms away from implicit acceptance
      //   3) still ~50ms away from implicit acceptance
      //   4) Implicit acceptance recorded. Data submission requested.
      //   5) Request still pending. No new submission requested.
      //
      // Note that, due to the inaccuracy of timers, 4 might not happen until 5
      // firings have occurred. Yay. So we watch times, not just counts.

      do_check_eq(listener.notifyUserCount, 1);

      if (count == 1) {
        listener.lastNotifyRequest.onUserNotifyComplete();
      }

      if (delta <= (policy.IMPLICIT_ACCEPTANCE_INTERVAL_MSEC + policy.POLL_INTERVAL_MSEC)) {
        do_check_false(policy.dataSubmissionPolicyAccepted);
        do_check_eq(listener.requestDataUploadCount, 0);
      } else if (count > 3) {
        do_check_true(policy.dataSubmissionPolicyAccepted);
        do_check_eq(policy.dataSubmissionPolicyResponseType,
                    "accepted-implicit-time-elapsed");
        do_check_eq(listener.requestDataUploadCount, 1);
      }

      if ((count > 4) && policy.dataSubmissionPolicyAccepted) {
        do_check_eq(listener.requestDataUploadCount, 1);
        policy.stopPolling();
        run_next_test();
      }
    }
  });

  policy.firstRunDate = new Date(Date.now() - 4 * 24 * 60 * 60 * 1000);
  policy.nextDataSubmissionDate = new Date(Date.now());
  start = Date.now();
  policy.startPolling();
});

add_task(function test_record_health_report_upload_enabled() {
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
  function createPolicy(shouldBeAccepted = false,
                        currentPolicyVersion = 1, minimumPolicyVersion = 1,
                        branchMinimumVersionOverride) {
    [policy, policyPrefs, hrPrefs, listener] =
      getPolicy("policy_version_test", currentPolicyVersion,
                minimumPolicyVersion, branchMinimumVersionOverride);
    let firstRun = now === undefined;
    if (firstRun) {
      firstRunTime = policy.firstRunDate.getTime();
      do_check_true(firstRunTime > 0);
      now = new Date(policy.firstRunDate.getTime() +
                     policy.SUBMISSION_NOTIFY_INTERVAL_MSEC);
    }
    else {
      // The first-run time should not be reset even after policy-version
      // upgrades.
      do_check_eq(policy.firstRunDate.getTime(), firstRunTime);
    }
    defineNow(policy, now);
    do_check_eq(policy.dataSubmissionPolicyAccepted, shouldBeAccepted);
  }

  function* triggerPolicyCheckAndEnsureNotified(notified = true, accept = true) {
    policy.checkStateAndTrigger();
    do_check_eq(listener.notifyUserCount, Number(notified));
    if (notified) {
      yield listener.lastNotifyRequest.onUserNotifyComplete();
      if (accept) {
        listener.lastNotifyRequest.onUserAccept("because,");
        do_check_true(policy.dataSubmissionPolicyAccepted);
        do_check_eq(policyPrefs.get("dataSubmissionPolicyAcceptedVersion"),
                    policyPrefs.get("currentPolicyVersion"));
      }
      else {
        do_check_false(policyPrefs.has("dataSubmissionPolicyAcceptedVersion"));
      }
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
  createPolicy(true, ++currentPolicyVersion, minimumPolicyVersion);
  yield triggerPolicyCheckAndEnsureNotified(false);
  do_check_true(policy.dataSubmissionPolicyAccepted);
  do_check_eq(policyPrefs.get("dataSubmissionPolicyAcceptedVersion"),
              minimumPolicyVersion);

  // Increase the minimum policy version and check if we're notified.
  createPolicy(false, currentPolicyVersion, ++minimumPolicyVersion);
  do_check_false(policyPrefs.has("dataSubmissionPolicyAcceptedVersion"));
  yield triggerPolicyCheckAndEnsureNotified();

  // Test increasing the minimum version just on the current channel.
  createPolicy(true, currentPolicyVersion, minimumPolicyVersion);
  yield triggerPolicyCheckAndEnsureNotified(false);
  createPolicy(false, ++currentPolicyVersion, minimumPolicyVersion, minimumPolicyVersion + 1);
  yield triggerPolicyCheckAndEnsureNotified(true);
});
