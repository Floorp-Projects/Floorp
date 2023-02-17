/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* Unit tests for the nsIUrlClassifierExceptionListService implementation. */

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);

const COLLECTION_NAME = "url-classifier-skip-urls";
const FEATURE_TRACKING_NAME = "tracking-annotation-test";
const FEATURE_TRACKING_PREF_NAME = "urlclassifier.tracking-annotation-test";
const FEATURE_SOCIAL_NAME = "socialtracking-annotation-test";
const FEATURE_SOCIAL_PREF_NAME = "urlclassifier.socialtracking-annotation-test";
const FEATURE_FINGERPRINTING_NAME = "fingerprinting-annotation-test";
const FEATURE_FINGERPRINTING_PREF_NAME =
  "urlclassifier.fingerprinting-annotation-test";

do_get_profile();

class UpdateEvent extends EventTarget {}
function waitForEvent(element, eventName) {
  return new Promise(function(resolve) {
    element.addEventListener(eventName, e => resolve(e.detail), { once: true });
  });
}

add_task(async function test_list_changes() {
  let exceptionListService = Cc[
    "@mozilla.org/url-classifier/exception-list-service;1"
  ].getService(Ci.nsIUrlClassifierExceptionListService);

  // Make sure we have a pref initially, since the exception list service
  // requires it.
  Services.prefs.setStringPref(FEATURE_TRACKING_PREF_NAME, "");

  let updateEvent = new UpdateEvent();
  let obs = data => {
    let event = new CustomEvent("update", { detail: data });
    updateEvent.dispatchEvent(event);
  };

  let records = [
    {
      id: "1",
      last_modified: 1000000000000001,
      feature: FEATURE_TRACKING_NAME,
      pattern: "example.com",
    },
  ];

  // Add some initial data.
  let db = RemoteSettings(COLLECTION_NAME).db;
  await db.importChanges({}, Date.now(), records);
  let promise = waitForEvent(updateEvent, "update");

  exceptionListService.registerAndRunExceptionListObserver(
    FEATURE_TRACKING_NAME,
    FEATURE_TRACKING_PREF_NAME,
    obs
  );

  Assert.equal(await promise, "", "No items in the list");

  // Second event is from the RemoteSettings record.
  let list = await waitForEvent(updateEvent, "update");
  Assert.equal(list, "example.com", "Has one item in the list");

  records.push(
    {
      id: "2",
      last_modified: 1000000000000002,
      feature: FEATURE_TRACKING_NAME,
      pattern: "MOZILLA.ORG",
    },
    {
      id: "3",
      last_modified: 1000000000000003,
      feature: "some-other-feature",
      pattern: "noinclude.com",
    },
    {
      last_modified: 1000000000000004,
      feature: FEATURE_TRACKING_NAME,
      pattern: "*.example.org",
    }
  );

  promise = waitForEvent(updateEvent, "update");

  await RemoteSettings(COLLECTION_NAME).emit("sync", {
    data: { current: records },
  });

  list = await promise;

  Assert.equal(
    list,
    "example.com,mozilla.org,*.example.org",
    "Has several items in the list"
  );

  promise = waitForEvent(updateEvent, "update");

  Services.prefs.setStringPref(FEATURE_TRACKING_PREF_NAME, "test.com");

  list = await promise;

  Assert.equal(
    list,
    "test.com,example.com,mozilla.org,*.example.org",
    "Has several items in the list"
  );

  promise = waitForEvent(updateEvent, "update");

  Services.prefs.setStringPref(
    FEATURE_TRACKING_PREF_NAME,
    "test.com,whatever.com,*.abc.com"
  );

  list = await promise;

  Assert.equal(
    list,
    "test.com,whatever.com,*.abc.com,example.com,mozilla.org,*.example.org",
    "Has several items in the list"
  );

  exceptionListService.unregisterExceptionListObserver(
    FEATURE_TRACKING_NAME,
    obs
  );
  exceptionListService.clear();

  await db.clear();
});

/**
 * This test make sure when a feature registers itself to exceptionlist service,
 * it can get the correct initial data.
 */
add_task(async function test_list_init_data() {
  let exceptionListService = Cc[
    "@mozilla.org/url-classifier/exception-list-service;1"
  ].getService(Ci.nsIUrlClassifierExceptionListService);

  // Make sure we have a pref initially, since the exception list service
  // requires it.
  Services.prefs.setStringPref(FEATURE_TRACKING_PREF_NAME, "");

  let updateEvent = new UpdateEvent();

  let records = [
    {
      id: "1",
      last_modified: 1000000000000001,
      feature: FEATURE_TRACKING_NAME,
      pattern: "tracking.example.com",
    },
    {
      id: "2",
      last_modified: 1000000000000002,
      feature: FEATURE_SOCIAL_NAME,
      pattern: "social.example.com",
    },
    {
      id: "3",
      last_modified: 1000000000000003,
      feature: FEATURE_TRACKING_NAME,
      pattern: "*.tracking.org",
    },
    {
      id: "4",
      last_modified: 1000000000000004,
      feature: FEATURE_SOCIAL_NAME,
      pattern: "MOZILLA.ORG",
    },
  ];

  // Add some initial data.
  let db = RemoteSettings(COLLECTION_NAME).db;
  await db.importChanges({}, Date.now(), records);

  // The first registered feature make ExceptionListService get the initial data
  // from remote setting.
  let promise = waitForEvent(updateEvent, "update");

  let obs = data => {
    let event = new CustomEvent("update", { detail: data });
    updateEvent.dispatchEvent(event);
  };
  exceptionListService.registerAndRunExceptionListObserver(
    FEATURE_TRACKING_NAME,
    FEATURE_TRACKING_PREF_NAME,
    obs
  );

  let list = await promise;
  Assert.equal(list, "", "Empty list initially");

  Assert.equal(
    await waitForEvent(updateEvent, "update"),
    "tracking.example.com,*.tracking.org",
    "Has several items in the list"
  );

  // Register another feature after ExceptionListService got the initial data.
  promise = waitForEvent(updateEvent, "update");

  exceptionListService.registerAndRunExceptionListObserver(
    FEATURE_SOCIAL_NAME,
    FEATURE_SOCIAL_PREF_NAME,
    obs
  );

  list = await promise;

  Assert.equal(
    list,
    "social.example.com,mozilla.org",
    "Has several items in the list"
  );

  // Test registering a feature after ExceptionListService recieved the synced data.
  records.push(
    {
      id: "5",
      last_modified: 1000000000000002,
      feature: FEATURE_FINGERPRINTING_NAME,
      pattern: "fingerprinting.example.com",
    },
    {
      id: "6",
      last_modified: 1000000000000002,
      feature: "other-fature",
      pattern: "not-a-fingerprinting.example.com",
    },
    {
      id: "7",
      last_modified: 1000000000000002,
      feature: FEATURE_FINGERPRINTING_NAME,
      pattern: "*.fingerprinting.org",
    }
  );

  await RemoteSettings(COLLECTION_NAME).emit("sync", {
    data: { current: records },
  });

  promise = waitForEvent(updateEvent, "update");

  exceptionListService.registerAndRunExceptionListObserver(
    FEATURE_FINGERPRINTING_NAME,
    FEATURE_FINGERPRINTING_PREF_NAME,
    obs
  );

  list = await promise;

  Assert.equal(
    list,
    "fingerprinting.example.com,*.fingerprinting.org",
    "Has several items in the list"
  );

  exceptionListService.unregisterExceptionListObserver(
    FEATURE_TRACKING_NAME,
    obs
  );
  exceptionListService.unregisterExceptionListObserver(
    FEATURE_SOCIAL_NAME,
    obs
  );
  exceptionListService.unregisterExceptionListObserver(
    FEATURE_FINGERPRINTING_NAME,
    obs
  );
  exceptionListService.clear();

  await db.clear();
});
