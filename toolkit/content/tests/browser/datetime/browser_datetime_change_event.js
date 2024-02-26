/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function open_change_and_expect_one_change_event(page) {
  await helper.openPicker(page);

  let changeEventPromise = helper.promiseChange();

  // Click the first item (top-left corner) of the calendar
  helper.click(helper.getElement(DAYS_VIEW).children[0]);
  await changeEventPromise;

  await helper.closePicker();

  let changeEvents = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function () {
      return content.wrappedJSObject.changeEventCount;
    }
  );
  is(changeEvents, 1, "Should've got one change event");
  await helper.tearDown();
}

add_task(async function test_change_event_simple() {
  await open_change_and_expect_one_change_event(`data:text/html,
    <!doctype html>
    <script>
      var changeEventCount = 0;
    </script>
    <input type="date" id="date" onchange="changeEventCount++">
  `);
});

add_task(async function test_change_event_with_mutation() {
  await open_change_and_expect_one_change_event(`data:text/html,
    <!doctype html>
    <script>
      var changeEventCount = 0;
    </script>
    <input type="date" id="date" onchange="this.value = ''; changeEventCount++">
  `);
});
