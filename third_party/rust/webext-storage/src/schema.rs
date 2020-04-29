/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// XXXXXX - This has been cloned from places/src/schema.rs, which has the
// comment:
// // This has been cloned from logins/src/schema.rs, on Thom's
// // wip-sync-sql-store branch.
// // We should work out how to turn this into something that can use a shared
// // db.rs.
//
// And we really should :) But not now.

use crate::error::Result;
use rusqlite::{Connection, NO_PARAMS};
use sql_support::ConnExt;

const VERSION: i64 = 1; // let's avoid bumping this and migrating for now!

const CREATE_SCHEMA_SQL: &str = include_str!("../sql/create_schema.sql");

fn get_current_schema_version(db: &Connection) -> Result<i64> {
    Ok(db.query_one::<i64>("PRAGMA user_version")?)
}

pub fn init(db: &Connection) -> Result<()> {
    let user_version = get_current_schema_version(db)?;
    if user_version == 0 {
        create(db)?;
    } else if user_version != VERSION {
        if user_version < VERSION {
            panic!("no migrations yet!");
        } else {
            log::warn!(
                "Loaded future schema version {} (we only understand version {}). \
                 Optimistically ",
                user_version,
                VERSION
            );
            // Downgrade the schema version, so that anything added with our
            // schema is migrated forward when the newer library reads our
            // database.
            db.execute_batch(&format!("PRAGMA user_version = {};", VERSION))?;
        }
    }
    Ok(())
}

fn create(db: &Connection) -> Result<()> {
    log::debug!("Creating schema");
    db.execute_batch(CREATE_SCHEMA_SQL)?;
    db.execute(
        &format!("PRAGMA user_version = {version}", version = VERSION),
        NO_PARAMS,
    )?;

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::db::test::new_mem_db;

    #[test]
    fn test_create_schema_twice() {
        let db = new_mem_db();
        db.execute_batch(CREATE_SCHEMA_SQL)
            .expect("should allow running twice");
    }
}
