/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use remote_settings::RemoteSettingsConfig;
mod db;
mod error;
mod keyword;
pub mod pocket;
mod provider;
mod rs;
mod schema;
mod store;
mod suggestion;
mod yelp;

pub use error::SuggestApiError;
pub use provider::SuggestionProvider;
pub use store::{SuggestIngestionConstraints, SuggestStore};
pub use suggestion::{raw_suggestion_url_matches, Suggestion};

pub(crate) type Result<T> = std::result::Result<T, error::Error>;
pub type SuggestApiResult<T> = std::result::Result<T, error::SuggestApiError>;

/// A query for suggestions to show in the address bar.
#[derive(Debug, Default)]
pub struct SuggestionQuery {
    pub keyword: String,
    pub providers: Vec<SuggestionProvider>,
    pub limit: Option<i32>,
}

uniffi::include_scaffolding!("suggest");
