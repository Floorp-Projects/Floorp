/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/// A suggestion from the database to show in the address bar.
#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub enum Suggestion {
    Amp {
        title: String,
        url: String,
        icon: Option<Vec<u8>>,
        full_keyword: String,
        block_id: i64,
        advertiser: String,
        iab_category: String,
        impression_url: String,
        click_url: String,
    },
    Wikipedia {
        title: String,
        url: String,
        icon: Option<Vec<u8>>,
        full_keyword: String,
    },
}

impl Suggestion {
    /// Returns `true` if the suggestion is sponsored.
    pub(crate) fn is_sponsored(&self) -> bool {
        matches!(self, Self::Amp { .. })
    }
}
