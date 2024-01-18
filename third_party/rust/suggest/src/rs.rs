/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//! Crate-internal types for interacting with Remote Settings (`rs`). Types in
//! this module describe records and attachments in the Suggest Remote Settings
//! collection.
//!
//! To add a new suggestion `T` to this component, you'll generally need to:
//!
//!  1. Add a variant named `T` to [`SuggestRecord`]. The variant must have a
//!     `#[serde(rename)]` attribute that matches the suggestion record's
//!     `type` field.
//!  2. Define a `DownloadedTSuggestion` type with the new suggestion's fields,
//!     matching their attachment's schema. Your new type must derive or
//!     implement [`serde::Deserialize`].
//!  3. Update the database schema in the [`schema`] module to store the new
//!     suggestion.
//!  4. Add an `insert_t_suggestions()` method to [`db::SuggestDao`] that
//!     inserts `DownloadedTSuggestion`s into the database.
//!  5. Update [`store::SuggestStoreInner::ingest()`] to download, deserialize,
//!     and store the new suggestion.
//!  6. Add a variant named `T` to [`suggestion::Suggestion`], with the fields
//!     that you'd like to expose to the application. These can be the same
//!     fields as `DownloadedTSuggestion`, or slightly different, depending on
//!     what the application needs to show the suggestion.
//!  7. Update the `Suggestion` enum definition in `suggest.udl` to match your
//!     new [`suggestion::Suggestion`] variant.
//!  8. Update any [`db::SuggestDao`] methods that query the database to include
//!     the new suggestion in their results, and return `Suggestion::T` variants
//!     as needed.

use std::borrow::Cow;

use remote_settings::{GetItemsOptions, RemoteSettingsResponse};
use serde::{Deserialize, Deserializer};

use crate::{provider::SuggestionProvider, Result};

/// The Suggest Remote Settings collection name.
pub(crate) const REMOTE_SETTINGS_COLLECTION: &str = "quicksuggest";

/// The maximum number of suggestions in a Suggest record's attachment.
///
/// This should be the same as the `BUCKET_SIZE` constant in the
/// `mozilla-services/quicksuggest-rs` repo.
pub(crate) const SUGGESTIONS_PER_ATTACHMENT: u64 = 200;

/// A trait for a client that downloads suggestions from Remote Settings.
///
/// This trait lets tests use a mock client.
pub(crate) trait SuggestRemoteSettingsClient {
    /// Fetches records from the Suggest Remote Settings collection.
    fn get_records_with_options(&self, options: &GetItemsOptions)
        -> Result<RemoteSettingsResponse>;

    /// Fetches a record's attachment from the Suggest Remote Settings
    /// collection.
    fn get_attachment(&self, location: &str) -> Result<Vec<u8>>;
}

impl SuggestRemoteSettingsClient for remote_settings::Client {
    fn get_records_with_options(
        &self,
        options: &GetItemsOptions,
    ) -> Result<RemoteSettingsResponse> {
        Ok(remote_settings::Client::get_records_with_options(
            self, options,
        )?)
    }

    fn get_attachment(&self, location: &str) -> Result<Vec<u8>> {
        Ok(remote_settings::Client::get_attachment(self, location)?)
    }
}

/// A record in the Suggest Remote Settings collection.
///
/// Except for the type, Suggest records don't carry additional fields. All
/// suggestions are stored in each record's attachment.
#[derive(Clone, Debug, Deserialize, PartialEq)]
#[serde(tag = "type")]
pub(crate) enum SuggestRecord {
    #[serde(rename = "icon")]
    Icon,
    #[serde(rename = "data")]
    AmpWikipedia,
    #[serde(rename = "amo-suggestions")]
    Amo,
    #[serde(rename = "pocket-suggestions")]
    Pocket,
}

/// Represents either a single value, or a list of values. This is used to
/// deserialize downloaded attachments.
#[derive(Clone, Debug, Deserialize)]
#[serde(untagged)]
enum OneOrMany<T> {
    One(T),
    Many(Vec<T>),
}

/// A downloaded Remote Settings attachment that contains suggestions.
#[derive(Clone, Debug, Deserialize)]
#[serde(transparent)]
pub(crate) struct SuggestAttachment<T>(OneOrMany<T>);

impl<T> SuggestAttachment<T> {
    /// Returns a slice of suggestions to ingest from the downloaded attachment.
    pub fn suggestions(&self) -> &[T] {
        match &self.0 {
            OneOrMany::One(value) => std::slice::from_ref(value),
            OneOrMany::Many(values) => values,
        }
    }
}

/// The ID of a record in the Suggest Remote Settings collection.
#[derive(Clone, Debug, Deserialize, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[serde(transparent)]
pub(crate) struct SuggestRecordId<'a>(Cow<'a, str>);

impl<'a> SuggestRecordId<'a> {
    pub fn as_str(&self) -> &str {
        &self.0
    }

    /// If this ID is for an icon record, extracts and returns the icon ID.
    ///
    /// The icon ID is the primary key for an ingested icon. Downloaded
    /// suggestions also reference these icon IDs, in
    /// [`DownloadedSuggestion::icon_id`].
    pub fn as_icon_id(&self) -> Option<&str> {
        self.0.strip_prefix("icon-")
    }
}

impl<'a, T> From<T> for SuggestRecordId<'a>
where
    T: Into<Cow<'a, str>>,
{
    fn from(value: T) -> Self {
        Self(value.into())
    }
}

/// Fields that are common to all downloaded suggestions.
#[derive(Clone, Debug, Deserialize, Eq, PartialEq)]
pub(crate) struct DownloadedSuggestionCommonDetails {
    pub keywords: Vec<String>,
    pub title: String,
    pub url: String,
}

/// An AMP suggestion to ingest from an AMP-Wikipedia attachment.
#[derive(Clone, Debug, Deserialize, Eq, PartialEq)]
pub(crate) struct DownloadedAmpSuggestion {
    #[serde(flatten)]
    pub common_details: DownloadedSuggestionCommonDetails,
    pub advertiser: String,
    #[serde(rename = "id")]
    pub block_id: i32,
    pub iab_category: String,
    pub click_url: String,
    pub impression_url: String,
    #[serde(rename = "icon")]
    pub icon_id: String,
}

/// A Wikipedia suggestion to ingest from an AMP-Wikipedia attachment.
#[derive(Clone, Debug, Deserialize, Eq, PartialEq)]
pub(crate) struct DownloadedWikipediaSuggestion {
    #[serde(flatten)]
    pub common_details: DownloadedSuggestionCommonDetails,
    #[serde(rename = "icon")]
    pub icon_id: String,
}

/// A suggestion to ingest from an AMP-Wikipedia attachment downloaded from
/// Remote Settings.
#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) enum DownloadedAmpWikipediaSuggestion {
    Amp(DownloadedAmpSuggestion),
    Wikipedia(DownloadedWikipediaSuggestion),
}

impl DownloadedAmpWikipediaSuggestion {
    /// Returns the details that are common to AMP and Wikipedia suggestions.
    pub fn common_details(&self) -> &DownloadedSuggestionCommonDetails {
        match self {
            Self::Amp(DownloadedAmpSuggestion { common_details, .. }) => common_details,
            Self::Wikipedia(DownloadedWikipediaSuggestion { common_details, .. }) => common_details,
        }
    }

    /// Returns the provider of this suggestion.
    pub fn provider(&self) -> SuggestionProvider {
        match self {
            DownloadedAmpWikipediaSuggestion::Amp(_) => SuggestionProvider::Amp,
            DownloadedAmpWikipediaSuggestion::Wikipedia(_) => SuggestionProvider::Wikipedia,
        }
    }
}

impl<'de> Deserialize<'de> for DownloadedAmpWikipediaSuggestion {
    fn deserialize<D>(
        deserializer: D,
    ) -> std::result::Result<DownloadedAmpWikipediaSuggestion, D::Error>
    where
        D: Deserializer<'de>,
    {
        // AMP and Wikipedia suggestions use the same schema. To separate them,
        // we use a "maybe tagged" outer enum with tagged and untagged variants,
        // and a "tagged" inner enum.
        //
        // Wikipedia suggestions will deserialize successfully into the tagged
        // variant. AMP suggestions will try the tagged variant, fail, and fall
        // back to the untagged variant.
        //
        // This approach works around serde-rs/serde#912.

        #[derive(Deserialize)]
        #[serde(untagged)]
        enum MaybeTagged {
            Tagged(Tagged),
            Untagged(DownloadedAmpSuggestion),
        }

        #[derive(Deserialize)]
        #[serde(tag = "advertiser")]
        enum Tagged {
            #[serde(rename = "Wikipedia")]
            Wikipedia(DownloadedWikipediaSuggestion),
        }

        Ok(match MaybeTagged::deserialize(deserializer)? {
            MaybeTagged::Tagged(Tagged::Wikipedia(wikipedia)) => Self::Wikipedia(wikipedia),
            MaybeTagged::Untagged(amp) => Self::Amp(amp),
        })
    }
}

/// An AMO suggestion to ingest from an attachment
#[derive(Clone, Debug, Deserialize)]
pub(crate) struct DownloadedAmoSuggestion {
    pub description: String,
    pub url: String,
    pub guid: String,
    #[serde(rename = "icon")]
    pub icon_url: String,
    pub rating: Option<String>,
    pub number_of_ratings: i64,
    pub title: String,
    pub keywords: Vec<String>,
    pub score: f64,
}
/// A Pocket suggestion to ingest from a Pocket Suggestion Attachment
#[derive(Clone, Debug, Deserialize)]
pub(crate) struct DownloadedPocketSuggestion {
    pub url: String,
    pub title: String,
    #[serde(rename = "lowConfidenceKeywords")]
    pub low_confidence_keywords: Vec<String>,
    #[serde(rename = "highConfidenceKeywords")]
    pub high_confidence_keywords: Vec<String>,
    pub score: f64,
}
