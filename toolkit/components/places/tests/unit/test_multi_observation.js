/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test whether registered listener capture proper events.
// We test the following combinations.
// * Listener: listen to single/multi event(s)
// * Event: fire single/multi type of event(s)
// * Timing: fire event(s) at same time/separately
// And also test notifying empty events.

add_task(async () => {
  info("Test for listening to single event and firing single event");

  const observer = startObservation(["page-visited"]);

  await PlacesUtils.history.insertMany([
    {
      title: "test",
      url: "http://example.com/test",
      visits: [{ transition: TRANSITION_LINK }],
    },
  ]);

  const expectedFiredEvents = [
    [
      {
        type: "page-visited",
        url: "http://example.com/test",
      },
    ],
  ];
  assertFiredEvents(observer.firedEvents, expectedFiredEvents);

  await PlacesUtils.history.clear();
});

add_task(async () => {
  info("Test for listening to multi events with firing single event");

  const observer = startObservation(["page-visited", "page-title-changed"]);

  await PlacesUtils.history.insertMany([
    {
      title: "test",
      url: "http://example.com/test",
      visits: [{ transition: TRANSITION_LINK }],
    },
  ]);

  const expectedFiredEvents = [
    [
      {
        type: "page-visited",
        url: "http://example.com/test",
      },
    ],
  ];
  assertFiredEvents(observer.firedEvents, expectedFiredEvents);

  await PlacesUtils.history.clear();
});

add_task(async () => {
  info(
    "Test for listening to single event with firing multi events at same time"
  );

  const vistedObserver = startObservation(["page-visited"]);
  const titleChangedObserver = startObservation(["page-title-changed"]);

  await PlacesUtils.history.insertMany([
    {
      title: "will change",
      url: "http://example.com/title",
      visits: [{ transition: TRANSITION_LINK }],
    },
    {
      title: "changed",
      url: "http://example.com/title",
      referrer: "http://example.com/title",
      visits: [{ transition: TRANSITION_LINK }],
    },
    {
      title: "another",
      url: "http://example.com/another",
      visits: [{ transition: TRANSITION_LINK }],
    },
  ]);

  const expectedVisitedFiredEvents = [
    [
      {
        type: "page-visited",
        url: "http://example.com/title",
      },
      {
        type: "page-visited",
        url: "http://example.com/title",
      },
      {
        type: "page-visited",
        url: "http://example.com/another",
      },
    ],
  ];
  assertFiredEvents(vistedObserver.firedEvents, expectedVisitedFiredEvents);

  const expectedTitleChangedFiredEvents = [
    [
      {
        type: "page-title-changed",
        url: "http://example.com/title",
        title: "changed",
      },
    ],
  ];
  assertFiredEvents(
    titleChangedObserver.firedEvents,
    expectedTitleChangedFiredEvents
  );

  await PlacesUtils.history.clear();
});

add_task(async () => {
  info(
    "Test for listening to single event with firing multi events separately"
  );

  const observer = startObservation(["page-visited"]);

  await PlacesUtils.history.insertMany([
    {
      title: "test",
      url: "http://example.com/test",
      visits: [{ transition: TRANSITION_LINK }],
    },
  ]);

  const bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "a folder",
  });

  await PlacesUtils.history.insertMany([
    {
      title: "another",
      url: "http://example.com/another",
      visits: [{ transition: TRANSITION_LINK }],
    },
  ]);

  const expectedFiredEvents = [
    [
      {
        type: "page-visited",
        url: "http://example.com/test",
      },
    ],
    [
      {
        type: "page-visited",
        url: "http://example.com/another",
      },
    ],
  ];
  assertFiredEvents(observer.firedEvents, expectedFiredEvents);

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.remove(bookmark.guid);
});

add_task(async () => {
  info("Test for listening to multi events with firing single event");

  const observer = startObservation([
    "page-visited",
    "page-title-changed",
    "bookmark-added",
  ]);

  await PlacesUtils.history.insertMany([
    {
      title: "test",
      url: "http://example.com/test",
      visits: [{ transition: TRANSITION_LINK }],
    },
  ]);

  const expectedFiredEvents = [
    [
      {
        type: "page-visited",
        url: "http://example.com/test",
      },
    ],
  ];
  assertFiredEvents(observer.firedEvents, expectedFiredEvents);

  await PlacesUtils.history.clear();
});

add_task(async () => {
  info(
    "Test for listening to multi events with firing multi events at same time"
  );

  const observer = startObservation([
    "page-visited",
    "page-title-changed",
    "bookmark-added",
  ]);

  await PlacesUtils.history.insertMany([
    {
      title: "will change",
      url: "http://example.com/title",
      visits: [{ transition: TRANSITION_LINK }],
    },
    {
      title: "changed",
      url: "http://example.com/title",
      referrer: "http://example.com/title",
      visits: [{ transition: TRANSITION_LINK }],
    },
    {
      title: "another",
      url: "http://example.com/another",
      visits: [{ transition: TRANSITION_LINK }],
    },
  ]);

  const expectedFiredEvents = [
    [
      {
        type: "page-visited",
        url: "http://example.com/title",
      },
      {
        type: "page-visited",
        url: "http://example.com/title",
      },
      {
        type: "page-title-changed",
        url: "http://example.com/title",
        title: "changed",
      },
      {
        type: "page-visited",
        url: "http://example.com/another",
      },
    ],
  ];
  assertFiredEvents(observer.firedEvents, expectedFiredEvents);

  await PlacesUtils.history.clear();
});

add_task(async () => {
  info(
    "Test for listening to multi events with firing multi events separately"
  );

  const observer = startObservation([
    "page-visited",
    "page-title-changed",
    "bookmark-added",
  ]);

  await PlacesUtils.history.insertMany([
    {
      title: "will change",
      url: "http://example.com/title",
      visits: [{ transition: TRANSITION_LINK }],
    },
  ]);

  const bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "a folder",
  });

  await PlacesUtils.history.insertMany([
    {
      title: "changed",
      url: "http://example.com/title",
      referrer: "http://example.com/title",
      visits: [{ transition: TRANSITION_LINK }],
    },
  ]);

  const expectedFiredEvents = [
    [
      {
        type: "page-visited",
        url: "http://example.com/title",
      },
    ],
    [
      {
        type: "bookmark-added",
        title: "a folder",
      },
    ],
    [
      {
        type: "page-visited",
        url: "http://example.com/title",
      },
      {
        type: "page-title-changed",
        url: "http://example.com/title",
        title: "changed",
      },
    ],
  ];
  assertFiredEvents(observer.firedEvents, expectedFiredEvents);

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.remove(bookmark.guid);
});

add_task(async function test_empty_notifications_array() {
  info("Test whether listener does not receive empty events");

  if (AppConstants.DEBUG) {
    info(
      "Ignore this test since we added a MOZ_ASSERT for empty events in debug build"
    );
    return;
  }

  const observer = startObservation(["page-visited"]);
  PlacesObservers.notifyListeners([]);
  Assert.equal(observer.firedEvents.length, 0, "Listener does not receive any");
});

function startObservation(targets) {
  const observer = {
    firedEvents: [],
    handle(events) {
      this.firedEvents.push(events);
    },
  };

  PlacesObservers.addListener(targets, observer.handle.bind(observer));

  return observer;
}

function assertFiredEvents(firedEvents, expectedFiredEvents) {
  Assert.equal(
    firedEvents.length,
    expectedFiredEvents.length,
    "Number events fired is correct"
  );

  for (let i = 0; i < firedEvents.length; i++) {
    info(`Check firedEvents[${i}]`);
    const events = firedEvents[i];
    const expectedEvents = expectedFiredEvents[i];
    assertEvents(events, expectedEvents);
  }
}

function assertEvents(events, expectedEvents) {
  Assert.equal(
    events.length,
    expectedEvents.length,
    "Number events is correct"
  );

  for (let i = 0; i < events.length; i++) {
    info(`Check events[${i}]`);
    const event = events[i];
    const expectedEvent = expectedEvents[i];

    for (let field in expectedEvent) {
      Assert.equal(event[field], expectedEvent[field], `${field} is correct`);
    }
  }
}
