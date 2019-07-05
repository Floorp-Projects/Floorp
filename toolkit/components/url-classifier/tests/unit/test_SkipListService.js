/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* Unit tests for the nsIUrlClassifierSkipListService implementation. */

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const COLLECTION_NAME = "url-classifier-skip-urls";
const FEATURE_NAME = "tracking-annotation-test";
const FEATURE_PREF_NAME = "urlclassifier.tracking-annotation-test";

XPCOMUtils.defineLazyGlobalGetters(this, ["EventTarget"]);

do_get_profile();

class UpdateEvent extends EventTarget {}
function waitForEvent(element, eventName) {
  return new Promise(function(resolve) {
    element.addEventListener(eventName, e => resolve(e.detail), { once: true });
  });
}

add_task(async function test_list_changes() {
  let skipListService = Cc[
    "@mozilla.org/url-classifier/skip-list-service;1"
  ].getService(Ci.nsIUrlClassifierSkipListService);

  // Make sure we have a pref initially, since the skip list service requires it.
  Services.prefs.setStringPref(FEATURE_PREF_NAME, "");

  let updateEvent = new UpdateEvent();
  let obs = data => {
    let event = new CustomEvent("update", { detail: data });
    updateEvent.dispatchEvent(event);
  };

  let records = [
    {
      id: "1",
      last_modified: 100000000000000000001,
      feature: FEATURE_NAME,
      pattern: "example.com",
    },
  ];

  // Add some initial data.
  let collection = await RemoteSettings(COLLECTION_NAME).openCollection();
  await collection.create(records[0], { synced: true });
  await collection.db.saveLastModified(42);

  let promise = waitForEvent(updateEvent, "update");

  skipListService.registerAndRunSkipListObserver(
    FEATURE_NAME,
    FEATURE_PREF_NAME,
    obs
  );

  let list = await promise;

  Assert.equal(list, "example.com", "Has one item in the list");

  records.push(
    {
      id: "2",
      last_modified: 100000000000000000002,
      feature: FEATURE_NAME,
      pattern: "MOZILLA.ORG",
    },
    {
      id: "3",
      last_modified: 100000000000000000003,
      feature: "some-other-feature",
      pattern: "noinclude.com",
    },
    {
      last_modified: 100000000000000000004,
      feature: FEATURE_NAME,
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

  Services.prefs.setStringPref(FEATURE_PREF_NAME, "test.com");

  list = await promise;

  Assert.equal(
    list,
    "test.com,example.com,mozilla.org,*.example.org",
    "Has several items in the list"
  );

  promise = waitForEvent(updateEvent, "update");

  Services.prefs.setStringPref(
    FEATURE_PREF_NAME,
    "test.com,whatever.com,*.abc.com"
  );

  list = await promise;

  Assert.equal(
    list,
    "test.com,whatever.com,*.abc.com,example.com,mozilla.org,*.example.org",
    "Has several items in the list"
  );

  skipListService.unregisterSkipListObserver(FEATURE_NAME, obs);

  await collection.clear();
});
