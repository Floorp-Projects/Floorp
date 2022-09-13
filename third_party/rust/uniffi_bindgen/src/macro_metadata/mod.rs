/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::ComponentInterface;
use camino::Utf8Path;

mod ci;
mod extract;

pub use ci::add_to_ci;
pub use extract::extract_from_library;

pub fn add_to_ci_from_library(
    iface: &mut ComponentInterface,
    library_path: &Utf8Path,
) -> anyhow::Result<()> {
    add_to_ci(iface, extract_from_library(library_path)?)
}
