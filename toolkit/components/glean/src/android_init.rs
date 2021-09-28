// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use nserror::{nsresult, NS_OK};
use nsstring::nsACString;

// Needed for re-export.
pub use glean_ffi;

/// On Android initialize the bare minimum of FOG.
///
/// This does not initialize Glean, which should be handled by the bundling application.
#[no_mangle]
pub unsafe extern "C" fn fog_init(
    _data_path_override: &nsACString,
    _app_id_override: &nsACString,
) -> nsresult {
    // Register all custom pings before we initialize.
    fog::pings::register_pings();

    NS_OK
}
