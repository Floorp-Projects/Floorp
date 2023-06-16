/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function makeBuffer(length) {
  return new Array(length + 1).join("x");
}

/**
 * @resolves |true| if execution proceeded without warning,
 * |false| if there was a warning.
 */
function checkWarning(pref, buffer) {
  return new Promise(resolve => {
    let complete = false;
    let listener = {
      observe(event) {
        let message = event.message;
        if (
          !(
            message.startsWith("Warning: attempting to write") &&
            message.includes(pref)
          )
        ) {
          return;
        }
        if (complete) {
          return;
        }
        complete = true;
        info("Warning while setting " + pref);
        Services.console.unregisterListener(listener);
        resolve(true);
      },
    };
    do_timeout(1000, function () {
      if (complete) {
        return;
      }
      complete = true;
      info("No warning while setting " + pref);
      Services.console.unregisterListener(listener);
      resolve(false);
    });
    Services.console.registerListener(listener);
    Services.prefs.setCharPref(pref, buffer);
  });
}

add_task(async function () {
  // Simple change, shouldn't cause a warning
  info("Checking that a simple change doesn't cause a warning");
  let buf = makeBuffer(100);
  let warned = await checkWarning("string.accept", buf);
  Assert.ok(!warned);

  // Large change, should cause a warning
  info("Checking that a large change causes a warning");
  buf = makeBuffer(32 * 1024);
  warned = await checkWarning("string.warn", buf);
  Assert.ok(warned);
});
