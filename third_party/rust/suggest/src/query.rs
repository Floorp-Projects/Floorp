/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::SuggestionProvider;

/// A query for suggestions to show in the address bar.
#[derive(Clone, Debug, Default)]
pub struct SuggestionQuery {
    pub keyword: String,
    pub providers: Vec<SuggestionProvider>,
    pub limit: Option<i32>,
}

impl SuggestionQuery {
    pub fn all_providers(keyword: &str) -> Self {
        Self {
            keyword: keyword.to_string(),
            providers: Vec::from(SuggestionProvider::all()),
            limit: None,
        }
    }

    pub fn with_providers(keyword: &str, providers: Vec<SuggestionProvider>) -> Self {
        Self {
            keyword: keyword.to_string(),
            providers,
            limit: None,
        }
    }

    pub fn all_providers_except(keyword: &str, provider: SuggestionProvider) -> Self {
        Self::with_providers(
            keyword,
            SuggestionProvider::all()
                .into_iter()
                .filter(|p| *p != provider)
                .collect(),
        )
    }

    pub fn amp(keyword: &str) -> Self {
        Self {
            keyword: keyword.into(),
            providers: vec![SuggestionProvider::Amp],
            limit: None,
        }
    }

    pub fn wikipedia(keyword: &str) -> Self {
        Self {
            keyword: keyword.into(),
            providers: vec![SuggestionProvider::Wikipedia],
            limit: None,
        }
    }

    pub fn amp_mobile(keyword: &str) -> Self {
        Self {
            keyword: keyword.into(),
            providers: vec![SuggestionProvider::AmpMobile],
            limit: None,
        }
    }

    pub fn amo(keyword: &str) -> Self {
        Self {
            keyword: keyword.into(),
            providers: vec![SuggestionProvider::Amo],
            limit: None,
        }
    }

    pub fn pocket(keyword: &str) -> Self {
        Self {
            keyword: keyword.into(),
            providers: vec![SuggestionProvider::Pocket],
            limit: None,
        }
    }

    pub fn yelp(keyword: &str) -> Self {
        Self {
            keyword: keyword.into(),
            providers: vec![SuggestionProvider::Yelp],
            limit: None,
        }
    }

    pub fn mdn(keyword: &str) -> Self {
        Self {
            keyword: keyword.into(),
            providers: vec![SuggestionProvider::Mdn],
            limit: None,
        }
    }

    pub fn weather(keyword: &str) -> Self {
        Self {
            keyword: keyword.into(),
            providers: vec![SuggestionProvider::Weather],
            limit: None,
        }
    }

    pub fn limit(self, limit: i32) -> Self {
        Self {
            limit: Some(limit),
            ..self
        }
    }
}
