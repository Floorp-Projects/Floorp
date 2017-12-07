/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that passing a null title to history insert or update doesn't overwrite
// an existing title, while an empty string does.

"use strict";

async function fetchTitle(url) {
  let entry;
  await TestUtils.waitForCondition(
    async () => {
      entry = await PlacesUtils.history.fetch(url);
      return !!entry;
    },
    "fetch title for entry");
  return entry.title;
}

add_task(async function() {
  const url = "http://mozilla.com";
  let title = "Mozilla";

  do_print("Insert a visit with a title");
  let result = await PlacesUtils.history.insert({
    url,
    title,
    visits: [
      { transition: PlacesUtils.history.TRANSITIONS.LINK }
    ]
  });
  Assert.equal(title, result.title, "title should be stored");
  Assert.equal(title, await fetchTitle(url), "title should be stored");

  // This is shared by the next tests.
  let promiseTitleChange = PlacesTestUtils.waitForNotification("onTitleChanged",
  () => notified = true, "history");

  do_print("Insert a visit with a null title, should not clear the previous title");
  let notified = false;
  result = await PlacesUtils.history.insert({
    url,
    title: null,
    visits: [
      { transition: PlacesUtils.history.TRANSITIONS.LINK }
    ]
  });
  Assert.equal(title, result.title, "title should be unchanged");
  Assert.equal(title, await fetchTitle(url), "title should be unchanged");
  await Promise.race([promiseTitleChange, new Promise(r => do_timeout(1000, r))]);
  Assert.ok(!notified, "A title change should not be notified");

  do_print("Insert a visit without specifying a title, should not clear the previous title");
  notified = false;
  result = await PlacesUtils.history.insert({
    url,
    visits: [
      { transition: PlacesUtils.history.TRANSITIONS.LINK }
    ]
  });
  Assert.equal(title, result.title, "title should be unchanged");
  Assert.equal(title, await fetchTitle(url), "title should be unchanged");
  await Promise.race([promiseTitleChange, new Promise(r => do_timeout(1000, r))]);
  Assert.ok(!notified, "A title change should not be notified");

  do_print("Insert a visit with an empty title, should clear the previous title");
  result = await PlacesUtils.history.insert({
    url,
    title: "",
    visits: [
      { transition: PlacesUtils.history.TRANSITIONS.LINK }
    ]
  });
  do_print("Waiting for the title change notification");
  await promiseTitleChange;
  Assert.equal("", result.title, "title should be empty");
  Assert.equal("", await fetchTitle(url), "title should be empty");
});
