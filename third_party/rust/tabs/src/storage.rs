/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// From https://searchfox.org/mozilla-central/rev/ea63a0888d406fae720cf24f4727d87569a8cab5/services/sync/modules/constants.js#75
const URI_LENGTH_MAX: usize = 65536;
// https://searchfox.org/mozilla-central/rev/ea63a0888d406fae720cf24f4727d87569a8cab5/services/sync/modules/engines/tabs.js#8
const TAB_ENTRIES_LIMIT: usize = 5;

use crate::error::*;
use crate::DeviceType;
use rusqlite::{Connection, OpenFlags};
use serde_derive::{Deserialize, Serialize};
use sql_support::open_database::{self, open_database_with_flags};
use sql_support::ConnExt;
use std::cell::RefCell;
use std::path::{Path, PathBuf};

pub type TabsDeviceType = crate::DeviceType;
pub type RemoteTabRecord = RemoteTab;

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct RemoteTab {
    pub title: String,
    pub url_history: Vec<String>,
    pub icon: Option<String>,
    pub last_used: i64, // In ms.
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ClientRemoteTabs {
    pub client_id: String, // Corresponds to the `clients` collection ID of the client.
    pub client_name: String,
    #[serde(
        default = "devicetype_default_deser",
        skip_serializing_if = "devicetype_is_unknown"
    )]
    pub device_type: DeviceType,
    // serde default so we can read old rows that didn't persist this.
    #[serde(default)]
    pub last_modified: i64,
    pub remote_tabs: Vec<RemoteTab>,
}

fn devicetype_default_deser() -> DeviceType {
    // replace with `DeviceType::default_deser` once #4861 lands.
    DeviceType::Unknown
}

// Unlike most other uses-cases, here we do allow serializing ::Unknown, but skip it.
fn devicetype_is_unknown(val: &DeviceType) -> bool {
    matches!(val, DeviceType::Unknown)
}

// Tabs has unique requirements for storage:
// * The "local_tabs" exist only so we can sync them out. There's no facility to
//   query "local tabs", so there's no need to store these persistently - ie, they
//   are write-only.
// * The "remote_tabs" exist purely for incoming items via sync - there's no facility
//   to set them locally - they are read-only.
// Note that this means a database is only actually needed after Sync fetches remote tabs,
// and because sync users are in the minority, the use of a database here is purely
// optional and created on demand. The implication here is that asking for the "remote tabs"
// when no database exists is considered a normal situation and just implies no remote tabs exist.
// (Note however we don't attempt to remove the database when no remote tabs exist, so having
// no remote tabs in an existing DB is also a normal situation)
pub struct TabsStorage {
    local_tabs: RefCell<Option<Vec<RemoteTab>>>,
    db_path: PathBuf,
    db_connection: Option<Connection>,
}

impl TabsStorage {
    pub fn new(db_path: impl AsRef<Path>) -> Self {
        Self {
            local_tabs: RefCell::default(),
            db_path: db_path.as_ref().to_path_buf(),
            db_connection: None,
        }
    }

    /// Arrange for a new memory-based TabsStorage. As per other DB semantics, creating
    /// this isn't enough to actually create the db!
    pub fn new_with_mem_path(db_path: &str) -> Self {
        let name = PathBuf::from(format!("file:{}?mode=memory&cache=shared", db_path));
        Self::new(name)
    }

    /// If a DB file exists, open and return it.
    pub fn open_if_exists(&mut self) -> Result<Option<&Connection>> {
        if let Some(ref existing) = self.db_connection {
            return Ok(Some(existing));
        }
        let flags = OpenFlags::SQLITE_OPEN_NO_MUTEX
            | OpenFlags::SQLITE_OPEN_URI
            | OpenFlags::SQLITE_OPEN_READ_WRITE;
        match open_database_with_flags(
            self.db_path.clone(),
            flags,
            &crate::schema::TabsMigrationLogin,
        ) {
            Ok(conn) => {
                self.db_connection = Some(conn);
                Ok(self.db_connection.as_ref())
            }
            Err(open_database::Error::SqlError(rusqlite::Error::SqliteFailure(code, _)))
                if code.code == rusqlite::ErrorCode::CannotOpen =>
            {
                Ok(None)
            }
            Err(e) => Err(e.into()),
        }
    }

    /// Open and return the DB, creating it if necessary.
    pub fn open_or_create(&mut self) -> Result<&Connection> {
        if let Some(ref existing) = self.db_connection {
            return Ok(existing);
        }
        let flags = OpenFlags::SQLITE_OPEN_NO_MUTEX
            | OpenFlags::SQLITE_OPEN_URI
            | OpenFlags::SQLITE_OPEN_READ_WRITE
            | OpenFlags::SQLITE_OPEN_CREATE;
        let conn = open_database_with_flags(
            self.db_path.clone(),
            flags,
            &crate::schema::TabsMigrationLogin,
        )?;
        self.db_connection = Some(conn);
        Ok(self.db_connection.as_ref().unwrap())
    }

    pub fn update_local_state(&mut self, local_state: Vec<RemoteTab>) {
        self.local_tabs.borrow_mut().replace(local_state);
    }

    pub fn prepare_local_tabs_for_upload(&self) -> Option<Vec<RemoteTab>> {
        if let Some(local_tabs) = self.local_tabs.borrow().as_ref() {
            return Some(
                local_tabs
                    .iter()
                    .cloned()
                    .filter_map(|mut tab| {
                        if tab.url_history.is_empty() || !is_url_syncable(&tab.url_history[0]) {
                            return None;
                        }
                        let mut sanitized_history = Vec::with_capacity(TAB_ENTRIES_LIMIT);
                        for url in tab.url_history {
                            if sanitized_history.len() == TAB_ENTRIES_LIMIT {
                                break;
                            }
                            if is_url_syncable(&url) {
                                sanitized_history.push(url);
                            }
                        }
                        tab.url_history = sanitized_history;
                        Some(tab)
                    })
                    .collect(),
            );
        }
        None
    }

    pub fn get_remote_tabs(&mut self) -> Option<Vec<ClientRemoteTabs>> {
        match self.open_if_exists() {
            Err(e) => {
                error_support::report_error!(
                    "tabs-read-remote",
                    "Failed to read remote tabs: {}",
                    e
                );
                None
            }
            Ok(None) => None,
            Ok(Some(c)) => {
                match c.query_rows_and_then_cached(
                    "SELECT payload FROM tabs",
                    [],
                    |row| -> Result<_> { Ok(serde_json::from_str(&row.get::<_, String>(0)?)?) },
                ) {
                    Ok(crts) => Some(crts),
                    Err(e) => {
                        error_support::report_error!(
                            "tabs-read-remote",
                            "Failed to read database: {}",
                            e
                        );
                        None
                    }
                }
            }
        }
    }
}

impl TabsStorage {
    pub(crate) fn replace_remote_tabs(
        &mut self,
        new_remote_tabs: Vec<ClientRemoteTabs>,
    ) -> Result<()> {
        let connection = self.open_or_create()?;
        let tx = connection.unchecked_transaction()?;
        // delete the world - we rebuild it from scratch every sync.
        tx.execute_batch("DELETE FROM tabs")?;

        for crt in new_remote_tabs {
            tx.execute_cached(
                "INSERT INTO tabs (payload) VALUES (:payload);",
                rusqlite::named_params! {
                    ":payload": serde_json::to_string(&crt).expect("tabs don't fail to serialize"),
                },
            )?;
        }
        tx.commit()?;
        Ok(())
    }

    pub(crate) fn wipe_remote_tabs(&mut self) -> Result<()> {
        if let Some(db) = self.open_if_exists()? {
            db.execute_batch("DELETE FROM tabs")?;
        }
        Ok(())
    }

    pub(crate) fn wipe_local_tabs(&self) {
        self.local_tabs.replace(None);
    }
}

fn is_url_syncable(url: &str) -> bool {
    url.len() <= URI_LENGTH_MAX
        && !(url.starts_with("about:")
            || url.starts_with("resource:")
            || url.starts_with("chrome:")
            || url.starts_with("wyciwyg:")
            || url.starts_with("blob:")
            || url.starts_with("file:")
            || url.starts_with("moz-extension:"))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_is_url_syncable() {
        assert!(is_url_syncable("https://bobo.com"));
        assert!(is_url_syncable("ftp://bobo.com"));
        assert!(!is_url_syncable("about:blank"));
        // XXX - this smells wrong - we should insist on a valid complete URL?
        assert!(is_url_syncable("aboutbobo.com"));
        assert!(!is_url_syncable("file:///Users/eoger/bobo"));
    }

    #[test]
    fn test_open_if_exists_no_file() {
        let dir = tempfile::tempdir().unwrap();
        let db_name = dir.path().join("test_open_for_read_no_file.db");
        let mut storage = TabsStorage::new(db_name.clone());
        assert!(storage.open_if_exists().unwrap().is_none());
        storage.open_or_create().unwrap(); // will have created it.
                                           // make a new storage, but leave the file alone.
        let mut storage = TabsStorage::new(db_name);
        // db file exists, so opening for read should open it.
        assert!(storage.open_if_exists().unwrap().is_some());
    }

    #[test]
    fn test_prepare_local_tabs_for_upload() {
        let mut storage = TabsStorage::new_with_mem_path("test_prepare_local_tabs_for_upload");
        assert_eq!(storage.prepare_local_tabs_for_upload(), None);
        storage.update_local_state(vec![
            RemoteTab {
                title: "".to_owned(),
                url_history: vec!["about:blank".to_owned(), "https://foo.bar".to_owned()],
                icon: None,
                last_used: 0,
            },
            RemoteTab {
                title: "".to_owned(),
                url_history: vec![
                    "https://foo.bar".to_owned(),
                    "about:blank".to_owned(),
                    "about:blank".to_owned(),
                    "about:blank".to_owned(),
                    "about:blank".to_owned(),
                    "about:blank".to_owned(),
                    "about:blank".to_owned(),
                    "about:blank".to_owned(),
                ],
                icon: None,
                last_used: 0,
            },
            RemoteTab {
                title: "".to_owned(),
                url_history: vec![
                    "https://foo.bar".to_owned(),
                    "about:blank".to_owned(),
                    "https://foo2.bar".to_owned(),
                    "https://foo3.bar".to_owned(),
                    "https://foo4.bar".to_owned(),
                    "https://foo5.bar".to_owned(),
                    "https://foo6.bar".to_owned(),
                ],
                icon: None,
                last_used: 0,
            },
            RemoteTab {
                title: "".to_owned(),
                url_history: vec![],
                icon: None,
                last_used: 0,
            },
        ]);
        assert_eq!(
            storage.prepare_local_tabs_for_upload(),
            Some(vec![
                RemoteTab {
                    title: "".to_owned(),
                    url_history: vec!["https://foo.bar".to_owned()],
                    icon: None,
                    last_used: 0,
                },
                RemoteTab {
                    title: "".to_owned(),
                    url_history: vec![
                        "https://foo.bar".to_owned(),
                        "https://foo2.bar".to_owned(),
                        "https://foo3.bar".to_owned(),
                        "https://foo4.bar".to_owned(),
                        "https://foo5.bar".to_owned()
                    ],
                    icon: None,
                    last_used: 0,
                },
            ])
        );
    }

    #[test]
    fn test_old_client_remote_tabs_version() {
        env_logger::try_init().ok();
        // The initial version of ClientRemoteTabs which we persisted looks like:
        let old = serde_json::json!({
            "client_id": "id",
            "client_name": "name",
            "remote_tabs": [
                serde_json::json!({
                    "title": "tab title",
                    "url_history": ["url"],
                    "last_used": 1234,
                }),
            ]
        });

        let dir = tempfile::tempdir().unwrap();
        let db_name = dir.path().join("test_old_client_remote_tabs_version.db");
        let mut storage = TabsStorage::new(db_name);

        let connection = storage.open_or_create().expect("should create");
        connection
            .execute_cached(
                "INSERT INTO tabs (payload) VALUES (:payload);",
                rusqlite::named_params! {
                    ":payload": serde_json::to_string(&old).expect("tabs don't fail to serialize"),
                },
            )
            .expect("should insert");

        // We should be able to read it out.
        let clients = storage.get_remote_tabs().expect("should work");
        assert_eq!(clients.len(), 1, "must be 1 tab");
        let client = &clients[0];
        assert_eq!(client.client_id, "id");
        assert_eq!(client.client_name, "name");
        assert_eq!(client.remote_tabs.len(), 1);
        assert_eq!(client.remote_tabs[0].title, "tab title");
        assert_eq!(client.remote_tabs[0].url_history, vec!["url".to_string()]);
        assert_eq!(client.remote_tabs[0].last_used, 1234);

        // The old version didn't have last_modified - check it is the default.
        assert_eq!(client.last_modified, 0);
    }
}
