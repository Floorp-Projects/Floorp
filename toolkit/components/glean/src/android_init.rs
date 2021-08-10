// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use nserror::{nsresult, NS_OK};
use nsstring::nsACString;

// Needed for re-export.
pub use glean_ffi;

/// Workaround to force a re-export of the `no_mangle` symbols from `glean_ffi`
///
/// Due to how linking works and hides symbols the symbols from `glean_ffi` might not be
/// re-exported and thus not usable.
/// By forcing use of _at least one_ symbol in an exported function the functions will also be
/// rexported.
/// This is only required for debug builds (and `debug_assertions` is the closest thing we have to
/// check that).
/// In release builds we rely on LTO builds to take care of it.
/// Our tests should ensure this actually happens.
///
/// See https://github.com/rust-lang/rust/issues/50007
#[no_mangle]
pub unsafe extern "C" fn _fog_force_reexport_donotcall() {
    glean_ffi::glean_enable_logging();
}

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
