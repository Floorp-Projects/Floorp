/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that expiration runs on idle-daily.

add_task(async function test_expiration_on_idle_daily() {
  // Set interval to a large value so we don't expire on it.
  setInterval(3600); // 1h

  let expirationPromise = TestUtils.topicObserved(PlacesUtils.TOPIC_EXPIRATION_FINISHED);

  let expire = Cc["@mozilla.org/places/expiration;1"].
               getService(Ci.nsIObserver);
  expire.observe(null, "idle-daily", null);

  await expirationPromise;
});
