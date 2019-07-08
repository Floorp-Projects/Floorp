/**
 * What this is aimed to test:
 *
 * Expiration relies on an interval, that is user-preffable setting
 * "places.history.expiration.interval_seconds".
 * On pref change it will stop current interval timer and fire a new one,
 * that will obey the new value.
 * If the pref is set to a number <= 0 we will use the default value.
 */

// Default timer value for expiration in seconds.  Must have same value as
// PREF_INTERVAL_SECONDS_NOTSET in nsPlacesExpiration.
const DEFAULT_TIMER_DELAY_SECONDS = 3 * 60;

// Sync this with the const value in the component.
const EXPIRE_AGGRESSIVITY_MULTIPLIER = 3;

var tests = [
  {
    desc: "Set interval to 1s.",
    interval: 1,
    expectedTimerDelay: 1 * EXPIRE_AGGRESSIVITY_MULTIPLIER,
  },

  {
    desc: "Set interval to a negative value.",
    interval: -1,
    expectedTimerDelay:
      DEFAULT_TIMER_DELAY_SECONDS * EXPIRE_AGGRESSIVITY_MULTIPLIER,
  },

  {
    desc: "Set interval to 0.",
    interval: 0,
    expectedTimerDelay:
      DEFAULT_TIMER_DELAY_SECONDS * EXPIRE_AGGRESSIVITY_MULTIPLIER,
  },

  {
    desc: "Set interval to a large value.",
    interval: 100,
    expectedTimerDelay: 100 * EXPIRE_AGGRESSIVITY_MULTIPLIER,
  },
];

add_task(async function test() {
  // The pref should not exist by default.
  Assert.throws(() => getInterval(), /NS_ERROR_UNEXPECTED/);

  // Force the component, so it will start observing preferences.
  force_expiration_start();

  for (let currentTest of tests) {
    currentTest = tests.shift();
    print(currentTest.desc);
    let promise = promiseTopicObserved("test-interval-changed");
    setInterval(currentTest.interval);
    let [, data] = await promise;
    Assert.equal(data, currentTest.expectedTimerDelay);
  }
  clearInterval();
});
