/*
 * Test suite for satchel notifications
 *
 * Tests notifications dispatched when modifying form history.
 *
 */

ChromeUtils.defineModuleGetter(
  this,
  "setTimeout",
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Preferences",
  "resource://gre/modules/Preferences.jsm"
);

const TestObserver = {
  observed: [],
  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),
  observe(subject, topic, data) {
    if (subject instanceof Ci.nsISupportsString) {
      subject = subject.toString();
    }
    this.observed.push({ subject, topic, data });
  },
  reset() {
    this.observed = [];
  },
};

const entry1 = ["entry1", "value1"];
const entry2 = ["entry2", "value2"];
const entry3 = ["entry3", "value3"];

add_setup(async () => {
  await promiseUpdateEntry("remove", null, null);
  const count = await promiseCountEntries(null, null);
  Assert.ok(!count, "Checking initial DB is empty");

  // Add the observer
  Services.obs.addObserver(TestObserver, "satchel-storage-changed");
});

add_task(async function addAndUpdateEntry() {
  // Add
  await promiseUpdateEntry("add", entry1[0], entry1[1]);
  Assert.equal(TestObserver.observed.length, 1);
  let { subject, data } = TestObserver.observed[0];
  Assert.equal(data, "formhistory-add");
  Assert.ok(isGUID.test(subject));

  let count = await promiseCountEntries(entry1[0], entry1[1]);
  Assert.equal(count, 1);

  // Update
  TestObserver.reset();

  await promiseUpdateEntry("update", entry1[0], entry1[1]);
  Assert.equal(TestObserver.observed.length, 1);
  ({ subject, data } = TestObserver.observed[0]);
  Assert.equal(data, "formhistory-update");
  Assert.ok(isGUID.test(subject));

  count = await promiseCountEntries(entry1[0], entry1[1]);
  Assert.equal(count, 1);

  // Clean-up
  await promiseUpdateEntry("remove", null, null);
});

add_task(async function removeEntry() {
  TestObserver.reset();
  await promiseUpdateEntry("add", entry1[0], entry1[1]);
  const guid = TestObserver.observed[0].subject;
  TestObserver.reset();

  await new Promise(res => {
    FormHistory.update(
      {
        op: "remove",
        fieldname: entry1[0],
        value: entry1[1],
        guid,
      },
      {
        handleError(error) {
          do_throw("Error occurred updating form history: " + error);
        },
        handleCompletion(reason) {
          if (!reason) {
            res();
          }
        },
      }
    );
  });
  Assert.equal(TestObserver.observed.length, 1);
  const { subject, data } = TestObserver.observed[0];
  Assert.equal(data, "formhistory-remove");
  Assert.ok(isGUID.test(subject));

  const count = await promiseCountEntries(entry1[0], entry1[1]);
  Assert.equal(count, 0, "doesn't exist after remove");
});

add_task(async function removeAllEntries() {
  await promiseAddEntry(entry1[0], entry1[1]);
  await promiseAddEntry(entry2[0], entry2[1]);
  await promiseAddEntry(entry3[0], entry3[1]);
  TestObserver.reset();

  await promiseUpdateEntry("remove", null, null);
  Assert.equal(TestObserver.observed.length, 3);
  for (const notification of TestObserver.observed) {
    const { subject, data } = notification;
    Assert.equal(data, "formhistory-remove");
    Assert.ok(isGUID.test(subject));
  }

  const count = await promiseCountEntries(null, null);
  Assert.equal(count, 0);
});

add_task(async function removeEntriesForName() {
  await promiseAddEntry(entry1[0], entry1[1]);
  await promiseAddEntry(entry2[0], entry2[1]);
  await promiseAddEntry(entry3[0], entry3[1]);
  TestObserver.reset();

  await promiseUpdateEntry("remove", entry2[0], null);
  Assert.equal(TestObserver.observed.length, 1);
  const { subject, data } = TestObserver.observed[0];
  Assert.equal(data, "formhistory-remove");
  Assert.ok(isGUID.test(subject));

  let count = await promiseCountEntries(entry2[0], entry2[1]);
  Assert.equal(count, 0);

  count = await promiseCountEntries(null, null);
  Assert.equal(count, 2, "the other entries are still there");

  // Clean-up
  await promiseUpdateEntry("remove", null, null);
});

add_task(async function removeEntriesByTimeframe() {
  let timerPrecision = Preferences.get("privacy.reduceTimerPrecision");
  Preferences.set("privacy.reduceTimerPrecision", false);

  registerCleanupFunction(function() {
    Preferences.set("privacy.reduceTimerPrecision", timerPrecision);
  });

  await promiseAddEntry(entry1[0], entry1[1]);
  await promiseAddEntry(entry2[0], entry2[1]);

  const cutoffDate = Date.now();
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(res => setTimeout(res, 10));

  await promiseAddEntry(entry3[0], entry3[1]);
  TestObserver.reset();

  await new Promise(res => {
    FormHistory.update(
      {
        op: "remove",
        firstUsedStart: 10,
        firstUsedEnd: cutoffDate * 1000,
      },
      {
        handleCompletion(reason) {
          if (!reason) {
            res();
          }
        },
        handleErrors(error) {
          do_throw("Error occurred updating form history: " + error);
        },
      }
    );
  });
  Assert.equal(TestObserver.observed.length, 2);
  for (const notification of TestObserver.observed) {
    const { subject, data } = notification;
    Assert.equal(data, "formhistory-remove");
    Assert.ok(isGUID.test(subject));
  }

  const count = await promiseCountEntries(null, null);
  Assert.equal(count, 1, "entry2 should still be there");

  // Clean-up
  await promiseUpdateEntry("remove", null, null);
});

add_task(async function teardown() {
  await promiseUpdateEntry("remove", null, null);
  Services.obs.removeObserver(TestObserver, "satchel-storage-changed");
});
