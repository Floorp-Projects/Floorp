// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#[macro_use]
mod macros;

define_metric_ffi!(COUNTER_MAP {
    test_has -> fog_counter_test_has_value,
    test_get -> fog_counter_test_get_value: i32,
    add -> fog_counter_add(amount: i32),
});
