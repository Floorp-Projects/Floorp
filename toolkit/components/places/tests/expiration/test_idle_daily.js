/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that expiration runs on idle-daily.

function run_test() {
  do_test_pending();

  // Set interval to a large value so we don't expire on it.
  setInterval(3600); // 1h

  Services.obs.addObserver(function observeExpiration(aSubject, aTopic, aData) {
    Services.obs.removeObserver(observeExpiration,
                                PlacesUtils.TOPIC_EXPIRATION_FINISHED);
    do_test_finished();
  }, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);

  let expire = Cc["@mozilla.org/places/expiration;1"].
               getService(Ci.nsIObserver);
  expire.observe(null, "idle-daily", null);
}
