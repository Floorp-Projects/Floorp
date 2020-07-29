// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use common::*;

use once_cell::sync::Lazy;

use glean::private::Ping;

// Smoke test for what should be the generated code.
static PROTOTYPE_PING: Lazy<Ping> = Lazy::new(|| Ping::new("prototype", false, true, vec![]));

#[test]
fn smoke_test_custom_ping() {
    let _lock = lock_test();
    let _t = setup_glean(None);

    // We can only check that nothing explodes.
    // `true` signals that the ping has been succesfully submitted (and written to disk).
    assert_eq!(true, PROTOTYPE_PING.submit(None));
}
