/**
 * @fileoverview Defines the environment for testharness.js files. This
 * is automatically included in (x)html files including
 * /resources/testharness.js.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// These globals are taken from dom/imptests/testharness.js, via the expose
// function.

module.exports = {
  globals: {
    EventWatcher: false,
    test: false,
    async_test: false,
    promise_test: false,
    promise_rejects: false,
    generate_tests: false,
    setup: false,
    done: false,
    on_event: false,
    step_timeout: false,
    format_value: false,
    assert_true: false,
    assert_false: false,
    assert_equals: false,
    assert_not_equals: false,
    assert_in_array: false,
    assert_object_equals: false,
    assert_array_equals: false,
    assert_approx_equals: false,
    assert_less_than: false,
    assert_greater_than: false,
    assert_between_exclusive: false,
    assert_less_than_equal: false,
    assert_greater_than_equal: false,
    assert_between_inclusive: false,
    assert_regexp_match: false,
    assert_class_string: false,
    assert_exists: false,
    assert_own_property: false,
    assert_not_exists: false,
    assert_inherits: false,
    assert_idl_attribute: false,
    assert_readonly: false,
    assert_throws: false,
    assert_unreaded: false,
    assert_any: false,
    fetch_tests_from_worker: false,
    timeout: false,
    add_start_callback: false,
    add_test_state_callback: false,
    add_result_callback: false,
    add_completion_callback: false,
  },
};
