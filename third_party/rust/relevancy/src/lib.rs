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
mod ingest;
mod interest;
mod rs;
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
    pub fn new(db_path: String) -> Self {
        Self {
            db: RelevancyDb::new(db_path),
        }
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
    pub fn ingest(&self, top_urls_by_frecency: Vec<String>) -> ApiResult<InterestVector> {
        ingest::ensure_interest_data_populated(&self.db)?;
        self.classify(top_urls_by_frecency)
    }

    pub fn classify(&self, top_urls_by_frecency: Vec<String>) -> Result<InterestVector> {
        // For experimentation purposes we are going to return an interest vector.
        // Eventually we would want to store this data in the DB and incrementally update it.
        let mut interest_vector = InterestVector::default();
        for url in top_urls_by_frecency {
            let interest_count = self.db.read(|dao| dao.get_url_interest_vector(&url))?;
            interest_vector = interest_vector + interest_count;
        }

        Ok(interest_vector)
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

#[cfg(test)]
mod test {
    use crate::url_hash::hash_url;

    use super::*;

    #[test]
    fn test_ingest() {
        let top_urls = vec![
            "https://food.com/".to_string(),
            "https://hello.com".to_string(),
            "https://pasta.com".to_string(),
            "https://dog.com".to_string(),
        ];
        let relevancy_store =
            RelevancyStore::new("file:test_store_data?mode=memory&cache=shared".to_owned());
        relevancy_store
            .db
            .read_write(|dao| {
                dao.add_url_interest(hash_url("https://food.com").unwrap(), Interest::Food)?;
                dao.add_url_interest(
                    hash_url("https://hello.com").unwrap(),
                    Interest::Inconclusive,
                )?;
                dao.add_url_interest(hash_url("https://pasta.com").unwrap(), Interest::Food)?;
                dao.add_url_interest(hash_url("https://dog.com").unwrap(), Interest::Animals)?;
                Ok(())
            })
            .expect("Insert should succeed");

        assert_eq!(
            relevancy_store.ingest(top_urls).unwrap(),
            InterestVector {
                inconclusive: 1,
                animals: 1,
                food: 2,
                ..InterestVector::default()
            }
        );
    }
}
