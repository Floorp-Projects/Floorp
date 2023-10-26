/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use std::{
    collections::BTreeMap,
    path::{Path, PathBuf},
};

use once_cell::sync::OnceCell;
use remote_settings::{
    self, GetItemsOptions, RemoteSettingsConfig, RemoteSettingsRecord, SortOrder,
};
use rusqlite::{
    types::{FromSql, ToSqlOutput},
    ToSql,
};
use serde::{de::DeserializeOwned, Deserialize, Serialize};

use crate::{
    db::{
        ConnectionType, SuggestDao, SuggestDb, LAST_INGEST_META_KEY, UNPARSABLE_RECORDS_META_KEY,
    },
    rs::{
        SuggestAttachment, SuggestRecord, SuggestRecordId, SuggestRemoteSettingsClient,
        REMOTE_SETTINGS_COLLECTION, SUGGESTIONS_PER_ATTACHMENT,
    },
    schema::VERSION,
    Result, SuggestApiResult, Suggestion, SuggestionQuery,
};

/// The chunk size used to request unparsable records.
pub const UNPARSABLE_IDS_PER_REQUEST: usize = 150;

/// The store is the entry point to the Suggest component. It incrementally
/// downloads suggestions from the Remote Settings service, stores them in a
/// local database, and returns them in response to user queries.
///
/// Your application should create a single store, and manage it as a singleton.
/// The store is thread-safe, and supports concurrent queries and ingests. We
/// expect that your application will call [`SuggestStore::query()`] to show
/// suggestions as the user types into the address bar, and periodically call
/// [`SuggestStore::ingest()`] in the background to update the database with
/// new suggestions from Remote Settings.
///
/// For responsiveness, we recommend always calling `query()` on a worker
/// thread. When the user types new input into the address bar, call
/// [`SuggestStore::interrupt()`] on the main thread to cancel the query
/// for the old input, and unblock the worker thread for the new query.
///
/// The store keeps track of the state needed to support incremental ingestion,
/// but doesn't schedule the ingestion work itself, or decide how many
/// suggestions to ingest at once. This is for two reasons:
///
/// 1. The primitives for scheduling background work vary between platforms, and
///    aren't available to the lower-level Rust layer. You might use an idle
///    timer on Desktop, `WorkManager` on Android, or `BGTaskScheduler` on iOS.
/// 2. Ingestion constraints can change, depending on the platform and the needs
///    of your application. A mobile device on a metered connection might want
///    to request a small subset of the Suggest data and download the rest
///    later, while a desktop on a fast link might download the entire dataset
///    on the first launch.
pub struct SuggestStore {
    inner: SuggestStoreInner<remote_settings::Client>,
}

/// For records that aren't currently parsable,
/// the record ID and the schema version it's first seen in
/// is recorded in the meta table using `UNPARSABLE_RECORDS_META_KEY` as its key.
/// On the first ingest after an upgrade, re-request those records from Remote Settings,
/// and try to ingest them again.
#[derive(Deserialize, Serialize, Default, Debug)]
#[serde(transparent)]
pub(crate) struct UnparsableRecords(pub BTreeMap<String, UnparsableRecord>);

impl FromSql for UnparsableRecords {
    fn column_result(value: rusqlite::types::ValueRef<'_>) -> rusqlite::types::FromSqlResult<Self> {
        serde_json::from_str(value.as_str()?)
            .map_err(|err| rusqlite::types::FromSqlError::Other(Box::new(err)))
    }
}

impl ToSql for UnparsableRecords {
    fn to_sql(&self) -> rusqlite::Result<rusqlite::types::ToSqlOutput<'_>> {
        Ok(ToSqlOutput::from(serde_json::to_string(self).map_err(
            |err| rusqlite::Error::ToSqlConversionFailure(Box::new(err)),
        )?))
    }
}

#[derive(Deserialize, Serialize, Debug)]
pub(crate) struct UnparsableRecord {
    #[serde(rename = "v")]
    pub schema_version: u32,
}

impl SuggestStore {
    /// Creates a Suggest store.
    pub fn new(
        path: &str,
        settings_config: Option<RemoteSettingsConfig>,
    ) -> SuggestApiResult<Self> {
        let settings_client = || -> Result<_> {
            Ok(remote_settings::Client::new(
                settings_config.unwrap_or_else(|| RemoteSettingsConfig {
                    server_url: None,
                    bucket_name: None,
                    collection_name: REMOTE_SETTINGS_COLLECTION.into(),
                }),
            )?)
        }()?;
        Ok(Self {
            inner: SuggestStoreInner::new(path, settings_client),
        })
    }

    /// Queries the database for suggestions.
    pub fn query(&self, query: SuggestionQuery) -> SuggestApiResult<Vec<Suggestion>> {
        Ok(self.inner.query(query)?)
    }

    /// Interrupts any ongoing queries.
    ///
    /// This should be called when the user types new input into the address
    /// bar, to ensure that they see fresh suggestions as they type. This
    /// method does not interrupt any ongoing ingests.
    pub fn interrupt(&self) {
        self.inner.interrupt()
    }

    /// Ingests new suggestions from Remote Settings.
    pub fn ingest(&self, constraints: SuggestIngestionConstraints) -> SuggestApiResult<()> {
        Ok(self.inner.ingest(constraints)?)
    }

    /// Removes all content from the database.
    pub fn clear(&self) -> SuggestApiResult<()> {
        Ok(self.inner.clear()?)
    }
}

/// Constraints limit which suggestions to ingest from Remote Settings.
#[derive(Clone, Default, Debug)]
pub struct SuggestIngestionConstraints {
    /// The approximate maximum number of suggestions to ingest. Set to [`None`]
    /// for "no limit".
    ///
    /// Because of how suggestions are partitioned in Remote Settings, this is a
    /// soft limit, and the store might ingest more than requested.
    pub max_suggestions: Option<u64>,
}

/// The implementation of the store. This is generic over the Remote Settings
/// client, and is split out from the concrete [`SuggestStore`] for testing
/// with a mock client.
pub(crate) struct SuggestStoreInner<S> {
    path: PathBuf,
    dbs: OnceCell<SuggestStoreDbs>,
    settings_client: S,
}

impl<S> SuggestStoreInner<S> {
    fn new(path: impl AsRef<Path>, settings_client: S) -> Self {
        Self {
            path: path.as_ref().into(),
            dbs: OnceCell::new(),
            settings_client,
        }
    }

    /// Returns this store's database connections, initializing them if
    /// they're not already open.
    fn dbs(&self) -> Result<&SuggestStoreDbs> {
        self.dbs
            .get_or_try_init(|| SuggestStoreDbs::open(&self.path))
    }

    fn query(&self, query: SuggestionQuery) -> Result<Vec<Suggestion>> {
        if query.keyword.is_empty() || query.providers.is_empty() {
            return Ok(Vec::new());
        }
        self.dbs()?.reader.read(|dao| dao.fetch_suggestions(&query))
    }

    fn interrupt(&self) {
        if let Some(dbs) = self.dbs.get() {
            // Only interrupt if the databases are already open.
            dbs.reader.interrupt_handle.interrupt();
        }
    }

    fn clear(&self) -> Result<()> {
        self.dbs()?.writer.write(|dao| dao.clear())
    }
}

impl<S> SuggestStoreInner<S>
where
    S: SuggestRemoteSettingsClient,
{
    fn ingest(&self, constraints: SuggestIngestionConstraints) -> Result<()> {
        let writer = &self.dbs()?.writer;

        if let Some(unparsable_records) =
            writer.read(|dao| dao.get_meta::<UnparsableRecords>(UNPARSABLE_RECORDS_META_KEY))?
        {
            let all_unparsable_ids = unparsable_records
                .0
                .iter()
                .filter(|(_, unparsable_record)| unparsable_record.schema_version < VERSION)
                .map(|(record_id, _)| record_id)
                .collect::<Vec<_>>();
            for unparsable_ids in all_unparsable_ids.chunks(UNPARSABLE_IDS_PER_REQUEST) {
                let mut options = GetItemsOptions::new();
                for unparsable_id in unparsable_ids {
                    options.eq("id", *unparsable_id);
                }
                let records_chunk = self
                    .settings_client
                    .get_records_with_options(&options)?
                    .records;

                self.ingest_records(writer, &records_chunk)?;
            }
        }

        let mut options = GetItemsOptions::new();
        // Remote Settings returns records in descending modification order
        // (newest first), but we want them in ascending order (oldest first),
        // so that we can eventually resume downloading where we left off.
        options.sort("last_modified", SortOrder::Ascending);
        if let Some(last_ingest) = writer.read(|dao| dao.get_meta::<u64>(LAST_INGEST_META_KEY))? {
            // Only download changes since our last ingest. If our last ingest
            // was interrupted, we'll pick up where we left off.
            options.gt("last_modified", last_ingest.to_string());
        }

        if let Some(max_suggestions) = constraints.max_suggestions {
            // Each record's attachment has 200 suggestions, so download enough
            // records to cover the requested maximum.
            let max_records = (max_suggestions.saturating_sub(1) / SUGGESTIONS_PER_ATTACHMENT) + 1;
            options.limit(max_records);
        }

        let records = self
            .settings_client
            .get_records_with_options(&options)?
            .records;
        self.ingest_records(writer, &records)?;

        Ok(())
    }

    fn ingest_records(&self, writer: &SuggestDb, records: &[RemoteSettingsRecord]) -> Result<()> {
        for record in records {
            let record_id = SuggestRecordId::from(&record.id);
            if record.deleted {
                // If the entire record was deleted, drop all its suggestions
                // and advance the last ingest time.
                writer.write(|dao| {
                    match record_id.as_icon_id() {
                        Some(icon_id) => dao.drop_icon(icon_id)?,
                        None => dao.drop_suggestions(&record_id)?,
                    };
                    dao.drop_unparsable_record_id(&record_id)?;
                    dao.put_last_ingest_if_newer(record.last_modified)?;

                    Ok(())
                })?;
                continue;
            }
            let Ok(fields) =
                serde_json::from_value(serde_json::Value::Object(record.fields.clone()))
            else {
                // We don't recognize this record's type, so we don't know how
                // to ingest its suggestions. Record this in the meta table.
                writer.write(|dao| {
                    dao.put_unparsable_record_id(&record_id)?;
                    dao.put_last_ingest_if_newer(record.last_modified)?;
                    Ok(())
                })?;
                continue;
            };
            match fields {
                SuggestRecord::AmpWikipedia => {
                    self.ingest_suggestions_from_record(
                        writer,
                        record,
                        |dao, record_id, suggestions| {
                            dao.insert_amp_wikipedia_suggestions(record_id, suggestions)
                        },
                    )?;
                }
                SuggestRecord::Icon => {
                    let (Some(icon_id), Some(attachment)) =
                        (record_id.as_icon_id(), record.attachment.as_ref())
                    else {
                        // An icon record should have an icon ID and an
                        // attachment. Icons that don't have these are
                        // malformed, so skip to the next record.
                        writer.write(|dao| dao.put_last_ingest_if_newer(record.last_modified))?;
                        continue;
                    };
                    let data = self.settings_client.get_attachment(&attachment.location)?;
                    writer.write(|dao| {
                        dao.put_icon(icon_id, &data)?;
                        dao.put_last_ingest_if_newer(record.last_modified)?;
                        // Remove this record's ID from the list of unparsable
                        // records, since we understand it now.
                        dao.drop_unparsable_record_id(&record_id)?;

                        Ok(())
                    })?;
                }
                SuggestRecord::Amo => {
                    self.ingest_suggestions_from_record(
                        writer,
                        record,
                        |dao, record_id, suggestions| {
                            dao.insert_amo_suggestions(record_id, suggestions)
                        },
                    )?;
                }
                SuggestRecord::Pocket => {
                    self.ingest_suggestions_from_record(
                        writer,
                        record,
                        |dao, record_id, suggestions| {
                            dao.insert_pocket_suggestions(record_id, suggestions)
                        },
                    )?;
                }
            }
        }
        Ok(())
    }

    fn ingest_suggestions_from_record<T>(
        &self,
        writer: &SuggestDb,
        record: &RemoteSettingsRecord,
        ingestion_handler: impl FnOnce(&mut SuggestDao<'_>, &SuggestRecordId, &[T]) -> Result<()>,
    ) -> Result<()>
    where
        T: DeserializeOwned,
    {
        let record_id = SuggestRecordId::from(&record.id);

        let Some(attachment) = record.attachment.as_ref() else {
            // A record should always have an
            // attachment with suggestions. If it doesn't, it's
            // malformed, so skip to the next record.
            writer.write(|dao| dao.put_last_ingest_if_newer(record.last_modified))?;
            return Ok(());
        };

        let attachment: SuggestAttachment<T> =
            serde_json::from_slice(&self.settings_client.get_attachment(&attachment.location)?)?;

        writer.write(|dao| {
            // Drop any suggestions that we previously ingested from
            // this record's attachment. Suggestions don't have a
            // stable identifier, and determining which suggestions in
            // the attachment actually changed is more complicated than
            // dropping and re-ingesting all of them.
            dao.drop_suggestions(&record_id)?;

            // Ingest (or re-ingest) all suggestions in the
            // attachment.
            ingestion_handler(dao, &record_id, attachment.suggestions())?;

            // Remove this record's ID from the list of unparsable
            // records, since we understand it now.
            dao.drop_unparsable_record_id(&record_id)?;

            // Advance the last fetch time, so that we can resume
            // fetching after this record if we're interrupted.
            dao.put_last_ingest_if_newer(record.last_modified)?;

            Ok(())
        })
    }
}

/// Holds a store's open connections to the Suggest database.
struct SuggestStoreDbs {
    /// A read-write connection used to update the database with new data.
    writer: SuggestDb,
    /// A read-only connection used to query the database.
    reader: SuggestDb,
}

impl SuggestStoreDbs {
    fn open(path: &Path) -> Result<Self> {
        // Order is important here: the writer must be opened first, so that it
        // can set up the database and run any migrations.
        let writer = SuggestDb::open(path, ConnectionType::ReadWrite)?;
        let reader = SuggestDb::open(path, ConnectionType::ReadOnly)?;
        Ok(Self { writer, reader })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use std::{cell::RefCell, collections::HashMap};

    use anyhow::{anyhow, Context};
    use expect_test::expect;
    use parking_lot::Once;
    use rc_crypto::rand;
    use remote_settings::{RemoteSettingsRecord, RemoteSettingsResponse};
    use serde_json::json;
    use sql_support::ConnExt;

    use crate::SuggestionProvider;

    /// Creates a unique in-memory Suggest store.
    fn unique_test_store<S>(settings_client: S) -> SuggestStoreInner<S>
    where
        S: SuggestRemoteSettingsClient,
    {
        let mut unique_suffix = [0u8; 8];
        rand::fill(&mut unique_suffix).expect("Failed to generate unique suffix for test store");
        // A store opens separate connections to the same database for reading
        // and writing, so we must give our in-memory database a name, and open
        // it in shared-cache mode so that both connections can access it.
        SuggestStoreInner::new(
            format!(
                "file:test_store_{}?mode=memory&cache=shared",
                hex::encode(unique_suffix),
            ),
            settings_client,
        )
    }

    /// A snapshot containing fake Remote Settings records and attachments for
    /// the store to ingest. We use snapshots to test the store's behavior in a
    /// data-driven way.
    struct Snapshot {
        records: Vec<RemoteSettingsRecord>,
        attachments: HashMap<&'static str, Vec<u8>>,
    }

    impl Snapshot {
        /// Creates a snapshot from a JSON value that represents a collection of
        /// Suggest Remote Settings records.
        ///
        /// You can use the [`serde_json::json!`] macro to construct the JSON
        /// value, then pass it to this function. It's easier to use the
        /// `Snapshot::with_records(json!(...))` idiom than to construct the
        /// records by hand.
        fn with_records(value: serde_json::Value) -> anyhow::Result<Self> {
            Ok(Self {
                records: serde_json::from_value(value)
                    .context("Couldn't create snapshot with Remote Settings records")?,
                attachments: HashMap::new(),
            })
        }

        /// Adds a data attachment with one or more suggestions to the snapshot.
        fn with_data(
            mut self,
            location: &'static str,
            value: serde_json::Value,
        ) -> anyhow::Result<Self> {
            self.attachments.insert(
                location,
                serde_json::to_vec(&value).context("Couldn't add data attachment to snapshot")?,
            );
            Ok(self)
        }

        /// Adds an icon attachment to the snapshot.
        fn with_icon(mut self, location: &'static str, bytes: Vec<u8>) -> Self {
            self.attachments.insert(location, bytes);
            self
        }
    }

    /// A fake Remote Settings client that returns records and attachments from
    /// a snapshot.
    struct SnapshotSettingsClient {
        /// The current snapshot. You can modify it using
        /// [`RefCell::borrow_mut()`] to simulate remote updates in tests.
        snapshot: RefCell<Snapshot>,

        /// The options passed to the last [`Self::get_records_with_options()`]
        /// call.
        last_get_records_options: RefCell<Option<GetItemsOptions>>,
    }

    impl SnapshotSettingsClient {
        /// Creates a client with an initial snapshot.
        fn with_snapshot(snapshot: Snapshot) -> Self {
            Self {
                snapshot: RefCell::new(snapshot),
                last_get_records_options: RefCell::default(),
            }
        }

        /// Returns the most recent value of an option passed to
        /// [`Self::get_records_with_options()`].
        fn last_get_records_option(&self, option: &str) -> Option<String> {
            self.last_get_records_options
                .borrow()
                .as_ref()
                .and_then(|options| {
                    options
                        .iter_query_pairs()
                        .find(|(key, _)| key == option)
                        .map(|(_, value)| value.into())
                })
        }
    }

    impl SuggestRemoteSettingsClient for SnapshotSettingsClient {
        fn get_records_with_options(
            &self,
            options: &GetItemsOptions,
        ) -> Result<RemoteSettingsResponse> {
            *self.last_get_records_options.borrow_mut() = Some(options.clone());
            let records = self.snapshot.borrow().records.clone();
            let last_modified = records
                .iter()
                .map(|record| record.last_modified)
                .max()
                .unwrap_or(0);
            Ok(RemoteSettingsResponse {
                records,
                last_modified,
            })
        }

        fn get_attachment(&self, location: &str) -> Result<Vec<u8>> {
            Ok(self
                .snapshot
                .borrow()
                .attachments
                .get(location)
                .unwrap_or_else(|| unreachable!("Unexpected request for attachment `{}`", location))
                .clone())
        }
    }

    fn before_each() {
        static ONCE: Once = Once::new();
        ONCE.call_once(|| {
            env_logger::init();
        });
    }

    /// Tests that `SuggestStore` is usable with UniFFI, which requires exposed
    /// interfaces to be `Send` and `Sync`.
    #[test]
    fn is_thread_safe() {
        before_each();

        fn is_send_sync<T: Send + Sync>() {}
        is_send_sync::<SuggestStore>();
    }

    /// Tests ingesting suggestions into an empty database.
    #[test]
    fn ingest_suggestions() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "1234",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Los Pollos Hermanos",
                "iab_category": "8 - Food & Drink",
                "keywords": ["lo", "los", "los p", "los pollos", "los pollos h", "los pollos hermanos"],
                "title": "Los Pollos Hermanos - Albuquerque",
                "url": "https://www.lph-nm.biz",
                "icon": "5678",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
            }]),
        )?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(dao.get_meta::<u64>(LAST_INGEST_META_KEY)?, Some(15));
            expect![[r#"
                [
                    Amp {
                        title: "Los Pollos Hermanos - Albuquerque",
                        url: "https://www.lph-nm.biz",
                        raw_url: "https://www.lph-nm.biz",
                        icon: None,
                        full_keyword: "los",
                        block_id: 0,
                        advertiser: "Los Pollos Hermanos",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "lo".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);

            Ok(())
        })?;

        Ok(())
    }

    /// Tests ingesting suggestions with icons.
    #[test]
    fn ingest_icons() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "icon-2",
            "type": "icon",
            "last_modified": 20,
            "attachment": {
                "filename": "icon-2.png",
                "mimetype": "image/png",
                "location": "icon-2.png",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Good Place Eats",
                "iab_category": "8 - Food & Drink",
                "keywords": ["la", "las", "lasa", "lasagna", "lasagna come out tomorrow"],
                "title": "Lasagna Come Out Tomorrow",
                "url": "https://www.lasagna.restaurant",
                "icon": "2",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url"
            }, {
                "id": 0,
                "advertiser": "Good Place Eats",
                "iab_category": "8 - Food & Drink",
                "keywords": ["pe", "pen", "penne", "penne for your thoughts"],
                "title": "Penne for Your Thoughts",
                "url": "https://penne.biz",
                "icon": "2",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url"
            }]),
        )?
        .with_icon("icon-2.png", "i-am-an-icon".as_bytes().into());

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            expect![[r#"
                [
                    Amp {
                        title: "Lasagna Come Out Tomorrow",
                        url: "https://www.lasagna.restaurant",
                        raw_url: "https://www.lasagna.restaurant",
                        icon: Some(
                            [
                                105,
                                45,
                                97,
                                109,
                                45,
                                97,
                                110,
                                45,
                                105,
                                99,
                                111,
                                110,
                            ],
                        ),
                        full_keyword: "lasagna",
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "la".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);
            expect![[r#"
                [
                    Amp {
                        title: "Penne for Your Thoughts",
                        url: "https://penne.biz",
                        raw_url: "https://penne.biz",
                        icon: Some(
                            [
                                105,
                                45,
                                97,
                                109,
                                45,
                                97,
                                110,
                                45,
                                105,
                                99,
                                111,
                                110,
                            ],
                        ),
                        full_keyword: "penne",
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "pe".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);

            Ok(())
        })?;

        Ok(())
    }

    /// Tests ingesting a data attachment containing a single suggestion,
    /// instead of an array of suggestions.
    #[test]
    fn ingest_one_suggestion_in_data_attachment() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!({
                "id": 0,
                 "advertiser": "Good Place Eats",
                 "iab_category": "8 - Food & Drink",
                 "keywords": ["la", "las", "lasa", "lasagna", "lasagna come out tomorrow"],
                 "title": "Lasagna Come Out Tomorrow",
                 "url": "https://www.lasagna.restaurant",
                 "icon": "2",
                 "impression_url": "https://example.com/impression_url",
                 "click_url": "https://example.com/click_url"
            }),
        )?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            expect![[r#"
                [
                    Amp {
                        title: "Lasagna Come Out Tomorrow",
                        url: "https://www.lasagna.restaurant",
                        raw_url: "https://www.lasagna.restaurant",
                        icon: None,
                        full_keyword: "lasagna",
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "la".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);

            Ok(())
        })?;

        Ok(())
    }

    /// Tests re-ingesting suggestions from an updated attachment.
    #[test]
    fn reingest_suggestions() -> anyhow::Result<()> {
        before_each();

        // Ingest suggestions from the initial snapshot.
        let initial_snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Good Place Eats",
                "iab_category": "8 - Food & Drink",
                "keywords": ["la", "las", "lasa", "lasagna", "lasagna come out tomorrow"],
                "title": "Lasagna Come Out Tomorrow",
                "url": "https://www.lasagna.restaurant",
                "icon": "1",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url"
            }, {
                "id": 0,
                "advertiser": "Los Pollos Hermanos",
                "iab_category": "8 - Food & Drink",
                "keywords": ["lo", "los p", "los pollos h"],
                "title": "Los Pollos Hermanos - Albuquerque",
                "url": "https://www.lph-nm.biz",
                "icon": "2",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url"
            }]),
        )?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(initial_snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(dao.get_meta(LAST_INGEST_META_KEY)?, Some(15u64));
            expect![[r#"
                [
                    Amp {
                        title: "Lasagna Come Out Tomorrow",
                        url: "https://www.lasagna.restaurant",
                        raw_url: "https://www.lasagna.restaurant",
                        icon: None,
                        full_keyword: "lasagna",
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "la".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);
            Ok(())
        })?;

        // Update the snapshot with new suggestions: drop Lasagna, update Los
        // Pollos, and add Penne.
        *store.settings_client.snapshot.borrow_mut() = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "data",
            "last_modified": 30,
            "attachment": {
                "filename": "data-1-1.json",
                "mimetype": "application/json",
                "location": "data-1-1.json",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1-1.json",
            json!([{
                "id": 0,
                "advertiser": "Los Pollos Hermanos",
                "iab_category": "8 - Food & Drink",
                "keywords": ["los ", "los pollos", "los pollos hermanos"],
                "title": "Los Pollos Hermanos - Now Serving at 14 Locations!",
                "url": "https://www.lph-nm.biz",
                "icon": "2",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url"
            }, {
                "id": 0,
                "advertiser": "Good Place Eats",
                "iab_category": "8 - Food & Drink",
                "keywords": ["pe", "pen", "penne", "penne for your thoughts"],
                "title": "Penne for Your Thoughts",
                "url": "https://penne.biz",
                "icon": "2",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url"
            }]),
        )?;

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(dao.get_meta(LAST_INGEST_META_KEY)?, Some(30u64));
            assert!(dao
                .fetch_suggestions(&SuggestionQuery {
                    keyword: "la".into(),
                    providers: vec![SuggestionProvider::Amp],
                    limit: None,
                })?
                .is_empty());
            expect![[r#"
                [
                    Amp {
                        title: "Los Pollos Hermanos - Now Serving at 14 Locations!",
                        url: "https://www.lph-nm.biz",
                        raw_url: "https://www.lph-nm.biz",
                        icon: None,
                        full_keyword: "los pollos",
                        block_id: 0,
                        advertiser: "Los Pollos Hermanos",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "los ".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);
            expect![[r#"
                [
                    Amp {
                        title: "Penne for Your Thoughts",
                        url: "https://penne.biz",
                        raw_url: "https://penne.biz",
                        icon: None,
                        full_keyword: "penne",
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "pe".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);
            Ok(())
        })?;

        Ok(())
    }

    /// Tests re-ingesting icons from an updated attachment.
    #[test]
    fn reingest_icons() -> anyhow::Result<()> {
        before_each();

        // Ingest suggestions and icons from the initial snapshot.
        let initial_snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "icon-2",
            "type": "icon",
            "last_modified": 20,
            "attachment": {
                "filename": "icon-2.png",
                "mimetype": "image/png",
                "location": "icon-2.png",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "icon-3",
            "type": "icon",
            "last_modified": 25,
            "attachment": {
                "filename": "icon-3.png",
                "mimetype": "image/png",
                "location": "icon-3.png",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Good Place Eats",
                "iab_category": "8 - Food & Drink",
                "keywords": ["la", "las", "lasa", "lasagna", "lasagna come out tomorrow"],
                "title": "Lasagna Come Out Tomorrow",
                "url": "https://www.lasagna.restaurant",
                "icon": "2",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url"
            }, {
                "id": 0,
                "advertiser": "Los Pollos Hermanos",
                "iab_category": "8 - Food & Drink",
                "keywords": ["lo", "los", "los pollos", "los pollos hermanos"],
                "title": "Los Pollos Hermanos - Albuquerque",
                "url": "https://www.lph-nm.biz",
                "icon": "3",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url"
            }]),
        )?
        .with_icon("icon-2.png", "lasagna-icon".as_bytes().into())
        .with_icon("icon-3.png", "pollos-icon".as_bytes().into());

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(initial_snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(dao.get_meta(LAST_INGEST_META_KEY)?, Some(25u64));
            assert_eq!(
                dao.conn
                    .query_one::<i64>("SELECT count(*) FROM suggestions")?,
                2
            );
            assert_eq!(dao.conn.query_one::<i64>("SELECT count(*) FROM icons")?, 2);
            Ok(())
        })?;

        // Update the snapshot with new icons.
        *store.settings_client.snapshot.borrow_mut() = Snapshot::with_records(json!([{
            "id": "icon-2",
            "type": "icon",
            "last_modified": 30,
            "attachment": {
                "filename": "icon-2.png",
                "mimetype": "image/png",
                "location": "icon-2.png",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "icon-3",
            "type": "icon",
            "last_modified": 35,
            "attachment": {
                "filename": "icon-3.png",
                "mimetype": "image/png",
                "location": "icon-3.png",
                "hash": "",
                "size": 0,
            }
        }]))?
        .with_icon("icon-2.png", "new-lasagna-icon".as_bytes().into())
        .with_icon("icon-3.png", "new-pollos-icon".as_bytes().into());

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(dao.get_meta(LAST_INGEST_META_KEY)?, Some(35u64));
            expect![[r#"
                [
                    Amp {
                        title: "Lasagna Come Out Tomorrow",
                        url: "https://www.lasagna.restaurant",
                        raw_url: "https://www.lasagna.restaurant",
                        icon: Some(
                            [
                                110,
                                101,
                                119,
                                45,
                                108,
                                97,
                                115,
                                97,
                                103,
                                110,
                                97,
                                45,
                                105,
                                99,
                                111,
                                110,
                            ],
                        ),
                        full_keyword: "lasagna",
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "la".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);
            expect![[r#"
                [
                    Amp {
                        title: "Los Pollos Hermanos - Albuquerque",
                        url: "https://www.lph-nm.biz",
                        raw_url: "https://www.lph-nm.biz",
                        icon: Some(
                            [
                                110,
                                101,
                                119,
                                45,
                                112,
                                111,
                                108,
                                108,
                                111,
                                115,
                                45,
                                105,
                                99,
                                111,
                                110,
                            ],
                        ),
                        full_keyword: "los",
                        block_id: 0,
                        advertiser: "Los Pollos Hermanos",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "lo".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);
            Ok(())
        })?;

        Ok(())
    }

    /// Tests ingesting tombstones for previously-ingested suggestions and
    /// icons.
    #[test]
    fn ingest_tombstones() -> anyhow::Result<()> {
        before_each();

        // Ingest suggestions and icons from the initial snapshot.
        let initial_snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "icon-2",
            "type": "icon",
            "last_modified": 20,
            "attachment": {
                "filename": "icon-2.png",
                "mimetype": "image/png",
                "location": "icon-2.png",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Good Place Eats",
                "iab_category": "8 - Food & Drink",
                "keywords": ["la", "las", "lasa", "lasagna", "lasagna come out tomorrow"],
                "title": "Lasagna Come Out Tomorrow",
                "url": "https://www.lasagna.restaurant",
                "icon": "2",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url"
            }]),
        )?
        .with_icon("icon-2.png", "i-am-an-icon".as_bytes().into());

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(initial_snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(dao.get_meta::<u64>(LAST_INGEST_META_KEY)?, Some(20));
            assert_eq!(
                dao.conn
                    .query_one::<i64>("SELECT count(*) FROM suggestions")?,
                1
            );
            assert_eq!(dao.conn.query_one::<i64>("SELECT count(*) FROM icons")?, 1);

            Ok(())
        })?;

        // Replace the records with tombstones. Ingesting these should remove
        // all their suggestions and icons.
        *store.settings_client.snapshot.borrow_mut() = Snapshot::with_records(json!([{
            "id": "data-1",
            "last_modified": 25,
            "deleted": true,
        }, {
            "id": "icon-2",
            "last_modified": 30,
            "deleted": true,
        }]))?;

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(dao.get_meta::<u64>(LAST_INGEST_META_KEY)?, Some(30));
            assert_eq!(
                dao.conn
                    .query_one::<i64>("SELECT count(*) FROM suggestions")?,
                0
            );
            assert_eq!(dao.conn.query_one::<i64>("SELECT count(*) FROM icons")?, 0);

            Ok(())
        })?;

        Ok(())
    }

    /// Tests ingesting suggestions with constraints.
    #[test]
    fn ingest_with_constraints() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([]))?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;
        assert_eq!(
            store.settings_client.last_get_records_option("_limit"),
            None,
        );

        // 200 suggestions per record, so test with numbers around that
        // boundary.
        let table = [
            (0, "1"),
            (199, "1"),
            (200, "1"),
            (201, "2"),
            (300, "2"),
            (400, "2"),
            (401, "3"),
        ];
        for (max_suggestions, expected_limit) in table {
            store.ingest(SuggestIngestionConstraints {
                max_suggestions: Some(max_suggestions),
            })?;
            let actual_limit = store
                .settings_client
                .last_get_records_option("_limit")
                .ok_or_else(|| {
                    anyhow!("Want limit = {} for {}", expected_limit, max_suggestions)
                })?;
            assert_eq!(
                actual_limit, expected_limit,
                "Want limit = {} for {}; got limit = {}",
                expected_limit, max_suggestions, actual_limit
            );
        }

        Ok(())
    }

    /// Tests clearing the store.
    #[test]
    fn clear() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Los Pollos Hermanos",
                "iab_category": "8 - Food & Drink",
                "keywords": ["lo", "los", "los p", "los pollos", "los pollos h", "los pollos hermanos"],
                "title": "Los Pollos Hermanos - Albuquerque",
                "url": "https://www.lph-nm.biz",
                "icon": "2",
                "impression_url": "https://example.com",
                "click_url": "https://example.com",
            }]),
        )?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(dao.get_meta::<u64>(LAST_INGEST_META_KEY)?, Some(15));
            assert_eq!(
                dao.conn
                    .query_one::<i64>("SELECT count(*) FROM suggestions")?,
                1
            );
            assert_eq!(
                dao.conn.query_one::<i64>("SELECT count(*) FROM keywords")?,
                6
            );

            Ok(())
        })?;

        store.clear()?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(dao.get_meta::<u64>(LAST_INGEST_META_KEY)?, None);
            assert_eq!(
                dao.conn
                    .query_one::<i64>("SELECT count(*) FROM suggestions")?,
                0
            );
            assert_eq!(
                dao.conn.query_one::<i64>("SELECT count(*) FROM keywords")?,
                0
            );

            Ok(())
        })?;

        Ok(())
    }

    /// Tests querying suggestions.
    #[test]
    fn query() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },

        }, {
            "id": "data-2",
            "type": "amo-suggestions",
            "last_modified": 15,
            "attachment": {
                "filename": "data-2.json",
                "mimetype": "application/json",
                "location": "data-2.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "data-3",
            "type": "pocket-suggestions",
            "last_modified": 15,
            "attachment": {
                "filename": "data-3.json",
                "mimetype": "application/json",
                "location": "data-3.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "icon-2",
            "type": "icon",
            "last_modified": 20,
            "attachment": {
                "filename": "icon-2.png",
                "mimetype": "image/png",
                "location": "icon-2.png",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "icon-3",
            "type": "icon",
            "last_modified": 25,
            "attachment": {
                "filename": "icon-3.png",
                "mimetype": "image/png",
                "location": "icon-3.png",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Good Place Eats",
                "iab_category": "8 - Food & Drink",
                "keywords": ["la", "las", "lasa", "lasagna", "lasagna come out tomorrow"],
                "title": "Lasagna Come Out Tomorrow",
                "url": "https://www.lasagna.restaurant",
                "icon": "2",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
            }, {
                "id": 0,
                "advertiser": "Wikipedia",
                "iab_category": "5 - Education",
                "keywords": ["cal", "cali", "california"],
                "title": "California",
                "url": "https://wikipedia.org/California",
                "icon": "3"
            }, {
                "id": 0,
                "advertiser": "Wikipedia",
                "iab_category": "5 - Education",
                "keywords": ["cal", "cali", "california", "institute", "technology"],
                "title": "California Institute of Technology",
                "url": "https://wikipedia.org/California_Institute_of_Technology",
                "icon": "3"
            },{
                "id": 0,
                "advertiser": "Wikipedia",
                "iab_category": "5 - Education",
                "keywords": ["multimatch"],
                "title": "Multimatch",
                "url": "https://wikipedia.org/Multimatch",
                "icon": "3"
            }]),
        )?
            .with_data(
                "data-2.json",
                json!([
                    {
                        "description": "amo suggestion",
                        "url": "https://addons.mozilla.org/en-US/firefox/addon/example",
                        "guid": "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                        "keywords": ["relay", "spam", "masking email", "alias"],
                        "title": "Firefox Relay",
                        "icon": "https://addons.mozilla.org/user-media/addon_icons/2633/2633704-64.png?modified=2c11a80b",
                        "rating": "4.9",
                        "number_of_ratings": 888,
                        "score": 0.25
                    },
                    {
                        "description": "amo suggestion multi-match",
                        "url": "https://addons.mozilla.org/en-US/firefox/addon/multimatch",
                        "guid": "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                        "keywords": ["multimatch"],
                        "title": "Firefox Multimatch",
                        "icon": "https://addons.mozilla.org/user-media/addon_icons/2633/2633704-64.png?modified=2c11a80b",
                        "rating": "4.9",
                        "number_of_ratings": 888,
                        "score": 0.25
                    },
                ]),
        )?
            .with_data(
            "data-3.json",
            json!([
                {
                    "description": "pocket suggestion",
                    "url": "https://getpocket.com/collections/its-not-just-burnout-how-grind-culture-failed-women",
                    "lowConfidenceKeywords": ["soft life", "workaholism", "toxic work culture", "work-life balance"],
                    "highConfidenceKeywords": ["burnout women", "grind culture", "women burnout"],
                    "title": "‘It’s Not Just Burnout:’ How Grind Culture Fails Women",
                    "score": 0.25
                },
                {
                    "description": "pocket suggestion multi-match",
                    "url": "https://getpocket.com/collections/multimatch",
                    "lowConfidenceKeywords": [],
                    "highConfidenceKeywords": ["multimatch"],
                    "title": "Multimatching",
                    "score": 0.25
                },
            ]),
        )?
        .with_icon("icon-2.png", "i-am-an-icon".as_bytes().into())
        .with_icon("icon-3.png", "also-an-icon".as_bytes().into());

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        let table = [
            (
                "empty keyword; all providers",
                SuggestionQuery {
                    keyword: String::new(),
                    providers: vec![
                        SuggestionProvider::Amp,
                        SuggestionProvider::Wikipedia,
                        SuggestionProvider::Amo,
                        SuggestionProvider::Pocket,
                    ],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `la`; all providers",
                SuggestionQuery {
                    keyword: "la".into(),
                    providers: vec![
                        SuggestionProvider::Amp,
                        SuggestionProvider::Wikipedia,
                        SuggestionProvider::Amo,
                        SuggestionProvider::Pocket,
                    ],
                    limit: None,
                },
                expect![[r#"
                    [
                        Amp {
                            title: "Lasagna Come Out Tomorrow",
                            url: "https://www.lasagna.restaurant",
                            raw_url: "https://www.lasagna.restaurant",
                            icon: Some(
                                [
                                    105,
                                    45,
                                    97,
                                    109,
                                    45,
                                    97,
                                    110,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            full_keyword: "lasagna",
                            block_id: 0,
                            advertiser: "Good Place Eats",
                            iab_category: "8 - Food & Drink",
                            impression_url: "https://example.com/impression_url",
                            click_url: "https://example.com/click_url",
                            raw_click_url: "https://example.com/click_url",
                        },
                    ]
                "#]],
            ),
            (
                "multimatch; all providers",
                SuggestionQuery {
                    keyword: "multimatch".into(),
                    providers: vec![
                        SuggestionProvider::Amp,
                        SuggestionProvider::Wikipedia,
                        SuggestionProvider::Amo,
                        SuggestionProvider::Pocket,
                    ],
                    limit: None,
                },
                expect![[r#"
                    [
                        Wikipedia {
                            title: "Multimatch",
                            url: "https://wikipedia.org/Multimatch",
                            icon: Some(
                                [
                                    97,
                                    108,
                                    115,
                                    111,
                                    45,
                                    97,
                                    110,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            full_keyword: "multimatch",
                        },
                        Amo {
                            title: "Firefox Multimatch",
                            url: "https://addons.mozilla.org/en-US/firefox/addon/multimatch",
                            icon_url: "https://addons.mozilla.org/user-media/addon_icons/2633/2633704-64.png?modified=2c11a80b",
                            description: "amo suggestion multi-match",
                            rating: Some(
                                "4.9",
                            ),
                            number_of_ratings: 888,
                            guid: "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                            score: 0.25,
                        },
                        Pocket {
                            title: "Multimatching",
                            url: "https://getpocket.com/collections/multimatch",
                            score: 0.25,
                            is_top_pick: true,
                        },
                    ]
                "#]],
            ),
            (
                "multimatch; all providers, limit 2",
                SuggestionQuery {
                    keyword: "multimatch".into(),
                    providers: vec![
                        SuggestionProvider::Amp,
                        SuggestionProvider::Wikipedia,
                        SuggestionProvider::Amo,
                        SuggestionProvider::Pocket,
                    ],
                    limit: Some(2),
                },
                expect![[r#"
                    [
                        Wikipedia {
                            title: "Multimatch",
                            url: "https://wikipedia.org/Multimatch",
                            icon: Some(
                                [
                                    97,
                                    108,
                                    115,
                                    111,
                                    45,
                                    97,
                                    110,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            full_keyword: "multimatch",
                        },
                        Amo {
                            title: "Firefox Multimatch",
                            url: "https://addons.mozilla.org/en-US/firefox/addon/multimatch",
                            icon_url: "https://addons.mozilla.org/user-media/addon_icons/2633/2633704-64.png?modified=2c11a80b",
                            description: "amo suggestion multi-match",
                            rating: Some(
                                "4.9",
                            ),
                            number_of_ratings: 888,
                            guid: "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                            score: 0.25,
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `la`; AMP only",
                SuggestionQuery {
                    keyword: "la".into(),
                    providers: vec![SuggestionProvider::Amp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Amp {
                            title: "Lasagna Come Out Tomorrow",
                            url: "https://www.lasagna.restaurant",
                            raw_url: "https://www.lasagna.restaurant",
                            icon: Some(
                                [
                                    105,
                                    45,
                                    97,
                                    109,
                                    45,
                                    97,
                                    110,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            full_keyword: "lasagna",
                            block_id: 0,
                            advertiser: "Good Place Eats",
                            iab_category: "8 - Food & Drink",
                            impression_url: "https://example.com/impression_url",
                            click_url: "https://example.com/click_url",
                            raw_click_url: "https://example.com/click_url",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `la`; Wikipedia, AMO, and Pocket",
                SuggestionQuery {
                    keyword: "la".into(),
                    providers: vec![
                        SuggestionProvider::Wikipedia,
                        SuggestionProvider::Amo,
                        SuggestionProvider::Pocket,
                    ],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `la`; no providers",
                SuggestionQuery {
                    keyword: "la".into(),
                    providers: vec![],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `cal`; AMP, AMO, and Pocket",
                SuggestionQuery {
                    keyword: "cal".into(),
                    providers: vec![
                        SuggestionProvider::Amp,
                        SuggestionProvider::Amo,
                        SuggestionProvider::Pocket,
                    ],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `cal`; Wikipedia only",
                SuggestionQuery {
                    keyword: "cal".into(),
                    providers: vec![SuggestionProvider::Wikipedia],
                    limit: None,
                },
                expect![[r#"
                    [
                        Wikipedia {
                            title: "California",
                            url: "https://wikipedia.org/California",
                            icon: Some(
                                [
                                    97,
                                    108,
                                    115,
                                    111,
                                    45,
                                    97,
                                    110,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            full_keyword: "california",
                        },
                        Wikipedia {
                            title: "California Institute of Technology",
                            url: "https://wikipedia.org/California_Institute_of_Technology",
                            icon: Some(
                                [
                                    97,
                                    108,
                                    115,
                                    111,
                                    45,
                                    97,
                                    110,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            full_keyword: "california",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `cal`; Wikipedia with limit 1",
                SuggestionQuery {
                    keyword: "cal".into(),
                    providers: vec![SuggestionProvider::Wikipedia],
                    limit: Some(1),
                },
                expect![[r#"
                    [
                        Wikipedia {
                            title: "California",
                            url: "https://wikipedia.org/California",
                            icon: Some(
                                [
                                    97,
                                    108,
                                    115,
                                    111,
                                    45,
                                    97,
                                    110,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            full_keyword: "california",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `cal`; no providers",
                SuggestionQuery {
                    keyword: "cal".into(),
                    providers: vec![],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `spam`; AMO only",
                SuggestionQuery {
                    keyword: "spam".into(),
                    providers: vec![SuggestionProvider::Amo],
                    limit: None,
                },
                expect![[r#"
                [
                    Amo {
                        title: "Firefox Relay",
                        url: "https://addons.mozilla.org/en-US/firefox/addon/example",
                        icon_url: "https://addons.mozilla.org/user-media/addon_icons/2633/2633704-64.png?modified=2c11a80b",
                        description: "amo suggestion",
                        rating: Some(
                            "4.9",
                        ),
                        number_of_ratings: 888,
                        guid: "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                        score: 0.25,
                    },
                ]
                "#]],
            ),
            (
                "keyword = `masking`; AMO only",
                SuggestionQuery {
                    keyword: "masking".into(),
                    providers: vec![SuggestionProvider::Amo],
                    limit: None,
                },
                expect![[r#"
                [
                    Amo {
                        title: "Firefox Relay",
                        url: "https://addons.mozilla.org/en-US/firefox/addon/example",
                        icon_url: "https://addons.mozilla.org/user-media/addon_icons/2633/2633704-64.png?modified=2c11a80b",
                        description: "amo suggestion",
                        rating: Some(
                            "4.9",
                        ),
                        number_of_ratings: 888,
                        guid: "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                        score: 0.25,
                    },
                ]
                "#]],
            ),
            (
                "keyword = `masking e`; AMO only",
                SuggestionQuery {
                    keyword: "masking e".into(),
                    providers: vec![SuggestionProvider::Amo],
                    limit: None,
                },
                expect![[r#"
                [
                    Amo {
                        title: "Firefox Relay",
                        url: "https://addons.mozilla.org/en-US/firefox/addon/example",
                        icon_url: "https://addons.mozilla.org/user-media/addon_icons/2633/2633704-64.png?modified=2c11a80b",
                        description: "amo suggestion",
                        rating: Some(
                            "4.9",
                        ),
                        number_of_ratings: 888,
                        guid: "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                        score: 0.25,
                    },
                ]
                "#]],
            ),
            (
                "keyword = `masking s`; AMO only",
                SuggestionQuery {
                    keyword: "masking s".into(),
                    providers: vec![SuggestionProvider::Amo],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `soft`; AMP and Wikipedia",
                SuggestionQuery {
                    keyword: "soft".into(),
                    providers: vec![SuggestionProvider::Amp, SuggestionProvider::Wikipedia],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `soft`; Pocket only",
                SuggestionQuery {
                    keyword: "soft".into(),
                    providers: vec![SuggestionProvider::Pocket],
                    limit: None,
                },
                expect![[r#"
                [
                    Pocket {
                        title: "‘It’s Not Just Burnout:’ How Grind Culture Fails Women",
                        url: "https://getpocket.com/collections/its-not-just-burnout-how-grind-culture-failed-women",
                        score: 0.25,
                        is_top_pick: false,
                    },
                ]
                "#]],
            ),
            (
                "keyword = `soft l`; Pocket only",
                SuggestionQuery {
                    keyword: "soft l".into(),
                    providers: vec![SuggestionProvider::Pocket],
                    limit: None,
                },
                expect![[r#"
                [
                    Pocket {
                        title: "‘It’s Not Just Burnout:’ How Grind Culture Fails Women",
                        url: "https://getpocket.com/collections/its-not-just-burnout-how-grind-culture-failed-women",
                        score: 0.25,
                        is_top_pick: false,
                    },
                ]
                "#]],
            ),
            (
                "keyword = `sof`; Pocket only",
                SuggestionQuery {
                    keyword: "sof".into(),
                    providers: vec![SuggestionProvider::Pocket],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `burnout women`; Pocket only",
                SuggestionQuery {
                    keyword: "burnout women".into(),
                    providers: vec![SuggestionProvider::Pocket],
                    limit: None,
                },
                expect![[r#"
                [
                    Pocket {
                        title: "‘It’s Not Just Burnout:’ How Grind Culture Fails Women",
                        url: "https://getpocket.com/collections/its-not-just-burnout-how-grind-culture-failed-women",
                        score: 0.25,
                        is_top_pick: true,
                    },
                ]
                "#]],
            ),
            (
                "keyword = `burnout person`; Pocket only",
                SuggestionQuery {
                    keyword: "burnout person".into(),
                    providers: vec![SuggestionProvider::Pocket],
                    limit: None,
                },
                expect![[r#"
                []
                "#]],
            ),
        ];
        for (what, query, expect) in table {
            expect.assert_debug_eq(
                &store
                    .query(query)
                    .with_context(|| format!("Couldn't query store for {}", what))?,
            );
        }

        Ok(())
    }

    /// Tests ingesting malformed Remote Settings records that we understand,
    /// but that are missing fields, or aren't in the format we expect.
    #[test]
    fn ingest_malformed() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            // Data record without an attachment.
            "id": "missing-data-attachment",
            "type": "data",
            "last_modified": 15,
        }, {
            // Icon record without an attachment.
            "id": "missing-icon-attachment",
            "type": "icon",
            "last_modified": 30,
        }, {
            // Icon record with an ID that's not `icon-{id}`, so suggestions in
            // the data attachment won't be able to reference it.
            "id": "bad-icon-id",
            "type": "icon",
            "last_modified": 45,
            "attachment": {
                "filename": "icon-1.png",
                "mimetype": "image/png",
                "location": "icon-1.png",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_icon("icon-1.png", "i-am-an-icon".as_bytes().into());

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(dao.get_meta::<u64>(LAST_INGEST_META_KEY)?, Some(45));
            assert_eq!(
                dao.conn
                    .query_one::<i64>("SELECT count(*) FROM suggestions")?,
                0
            );
            assert_eq!(dao.conn.query_one::<i64>("SELECT count(*) FROM icons")?, 0);

            Ok(())
        })?;

        Ok(())
    }

    /// Tests unparsable Remote Settings records, which we don't know how to
    /// ingest at all.
    #[test]
    fn ingest_unparsable() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "fancy-new-suggestions-1",
            "type": "fancy-new-suggestions",
            "last_modified": 15,
        }, {
            "id": "clippy-2",
            "type": "clippy",
            "last_modified": 30,
        }]))?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(dao.get_meta::<u64>(LAST_INGEST_META_KEY)?, Some(30));
            expect![[r#"
                Some(
                    UnparsableRecords(
                        {
                            "clippy-2": UnparsableRecord {
                                schema_version: 8,
                            },
                            "fancy-new-suggestions-1": UnparsableRecord {
                                schema_version: 8,
                            },
                        },
                    ),
                )
            "#]]
            .assert_debug_eq(&dao.get_meta::<UnparsableRecords>(UNPARSABLE_RECORDS_META_KEY)?);
            Ok(())
        })?;

        Ok(())
    }

    #[test]
    fn ingest_mixed_parsable_unparsable_records() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "fancy-new-suggestions-1",
            "type": "fancy-new-suggestions",
            "last_modified": 15,
        },
        {
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        },
        {
            "id": "clippy-2",
            "type": "clippy",
            "last_modified": 30,
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Los Pollos Hermanos",
                "iab_category": "8 - Food & Drink",
                "keywords": ["lo", "los", "los p", "los pollos", "los pollos h", "los pollos hermanos"],
                "title": "Los Pollos Hermanos - Albuquerque",
                "url": "https://www.lph-nm.biz",
                "icon": "5678",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
            }]),
        )?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(dao.get_meta::<u64>(LAST_INGEST_META_KEY)?, Some(30));
            expect![[r#"
                Some(
                    UnparsableRecords(
                        {
                            "clippy-2": UnparsableRecord {
                                schema_version: 8,
                            },
                            "fancy-new-suggestions-1": UnparsableRecord {
                                schema_version: 8,
                            },
                        },
                    ),
                )
            "#]]
            .assert_debug_eq(&dao.get_meta::<UnparsableRecords>(UNPARSABLE_RECORDS_META_KEY)?);
            Ok(())
        })?;

        Ok(())
    }

    /// Tests meta update field isn't updated for old unparsable Remote Settings
    /// records.
    #[test]
    fn ingest_unparsable_and_meta_update_stays_the_same() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "fancy-new-suggestions-1",
            "type": "fancy-new-suggestions",
            "last_modified": 15,
        }]))?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));
        store.dbs()?.writer.write(|dao| {
            dao.put_meta(LAST_INGEST_META_KEY, 30)?;
            Ok(())
        })?;
        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(dao.get_meta::<u64>(LAST_INGEST_META_KEY)?, Some(30));
            Ok(())
        })?;

        Ok(())
    }

    #[test]
    fn remove_known_records_out_of_meta_table() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "fancy-new-suggestions-1",
            "type": "fancy-new-suggestions",
            "last_modified": 15,
        },
        {
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        },
        {
            "id": "clippy-2",
            "type": "clippy",
            "last_modified": 15,
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Los Pollos Hermanos",
                "iab_category": "8 - Food & Drink",
                "keywords": ["lo", "los", "los p", "los pollos", "los pollos h", "los pollos hermanos"],
                "title": "Los Pollos Hermanos - Albuquerque",
                "url": "https://www.lph-nm.biz",
                "icon": "5678",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
            }]),
        )?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));
        let mut initial_data = UnparsableRecords::default();
        initial_data
            .0
            .insert("data-1".to_string(), UnparsableRecord { schema_version: 1 });
        initial_data.0.insert(
            "clippy-2".to_string(),
            UnparsableRecord { schema_version: 1 },
        );
        store.dbs()?.writer.write(|dao| {
            dao.put_meta(UNPARSABLE_RECORDS_META_KEY, initial_data)?;
            Ok(())
        })?;

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            expect![[r#"
                Some(
                    UnparsableRecords(
                        {
                            "clippy-2": UnparsableRecord {
                                schema_version: 8,
                            },
                            "fancy-new-suggestions-1": UnparsableRecord {
                                schema_version: 8,
                            },
                        },
                    ),
                )
            "#]]
            .assert_debug_eq(&dao.get_meta::<UnparsableRecords>(UNPARSABLE_RECORDS_META_KEY)?);
            Ok(())
        })?;

        Ok(())
    }

    #[test]
    fn unparsable_record_serialized_correctly() -> anyhow::Result<()> {
        let unparseable_record = UnparsableRecord { schema_version: 1 };
        assert_eq!(serde_json::to_value(unparseable_record)?, json!({ "v": 1 }),);
        Ok(())
    }
}
