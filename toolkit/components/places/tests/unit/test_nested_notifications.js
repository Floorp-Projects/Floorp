/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for nested notifications of Places events.
 * In this test, we check behavior of listeners of Places event upon firing nested
 * notification from inside of listener that received notifications.
 */
add_task(async function () {
  // We prepare 6 listeners for the test.
  // 1. Listener that added before root notification.
  const addRoot = new Observer();
  // 2. Listener that added before root notification
  //    but removed before first nest notification.
  const addRootRemoveFirst = new Observer();
  // 3. Listener that added before root notification
  //    but removed before second nest notification.
  const addRootRemoveSecond = new Observer();
  // 4. Listener that added before first nest notification.
  const addFirst = new Observer();
  // 5. Listener that added before first nest notification
  //    but removed before second nest notification.
  const addFirstRemoveSecond = new Observer();
  // 6. Listener that added before second nest notification.
  const addSecond = new Observer();

  // This is a listener listened the root notification
  // and do what we have to do for test in the first nest.
  const firstNestOperator = () => {
    info("Start to operate at first nest");

    // Remove itself to avoid listening more.
    removePlacesListener(firstNestOperator);

    info("Add/Remove test listeners at first nest");
    removePlacesListener(addRootRemoveFirst.handle);
    addPlacesListener(addFirst.handle);
    addPlacesListener(addFirstRemoveSecond.handle);

    // Add second nest operator.
    addPlacesListener(secondNestOperator);

    info("Send notification at first nest");
    notifyPlacesEvent("first");
  };

  // This is a listener listened the first nest notification
  // and do what we have to do for test in the second nest.
  const secondNestOperator = () => {
    info("Start to operate at second nest");

    // Remove itself to avoid listening more.
    removePlacesListener(secondNestOperator);

    info("Add/Remove test listeners at second nest");
    removePlacesListener(addRootRemoveSecond.handle);
    removePlacesListener(addFirstRemoveSecond.handle);
    addPlacesListener(addSecond.handle);

    info("Send notification at second nest");
    notifyPlacesEvent("second");
  };

  info("Add test listeners that handle notification sent at root");
  addPlacesListener(addRoot.handle);
  addPlacesListener(addRootRemoveFirst.handle);
  addPlacesListener(addRootRemoveSecond.handle);

  // Add first nest operator.
  addPlacesListener(firstNestOperator);

  info("Send notification at root");
  notifyPlacesEvent("root");

  info("Check whether or not test listeners could get expected notifications");
  assertNotifications(addRoot.notifications, [
    [{ guid: "root" }],
    [{ guid: "first" }],
    [{ guid: "second" }],
  ]);
  assertNotifications(addRootRemoveFirst.notifications, [[{ guid: "root" }]]);
  assertNotifications(addRootRemoveSecond.notifications, [
    [{ guid: "root" }],
    [{ guid: "first" }],
  ]);
  assertNotifications(addFirst.notifications, [
    [{ guid: "first" }],
    [{ guid: "second" }],
  ]);
  assertNotifications(addFirstRemoveSecond.notifications, [
    [{ guid: "first" }],
  ]);
  assertNotifications(addSecond.notifications, [[{ guid: "second" }]]);
});

function addPlacesListener(listener) {
  PlacesObservers.addListener(["bookmark-added"], listener);
}

function removePlacesListener(listener) {
  PlacesObservers.removeListener(["bookmark-added"], listener);
}

function notifyPlacesEvent(guid) {
  PlacesObservers.notifyListeners([
    new PlacesBookmarkAddition({
      dateAdded: 0,
      guid,
      id: -1,
      index: 0,
      isTagging: false,
      itemType: 1,
      parentGuid: "fake",
      parentId: -2,
      source: 0,
      title: guid,
      tags: "tags",
      url: `http://example.com/${guid}`,
      frecency: 0,
      hidden: false,
      visitCount: 0,
      lastVisitDate: 0,
      targetFolderGuid: null,
      targetFolderItemId: -1,
      targetFolderTitle: null,
    }),
  ]);
}

class Observer {
  constructor() {
    this.notifications = [];
    this.handle = this.handle.bind(this);
  }

  handle(events) {
    this.notifications.push(events);
  }
}

/**
 * Assert notifications the observer received.
 *
 * @param Array - notifications
 * @param Array - expectedNotifications
 */
function assertNotifications(notifications, expectedNotifications) {
  Assert.equal(
    notifications.length,
    expectedNotifications.length,
    "Number of notifications is correct"
  );

  for (let i = 0; i < notifications.length; i++) {
    info(`Check notifications[${i}]`);
    const placesEvents = notifications[i];
    const expectedPlacesEvents = expectedNotifications[i];
    assertPlacesEvents(placesEvents, expectedPlacesEvents);
  }
}

/**
 * Assert Places events.
 * This function checks given expected event field only.
 *
 * @param Array - events
 * @param Array - expectedEvents
 */
function assertPlacesEvents(events, expectedEvents) {
  Assert.equal(
    events.length,
    expectedEvents.length,
    "Number of Places events is correct"
  );

  for (let i = 0; i < events.length; i++) {
    info(`Check Places events[${i}]`);
    const event = events[i];
    const expectedEvent = expectedEvents[i];

    for (let field in expectedEvent) {
      Assert.equal(event[field], expectedEvent[field], `${field} is correct`);
    }
  }
}
