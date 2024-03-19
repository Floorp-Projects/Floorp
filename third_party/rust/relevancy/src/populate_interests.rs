/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{url_hash::UrlHash, Error, Interest, RelevancyDb, Result};
use std::io::{Cursor, Read};

pub fn ensure_interest_data_populated(db: &RelevancyDb) -> Result<()> {
    if !db.read(|dao| dao.need_to_load_url_interests())? {
        return Ok(());
    }
    let interest_data = match fetch_interest_data() {
        Ok(data) => data,
        Err(e) => {
            log::warn!("error fetching interest data: {e}");
            return Err(Error::FetchInterestDataError);
        }
    };
    db.read_write(move |dao| {
        for (url_hash, interest) in interest_data {
            dao.add_url_interest(url_hash, interest)?;
        }
        Ok(())
    })
}

/// Fetch the interest data
fn fetch_interest_data() -> std::io::Result<Vec<(UrlHash, Interest)>> {
    // TODO: this hack should be replaced with something that fetches from remote settings
    let bytes = include_bytes!("../test-data");
    let mut reader = Cursor::new(&bytes);
    let mut data = vec![];

    // Loop over all possible interests
    for interest in Interest::all() {
        // read the count
        let mut buf = [0u8; 4];
        reader.read_exact(&mut buf)?;
        let count = u32::from_le_bytes(buf);
        for _ in 0..count {
            let mut url_hash: UrlHash = [0u8; 16];
            reader.read_exact(&mut url_hash)?;
            data.push((url_hash, interest));
        }
    }
    Ok(data)
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::InterestVector;

    #[test]
    fn test_interest_vectors() {
        let db = RelevancyDb::open_for_test();
        ensure_interest_data_populated(&db).unwrap();
        db.read(|dao| {
            // Test that the interest data matches the values we started from in
            // `bin/generate-test-data.rs`
            assert_eq!(
                dao.get_url_interest_vector("https://espn.com/").unwrap(),
                InterestVector {
                    sports: 1,
                    ..InterestVector::default()
                }
            );
            assert_eq!(
                dao.get_url_interest_vector("https://dogs.com/").unwrap(),
                InterestVector {
                    animals: 1,
                    ..InterestVector::default()
                }
            );
            assert_eq!(
                dao.get_url_interest_vector("https://cars.com/").unwrap(),
                InterestVector {
                    autos: 1,
                    ..InterestVector::default()
                }
            );
            assert_eq!(
                dao.get_url_interest_vector("https://www.vouge.com/")
                    .unwrap(),
                InterestVector {
                    fashion: 1,
                    ..InterestVector::default()
                }
            );
            assert_eq!(
                dao.get_url_interest_vector("https://slashdot.org/")
                    .unwrap(),
                InterestVector {
                    tech: 1,
                    ..InterestVector::default()
                }
            );
            assert_eq!(
                dao.get_url_interest_vector("https://www.nascar.com/")
                    .unwrap(),
                InterestVector {
                    autos: 1,
                    sports: 1,
                    ..InterestVector::default()
                }
            );
            assert_eq!(
                dao.get_url_interest_vector("https://unknown.url/").unwrap(),
                InterestVector::default()
            );
            Ok(())
        })
        .unwrap();
    }

    #[test]
    fn test_variations_on_the_url() {
        let db = RelevancyDb::open_for_test();
        ensure_interest_data_populated(&db).unwrap();
        db.read(|dao| {
            // Different paths/queries should work
            assert_eq!(
                dao.get_url_interest_vector("https://espn.com/foo/bar/?baz")
                    .unwrap(),
                InterestVector {
                    sports: 1,
                    ..InterestVector::default()
                }
            );
            // Different schemes should too
            assert_eq!(
                dao.get_url_interest_vector("http://espn.com/").unwrap(),
                InterestVector {
                    sports: 1,
                    ..InterestVector::default()
                }
            );
            // But changes to the domain shouldn't
            assert_eq!(
                dao.get_url_interest_vector("http://www.espn.com/").unwrap(),
                InterestVector::default()
            );
            // However, extra components past the 3rd one in the domain are ignored
            assert_eq!(
                dao.get_url_interest_vector("https://foo.www.nascar.com/")
                    .unwrap(),
                InterestVector {
                    autos: 1,
                    sports: 1,
                    ..InterestVector::default()
                }
            );
            Ok(())
        })
        .unwrap();
    }
}
