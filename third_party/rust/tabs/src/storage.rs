/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// From https://searchfox.org/mozilla-central/rev/ea63a0888d406fae720cf24f4727d87569a8cab5/services/sync/modules/constants.js#75
const URI_LENGTH_MAX: usize = 65536;
// https://searchfox.org/mozilla-central/rev/ea63a0888d406fae720cf24f4727d87569a8cab5/services/sync/modules/engines/tabs.js#8
const TAB_ENTRIES_LIMIT: usize = 5;

// How long we expect a remote command to live. After this time we assume it's
// either been delivered or will not be.
// Matches COMMAND_TTL in close_tabs.rs in fxa-client.
const REMOTE_COMMAND_TTL_MS: u64 = 2 * 24 * 60 * 60 * 1000; // 48 hours.

use crate::error::*;
use crate::schema;
use crate::sync::record::TabsRecord;
use crate::DeviceType;
use crate::{PendingCommand, RemoteCommand, Timestamp};
use rusqlite::{
    types::{FromSql, FromSqlError, FromSqlResult, ToSql, ToSqlOutput, ValueRef},
    Connection, OpenFlags,
};
use serde_derive::{Deserialize, Serialize};
use sql_support::open_database::{self, open_database_with_flags};
use sql_support::ConnExt;
use std::cell::RefCell;
use std::collections::HashMap;
use std::path::{Path, PathBuf};
use sync15::{RemoteClient, ServerTimestamp};
pub type TabsDeviceType = crate::DeviceType;
pub type RemoteTabRecord = RemoteTab;

pub(crate) const TABS_CLIENT_TTL: u32 = 15_552_000; // 180 days, same as CLIENTS_TTL
const FAR_FUTURE: i64 = 4_102_405_200_000; // 2100/01/01
const MAX_PAYLOAD_SIZE: usize = 512 * 1024; // Twice as big as desktop, still smaller than server max (2MB)
const MAX_TITLE_CHAR_LENGTH: usize = 512; // We put an upper limit on title sizes for tabs to reduce memory

#[derive(Clone, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct RemoteTab {
    pub title: String,
    pub url_history: Vec<String>,
    pub icon: Option<String>,
    pub last_used: i64, // In ms.
    pub inactive: bool,
}

#[derive(Clone, Debug)]
pub struct ClientRemoteTabs {
    // The fxa_device_id of the client. *Should not* come from the id in the `clients` collection,
    // because that may or may not be the fxa_device_id (currently, it will not be for desktop
    // records.)
    pub client_id: String,
    pub client_name: String,
    pub device_type: DeviceType,
    pub last_modified: i64,
    pub remote_tabs: Vec<RemoteTab>,
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
            &crate::schema::TabsMigrationLogic,
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
            &crate::schema::TabsMigrationLogic,
        )?;
        self.db_connection = Some(conn);
        Ok(self.db_connection.as_ref().unwrap())
    }

    pub fn update_local_state(&mut self, local_state: Vec<RemoteTab>) {
        self.local_tabs.borrow_mut().replace(local_state);
    }

    // We try our best to fit as many tabs in a payload as possible, this includes
    // limiting the url history entries, title character count and finally drop enough tabs
    // until we have small enough payload that the server will accept
    pub fn prepare_local_tabs_for_upload(&self) -> Option<Vec<RemoteTab>> {
        if let Some(local_tabs) = self.local_tabs.borrow().as_ref() {
            let mut sanitized_tabs: Vec<RemoteTab> = local_tabs
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
                    // Potentially truncate the title to some limit
                    tab.title = slice_up_to(tab.title, MAX_TITLE_CHAR_LENGTH);
                    Some(tab)
                })
                .collect();
            // Sort the tabs so when we trim tabs it's the oldest tabs
            sanitized_tabs.sort_by(|a, b| b.last_used.cmp(&a.last_used));
            // If trimming the tab length failed for some reason, just return the untrimmed tabs
            trim_tabs_length(&mut sanitized_tabs, MAX_PAYLOAD_SIZE);
            return Some(sanitized_tabs);
        }
        None
    }

    pub fn get_remote_tabs(&mut self) -> Option<Vec<ClientRemoteTabs>> {
        let conn = match self.open_if_exists() {
            Err(e) => {
                error_support::report_error!(
                    "tabs-read-remote",
                    "Failed to read remote tabs: {}",
                    e
                );
                return None;
            }
            Ok(None) => return None,
            Ok(Some(conn)) => conn,
        };

        let records: Vec<(TabsRecord, ServerTimestamp)> = match conn.query_rows_and_then_cached(
            "SELECT record, last_modified FROM tabs",
            [],
            |row| -> Result<_> {
                Ok((
                    serde_json::from_str(&row.get::<_, String>(0)?)?,
                    ServerTimestamp(row.get::<_, i64>(1)?),
                ))
            },
        ) {
            Ok(records) => records,
            Err(e) => {
                error_support::report_error!("tabs-read-remote", "Failed to read database: {}", e);
                return None;
            }
        };
        let mut crts: Vec<ClientRemoteTabs> = Vec::new();
        let remote_clients: HashMap<String, RemoteClient> =
            match self.get_meta::<String>(schema::REMOTE_CLIENTS_KEY) {
                Err(e) => {
                    error_support::report_error!(
                        "tabs-read-remote",
                        "Failed to get remote clients: {}",
                        e
                    );
                    return None;
                }
                // We don't return early here since we still store tabs even if we don't
                // "know" about the client it's associated with (incase it becomes available later)
                Ok(None) => HashMap::default(),
                Ok(Some(json)) => serde_json::from_str(&json).unwrap(),
            };
        for (record, last_modified) in records {
            let id = record.id.clone();
            let crt = if let Some(remote_client) = remote_clients.get(&id) {
                ClientRemoteTabs::from_record_with_remote_client(
                    remote_client
                        .fxa_device_id
                        .as_ref()
                        .unwrap_or(&id)
                        .to_owned(),
                    last_modified,
                    remote_client,
                    record,
                )
            } else {
                // A record with a device that's not in our remote clients seems unlikely, but
                // could happen - in most cases though, it will be due to a disconnected client -
                // so we really should consider just dropping it? (Sadly though, it does seem
                // possible it's actually a very recently connected client, so we keep it)
                // We should get rid of this eventually - https://github.com/mozilla/application-services/issues/5199
                log::info!(
                    "Storing tabs from a client that doesn't appear in the devices list: {}",
                    id,
                );
                ClientRemoteTabs::from_record(id, last_modified, record)
            };
            crts.push(crt);
        }
        // Filter out any tabs the user requested to be closed on other devices but those devices
        // have not yet actually closed the tab, so we hide them from the user until such time
        // Should we add a flag here to give the call an option of not doing this?
        let filtered_crts = self.filter_pending_remote_tabs(crts);
        Some(filtered_crts)
    }

    fn filter_pending_remote_tabs(&mut self, crts: Vec<ClientRemoteTabs>) -> Vec<ClientRemoteTabs> {
        let conn = match self.open_if_exists() {
            Err(e) => {
                error_support::report_error!(
                    "tabs-read-remote",
                    "Failed to read remote tabs: {}",
                    e
                );
                return crts;
            }
            Ok(None) => return crts,
            Ok(Some(conn)) => conn,
        };
        let pending_tabs_result: Result<Vec<(String, String)>> = conn.query_rows_and_then_cached(
            "SELECT device_id, url FROM remote_tab_commands",
            [],
            |row| {
                Ok((
                    row.get::<_, String>(0)?, // device_id
                    row.get::<_, String>(1)?, // url
                ))
            },
        );
        // Make a hash map of all urls per client_id that we potentially want to filter
        let pending_closures = match pending_tabs_result {
            Ok(pending_closures) => pending_closures.into_iter().fold(
                HashMap::new(),
                |mut acc: HashMap<String, Vec<String>>, (device_id, url)| {
                    acc.entry(device_id).or_default().push(url);
                    acc
                },
            ),
            Err(e) => {
                error_support::report_error!("tabs-read-remote", "Failed to read database: {}", e);
                return crts;
            }
        };
        // Check if any of the client records that were passed in have urls that the user closed
        // This means that they requested to close those tabs but those devices have not yet got
        // actually closed the tabs
        let filtered_crts: Vec<ClientRemoteTabs> = crts
            .into_iter()
            .map(|mut crt| {
                crt.remote_tabs.retain(|tab| {
                    !pending_closures
                        .get(&crt.client_id)
                        // The top level in the url_history is the "active" tab, which we should use
                        // TODO: probably not the best way to url check
                        .map_or(false, |urls| urls.contains(&tab.url_history[0]))
                });
                crt
            })
            .collect();
        // Return the filtered crts
        filtered_crts
    }

    // Keep DB from growing infinitely since we only ask for records since our last sync
    // and may or may not know about the client it's associated with -- but we could at some point
    // and should start returning those tabs immediately. If that client hasn't been seen in 3 weeks,
    // we remove it until it reconnects
    pub fn remove_stale_clients(&mut self) -> Result<()> {
        let last_sync = self.get_meta::<i64>(schema::LAST_SYNC_META_KEY)?;
        if let Some(conn) = self.open_if_exists()? {
            if let Some(last_sync) = last_sync {
                let client_ttl_ms = (TABS_CLIENT_TTL as i64) * 1000;
                // On desktop, a quick write temporarily sets the last_sync to FAR_FUTURE
                // but if it doesn't set it back to the original (crash, etc) it
                // means we'll most likely trash all our records (as it's more than any TTL we'd ever do)
                // so we need to detect this for now until we have native quick write support
                if last_sync - client_ttl_ms >= 0 && last_sync != (FAR_FUTURE * 1000) {
                    let tx = conn.unchecked_transaction()?;
                    let num_removed = tx.execute_cached(
                        "DELETE FROM tabs WHERE last_modified <= :last_sync - :ttl",
                        rusqlite::named_params! {
                            ":last_sync": last_sync,
                            ":ttl": client_ttl_ms,
                        },
                    )?;
                    log::info!(
                        "removed {} stale clients (threshold was {})",
                        num_removed,
                        last_sync - client_ttl_ms
                    );
                    tx.commit()?;
                }
            }
        }
        Ok(())
    }

    pub(crate) fn replace_remote_tabs(
        &mut self,
        // This is a tuple because we need to know what the server reports
        // as the last time a record was modified
        new_remote_tabs: &Vec<(TabsRecord, ServerTimestamp)>,
    ) -> Result<()> {
        let connection = self.open_or_create()?;
        let tx = connection.unchecked_transaction()?;

        // For tabs it's fine if we override the existing tabs for a remote
        // there can only ever be one record for each client
        for remote_tab in new_remote_tabs {
            let record = &remote_tab.0;
            let last_modified = remote_tab.1;
            log::info!(
                "inserting tab for device {}, last modified at {}",
                record.id,
                last_modified.as_millis()
            );
            tx.execute_cached(
                "INSERT OR REPLACE INTO tabs (guid, record, last_modified) VALUES (:guid, :record, :last_modified);",
                rusqlite::named_params! {
                    ":guid": &record.id,
                    ":record": serde_json::to_string(&record).expect("tabs don't fail to serialize"),
                    ":last_modified": last_modified.as_millis()
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

    pub(crate) fn put_meta(&mut self, key: &str, value: &dyn ToSql) -> Result<()> {
        let db = self.open_or_create()?;
        db.execute_cached(
            "REPLACE INTO moz_meta (key, value) VALUES (:key, :value)",
            &[(":key", &key as &dyn ToSql), (":value", value)],
        )?;
        Ok(())
    }

    pub(crate) fn get_meta<T: FromSql>(&mut self, key: &str) -> Result<Option<T>> {
        match self.open_if_exists() {
            Ok(Some(db)) => {
                let res = db.try_query_one(
                    "SELECT value FROM moz_meta WHERE key = :key",
                    &[(":key", &key)],
                    true,
                )?;
                Ok(res)
            }
            Err(e) => Err(e),
            Ok(None) => Ok(None),
        }
    }

    pub(crate) fn delete_meta(&mut self, key: &str) -> Result<()> {
        if let Some(db) = self.open_if_exists()? {
            db.execute_cached("DELETE FROM moz_meta WHERE key = :key", &[(":key", &key)])?;
        }
        Ok(())
    }
}

// Implementations related to storage of remotely closing remote tabs.
// We should probably split this module!
impl TabsStorage {
    /// Store tabs that we requested to close on other devices but
    /// not yet executed on target device, other calls like getAll()
    /// will check against this table to filter out any urls
    pub fn add_remote_tab_command(
        &mut self,
        device_id: &str,
        command: &RemoteCommand,
    ) -> Result<bool> {
        self.add_remote_tab_command_at(device_id, command, Timestamp::now())
    }

    pub fn add_remote_tab_command_at(
        &mut self,
        device_id: &str,
        command: &RemoteCommand,
        time_requested: Timestamp,
    ) -> Result<bool> {
        let connection = self.open_or_create()?;
        let RemoteCommand::CloseTab { url } = command;
        log::info!("Adding remote command for {device_id} at {time_requested}");
        log::trace!("command is {command:?}");
        // tx maybe not needed for single write?
        let tx = connection.unchecked_transaction()?;
        let changes = tx.execute_cached(
            "INSERT OR IGNORE INTO remote_tab_commands
                (device_id, command, url, time_requested, time_sent)
            VALUES (:device_id, :command, :url, :time_requested, null)",
            rusqlite::named_params! {
                ":device_id": &device_id,
                ":url": url,
                ":time_requested": time_requested,
                ":command": command.as_ref(),
            },
        )?;
        tx.commit()?;
        Ok(changes != 0)
    }

    pub fn remove_remote_tab_command(
        &mut self,
        device_id: &str,
        command: &RemoteCommand,
    ) -> Result<bool> {
        let connection = self.open_or_create()?;
        let RemoteCommand::CloseTab { url } = command;
        log::info!("removing remote tab close details: client={device_id}");
        let tx = connection.unchecked_transaction()?;
        let changes = tx.execute_cached(
            "DELETE FROM remote_tab_commands
             WHERE device_id = :device_id AND command = :command AND url = :url;",
            rusqlite::named_params! {
                ":device_id": &device_id,
                ":url": url,
                ":command": command.as_ref(),
            },
        )?;
        tx.commit()?;
        Ok(changes != 0)
    }

    pub fn get_unsent_commands(&mut self) -> Result<Vec<PendingCommand>> {
        self.do_get_pending_commands("WHERE time_sent IS NULL")
    }

    fn do_get_pending_commands(&mut self, where_clause: &str) -> Result<Vec<PendingCommand>> {
        let Some(conn) = self.open_if_exists()? else {
            return Ok(Vec::new());
        };
        let records: Vec<Option<PendingCommand>> = match conn.query_rows_and_then_cached(
            &format!(
                "SELECT device_id, command, url, time_requested, time_sent
                    FROM remote_tab_commands
                    {where_clause}
                    ORDER BY time_requested
                    LIMIT 1000 -- sue me!"
            ),
            [],
            |row| -> Result<_> {
                // overly cautious I guess - ignore bad enum values rather than failing
                let command = match row.get::<_, CommandKind>(1) {
                    Ok(c) => c,
                    Err(e) => {
                        log::error!(
                            "do_get_pending_commands: ignoring error fetching command: {e:?}"
                        );
                        return Ok(None);
                    }
                };
                Ok(Some(match command {
                    CommandKind::CloseTab => PendingCommand {
                        device_id: row.get::<_, String>(0)?,
                        command: RemoteCommand::CloseTab {
                            url: row.get::<_, String>(2)?,
                        },
                        time_requested: row.get::<_, Timestamp>(3)?,
                        time_sent: row.get::<_, Option<Timestamp>>(4)?,
                    },
                }))
            },
        ) {
            Ok(records) => records,
            Err(e) => {
                error_support::report_error!("tabs-get_unsent", "Failed to read database: {}", e);
                return Ok(Vec::new());
            }
        };

        Ok(records.into_iter().flatten().collect())
    }

    pub fn set_pending_command_sent(&mut self, command: &PendingCommand) -> Result<bool> {
        let connection = self.open_or_create()?;
        let RemoteCommand::CloseTab { url } = &command.command;
        log::info!("setting remote tab sent: client={}", command.device_id);
        log::trace!("command: {command:?}");
        let tx = connection.unchecked_transaction()?;
        let ts = Timestamp::now();
        let changes = tx.execute_cached(
            "UPDATE remote_tab_commands
             SET time_sent = :ts
             WHERE device_id = :device_id AND command = :command AND url = :url;",
            rusqlite::named_params! {
                ":command": command.command.as_ref(),
                ":device_id": &command.device_id,
                ":url": url,
                ":ts": &ts,
            },
        )?;
        tx.commit()?;
        Ok(changes != 0)
    }

    // Remove any pending tabs that are 24hrs older than the last time that client has synced
    // Or that client's incoming tabs does not have those tabs anymore
    pub fn remove_old_pending_closures(
        &mut self,
        // This is a tuple because we need to know what the server reports
        // as the last time a record was modified
        new_remote_tabs: &[(TabsRecord, ServerTimestamp)],
    ) -> Result<()> {
        // we need to load our map of client-id -> RemoteClient so we can use the
        // fxa device ID and not the sync client id.
        let remote_clients: HashMap<String, RemoteClient> = {
            match self.get_meta::<String>(schema::REMOTE_CLIENTS_KEY)? {
                None => HashMap::default(),
                Some(json) => serde_json::from_str(&json).unwrap(),
            }
        };

        let conn = self.open_or_create()?;
        let tx = conn.unchecked_transaction()?;

        // Insert new remote tabs into a temporary table
        conn.execute(
            "CREATE TEMP TABLE if not exists new_remote_tabs (device_id TEXT, url TEXT)",
            [],
        )?;
        conn.execute("DELETE FROM new_remote_tabs", [])?; // Clear previous entries

        for (record, _) in new_remote_tabs.iter() {
            let fxa_id = remote_clients
                .get(&record.id)
                .and_then(|r| r.fxa_device_id.as_ref())
                .unwrap_or(&record.id);
            if let Some(url) = record.tabs.first().and_then(|tab| tab.url_history.first()) {
                conn.execute(
                    "INSERT INTO new_remote_tabs (device_id, url) VALUES (?, ?)",
                    rusqlite::params![fxa_id, url],
                )?;
            }
        }

        // Delete entries from pending closures that do not exist in the new remote tabs
        let delete_sql = "
         DELETE FROM remote_tab_commands
         WHERE NOT EXISTS (
             SELECT 1 FROM new_remote_tabs
             WHERE new_remote_tabs.device_id = remote_tab_commands.device_id
             AND :command_close_tab = remote_tab_commands.command
             AND new_remote_tabs.url = remote_tab_commands.url
         )";
        conn.execute(
            delete_sql,
            rusqlite::named_params! {
                ":command_close_tab": CommandKind::CloseTab,
            },
        )?;

        log::info!(
            "deleted {} pending tab closures because they were not in the new tabs",
            conn.changes()
        );

        // Anything that couldn't be removed above and is older than 24 hours
        // is assumed not closeable and we can remove it from the list
        let sql = format!("
            DELETE FROM remote_tab_commands
            WHERE device_id IN (
                SELECT guid FROM tabs
            ) AND (SELECT last_modified FROM tabs WHERE guid = device_id) - time_requested >= {REMOTE_COMMAND_TTL_MS}
        ");
        tx.execute_cached(&sql, [])?;
        log::info!("deleted {} records because they timed out", conn.changes());

        // Commit changes and clean up temp
        tx.commit()?;
        conn.execute("DROP TABLE new_remote_tabs", [])?;
        Ok(())
    }
}

// Simple enum for the DB.
#[derive(Debug, Copy, Clone)]
#[repr(u8)]
enum CommandKind {
    CloseTab = 0,
}

impl AsRef<CommandKind> for RemoteCommand {
    // Required method
    fn as_ref(&self) -> &CommandKind {
        match self {
            RemoteCommand::CloseTab { .. } => &CommandKind::CloseTab,
        }
    }
}

impl FromSql for CommandKind {
    fn column_result(value: ValueRef<'_>) -> FromSqlResult<Self> {
        Ok(match value.as_i64()? {
            0 => CommandKind::CloseTab,
            _ => return Err(FromSqlError::InvalidType),
        })
    }
}

impl ToSql for CommandKind {
    fn to_sql(&self) -> rusqlite::Result<ToSqlOutput<'_>> {
        Ok(ToSqlOutput::from(*self as u8))
    }
}

// Trim the amount of tabs in a list to fit the specified memory size
fn trim_tabs_length(tabs: &mut Vec<RemoteTab>, payload_size_max_bytes: usize) {
    // Ported from https://searchfox.org/mozilla-central/rev/84fb1c4511312a0b9187f647d90059e3a6dd27f8/services/sync/modules/util.sys.mjs#422
    // See bug 535326 comment 8 for an explanation of the estimation
    let max_serialized_size = (payload_size_max_bytes / 4) * 3 - 1500;
    let size = compute_serialized_size(tabs);
    if size > max_serialized_size {
        // Estimate a little more than the direct fraction to maximize packing
        let cutoff = (tabs.len() * max_serialized_size) / size;
        tabs.truncate(cutoff);

        // Keep dropping off the last entry until the data fits.
        while compute_serialized_size(tabs) > max_serialized_size {
            tabs.pop();
        }
    }
}

fn compute_serialized_size(v: &Vec<RemoteTab>) -> usize {
    serde_json::to_string(v).unwrap_or_default().len()
}

// Similar to places/utils.js
// This method ensures we safely truncate a string up to a certain max_len while
// respecting char bounds to prevent rust panics. If we do end up truncating, we
// append an ellipsis to the string
pub fn slice_up_to(s: String, max_len: usize) -> String {
    if max_len >= s.len() {
        return s;
    }

    let ellipsis = '\u{2026}';
    // Ensure we leave space for the ellipsis while still being under the max
    let mut idx = max_len - ellipsis.len_utf8();
    while !s.is_char_boundary(idx) {
        idx -= 1;
    }
    let mut new_str = s[..idx].to_string();
    new_str.push(ellipsis);
    new_str
}

// Try to keep in sync with https://searchfox.org/mozilla-central/rev/2ad13433da20a0749e1e9a10ec0ab49b987c2c8e/modules/libpref/init/all.js#3927
fn is_url_syncable(url: &str) -> bool {
    url.len() <= URI_LENGTH_MAX
        && !(url.starts_with("about:")
            || url.starts_with("resource:")
            || url.starts_with("chrome:")
            || url.starts_with("wyciwyg:")
            || url.starts_with("blob:")
            || url.starts_with("file:")
            || url.starts_with("moz-extension:")
            || url.starts_with("data:"))
}

#[cfg(test)]
mod tests {
    use std::time::Duration;

    use super::*;
    use crate::{sync::record::TabsRecordTab, PendingCommand};

    impl RemoteCommand {
        fn close_tab(url: &str) -> Self {
            RemoteCommand::CloseTab {
                url: url.to_string(),
            }
        }
    }

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
        env_logger::try_init().ok();
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
    fn test_tabs_meta() {
        env_logger::try_init().ok();
        let dir = tempfile::tempdir().unwrap();
        let db_name = dir.path().join("test_tabs_meta.db");
        let mut db = TabsStorage::new(db_name);
        let test_key = "TEST KEY A";
        let test_value = "TEST VALUE A";
        let test_key2 = "TEST KEY B";
        let test_value2 = "TEST VALUE B";

        // should automatically make the DB if one doesn't exist
        db.put_meta(test_key, &test_value).unwrap();
        db.put_meta(test_key2, &test_value2).unwrap();

        let retrieved_value: String = db.get_meta(test_key).unwrap().expect("test value");
        let retrieved_value2: String = db.get_meta(test_key2).unwrap().expect("test value 2");

        assert_eq!(retrieved_value, test_value);
        assert_eq!(retrieved_value2, test_value2);

        // check that the value of an existing key can be updated
        let test_value3 = "TEST VALUE C";
        db.put_meta(test_key, &test_value3).unwrap();

        let retrieved_value3: String = db.get_meta(test_key).unwrap().expect("test value 3");

        assert_eq!(retrieved_value3, test_value3);

        // check that a deleted key is not retrieved
        db.delete_meta(test_key).unwrap();
        let retrieved_value4: Option<String> = db.get_meta(test_key).unwrap();
        assert!(retrieved_value4.is_none());
    }

    #[test]
    fn test_prepare_local_tabs_for_upload() {
        env_logger::try_init().ok();
        let mut storage = TabsStorage::new_with_mem_path("test_prepare_local_tabs_for_upload");
        assert_eq!(storage.prepare_local_tabs_for_upload(), None);
        storage.update_local_state(vec![
            RemoteTab {
                url_history: vec!["about:blank".to_owned(), "https://foo.bar".to_owned()],
                ..Default::default()
            },
            RemoteTab {
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
                ..Default::default()
            },
            RemoteTab {
                url_history: vec![
                    "https://foo.bar".to_owned(),
                    "about:blank".to_owned(),
                    "https://foo2.bar".to_owned(),
                    "https://foo3.bar".to_owned(),
                    "https://foo4.bar".to_owned(),
                    "https://foo5.bar".to_owned(),
                    "https://foo6.bar".to_owned(),
                ],
                ..Default::default()
            },
            RemoteTab {
                ..Default::default()
            },
        ]);
        assert_eq!(
            storage.prepare_local_tabs_for_upload(),
            Some(vec![
                RemoteTab {
                    url_history: vec!["https://foo.bar".to_owned()],
                    ..Default::default()
                },
                RemoteTab {
                    url_history: vec![
                        "https://foo.bar".to_owned(),
                        "https://foo2.bar".to_owned(),
                        "https://foo3.bar".to_owned(),
                        "https://foo4.bar".to_owned(),
                        "https://foo5.bar".to_owned()
                    ],
                    ..Default::default()
                },
            ])
        );
    }
    #[test]
    fn test_trimming_tab_title() {
        env_logger::try_init().ok();
        let mut storage = TabsStorage::new_with_mem_path("test_prepare_local_tabs_for_upload");
        assert_eq!(storage.prepare_local_tabs_for_upload(), None);
        storage.update_local_state(vec![RemoteTab {
            title: "a".repeat(MAX_TITLE_CHAR_LENGTH + 10), // Fill a string more than max
            url_history: vec!["https://foo.bar".to_owned()],
            ..Default::default()
        }]);
        let ellipsis_char = '\u{2026}';
        let mut truncated_title = "a".repeat(MAX_TITLE_CHAR_LENGTH - ellipsis_char.len_utf8());
        truncated_title.push(ellipsis_char);
        assert_eq!(
            storage.prepare_local_tabs_for_upload(),
            Some(vec![
                // title trimmed to 50 characters
                RemoteTab {
                    title: truncated_title, // title was trimmed to only max char length
                    url_history: vec!["https://foo.bar".to_owned()],
                    ..Default::default()
                },
            ])
        );
    }
    #[test]
    fn test_utf8_safe_title_trim() {
        env_logger::try_init().ok();
        let mut storage = TabsStorage::new_with_mem_path("test_prepare_local_tabs_for_upload");
        assert_eq!(storage.prepare_local_tabs_for_upload(), None);
        storage.update_local_state(vec![
            RemoteTab {
                title: "😍".repeat(MAX_TITLE_CHAR_LENGTH + 10), // Fill a string more than max
                url_history: vec!["https://foo.bar".to_owned()],
                ..Default::default()
            },
            RemoteTab {
                title: "を".repeat(MAX_TITLE_CHAR_LENGTH + 5), // Fill a string more than max
                url_history: vec!["https://foo_jp.bar".to_owned()],
                ..Default::default()
            },
        ]);
        let ellipsis_char = '\u{2026}';
        // (MAX_TITLE_CHAR_LENGTH - ellipsis / "😍" bytes)
        let mut truncated_title = "😍".repeat(127);
        // (MAX_TITLE_CHAR_LENGTH - ellipsis / "を" bytes)
        let mut truncated_jp_title = "を".repeat(169);
        truncated_title.push(ellipsis_char);
        truncated_jp_title.push(ellipsis_char);
        let remote_tabs = storage.prepare_local_tabs_for_upload().unwrap();
        assert_eq!(
            remote_tabs,
            vec![
                RemoteTab {
                    title: truncated_title, // title was trimmed to only max char length
                    url_history: vec!["https://foo.bar".to_owned()],
                    ..Default::default()
                },
                RemoteTab {
                    title: truncated_jp_title, // title was trimmed to only max char length
                    url_history: vec!["https://foo_jp.bar".to_owned()],
                    ..Default::default()
                },
            ]
        );
        // We should be less than max
        assert!(remote_tabs[0].title.chars().count() <= MAX_TITLE_CHAR_LENGTH);
        assert!(remote_tabs[1].title.chars().count() <= MAX_TITLE_CHAR_LENGTH);
    }
    #[test]
    fn test_trim_tabs_length() {
        env_logger::try_init().ok();
        let mut storage = TabsStorage::new_with_mem_path("test_prepare_local_tabs_for_upload");
        assert_eq!(storage.prepare_local_tabs_for_upload(), None);
        let mut too_many_tabs: Vec<RemoteTab> = Vec::new();
        for n in 1..5000 {
            too_many_tabs.push(RemoteTab {
                title: "aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa" //50 characters
                    .to_owned(),
                url_history: vec![format!("https://foo{}.bar", n)],
                ..Default::default()
            });
        }
        let tabs_mem_size = compute_serialized_size(&too_many_tabs);
        // ensure we are definitely over the payload limit
        assert!(tabs_mem_size > MAX_PAYLOAD_SIZE);
        // Add our over-the-limit tabs to the local state
        storage.update_local_state(too_many_tabs.clone());
        // prepare_local_tabs_for_upload did the trimming we needed to get under payload size
        let tabs_to_upload = &storage.prepare_local_tabs_for_upload().unwrap();
        assert!(compute_serialized_size(tabs_to_upload) <= MAX_PAYLOAD_SIZE);
    }
    // Helper struct to model what's stored in the DB
    struct TabsSQLRecord {
        guid: String,
        record: TabsRecord,
        last_modified: i64,
    }
    #[test]
    fn test_remove_stale_clients() {
        env_logger::try_init().ok();
        let dir = tempfile::tempdir().unwrap();
        let db_name = dir.path().join("test_remove_stale_clients.db");
        let mut storage = TabsStorage::new(db_name);
        storage.open_or_create().unwrap();
        assert!(storage.open_if_exists().unwrap().is_some());

        let records = vec![
            TabsSQLRecord {
                guid: "device-1".to_string(),
                record: TabsRecord {
                    id: "device-1".to_string(),
                    client_name: "Device #1".to_string(),
                    tabs: vec![TabsRecordTab {
                        title: "the title".to_string(),
                        url_history: vec!["https://mozilla.org/".to_string()],
                        icon: Some("https://mozilla.org/icon".to_string()),
                        last_used: 1643764207000,
                        ..Default::default()
                    }],
                },
                last_modified: 1643764207000,
            },
            TabsSQLRecord {
                guid: "device-outdated".to_string(),
                record: TabsRecord {
                    id: "device-outdated".to_string(),
                    client_name: "Device outdated".to_string(),
                    tabs: vec![TabsRecordTab {
                        title: "the title".to_string(),
                        url_history: vec!["https://mozilla.org/".to_string()],
                        icon: Some("https://mozilla.org/icon".to_string()),
                        last_used: 1643764207000,
                        ..Default::default()
                    }],
                },
                last_modified: 1443764207000, // old
            },
        ];
        let db = storage.open_if_exists().unwrap().unwrap();
        for record in records {
            db.execute(
                "INSERT INTO tabs (guid, record, last_modified) VALUES (:guid, :record, :last_modified);",
                rusqlite::named_params! {
                    ":guid": &record.guid,
                    ":record": serde_json::to_string(&record.record).unwrap(),
                    ":last_modified": &record.last_modified,
                },
            ).unwrap();
        }
        // pretend we just synced
        let last_synced = 1643764207000_i64;
        storage
            .put_meta(schema::LAST_SYNC_META_KEY, &last_synced)
            .unwrap();
        storage.remove_stale_clients().unwrap();

        let remote_tabs = storage.get_remote_tabs().unwrap();
        // We should've removed the outdated device
        assert_eq!(remote_tabs.len(), 1);
        // Assert the correct record is still being returned
        assert_eq!(remote_tabs[0].client_id, "device-1");
    }

    fn pending_url_command(device_id: &str, url: &str, ts: Timestamp) -> PendingCommand {
        PendingCommand {
            device_id: device_id.to_string(),
            command: RemoteCommand::CloseTab {
                url: url.to_string(),
            },
            time_requested: ts,
            time_sent: None,
        }
    }

    #[test]
    fn test_add_pending_dupe_simple() {
        env_logger::try_init().ok();
        let mut storage = TabsStorage::new_with_mem_path("test_add_pending_dupe_simple");
        let command = RemoteCommand::close_tab("https://example1.com");
        // returns a bool to say if it's new or not.
        assert!(storage
            .add_remote_tab_command("device-1", &command)
            .expect("should work"));
        assert!(!storage
            .add_remote_tab_command("device-1", &command)
            .expect("should work"));
        assert!(storage
            .remove_remote_tab_command("device-1", &command)
            .expect("should work"));
        assert!(storage
            .add_remote_tab_command("device-1", &command)
            .expect("should work"));
    }

    #[test]
    fn test_add_pending_remote_close() {
        env_logger::try_init().ok();
        let mut storage = TabsStorage::new_with_mem_path("test_add_pending_remote_close");
        storage.open_or_create().unwrap();
        assert!(storage.open_if_exists().unwrap().is_some());

        let now = Timestamp::now();
        let earliest = now.checked_sub(Duration::from_millis(1)).unwrap();
        let later = now.checked_add(Duration::from_millis(1)).unwrap();
        let latest = now.checked_add(Duration::from_millis(2)).unwrap();
        // The tabs requested to to be closed. We'll insert them in the "wrong" order
        // relative to their time-stamp.
        storage
            .add_remote_tab_command_at(
                "device-1",
                &RemoteCommand::close_tab("https://example1.com"),
                latest,
            )
            .expect("should work");
        storage
            .add_remote_tab_command_at(
                "device-1",
                &RemoteCommand::close_tab("https://example2.com"),
                earliest,
            )
            .expect("should work");
        storage
            .add_remote_tab_command_at(
                "device-2",
                &RemoteCommand::close_tab("https://example2.com"),
                now,
            )
            .expect("should work");
        storage
            .add_remote_tab_command_at(
                "device-2",
                &RemoteCommand::close_tab("https://example3.com"),
                later,
            )
            .expect("should work");

        let got = storage.get_unsent_commands().unwrap();

        assert_eq!(got.len(), 4);
        assert_eq!(
            got,
            vec![
                pending_url_command("device-1", "https://example2.com", earliest),
                pending_url_command("device-2", "https://example2.com", now),
                pending_url_command("device-2", "https://example3.com", later),
                pending_url_command("device-1", "https://example1.com", latest),
            ]
        );
    }

    #[test]
    fn test_remote_tabs_filters_pending_closures() {
        env_logger::try_init().ok();
        let mut storage =
            TabsStorage::new_with_mem_path("test_remote_tabs_filters_pending_closures");
        let records = vec![
            TabsSQLRecord {
                guid: "device-1".to_string(),
                record: TabsRecord {
                    id: "device-1".to_string(),
                    client_name: "Device #1".to_string(),
                    tabs: vec![TabsRecordTab {
                        title: "the title".to_string(),
                        url_history: vec!["https://mozilla.org/".to_string()],
                        icon: Some("https://mozilla.org/icon".to_string()),
                        last_used: 1711929600015, // 4/1/2024
                        ..Default::default()
                    }],
                },
                last_modified: 1711929600015, // 4/1/2024
            },
            TabsSQLRecord {
                guid: "device-2".to_string(),
                record: TabsRecord {
                    id: "device-2".to_string(),
                    client_name: "Another device".to_string(),
                    tabs: vec![
                        TabsRecordTab {
                            title: "the title".to_string(),
                            url_history: vec!["https://mozilla.org/".to_string()],
                            icon: Some("https://mozilla.org/icon".to_string()),
                            last_used: 1711929600015, // 4/1/2024
                            ..Default::default()
                        },
                        TabsRecordTab {
                            title: "the title".to_string(),
                            url_history: vec![
                                "https://example.com/".to_string(),
                                "https://example1.com/".to_string(),
                            ],
                            icon: None,
                            last_used: 1711929600015, // 4/1/2024
                            ..Default::default()
                        },
                        TabsRecordTab {
                            title: "the title".to_string(),
                            url_history: vec!["https://example1.com/".to_string()],
                            icon: None,
                            last_used: 1711929600015, // 4/1/2024
                            ..Default::default()
                        },
                    ],
                },
                last_modified: 1711929600015, // 4/1/2024
            },
        ];

        let db = storage.open_if_exists().unwrap().unwrap();
        for record in records {
            db.execute(
                "INSERT INTO tabs (guid, record, last_modified) VALUES (:guid, :record, :last_modified);",
                rusqlite::named_params! {
                    ":guid": &record.guid,
                    ":record": serde_json::to_string(&record.record).unwrap(),
                    ":last_modified": &record.last_modified,
                },
            ).unwrap();
        }

        // Some tabs were requested to be closed
        storage
            .add_remote_tab_command(
                "device-1",
                &RemoteCommand::close_tab("https://mozilla.org/"),
            )
            .unwrap();
        storage
            .add_remote_tab_command(
                "device-2",
                &RemoteCommand::close_tab("https://example.com/"),
            )
            .unwrap();
        storage
            .add_remote_tab_command(
                "device-2",
                &RemoteCommand::close_tab("https://example1.com/"),
            )
            .unwrap();

        let remote_tabs = storage.get_remote_tabs().unwrap();

        assert_eq!(remote_tabs.len(), 2);

        // Device 1 had only 1 tab synced, we remotely closed it, so we expect no tabs
        assert_eq!(remote_tabs[0].client_id, "device-1");
        assert_eq!(remote_tabs[0].remote_tabs.len(), 0);

        // Device 2 had 3 tabs open and we remotely closed 2, so we expect 1 tab returned
        assert_eq!(remote_tabs[1].client_id, "device-2");
        assert_eq!(remote_tabs[1].remote_tabs.len(), 1);
        assert_eq!(
            remote_tabs[1].remote_tabs[0],
            RemoteTab {
                title: "the title".to_string(),
                url_history: vec!["https://mozilla.org/".to_string()],
                icon: Some("https://mozilla.org/icon".to_string()),
                last_used: 1711929600015000, //server time is ns, so 1000 bigger than local.
                ..Default::default()
            }
        );
    }

    #[test]
    fn test_remove_old_pending_closures_timed_removal() {
        env_logger::try_init().ok();
        let mut storage =
            TabsStorage::new_with_mem_path("test_remove_old_pending_closures_timed_removal");

        let now = Timestamp::now();
        let older = now
            .checked_sub(Duration::from_millis(REMOTE_COMMAND_TTL_MS))
            .unwrap();

        {
            let db = storage.open_if_exists().unwrap().unwrap();

            // We manually insert two devices, one that hasn't updated in awhile and one that's
            // updated recently
            db.execute(
                "INSERT INTO tabs (guid, record, last_modified) VALUES ('device-synced', '', :now);",
                rusqlite::named_params! {
                    ":now" : now,
                },
            )
            .unwrap();

            db.execute(
                "INSERT INTO tabs (guid, record, last_modified) VALUES ('device-not-synced', '', :old);",
                    rusqlite::named_params! {
                        ":old" : older,
                    },
            ).unwrap();
        }
        // We also manually insert some pending remote tab closures, we specifically add a recent one
        // and one that is 48hrs older since that device updated, which should get removed
        storage
            .add_remote_tab_command_at(
                "device-synced",
                &RemoteCommand::close_tab("https://example.com"),
                older,
            )
            .unwrap();

        storage
            .add_remote_tab_command_at(
                "device-not-synced",
                &RemoteCommand::close_tab("https://example2.com"),
                now,
            )
            .unwrap();

        {
            let db = storage.open_if_exists().unwrap().unwrap();

            // Verify we actually have 2 pending closures
            let before_count: i64 = db
                .query_one("SELECT COUNT(*) FROM remote_tab_commands")
                .unwrap();
            assert_eq!(before_count, 2);
        }
        // "incoming" records from other devices
        let new_records = vec![(
            TabsRecord {
                id: "device-not-synced".to_string(),
                client_name: "".to_string(),
                tabs: vec![TabsRecordTab {
                    url_history: vec!["https://example2.com".to_string()],
                    ..Default::default()
                }],
            },
            ServerTimestamp::from_millis(now.as_millis_i64()),
        )];
        // Cleanup old pending closures
        storage.remove_old_pending_closures(&new_records).unwrap();

        let reopen_db = storage.open_if_exists().unwrap().unwrap();
        let after_count: i64 = reopen_db
            .query_one("SELECT COUNT(*) FROM remote_tab_commands")
            .unwrap();
        assert_eq!(after_count, 1);

        let remaining_device_id: String = reopen_db
            .query_one("SELECT device_id FROM remote_tab_commands")
            .unwrap();

        // Only the device that still hasn't synced keeps
        assert_eq!(remaining_device_id, "device-not-synced");
    }
    #[test]
    fn test_remove_old_pending_closures_no_tab_removal() {
        env_logger::try_init().ok();
        let mut storage =
            TabsStorage::new_with_mem_path("test_remove_old_pending_closures_no_tab_removal");
        let db = storage.open_if_exists().unwrap().unwrap();

        let now_ms: u64 = Timestamp::now().as_millis();

        // Set up the initial state with tabs that have been synced recently
        db.execute(
            "INSERT INTO tabs (guid, record, last_modified) VALUES ('device-recent', '', :now);",
            rusqlite::named_params! {
                ":now": now_ms,
            },
        )
        .unwrap();

        // Insert pending closures for a device
        db.execute(
        "INSERT INTO remote_tab_commands (device_id, command, url, time_requested) VALUES (:device_id, :command, :url, :time_requested)",
        rusqlite::named_params! {
            ":command": CommandKind::CloseTab,
            ":device_id": "device-recent",
            ":url": "https://example.com",
            ":time_requested": now_ms,
        },
    ).unwrap();

        db.execute(
        "INSERT INTO remote_tab_commands (device_id, command, url, time_requested) VALUES (:device_id, :command, :url, :time_requested)",
        rusqlite::named_params! {
            ":command": CommandKind::CloseTab,
            ":device_id": "device-recent",
            ":url": "https://old-url.com",
            ":time_requested": now_ms,
        },
    ).unwrap();

        // Verify initial state has 2 pending closures
        let before_count: i64 = db
            .query_row("SELECT COUNT(*) FROM remote_tab_commands", [], |row| {
                row.get(0)
            })
            .unwrap();
        assert_eq!(before_count, 2);

        // Simulate incoming data that no longer includes one of the URLs
        let new_records = vec![(
            TabsRecord {
                id: "device-recent".to_string(),
                client_name: "".to_string(),
                tabs: vec![TabsRecordTab {
                    url_history: vec!["https://example.com".to_string()],
                    ..Default::default()
                }],
            },
            ServerTimestamp::default(),
        )];

        // Perform the cleanup
        storage.remove_old_pending_closures(&new_records).unwrap();

        // need to reopen db to avoid mutable errors
        let reopen_db = storage.open_if_exists().unwrap().unwrap();
        // Check results after cleanup
        let after_count: i64 = reopen_db
            .query_row("SELECT COUNT(*) FROM remote_tab_commands", [], |row| {
                row.get(0)
            })
            .unwrap();
        assert_eq!(after_count, 1); // Only one entry should remain

        let remaining_url: String = reopen_db
            .query_row("SELECT url FROM remote_tab_commands", [], |row| row.get(0))
            .unwrap();

        assert_eq!(remaining_url, "https://example.com"); // The URL still present in new_records should remain
    }

    #[test]
    fn test_remove_pending_command() {
        env_logger::try_init().ok();
        let mut storage = TabsStorage::new_with_mem_path("test_remove_pending_command");
        storage.open_or_create().unwrap();
        assert!(storage.open_if_exists().unwrap().is_some());

        storage
            .add_remote_tab_command(
                "device-1",
                &RemoteCommand::close_tab("https://example1.com"),
            )
            .expect("should work");

        assert_eq!(storage.get_unsent_commands().unwrap().len(), 1);
        assert!(!storage
            .remove_remote_tab_command(
                "no-devce",
                &RemoteCommand::close_tab("https://example1.com"),
            )
            .unwrap());
        assert_eq!(storage.get_unsent_commands().unwrap().len(), 1);

        assert!(!storage
            .remove_remote_tab_command(
                "device-1",
                &RemoteCommand::close_tab("https://example9.com"),
            )
            .unwrap());
        assert_eq!(storage.get_unsent_commands().unwrap().len(), 1);

        assert!(storage
            .remove_remote_tab_command(
                "device-1",
                &RemoteCommand::close_tab("https://example1.com"),
            )
            .unwrap());
        assert_eq!(storage.get_unsent_commands().unwrap().len(), 0);
    }

    #[test]
    fn test_sent_command() {
        env_logger::try_init().ok();
        let mut storage = TabsStorage::new_with_mem_path("test_sent_command");
        let command = RemoteCommand::close_tab("https://example1.com");
        storage
            .add_remote_tab_command("device-1", &command)
            .expect("should work");

        assert_eq!(storage.get_unsent_commands().unwrap().len(), 1);
        let pending_command = PendingCommand {
            device_id: "device-1".to_string(),
            command: command.clone(),
            time_requested: Timestamp::now(),
            time_sent: None,
        };
        assert!(storage.set_pending_command_sent(&pending_command).unwrap());
        assert_eq!(storage.get_unsent_commands().unwrap().len(), 0);
        // but can't re-add it because it's still alive.
        assert!(!storage
            .add_remote_tab_command("device-1", &command)
            .unwrap());
        // can remove it.
        assert!(storage
            .remove_remote_tab_command("device-1", &command)
            .unwrap());
        // now can re-add it.
        assert!(storage
            .add_remote_tab_command("device-1", &command)
            .unwrap());
        assert_eq!(storage.get_unsent_commands().unwrap().len(), 1);
    }
}
