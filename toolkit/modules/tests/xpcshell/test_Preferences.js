/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var {classes: Cc, interfaces: Ci, results: Cr, utils: Cu, manager: Cm} = Components;

Cu.import("resource://gre/modules/Preferences.jsm");

function run_test() {
  run_next_test();
}

add_test(function test_set_get_pref() {
  Preferences.set("test_set_get_pref.integer", 1);
  do_check_eq(Preferences.get("test_set_get_pref.integer"), 1);

  Preferences.set("test_set_get_pref.string", "foo");
  do_check_eq(Preferences.get("test_set_get_pref.string"), "foo");

  Preferences.set("test_set_get_pref.boolean", true);
  do_check_eq(Preferences.get("test_set_get_pref.boolean"), true);

  // Clean up.
  Preferences.resetBranch("test_set_get_pref.");

  run_next_test();
});

add_test(function test_set_get_branch_pref() {
  let prefs = new Preferences("test_set_get_branch_pref.");

  prefs.set("something", 1);
  do_check_eq(prefs.get("something"), 1);
  do_check_false(Preferences.has("something"));

  // Clean up.
  prefs.reset("something");

  run_next_test();
});

add_test(function test_set_get_multiple_prefs() {
  Preferences.set({ "test_set_get_multiple_prefs.integer":  1,
                    "test_set_get_multiple_prefs.string":   "foo",
                    "test_set_get_multiple_prefs.boolean":  true });

  let [i, s, b] = Preferences.get(["test_set_get_multiple_prefs.integer",
                                   "test_set_get_multiple_prefs.string",
                                   "test_set_get_multiple_prefs.boolean"]);

  do_check_eq(i, 1);
  do_check_eq(s, "foo");
  do_check_eq(b, true);

  // Clean up.
  Preferences.resetBranch("test_set_get_multiple_prefs.");

  run_next_test();
});

add_test(function test_get_multiple_prefs_with_default_value() {
  Preferences.set({ "test_get_multiple_prefs_with_default_value.a":  1,
                    "test_get_multiple_prefs_with_default_value.b":  2 });

  let [a, b, c] = Preferences.get(["test_get_multiple_prefs_with_default_value.a",
                                   "test_get_multiple_prefs_with_default_value.b",
                                   "test_get_multiple_prefs_with_default_value.c"],
                                  0);

  do_check_eq(a, 1);
  do_check_eq(b, 2);
  do_check_eq(c, 0);

  // Clean up.
  Preferences.resetBranch("test_get_multiple_prefs_with_default_value.");

  run_next_test();
});

add_test(function test_set_get_unicode_pref() {
  Preferences.set("test_set_get_unicode_pref", String.fromCharCode(960));
  do_check_eq(Preferences.get("test_set_get_unicode_pref"), String.fromCharCode(960));

  // Clean up.
  Preferences.reset("test_set_get_unicode_pref");

  run_next_test();
});

add_test(function test_set_null_pref() {
  try {
    Preferences.set("test_set_null_pref", null);
    // We expect this to throw, so the test is designed to fail if it doesn't.
    do_check_true(false);
  } catch (ex) {}

  run_next_test();
});

add_test(function test_set_undefined_pref() {
  try {
    Preferences.set("test_set_undefined_pref");
    // We expect this to throw, so the test is designed to fail if it doesn't.
    do_check_true(false);
  } catch (ex) {}

  run_next_test();
});

add_test(function test_set_unsupported_pref() {
  try {
    Preferences.set("test_set_unsupported_pref", new Array());
    // We expect this to throw, so the test is designed to fail if it doesn't.
    do_check_true(false);
  } catch (ex) {}

  run_next_test();
});

// Make sure that we can get a string pref that we didn't set ourselves
// (i.e. that the way we get a string pref using getComplexValue doesn't
// hork us getting a string pref that wasn't set using setComplexValue).
add_test(function test_get_string_pref() {
  let svc = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefService).
            getBranch("");
  svc.setCharPref("test_get_string_pref", "a normal string");
  do_check_eq(Preferences.get("test_get_string_pref"), "a normal string");

  // Clean up.
  Preferences.reset("test_get_string_pref");

  run_next_test();
});

add_test(function test_get_localized_string_pref() {
  let svc = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefService).
            getBranch("");
  let prefName = "test_get_localized_string_pref";
  let localizedString = Cc["@mozilla.org/pref-localizedstring;1"]
    .createInstance(Ci.nsIPrefLocalizedString);
  localizedString.data = "a localized string";
  svc.setComplexValue(prefName, Ci.nsIPrefLocalizedString, localizedString);
  do_check_eq(Preferences.get(prefName, null, Ci.nsIPrefLocalizedString),
    "a localized string");

  // Clean up.
  Preferences.reset(prefName);

  run_next_test();
});

add_test(function test_set_get_number_pref() {
  Preferences.set("test_set_get_number_pref", 5);
  do_check_eq(Preferences.get("test_set_get_number_pref"), 5);

  // Non-integer values get converted to integers.
  Preferences.set("test_set_get_number_pref", 3.14159);
  do_check_eq(Preferences.get("test_set_get_number_pref"), 3);

  // Values outside the range -(2^31-1) to 2^31-1 overflow.
  try {
    Preferences.set("test_set_get_number_pref", Math.pow(2, 31));
    // We expect this to throw, so the test is designed to fail if it doesn't.
    do_check_true(false);
  } catch (ex) {}

  // Clean up.
  Preferences.reset("test_set_get_number_pref");

  run_next_test();
});

add_test(function test_reset_pref() {
  Preferences.set("test_reset_pref", 1);
  Preferences.reset("test_reset_pref");
  do_check_eq(Preferences.get("test_reset_pref"), undefined);

  run_next_test();
});

add_test(function test_reset_pref_branch() {
  Preferences.set("test_reset_pref_branch.foo", 1);
  Preferences.set("test_reset_pref_branch.bar", 2);
  Preferences.resetBranch("test_reset_pref_branch.");
  do_check_eq(Preferences.get("test_reset_pref_branch.foo"), undefined);
  do_check_eq(Preferences.get("test_reset_pref_branch.bar"), undefined);

  run_next_test();
});

// Make sure the module doesn't throw an exception when asked to reset
// a nonexistent pref.
add_test(function test_reset_nonexistent_pref() {
  Preferences.reset("test_reset_nonexistent_pref");

  run_next_test();
});

// Make sure the module doesn't throw an exception when asked to reset
// a nonexistent pref branch.
add_test(function test_reset_nonexistent_pref_branch() {
  Preferences.resetBranch("test_reset_nonexistent_pref_branch.");

  run_next_test();
});

add_test(function test_observe_prefs_function() {
  let observed = false;
  let observer = function() { observed = !observed };

  Preferences.observe("test_observe_prefs_function", observer);
  Preferences.set("test_observe_prefs_function.subpref", "something");
  do_check_false(observed);
  Preferences.set("test_observe_prefs_function", "something");
  do_check_true(observed);

  Preferences.ignore("test_observe_prefs_function", observer);
  Preferences.set("test_observe_prefs_function", "something else");
  do_check_true(observed);

  // Clean up.
  Preferences.reset("test_observe_prefs_function");
  Preferences.reset("test_observe_prefs_function.subpref");

  run_next_test();
});

add_test(function test_observe_prefs_object() {
  let observer = {
    observed: false,
    observe() {
      this.observed = !this.observed;
    }
  };

  Preferences.observe("test_observe_prefs_object", observer.observe, observer);
  Preferences.set("test_observe_prefs_object.subpref", "something");
  do_check_false(observer.observed);
  Preferences.set("test_observe_prefs_object", "something");
  do_check_true(observer.observed);

  Preferences.ignore("test_observe_prefs_object", observer.observe, observer);
  Preferences.set("test_observe_prefs_object", "something else");
  do_check_true(observer.observed);

  // Clean up.
  Preferences.reset("test_observe_prefs_object");
  Preferences.reset("test_observe_prefs_object.subpref");

  run_next_test();
});

add_test(function test_observe_prefs_nsIObserver() {
  let observer = {
    observed: false,
    observe(subject, topic, data) {
      this.observed = !this.observed;
      do_check_true(subject instanceof Ci.nsIPrefBranch);
      do_check_eq(topic, "nsPref:changed");
      do_check_eq(data, "test_observe_prefs_nsIObserver");
    }
  };

  Preferences.observe("test_observe_prefs_nsIObserver", observer);
  Preferences.set("test_observe_prefs_nsIObserver.subpref", "something");
  Preferences.set("test_observe_prefs_nsIObserver", "something");
  do_check_true(observer.observed);

  Preferences.ignore("test_observe_prefs_nsIObserver", observer);
  Preferences.set("test_observe_prefs_nsIObserver", "something else");
  do_check_true(observer.observed);

  // Clean up.
  Preferences.reset("test_observe_prefs_nsIObserver");
  Preferences.reset("test_observe_prefs_nsIObserver.subpref");

  run_next_test();
});

// This should not need to be said, but *DO NOT DISABLE THIS TEST*.
//
// Existing consumers of the observer API depend on the observer only
// being triggered for the exact preference they are observing. This is
// expecially true for consumers of the function callback variant which
// passes the preference's new value but not its name.
add_test(function test_observe_exact_pref() {
  let observed = false;
  let observer = function() { observed = !observed };

  Preferences.observe("test_observe_exact_pref", observer);
  Preferences.set("test_observe_exact_pref.sub-pref", "something");
  do_check_false(observed);

  // Clean up.
  Preferences.ignore("test_observe_exact_pref", observer);
  Preferences.reset("test_observe_exact_pref.sub-pref");

  run_next_test();
});

add_test(function test_observe_value_of_set_pref() {
  let observer = function(newVal) { do_check_eq(newVal, "something") };

  Preferences.observe("test_observe_value_of_set_pref", observer);
  Preferences.set("test_observe_value_of_set_pref.subpref", "somethingelse");
  Preferences.set("test_observe_value_of_set_pref", "something");

  // Clean up.
  Preferences.ignore("test_observe_value_of_set_pref", observer);
  Preferences.reset("test_observe_value_of_set_pref");
  Preferences.reset("test_observe_value_of_set_pref.subpref");

  run_next_test();
});

add_test(function test_observe_value_of_reset_pref() {
  let observer = function(newVal) { do_check_true(typeof newVal == "undefined") };

  Preferences.set("test_observe_value_of_reset_pref", "something");
  Preferences.observe("test_observe_value_of_reset_pref", observer);
  Preferences.reset("test_observe_value_of_reset_pref");

  // Clean up.
  Preferences.ignore("test_observe_value_of_reset_pref", observer);

  run_next_test();
});

add_test(function test_has_pref() {
  do_check_false(Preferences.has("test_has_pref"));
  Preferences.set("test_has_pref", "foo");
  do_check_true(Preferences.has("test_has_pref"));

  Preferences.set("test_has_pref.foo", "foo");
  Preferences.set("test_has_pref.bar", "bar");
  let [hasFoo, hasBar, hasBaz] = Preferences.has(["test_has_pref.foo",
                                                  "test_has_pref.bar",
                                                  "test_has_pref.baz"]);
  do_check_true(hasFoo);
  do_check_true(hasBar);
  do_check_false(hasBaz);

  // Clean up.
  Preferences.resetBranch("test_has_pref");

  run_next_test();
});

add_test(function test_isSet_pref() {
  // Use a pref that we know has a default value but no user-set value.
  // This feels dangerous; perhaps we should create some other default prefs
  // that we can use for testing.
  do_check_false(Preferences.isSet("toolkit.defaultChromeURI"));
  Preferences.set("toolkit.defaultChromeURI", "foo");
  do_check_true(Preferences.isSet("toolkit.defaultChromeURI"));

  // Clean up.
  Preferences.reset("toolkit.defaultChromeURI");

  run_next_test();
});

/*
add_test(function test_lock_prefs() {
  // Use a pref that we know has a default value.
  // This feels dangerous; perhaps we should create some other default prefs
  // that we can use for testing.
  do_check_false(Preferences.locked("toolkit.defaultChromeURI"));
  Preferences.lock("toolkit.defaultChromeURI");
  do_check_true(Preferences.locked("toolkit.defaultChromeURI"));
  Preferences.unlock("toolkit.defaultChromeURI");
  do_check_false(Preferences.locked("toolkit.defaultChromeURI"));

  let val = Preferences.get("toolkit.defaultChromeURI");
  Preferences.set("toolkit.defaultChromeURI", "test_lock_prefs");
  do_check_eq(Preferences.get("toolkit.defaultChromeURI"), "test_lock_prefs");
  Preferences.lock("toolkit.defaultChromeURI");
  do_check_eq(Preferences.get("toolkit.defaultChromeURI"), val);
  Preferences.unlock("toolkit.defaultChromeURI");
  do_check_eq(Preferences.get("toolkit.defaultChromeURI"), "test_lock_prefs");

  // Clean up.
  Preferences.reset("toolkit.defaultChromeURI");

  run_next_test();
});
*/
