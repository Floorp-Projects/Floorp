// This test ensures that the URL decoration annotations service works as
// expected, and also we successfully downgrade document.referrer to the
// eTLD+1 URL when tracking identifiers controlled by this service are
// present in the referrer URI.

"use strict";

/* Unit tests for the nsIPartitioningExceptionListService implementation. */

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);

const COLLECTION_NAME = "partitioning-exempt-urls";
const PREF_NAME = "privacy.restrict3rdpartystorage.skip_list";

XPCOMUtils.defineLazyGlobalGetters(this, ["EventTarget"]);

do_get_profile();

class UpdateEvent extends EventTarget {}
function waitForEvent(element, eventName) {
  return new Promise(function(resolve) {
    element.addEventListener(eventName, e => resolve(e.detail), { once: true });
  });
}

add_task(async _ => {
  let peuService = Cc[
    "@mozilla.org/partitioning/exception-list-service;1"
  ].getService(Ci.nsIPartitioningExceptionListService);

  // Make sure we have a pref initially, since the exception list service
  // requires it.
  Services.prefs.setStringPref(PREF_NAME, "");

  let updateEvent = new UpdateEvent();
  let records = [
    {
      id: "1",
      last_modified: 100000000000000000001,
      firstPartyOrigin: "https://example.org",
      thirdPartyOrigin: "https://tracking.example.com",
    },
  ];

  // Add some initial data
  let db = await RemoteSettings(COLLECTION_NAME).db;
  await db.importChanges({}, 42, records);

  let promise = waitForEvent(updateEvent, "update");
  let obs = data => {
    let event = new CustomEvent("update", { detail: data });
    updateEvent.dispatchEvent(event);
  };
  peuService.registerAndRunExceptionListObserver(obs);
  let list = await promise;
  Assert.equal(list, "", "No items in the list");

  // Second event is from the RemoteSettings record.
  list = await waitForEvent(updateEvent, "update");
  Assert.equal(
    list,
    "https://example.org,https://tracking.example.com",
    "Has one item in the list"
  );

  records.push({
    id: "2",
    last_modified: 100000000000000000002,
    firstPartyOrigin: "https://foo.org",
    thirdPartyOrigin: "https://bar.com",
  });

  promise = waitForEvent(updateEvent, "update");
  await RemoteSettings(COLLECTION_NAME).emit("sync", {
    data: { current: records },
  });
  list = await promise;
  Assert.equal(
    list,
    "https://example.org,https://tracking.example.com;https://foo.org,https://bar.com",
    "Has several items in the list"
  );

  promise = waitForEvent(updateEvent, "update");
  Services.prefs.setStringPref(PREF_NAME, "https://test.com,https://test3.com");
  list = await promise;
  Assert.equal(
    list,
    "https://test.com,https://test3.com;https://example.org,https://tracking.example.com;https://foo.org,https://bar.com",
    "Has several items in the list"
  );

  promise = waitForEvent(updateEvent, "update");
  Services.prefs.setStringPref(
    PREF_NAME,
    "https://test.com,https://test3.com;https://abc.com,https://def.com"
  );
  list = await promise;
  Assert.equal(
    list,
    "https://test.com,https://test3.com;https://abc.com,https://def.com;https://example.org,https://tracking.example.com;https://foo.org,https://bar.com",
    "Has several items in the list"
  );

  peuService.unregisterExceptionListObserver(obs);
  await db.clear();
});
