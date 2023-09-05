/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use std::path::{Path, PathBuf};

use once_cell::sync::OnceCell;
use remote_settings::{self, GetItemsOptions, RemoteSettingsConfig, SortOrder};

use crate::{
    db::{ConnectionType, SuggestDb, LAST_INGEST_META_KEY},
    rs::{
        SuggestRecord, SuggestRemoteSettingsClient, TypedSuggestRecord, REMOTE_SETTINGS_COLLECTION,
        SUGGESTIONS_PER_ATTACHMENT,
    },
    Result, SuggestApiResult, Suggestion, SuggestionQuery,
};

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
        if query.keyword.is_empty() {
            return Ok(Vec::new());
        }
        let suggestions = self
            .dbs()?
            .reader
            .read(|dao| dao.fetch_by_keyword(&query.keyword))?;
        Ok(suggestions
            .into_iter()
            .filter(|suggestion| {
                (suggestion.is_sponsored && query.include_sponsored)
                    || (!suggestion.is_sponsored && query.include_non_sponsored)
            })
            .collect())
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
            .data;
        for record in &records {
            match record {
                SuggestRecord::Typed(TypedSuggestRecord::Data {
                    id: record_id,
                    last_modified,
                    attachment,
                }) => {
                    let suggestions = self
                        .settings_client
                        .get_data_attachment(&attachment.location)?
                        .0;

                    writer.write(|dao| {
                        // Drop any suggestions that we previously ingested from
                        // this record's attachment. Suggestions don't have a
                        // stable identifier, and determining which suggestions in
                        // the attachment actually changed is more complicated than
                        // dropping and re-ingesting all of them.
                        dao.drop_suggestions(record_id)?;

                        // Ingest (or re-ingest) all suggestions in the attachment.
                        dao.insert_suggestions(record_id, &suggestions)?;

                        // Advance the last fetch time, so that we can resume
                        // fetching after this record if we're interrupted.
                        dao.put_meta(LAST_INGEST_META_KEY, last_modified)?;

                        Ok(())
                    })?;
                }
                SuggestRecord::Untyped {
                    id: record_id,
                    last_modified,
                    deleted,
                } if *deleted => {
                    // If the entire record was deleted, drop all its
                    // suggestions and advance the last fetch time.
                    writer.write(|dao| {
                        match record_id.as_icon_id() {
                            Some(icon_id) => dao.drop_icon(icon_id)?,
                            None => dao.drop_suggestions(record_id)?,
                        };
                        dao.put_meta(LAST_INGEST_META_KEY, last_modified)?;
                        Ok(())
                    })?;
                }
                SuggestRecord::Typed(TypedSuggestRecord::Icon {
                    id: record_id,
                    last_modified,
                    attachment,
                }) => {
                    let Some(icon_id) = record_id.as_icon_id() else {
                        continue
                    };
                    let data = self
                        .settings_client
                        .get_icon_attachment(&attachment.location)?;
                    writer.write(|dao| {
                        dao.insert_icon(icon_id, &data)?;
                        dao.put_meta(LAST_INGEST_META_KEY, last_modified)?;
                        Ok(())
                    })?;
                }
                _ => continue,
            }
        }

        Ok(())
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
    use serde_json::json;
    use sql_support::ConnExt;

    use crate::rs::{DownloadedSuggestDataAttachment, SuggestRemoteSettingsRecords};

    /// A snapshot containing fake Remote Settings records and attachments for
    /// the store to ingest. We use snapshots to test the store's behavior in a
    /// data-driven way.
    struct Snapshot {
        records: SuggestRemoteSettingsRecords,
        data: HashMap<&'static str, DownloadedSuggestDataAttachment>,
        icons: HashMap<&'static str, Vec<u8>>,
    }

    impl Snapshot {
        /// Creates a snapshot from a JSON value that represents a collection of
        /// Suggest Remote Settings records.
        ///
        /// You can use the [`serde_json::json!`] macro to construct the JSON
        /// value, then pass it to this function. It's easier to use the
        /// `Snapshot::with_records(json!(...))` idiom than to construct the
        /// nested `SuggestRemoteSettingsRecords` structure by hand.
        fn with_records(value: serde_json::Value) -> anyhow::Result<Self> {
            Ok(Self {
                records: serde_json::from_value(value)
                    .context("Couldn't create snapshot with Remote Settings records")?,
                data: HashMap::new(),
                icons: HashMap::new(),
            })
        }

        /// Adds a data attachment with one or more suggestions to the snapshot.
        fn with_data(
            mut self,
            location: &'static str,
            value: serde_json::Value,
        ) -> anyhow::Result<Self> {
            self.data.insert(
                location,
                serde_json::from_value(value)
                    .context("Couldn't add data attachment to snapshot")?,
            );
            Ok(self)
        }

        /// Adds an icon attachment to the snapshot.
        fn with_icon(mut self, location: &'static str, bytes: Vec<u8>) -> Self {
            self.icons.insert(location, bytes);
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
        ) -> Result<SuggestRemoteSettingsRecords> {
            *self.last_get_records_options.borrow_mut() = Some(options.clone());
            Ok(self.snapshot.borrow().records.clone())
        }

        fn get_data_attachment(&self, location: &str) -> Result<DownloadedSuggestDataAttachment> {
            Ok(self
                .snapshot
                .borrow()
                .data
                .get(location)
                .unwrap_or_else(|| {
                    unreachable!("Unexpected request for data attachment `{}`", location)
                })
                .clone())
        }

        fn get_icon_attachment(&self, location: &str) -> Result<Vec<u8>> {
            Ok(self
                .snapshot
                .borrow()
                .icons
                .get(location)
                .unwrap_or_else(|| {
                    unreachable!("Unexpected request for icon attachment `{}`", location)
                })
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

        let snapshot = Snapshot::with_records(json!({
            "data": [{
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
            }],
        }))?
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
            }]),
        )?;

        // We use SQLite's URI filename syntax to open a named in-memory
        // database in shared-cache mode, so that it can be accessed by the
        // store's reader and writer.
        //
        // The database name should be unique for each test, to avoid
        // cross-contamination.
        let store = SuggestStoreInner::new(
            "file:ingest_suggestions?mode=memory&cache=shared",
            SnapshotSettingsClient::with_snapshot(snapshot),
        );

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(dao.get_meta::<u64>(LAST_INGEST_META_KEY)?, Some(15));
            expect![[r#"
                [
                    Suggestion {
                        block_id: 0,
                        advertiser: "Los Pollos Hermanos",
                        iab_category: "8 - Food & Drink",
                        is_sponsored: true,
                        full_keyword: "los",
                        title: "Los Pollos Hermanos - Albuquerque",
                        url: "https://www.lph-nm.biz",
                        icon: None,
                        impression_url: None,
                        click_url: None,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_by_keyword("lo")?);

            Ok(())
        })?;

        Ok(())
    }

    /// Tests ingesting suggestions with icons.
    #[test]
    fn ingest_icons() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!({
            "data": [{
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
            }],
        }))?
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
            }, {
                "id": 0,
                "advertiser": "Good Place Eats",
                "iab_category": "8 - Food & Drink",
                "keywords": ["pe", "pen", "penne", "penne for your thoughts"],
                "title": "Penne for Your Thoughts",
                "url": "https://penne.biz",
                "icon": "2",
            }]),
        )?
        .with_icon("icon-2.png", "i-am-an-icon".as_bytes().into());

        let store = SuggestStoreInner::new(
            "file:ingest_icons?mode=memory&cache=shared",
            SnapshotSettingsClient::with_snapshot(snapshot),
        );

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            expect![[r#"
                [
                    Suggestion {
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        is_sponsored: true,
                        full_keyword: "lasagna",
                        title: "Lasagna Come Out Tomorrow",
                        url: "https://www.lasagna.restaurant",
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
                        impression_url: None,
                        click_url: None,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_by_keyword("la")?);
            expect![[r#"
                [
                    Suggestion {
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        is_sponsored: true,
                        full_keyword: "penne",
                        title: "Penne for Your Thoughts",
                        url: "https://penne.biz",
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
                        impression_url: None,
                        click_url: None,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_by_keyword("pe")?);

            Ok(())
        })?;

        Ok(())
    }

    /// Tests ingesting a data attachment containing a single suggestion,
    /// instead of an array of suggestions.
    #[test]
    fn ingest_one_suggestion_in_data_attachment() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!({
            "data": [{
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
            }],
        }))?
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
            }),
        )?;

        let store = SuggestStoreInner::new(
            "file:ingest_one_suggestion_in_data_attachment?mode=memory&cache=shared",
            SnapshotSettingsClient::with_snapshot(snapshot),
        );

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            expect![[r#"
                [
                    Suggestion {
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        is_sponsored: true,
                        full_keyword: "lasagna",
                        title: "Lasagna Come Out Tomorrow",
                        url: "https://www.lasagna.restaurant",
                        icon: None,
                        impression_url: None,
                        click_url: None,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_by_keyword("la")?);

            Ok(())
        })?;

        Ok(())
    }

    /// Tests re-ingesting suggestions from an updated attachment.
    #[test]
    fn reingest() -> anyhow::Result<()> {
        before_each();

        // Ingest suggestions from the initial snapshot.
        let initial_snapshot = Snapshot::with_records(json!({
            "data": [{
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
            }],
        }))?
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
            }, {
                "id": 0,
                "advertiser": "Los Pollos Hermanos",
                "iab_category": "8 - Food & Drink",
                "keywords": ["lo", "los p", "los pollos h"],
                "title": "Los Pollos Hermanos - Albuquerque",
                "url": "https://www.lph-nm.biz",
                "icon": "2",
            }]),
        )?;

        let store = SuggestStoreInner::new(
            "file:reingest?mode=memory&cache=shared",
            SnapshotSettingsClient::with_snapshot(initial_snapshot),
        );

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(dao.get_meta(LAST_INGEST_META_KEY)?, Some(15u64));
            expect![[r#"
                [
                    Suggestion {
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        is_sponsored: true,
                        full_keyword: "lasagna",
                        title: "Lasagna Come Out Tomorrow",
                        url: "https://www.lasagna.restaurant",
                        icon: None,
                        impression_url: None,
                        click_url: None,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_by_keyword("la")?);
            Ok(())
        })?;

        // Update the snapshot with new suggestions: drop Lasagna, update Los
        // Pollos, and add Penne.
        *store.settings_client.snapshot.borrow_mut() = Snapshot::with_records(json!({
            "data": [{
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
            }],
        }))?
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
            }, {
                "id": 0,
                "advertiser": "Good Place Eats",
                "iab_category": "8 - Food & Drink",
                "keywords": ["pe", "pen", "penne", "penne for your thoughts"],
                "title": "Penne for Your Thoughts",
                "url": "https://penne.biz",
                "icon": "2",
            }]),
        )?;

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(dao.get_meta(LAST_INGEST_META_KEY)?, Some(30u64));
            assert!(dao.fetch_by_keyword("la")?.is_empty());
            expect![[r#"
                [
                    Suggestion {
                        block_id: 0,
                        advertiser: "Los Pollos Hermanos",
                        iab_category: "8 - Food & Drink",
                        is_sponsored: true,
                        full_keyword: "los pollos",
                        title: "Los Pollos Hermanos - Now Serving at 14 Locations!",
                        url: "https://www.lph-nm.biz",
                        icon: None,
                        impression_url: None,
                        click_url: None,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_by_keyword("los ")?);
            expect![[r#"
                [
                    Suggestion {
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        is_sponsored: true,
                        full_keyword: "penne",
                        title: "Penne for Your Thoughts",
                        url: "https://penne.biz",
                        icon: None,
                        impression_url: None,
                        click_url: None,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_by_keyword("pe")?);
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
        let initial_snapshot = Snapshot::with_records(json!({
            "data": [{
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
            }],
        }))?
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
            }]),
        )?
        .with_icon("icon-2.png", "i-am-an-icon".as_bytes().into());

        let store = SuggestStoreInner::new(
            "file:ingest_tombstones?mode=memory&cache=shared",
            SnapshotSettingsClient::with_snapshot(initial_snapshot),
        );

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
        *store.settings_client.snapshot.borrow_mut() = Snapshot::with_records(json!({
            "data": [{
                "id": "data-1",
                "last_modified": 25,
                "deleted": true,
            }, {
                "id": "icon-2",
                "last_modified": 30,
                "deleted": true,
            }],
        }))?;

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

        let snapshot = Snapshot::with_records(json!({
            "data": [],
        }))?;

        let store = SuggestStoreInner::new(
            "file:ingest_with_constraints?mode=memory&cache=shared",
            SnapshotSettingsClient::with_snapshot(snapshot),
        );

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

        let snapshot = Snapshot::with_records(json!({
            "data": [{
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
            }],
        }))?
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
            }]),
        )?;

        let store = SuggestStoreInner::new(
            "file:clear?mode=memory&cache=shared",
            SnapshotSettingsClient::with_snapshot(snapshot),
        );

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

        let snapshot = Snapshot::with_records(json!({
            "data": [{
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
            }],
        }))?
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
            }, {
                "id": 0,
                "advertiser": "Wikipedia",
                "iab_category": "5 - Education",
                "keywords": ["cal", "cali", "california"],
                "title": "California",
                "url": "https://wikipedia.org/California",
                "icon": "3",
            }]),
        )?
        .with_icon("icon-2.png", "i-am-an-icon".as_bytes().into())
        .with_icon("icon-3.png", "also-an-icon".as_bytes().into());

        let store = SuggestStoreInner::new(
            "file:query?mode=memory&cache=shared",
            SnapshotSettingsClient::with_snapshot(snapshot),
        );

        store.ingest(SuggestIngestionConstraints::default())?;

        let table = [
            (
                "empty keyword",
                SuggestionQuery {
                    keyword: String::new(),
                    include_sponsored: true,
                    include_non_sponsored: true,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `la`; sponsored and non-sponsored",
                SuggestionQuery {
                    keyword: "la".into(),
                    include_sponsored: true,
                    include_non_sponsored: true,
                },
                expect![[r#"
                    [
                        Suggestion {
                            block_id: 0,
                            advertiser: "Good Place Eats",
                            iab_category: "8 - Food & Drink",
                            is_sponsored: true,
                            full_keyword: "lasagna",
                            title: "Lasagna Come Out Tomorrow",
                            url: "https://www.lasagna.restaurant",
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
                            impression_url: None,
                            click_url: None,
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `la`; sponsored only",
                SuggestionQuery {
                    keyword: "la".into(),
                    include_sponsored: true,
                    include_non_sponsored: false,
                },
                expect![[r#"
                    [
                        Suggestion {
                            block_id: 0,
                            advertiser: "Good Place Eats",
                            iab_category: "8 - Food & Drink",
                            is_sponsored: true,
                            full_keyword: "lasagna",
                            title: "Lasagna Come Out Tomorrow",
                            url: "https://www.lasagna.restaurant",
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
                            impression_url: None,
                            click_url: None,
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `la`; non-sponsored only",
                SuggestionQuery {
                    keyword: "la".into(),
                    include_sponsored: false,
                    include_non_sponsored: true,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `la`; no types",
                SuggestionQuery {
                    keyword: "la".into(),
                    include_sponsored: false,
                    include_non_sponsored: false,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `cal`; sponsored and non-sponsored",
                SuggestionQuery {
                    keyword: "cal".into(),
                    include_sponsored: true,
                    include_non_sponsored: false,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `cal`; sponsored only",
                SuggestionQuery {
                    keyword: "cal".into(),
                    include_sponsored: true,
                    include_non_sponsored: false,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `cal`; non-sponsored only",
                SuggestionQuery {
                    keyword: "cal".into(),
                    include_sponsored: false,
                    include_non_sponsored: true,
                },
                expect![[r#"
                    [
                        Suggestion {
                            block_id: 0,
                            advertiser: "Wikipedia",
                            iab_category: "5 - Education",
                            is_sponsored: false,
                            full_keyword: "california",
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
                            impression_url: None,
                            click_url: None,
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `cal`; no types",
                SuggestionQuery {
                    keyword: "cal".into(),
                    include_sponsored: false,
                    include_non_sponsored: false,
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
}
