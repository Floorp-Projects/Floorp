// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use nserror::{nsresult, NS_OK};
use nsstring::nsAString;

#[no_mangle]
pub unsafe extern "C" fn fog_init(_data_dir: &nsAString, _pingsender_path: &nsAString) -> nsresult {
    NS_OK
}
