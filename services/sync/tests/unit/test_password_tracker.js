/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { PasswordEngine } = ChromeUtils.import(
  "resource://services-sync/engines/passwords.js"
);
const { Service } = ChromeUtils.import("resource://services-sync/service.js");

let engine;
let store;
let tracker;

add_task(async function setup() {
  await Service.engineManager.register(PasswordEngine);
  engine = Service.engineManager.get("passwords");
  store = engine._store;
  tracker = engine._tracker;

  // Don't do asynchronous writes.
  tracker.persistChangedIDs = false;
});

add_task(async function test_tracking() {
  let recordNum = 0;

  _("Verify we've got an empty tracker to work with.");
  let changes = await tracker.getChangedIDs();
  do_check_empty(changes);

  async function createPassword() {
    _("RECORD NUM: " + recordNum);
    let record = {
      id: "GUID" + recordNum,
      hostname: "http://foo.bar.com",
      formSubmitURL: "http://foo.bar.com",
      username: "john" + recordNum,
      password: "smith",
      usernameField: "username",
      passwordField: "password",
    };
    recordNum++;
    let login = store._nsLoginInfoFromRecord(record);
    Services.logins.addLogin(login);
    await tracker.asyncObserver.promiseObserversComplete();
  }

  try {
    _(
      "Create a password record. Won't show because we haven't started tracking yet"
    );
    await createPassword();
    changes = await tracker.getChangedIDs();
    do_check_empty(changes);
    Assert.equal(tracker.score, 0);

    _("Tell the tracker to start tracking changes.");
    tracker.start();
    await createPassword();
    changes = await tracker.getChangedIDs();
    do_check_attribute_count(changes, 1);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE);

    _("Starting twice won't do any harm.");
    tracker.start();
    await createPassword();
    changes = await tracker.getChangedIDs();
    do_check_attribute_count(changes, 2);
    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE * 2);

    _("Let's stop tracking again.");
    await tracker.clearChangedIDs();
    tracker.resetScore();
    await tracker.stop();
    await createPassword();
    changes = await tracker.getChangedIDs();
    do_check_empty(changes);
    Assert.equal(tracker.score, 0);

    _("Stopping twice won't do any harm.");
    await tracker.stop();
    await createPassword();
    changes = await tracker.getChangedIDs();
    do_check_empty(changes);
    Assert.equal(tracker.score, 0);
  } finally {
    _("Clean up.");
    await store.wipe();
    await tracker.clearChangedIDs();
    tracker.resetScore();
    await tracker.stop();
  }
});

add_task(async function test_onWipe() {
  _("Verify we've got an empty tracker to work with.");
  const changes = await tracker.getChangedIDs();
  do_check_empty(changes);
  Assert.equal(tracker.score, 0);

  try {
    _("A store wipe should increment the score");
    tracker.start();
    await store.wipe();
    await tracker.asyncObserver.promiseObserversComplete();

    Assert.equal(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    tracker.resetScore();
    await tracker.stop();
  }
});
