/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);

add_test(function test_set_get_pref() {
  Preferences.set("test_set_get_pref.integer", 1);
  Assert.equal(Preferences.get("test_set_get_pref.integer"), 1);

  Preferences.set("test_set_get_pref.string", "foo");
  Assert.equal(Preferences.get("test_set_get_pref.string"), "foo");

  Preferences.set("test_set_get_pref.boolean", true);
  Assert.equal(Preferences.get("test_set_get_pref.boolean"), true);

  // Clean up.
  Preferences.resetBranch("test_set_get_pref.");

  run_next_test();
});

add_test(function test_set_get_branch_pref() {
  let prefs = new Preferences("test_set_get_branch_pref.");

  prefs.set("something", 1);
  Assert.equal(prefs.get("something"), 1);
  Assert.ok(!Preferences.has("something"));

  // Clean up.
  prefs.reset("something");

  run_next_test();
});

add_test(function test_set_get_multiple_prefs() {
  Preferences.set({
    "test_set_get_multiple_prefs.integer": 1,
    "test_set_get_multiple_prefs.string": "foo",
    "test_set_get_multiple_prefs.boolean": true,
  });

  let [i, s, b] = Preferences.get([
    "test_set_get_multiple_prefs.integer",
    "test_set_get_multiple_prefs.string",
    "test_set_get_multiple_prefs.boolean",
  ]);

  Assert.equal(i, 1);
  Assert.equal(s, "foo");
  Assert.equal(b, true);

  // Clean up.
  Preferences.resetBranch("test_set_get_multiple_prefs.");

  run_next_test();
});

add_test(function test_get_multiple_prefs_with_default_value() {
  Preferences.set({
    "test_get_multiple_prefs_with_default_value.a": 1,
    "test_get_multiple_prefs_with_default_value.b": 2,
  });

  let [a, b, c] = Preferences.get(
    [
      "test_get_multiple_prefs_with_default_value.a",
      "test_get_multiple_prefs_with_default_value.b",
      "test_get_multiple_prefs_with_default_value.c",
    ],
    0
  );

  Assert.equal(a, 1);
  Assert.equal(b, 2);
  Assert.equal(c, 0);

  // Clean up.
  Preferences.resetBranch("test_get_multiple_prefs_with_default_value.");

  run_next_test();
});

add_test(function test_set_get_unicode_pref() {
  Preferences.set("test_set_get_unicode_pref", String.fromCharCode(960));
  Assert.equal(
    Preferences.get("test_set_get_unicode_pref"),
    String.fromCharCode(960)
  );

  // Clean up.
  Preferences.reset("test_set_get_unicode_pref");

  run_next_test();
});

add_test(function test_set_null_pref() {
  try {
    Preferences.set("test_set_null_pref", null);
    // We expect this to throw, so the test is designed to fail if it doesn't.
    Assert.ok(false);
  } catch (ex) {}

  run_next_test();
});

add_test(function test_set_undefined_pref() {
  try {
    Preferences.set("test_set_undefined_pref");
    // We expect this to throw, so the test is designed to fail if it doesn't.
    Assert.ok(false);
  } catch (ex) {}

  run_next_test();
});

add_test(function test_set_unsupported_pref() {
  try {
    Preferences.set("test_set_unsupported_pref", []);
    // We expect this to throw, so the test is designed to fail if it doesn't.
    Assert.ok(false);
  } catch (ex) {}

  run_next_test();
});

// Make sure that we can get a string pref that we didn't set ourselves.
add_test(function test_get_string_pref() {
  Services.prefs.setCharPref("test_get_string_pref", "a normal string");
  Assert.equal(Preferences.get("test_get_string_pref"), "a normal string");

  // Clean up.
  Preferences.reset("test_get_string_pref");

  run_next_test();
});

add_test(function test_get_localized_string_pref() {
  let prefName = "test_get_localized_string_pref";
  let localizedString = Cc[
    "@mozilla.org/pref-localizedstring;1"
  ].createInstance(Ci.nsIPrefLocalizedString);
  localizedString.data = "a localized string";
  Services.prefs.setComplexValue(
    prefName,
    Ci.nsIPrefLocalizedString,
    localizedString
  );
  Assert.equal(
    Preferences.get(prefName, null, Ci.nsIPrefLocalizedString),
    "a localized string"
  );

  // Clean up.
  Preferences.reset(prefName);

  run_next_test();
});

add_test(function test_set_get_number_pref() {
  Preferences.set("test_set_get_number_pref", 5);
  Assert.equal(Preferences.get("test_set_get_number_pref"), 5);

  // Non-integer values get converted to integers.
  Preferences.set("test_set_get_number_pref", 3.14159);
  Assert.equal(Preferences.get("test_set_get_number_pref"), 3);

  // Values outside the range -(2^31-1) to 2^31-1 overflow.
  try {
    Preferences.set("test_set_get_number_pref", Math.pow(2, 31));
    // We expect this to throw, so the test is designed to fail if it doesn't.
    Assert.ok(false);
  } catch (ex) {}

  // Clean up.
  Preferences.reset("test_set_get_number_pref");

  run_next_test();
});

add_test(function test_reset_pref() {
  Preferences.set("test_reset_pref", 1);
  Preferences.reset("test_reset_pref");
  Assert.equal(Preferences.get("test_reset_pref"), undefined);

  run_next_test();
});

add_test(function test_reset_pref_branch() {
  Preferences.set("test_reset_pref_branch.foo", 1);
  Preferences.set("test_reset_pref_branch.bar", 2);
  Preferences.resetBranch("test_reset_pref_branch.");
  Assert.equal(Preferences.get("test_reset_pref_branch.foo"), undefined);
  Assert.equal(Preferences.get("test_reset_pref_branch.bar"), undefined);

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
  let observer = function() {
    observed = !observed;
  };

  Preferences.observe("test_observe_prefs_function", observer);
  Preferences.set("test_observe_prefs_function.subpref", "something");
  Assert.ok(!observed);
  Preferences.set("test_observe_prefs_function", "something");
  Assert.ok(observed);

  Preferences.ignore("test_observe_prefs_function", observer);
  Preferences.set("test_observe_prefs_function", "something else");
  Assert.ok(observed);

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
    },
  };

  Preferences.observe("test_observe_prefs_object", observer.observe, observer);
  Preferences.set("test_observe_prefs_object.subpref", "something");
  Assert.ok(!observer.observed);
  Preferences.set("test_observe_prefs_object", "something");
  Assert.ok(observer.observed);

  Preferences.ignore("test_observe_prefs_object", observer.observe, observer);
  Preferences.set("test_observe_prefs_object", "something else");
  Assert.ok(observer.observed);

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
      Assert.ok(subject instanceof Ci.nsIPrefBranch);
      Assert.equal(topic, "nsPref:changed");
      Assert.equal(data, "test_observe_prefs_nsIObserver");
    },
  };

  Preferences.observe("test_observe_prefs_nsIObserver", observer);
  Preferences.set("test_observe_prefs_nsIObserver.subpref", "something");
  Preferences.set("test_observe_prefs_nsIObserver", "something");
  Assert.ok(observer.observed);

  Preferences.ignore("test_observe_prefs_nsIObserver", observer);
  Preferences.set("test_observe_prefs_nsIObserver", "something else");
  Assert.ok(observer.observed);

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
  let observer = function() {
    observed = !observed;
  };

  Preferences.observe("test_observe_exact_pref", observer);
  Preferences.set("test_observe_exact_pref.sub-pref", "something");
  Assert.ok(!observed);

  // Clean up.
  Preferences.ignore("test_observe_exact_pref", observer);
  Preferences.reset("test_observe_exact_pref.sub-pref");

  run_next_test();
});

add_test(function test_observe_value_of_set_pref() {
  let observer = function(newVal) {
    Assert.equal(newVal, "something");
  };

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
  let observer = function(newVal) {
    Assert.ok(typeof newVal == "undefined");
  };

  Preferences.set("test_observe_value_of_reset_pref", "something");
  Preferences.observe("test_observe_value_of_reset_pref", observer);
  Preferences.reset("test_observe_value_of_reset_pref");

  // Clean up.
  Preferences.ignore("test_observe_value_of_reset_pref", observer);

  run_next_test();
});

add_test(function test_has_pref() {
  Assert.ok(!Preferences.has("test_has_pref"));
  Preferences.set("test_has_pref", "foo");
  Assert.ok(Preferences.has("test_has_pref"));

  Preferences.set("test_has_pref.foo", "foo");
  Preferences.set("test_has_pref.bar", "bar");
  let [hasFoo, hasBar, hasBaz] = Preferences.has([
    "test_has_pref.foo",
    "test_has_pref.bar",
    "test_has_pref.baz",
  ]);
  Assert.ok(hasFoo);
  Assert.ok(hasBar);
  Assert.ok(!hasBaz);

  // Clean up.
  Preferences.resetBranch("test_has_pref");

  run_next_test();
});

add_test(function test_isSet_pref() {
  // Use a pref that we know has a default value but no user-set value.
  // This feels dangerous; perhaps we should create some other default prefs
  // that we can use for testing.
  Assert.ok(!Preferences.isSet("toolkit.defaultChromeURI"));
  Preferences.set("toolkit.defaultChromeURI", "foo");
  Assert.ok(Preferences.isSet("toolkit.defaultChromeURI"));

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
