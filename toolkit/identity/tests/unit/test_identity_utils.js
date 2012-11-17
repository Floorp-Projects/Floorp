
"use strict";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/identity/IdentityUtils.jsm');

function test_check_deprecated() {
  let options = {
    id: 123,
    loggedInEmail: "jed@foo.com",
    pies: 42
  };

  do_check_true(checkDeprecated(options, "loggedInEmail"));
  do_check_false(checkDeprecated(options, "flans"));

  run_next_test();
}

function test_check_renamed() {
  let options = {
    id: 123,
    loggedInEmail: "jed@foo.com",
    pies: 42
  };

  checkRenamed(options, "loggedInEmail", "loggedInUser");

  // It moves loggedInEmail to loggedInUser
  do_check_false(!!options.loggedInEmail);
  do_check_eq(options.loggedInUser, "jed@foo.com");

  run_next_test();
}

let TESTS = [
  test_check_deprecated,
  test_check_renamed
];

TESTS.forEach(add_test);

function run_test() {
  run_next_test();
}
