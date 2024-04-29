/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Proposed API for the relevancy component (validation phase)
//!
//! The goal here is to allow us to validate that we can reliably detect user interests from
//! history data, without spending too much time building the API out.  There's some hand-waving
//! towards how we would use this data to rank search results, but we don't need to come to a final
//! decision on that yet.

mod db;
mod error;
mod interest;
mod populate_interests;
mod schema;
pub mod url_hash;

pub use db::RelevancyDb;
pub use error::{ApiResult, Error, RelevancyApiError, Result};
pub use interest::{Interest, InterestVector};

use error_support::handle_error;

pub struct RelevancyStore {
    db: RelevancyDb,
}

/// Top-level API for the Relevancy component
impl RelevancyStore {
    #[handle_error(Error)]
    pub fn new(db_path: String) -> ApiResult<Self> {
        Ok(Self {
            db: RelevancyDb::new(db_path)?,
        })
    }

    pub fn close(&self) {
        self.db.close()
    }

    pub fn interrupt(&self) {
        self.db.interrupt()
    }

    /// Ingest top URLs to build the user's interest vector.
    ///
    /// Consumer should pass a list of the user's top URLs by frecency to this method.  It will
    /// then:
    ///
    ///  - Download the URL interest data from remote settings.  Eventually this should be cached /
    ///    stored in the database, but for now it would be fine to download fresh data each time.
    ///  - Match the user's top URls against the interest data to build up their interest vector.
    ///  - Store the user's interest vector in the database.
    ///
    ///  This method may execute for a long time and should only be called from a worker thread.
    #[handle_error(Error)]
    pub fn ingest(&self, _top_urls_by_frecency: Vec<String>) -> ApiResult<()> {
        populate_interests::ensure_interest_data_populated(&self.db)?;
        todo!()
    }

    /// Calculate metrics for the validation phase
    ///
    /// This runs after [Self::ingest].  It takes the interest vector that ingest created and
    /// calculates a set of metrics that we can report to glean.
    #[handle_error(Error)]
    pub fn calculate_metrics(&self) -> ApiResult<InterestMetrics> {
        todo!()
    }

    /// Get the user's interest vector directly.
    ///
    /// This runs after [Self::ingest].  It returns the interest vector directly so that the
    /// consumer can show it in an `about:` page.
    #[handle_error(Error)]
    pub fn user_interest_vector(&self) -> ApiResult<InterestVector> {
        todo!()
    }
}

/// Interest metric data.  See `relevancy.udl` for details.
pub struct InterestMetrics {
    pub top_single_interest_similarity: u32,
    pub top_2interest_similarity: u32,
    pub top_3interest_similarity: u32,
}

uniffi::include_scaffolding!("relevancy");
