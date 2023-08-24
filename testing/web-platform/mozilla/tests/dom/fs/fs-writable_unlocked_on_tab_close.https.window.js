// META: title=Origin private file system used from multiple tabs
// META: script=support/testHelpers.js

promise_test(async t => {
  const bc = new BroadcastChannel("Coordinate writables");

  function firstReady(win) {
    return new Promise(resolve => {
      // Blur triggers after the unload event and after window.closed is set
      win.onblur = () => {
        // Closing the low-level file handle may get stuck, but the unlocking
        // request can only be sent to the parent after the handle is closed.
        // There is no guarantee when or if the closing will be complete.
        //
        // Therefore, the content process shutdown does not wait for the
        // completion but sets window.closed and calls the unload listeners
        // while actually still holding onto some resources.
        //
        // Since in this test we mainly want to ensure that a file
        // does not remain locked indefinitely, we wait for a reasonable amount
        // of time before creating a new writable, corresponding roughly to
        // a 500ms closing delay.
        const timeoutMs = 400;
        setTimeout(() => {
          resolve(win);
        }, timeoutMs);
      };
    });
  }

  const firstTabName = "support/fs-open_writable_then_close_tab.sub.html";
  const firstTab = await firstReady(window.open(firstTabName));
  assert_true(firstTab.closed, "Is the first tab already closed?");

  function secondReady(win) {
    return new Promise(resolve => {
      bc.onmessage = e => {
        if (e.data === "Second window ready!") {
          resolve(win);
        }
      };
    });
  }

  const secondTabName = "support/fs-open_writable_after_trigger.sub.html";
  const secondTab = await secondReady(window.open(secondTabName));

  let isDone = false;
  let childStatus = "Success";

  const secondSucceeded = new Promise(resolve => {
    bc.onmessage = e => {
      isDone = true;
      if (e.data !== "Success") {
        childStatus = e.data;
      }

      resolve();
    };
  });

  bc.postMessage("Create writable in the second window");

  await secondSucceeded;
  assert_true(isDone, "Did the second tab respond?");

  await fetch_tests_from_window(secondTab);

  assert_equals(childStatus, "Success");
}, `writable available for other tabs after one tab is closed`);
