/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use crate::{
    schema::RelevancyConnectionInitializer,
    url_hash::{hash_url, UrlHash},
    Interest, InterestVector, Result,
};
use parking_lot::Mutex;
use rusqlite::{Connection, OpenFlags};
use sql_support::{open_database::open_database_with_flags, ConnExt};
use std::path::Path;

/// A thread-safe wrapper around an SQLite connection to the Relevancy database
pub struct RelevancyDb {
    pub conn: Mutex<Connection>,
}

impl RelevancyDb {
    pub fn open(path: impl AsRef<Path>) -> Result<Self> {
        let conn = open_database_with_flags(
            path,
            OpenFlags::SQLITE_OPEN_URI
                | OpenFlags::SQLITE_OPEN_NO_MUTEX
                | OpenFlags::SQLITE_OPEN_CREATE
                | OpenFlags::SQLITE_OPEN_READ_WRITE,
            &RelevancyConnectionInitializer,
        )?;
        Ok(Self {
            conn: Mutex::new(conn),
        })
    }

    #[cfg(test)]
    pub fn open_for_test() -> Self {
        use std::sync::atomic::{AtomicU32, Ordering};
        static COUNTER: AtomicU32 = AtomicU32::new(0);
        let count = COUNTER.fetch_add(1, Ordering::Relaxed);
        Self::open(format!("file:test{count}.sqlite?mode=memory&cache=shared")).unwrap()
    }

    /// Accesses the Suggest database in a transaction for reading.
    pub fn read<T>(&self, op: impl FnOnce(&RelevancyDao) -> Result<T>) -> Result<T> {
        let mut conn = self.conn.lock();
        let tx = conn.transaction()?;
        let dao = RelevancyDao::new(&tx);
        op(&dao)
    }

    /// Accesses the Suggest database in a transaction for reading and writing.
    pub fn read_write<T>(&self, op: impl FnOnce(&mut RelevancyDao) -> Result<T>) -> Result<T> {
        let mut conn = self.conn.lock();
        let tx = conn.transaction()?;
        let mut dao = RelevancyDao::new(&tx);
        let result = op(&mut dao)?;
        tx.commit()?;
        Ok(result)
    }
}

/// A data access object (DAO) that wraps a connection to the Relevancy database
///
/// Methods that only read from the database take an immutable reference to
/// `self` (`&self`), and methods that write to the database take a mutable
/// reference (`&mut self`).
pub struct RelevancyDao<'a> {
    pub conn: &'a Connection,
}

impl<'a> RelevancyDao<'a> {
    fn new(conn: &'a Connection) -> Self {
        Self { conn }
    }

    /// Associate a URL with an interest
    pub fn add_url_interest(&mut self, url_hash: UrlHash, interest: Interest) -> Result<()> {
        let sql = "
            INSERT OR REPLACE INTO url_interest(url_hash, interest_code)
            VALUES (?, ?)
        ";
        self.conn.execute(sql, (url_hash, interest as u32))?;
        Ok(())
    }

    /// Get an interest vector for a URL
    pub fn get_url_interest_vector(&self, url: &str) -> Result<InterestVector> {
        let hash = match hash_url(url) {
            Some(u) => u,
            None => return Ok(InterestVector::default()),
        };
        let mut stmt = self.conn.prepare_cached(
            "
            SELECT interest_code
            FROM url_interest
            WHERE url_hash=?
        ",
        )?;
        let interests = stmt.query_and_then((hash,), |row| -> Result<Interest> {
            Ok(row.get::<_, u32>(0)?.into())
        })?;

        let mut interest_vec = InterestVector::default();
        for interest in interests {
            interest_vec[interest?] += 1
        }
        Ok(interest_vec)
    }

    /// Do we need to load the interest data?
    pub fn need_to_load_url_interests(&self) -> Result<bool> {
        // TODO: we probably will need a better check than this.
        Ok(self
            .conn
            .query_one("SELECT NOT EXISTS (SELECT 1 FROM url_interest)")?)
    }
}
