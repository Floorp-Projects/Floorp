// testPrefSticky.js defined this pref as a sticky_pref(). Once a sticky
// pref has been changed, it's written as a user_pref().
// So this test file reflects that scenario.
// Note the default in testPrefSticky.js is also false.
user_pref("testPref.sticky.bool", false);
