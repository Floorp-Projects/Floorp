// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

/// The minimum version of NSS that is required by this version of neqo.
/// Note that the string may contain whitespace at the beginning and/or end.
pub(crate) const MINIMUM_NSS_VERSION: &str = include_str!("../min_version.txt");
