/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{convert::TryFrom, fmt, time::SystemTime};

use dogear::{
    debug, AbortSignal, CompletionOps, Content, Deletion, Guid, Item, Kind, MergedRoot, Tree,
    Upload, Validity,
};
use nsstring::nsString;
use storage::{Conn, Step};
use xpcom::interfaces::{mozISyncedBookmarksMerger, nsINavBookmarksService};

use crate::driver::{AbortController, Driver};
use crate::error::{Error, Result};

pub const LMANNO_FEEDURI: &'static str = "livemark/feedURI";

const SQLITE_MAX_VARIABLE_NUMBER: usize = 999;

extern "C" {
    fn NS_NavBookmarksTotalSyncChanges() -> i64;
}

fn total_sync_changes() -> i64 {
    unsafe { NS_NavBookmarksTotalSyncChanges() }
}

pub struct Store<'s> {
    db: &'s mut Conn,
    driver: &'s Driver,
    controller: &'s AbortController,

    /// The total Sync change count before merging. We store this before
    /// accessing Places, and compare the current and stored counts after
    /// opening our transaction. If they match, we can safely apply the
    /// tree. Otherwise, we bail and try merging again on the next sync.
    total_sync_changes: i64,

    local_time_millis: i64,
    remote_time_millis: i64,
    weak_uploads: &'s [nsString],
}

impl<'s> Store<'s> {
    pub fn new(
        db: &'s mut Conn,
        driver: &'s Driver,
        controller: &'s AbortController,
        local_time_millis: i64,
        remote_time_millis: i64,
        weak_uploads: &'s [nsString],
    ) -> Store<'s> {
        Store {
            db,
            driver,
            controller,
            total_sync_changes: total_sync_changes(),
            local_time_millis,
            remote_time_millis,
            weak_uploads,
        }
    }

    /// Ensures that all local roots are parented correctly.
    ///
    /// The Places root can't be in another folder, or we'll recurse infinitely
    /// when we try to fetch the local tree.
    ///
    /// The five built-in roots should be under the Places root, or we'll build
    /// and sync an invalid tree (bug 1453994, bug 1472127).
    pub fn validate(&self) -> Result<()> {
        self.controller.err_if_aborted()?;
        let mut statement = self.db.prepare(format!(
            "SELECT NOT EXISTS(
               SELECT 1 FROM moz_bookmarks
               WHERE id = (SELECT parent FROM moz_bookmarks
                           WHERE guid = '{0}')
             ) AND NOT EXISTS(
               SELECT 1 FROM moz_bookmarks b
               JOIN moz_bookmarks p ON p.id = b.parent
               WHERE b.guid IN ('{1}', '{2}', '{3}', '{4}', '{5}') AND
                     p.guid <> '{0}'
             )",
            dogear::ROOT_GUID,
            dogear::MENU_GUID,
            dogear::MOBILE_GUID,
            dogear::TAGS_GUID,
            dogear::TOOLBAR_GUID,
            dogear::UNFILED_GUID,
        ))?;
        let has_valid_roots = match statement.step()? {
            Some(row) => row.get_by_index::<i64>(0)? == 1,
            None => false,
        };
        if has_valid_roots {
            Ok(())
        } else {
            Err(Error::InvalidLocalRoots)
        }
    }

    /// Prepares the mirror database for a merge.
    pub fn prepare(&self) -> Result<()> {
        // Sync associates keywords with bookmarks, and doesn't sync POST data;
        // Places associates keywords with (URL, POST data) pairs, and multiple
        // bookmarks may have the same URL. When a keyword changes, clients
        // should reupload all bookmarks with the affected URL (see
        // `PlacesSyncUtils.bookmarks.addSyncChangesForBookmarksWithURL` and
        // bug 1328737). Just in case, we flag any remote bookmarks that have
        // different keywords for the same URL, or the same keyword for
        // different URLs, for reupload.
        self.controller.err_if_aborted()?;
        self.db.exec(format!(
            "UPDATE items SET
               validity = {}
             WHERE validity = {} AND (
               urlId IN (
                 /* Same URL, different keywords. `COUNT` ignores NULLs, so
                    we need to count them separately. This handles cases where
                    a keyword was removed from one, but not all bookmarks with
                    the same URL. */
                 SELECT urlId FROM items
                 GROUP BY urlId
                 HAVING COUNT(DISTINCT keyword) +
                        COUNT(DISTINCT CASE WHEN keyword IS NULL
                                       THEN 1 END) > 1
               ) OR keyword IN (
                 /* Different URLs, same keyword. Bookmarks with keywords but
                    without URLs are already invalid, so we don't need to handle
                    NULLs here. */
                 SELECT keyword FROM items
                 WHERE keyword NOT NULL
                 GROUP BY keyword
                 HAVING COUNT(DISTINCT urlId) > 1
               )
             )",
            mozISyncedBookmarksMerger::VALIDITY_REUPLOAD,
            mozISyncedBookmarksMerger::VALIDITY_VALID,
        ))?;
        Ok(())
    }

    /// Creates a local tree item from a row in the `localItems` CTE.
    fn local_row_to_item(&self, step: &Step) -> Result<(Item, Option<Content>)> {
        let raw_guid: nsString = step.get_by_name("guid")?;
        let guid = Guid::from_utf16(&*raw_guid)?;

        let raw_url_href: Option<nsString> = step.get_by_name("url")?;
        let url_href = match raw_url_href {
            Some(raw_url_href) => Some(String::from_utf16(&*raw_url_href)?),
            None => None,
        };

        let typ: i64 = step.get_by_name("type")?;
        let kind = match typ {
            nsINavBookmarksService::TYPE_BOOKMARK => match url_href.as_ref() {
                Some(u) if u.starts_with("place:") => Kind::Query,
                _ => Kind::Bookmark,
            },
            nsINavBookmarksService::TYPE_FOLDER => {
                let is_livemark: i64 = step.get_by_name("isLivemark")?;
                if is_livemark == 1 {
                    Kind::Livemark
                } else {
                    Kind::Folder
                }
            }
            nsINavBookmarksService::TYPE_SEPARATOR => Kind::Separator,
            _ => return Err(Error::UnknownItemType(typ)),
        };

        let mut item = Item::new(guid, kind);

        let local_modified: i64 = step.get_by_name_or_default("localModified");
        item.age = (self.local_time_millis - local_modified).max(0);

        let sync_change_counter: i64 = step.get_by_name("syncChangeCounter")?;
        item.needs_merge = sync_change_counter > 0;

        let content = if item.guid == dogear::ROOT_GUID {
            None
        } else {
            let sync_status: i64 = step.get_by_name("syncStatus")?;
            match sync_status {
                nsINavBookmarksService::SYNC_STATUS_NORMAL => None,
                _ => match kind {
                    Kind::Bookmark | Kind::Query => {
                        let raw_title: nsString = step.get_by_name("title")?;
                        let title = String::from_utf16(&*raw_title)?;
                        url_href.map(|url_href| Content::Bookmark { title, url_href })
                    }
                    Kind::Folder | Kind::Livemark => {
                        let raw_title: nsString = step.get_by_name("title")?;
                        let title = String::from_utf16(&*raw_title)?;
                        Some(Content::Folder { title })
                    }
                    Kind::Separator => Some(Content::Separator),
                },
            }
        };

        Ok((item, content))
    }

    /// Creates a remote tree item from a row in `mirror.items`.
    fn remote_row_to_item(&self, step: &Step) -> Result<(Item, Option<Content>)> {
        let raw_guid: nsString = step.get_by_name("guid")?;
        let guid = Guid::from_utf16(&*raw_guid)?;

        let raw_kind: i64 = step.get_by_name("kind")?;
        let kind = Kind::from_column(raw_kind)?;

        let mut item = Item::new(guid, kind);

        let remote_modified: i64 = step.get_by_name("serverModified")?;
        item.age = (self.remote_time_millis - remote_modified).max(0);

        let needs_merge: i32 = step.get_by_name("needsMerge")?;
        item.needs_merge = needs_merge == 1;

        let raw_validity: i64 = step.get_by_name("validity")?;
        item.validity = Validity::from_column(raw_validity)?;

        let content = if item.guid == dogear::ROOT_GUID || !item.needs_merge {
            None
        } else {
            match kind {
                Kind::Bookmark | Kind::Query => {
                    let raw_title: nsString = step.get_by_name("title")?;
                    let title = String::from_utf16(&*raw_title)?;
                    let raw_url_href: Option<nsString> = step.get_by_name("url")?;
                    match raw_url_href {
                        Some(raw_url_href) => {
                            let url_href = String::from_utf16(&*raw_url_href)?;
                            Some(Content::Bookmark { title, url_href })
                        }
                        None => None,
                    }
                }
                Kind::Folder | Kind::Livemark => {
                    let raw_title: nsString = step.get_by_name("title")?;
                    let title = String::from_utf16(&*raw_title)?;
                    Some(Content::Folder { title })
                }
                Kind::Separator => Some(Content::Separator),
            }
        };

        Ok((item, content))
    }
}

impl<'s> dogear::Store for Store<'s> {
    type Ok = ApplyStatus;
    type Error = Error;

    /// Builds a fully rooted, consistent tree from the items and tombstones in
    /// Places.
    fn fetch_local_tree(&self) -> Result<Tree> {
        let mut items_statement = self.db.prepare(format!(
            "WITH RECURSIVE
             localItems(id, guid, parentId, parentGuid, position, type, title,
                        parentTitle, placeId, dateAdded, lastModified,
                        syncChangeCounter, syncStatus, level)
             AS (
               SELECT b.id, b.guid, 0, NULL, b.position, b.type, b.title,
                      NULL, b.fk, b.dateAdded, b.lastModified,
                      b.syncChangeCounter, b.syncStatus, 0
               FROM moz_bookmarks b
               WHERE b.guid = 'root________'
               UNION ALL
               SELECT b.id, b.guid, s.id, s.guid, b.position, b.type, b.title,
                      s.title, b.fk, b.dateAdded, b.lastModified,
                      b.syncChangeCounter, b.syncStatus, s.level + 1
               FROM moz_bookmarks b
               JOIN localItems s ON s.id = b.parent
             )
             SELECT s.guid, s.parentGuid, s.type,  s.syncChangeCounter,
                    s.syncStatus, s.lastModified / 1000 AS localModified,
                    IFNULL(s.title, '') AS title, s.position,
                    (SELECT h.url FROM moz_places h
                     WHERE h.id = s.placeId) AS url,
                    EXISTS(SELECT 1 FROM moz_items_annos a
                           JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id
                           WHERE a.item_id = s.id AND
                                 n.name = '{}') AS isLivemark
             FROM localItems s
             ORDER BY s.level, s.parentId, s.position",
            LMANNO_FEEDURI,
        ))?;
        let mut builder = match items_statement.step()? {
            // The first row is always the root.
            Some(step) => {
                let (item, _) = self.local_row_to_item(&step)?;
                Tree::with_root(item)
            }
            None => return Err(Error::InvalidLocalRoots),
        };
        while let Some(step) = items_statement.step()? {
            // All subsequent rows are descendants.
            self.controller.err_if_aborted()?;
            let raw_parent_guid: nsString = step.get_by_name("parentGuid")?;
            let parent_guid = Guid::from_utf16(&*raw_parent_guid)?;

            let (item, content) = self.local_row_to_item(&step)?;
            let mut p = builder.item(item)?;

            if let Some(content) = content {
                p.content(content);
            }

            p.by_structure(&parent_guid)?;
        }

        let mut deletions_statement = self.db.prepare("SELECT guid FROM moz_bookmarks_deleted")?;
        while let Some(step) = deletions_statement.step()? {
            self.controller.err_if_aborted()?;
            let raw_guid: nsString = step.get_by_name("guid")?;
            let guid = Guid::from_utf16(&*raw_guid)?;
            builder.deletion(guid);
        }

        let tree = Tree::try_from(builder)?;
        Ok(tree)
    }

    /// Builds a fully rooted, consistent tree from the items and tombstones in the
    /// mirror.
    fn fetch_remote_tree(&self) -> Result<Tree> {
        let mut root_statement = self.db.prepare(format!(
            "SELECT guid, serverModified, kind, needsMerge, validity
             FROM items
             WHERE guid = '{}'",
            dogear::ROOT_GUID,
        ))?;
        let mut builder = match root_statement.step()? {
            Some(step) => {
                let (item, _) = self.remote_row_to_item(&step)?;
                Tree::with_root(item)
            }
            None => return Err(Error::InvalidRemoteRoots),
        };
        builder.reparent_orphans_to(&dogear::UNFILED_GUID);

        let mut items_statement = self.db.prepare(format!(
            "SELECT v.guid, v.parentGuid, v.serverModified, v.kind,
                    IFNULL(v.title, '') AS title, v.needsMerge, v.validity,
                    v.isDeleted,
                    (SELECT u.url FROM urls u
                     WHERE u.id = v.urlId) AS url
             FROM items v
             WHERE v.guid <> '{}'
             ORDER BY v.guid",
            dogear::ROOT_GUID,
        ))?;
        while let Some(step) = items_statement.step()? {
            self.controller.err_if_aborted()?;

            let is_deleted: i64 = step.get_by_name("isDeleted")?;
            if is_deleted == 1 {
                let raw_guid: nsString = step.get_by_name("guid")?;
                let guid = Guid::from_utf16(&*raw_guid)?;
                builder.deletion(guid);
            } else {
                let (item, content) = self.remote_row_to_item(&step)?;
                let mut p = builder.item(item)?;
                if let Some(content) = content {
                    p.content(content);
                }
                let raw_parent_guid: Option<nsString> = step.get_by_name("parentGuid")?;
                if let Some(raw_parent_guid) = raw_parent_guid {
                    p.by_parent_guid(Guid::from_utf16(&*raw_parent_guid)?)?;
                }
            }
        }

        let mut structure_statement = self.db.prepare(format!(
            "SELECT guid, parentGuid FROM structure
             WHERE guid <> '{}'
             ORDER BY parentGuid, position",
            dogear::ROOT_GUID,
        ))?;
        while let Some(step) = structure_statement.step()? {
            self.controller.err_if_aborted()?;
            let raw_guid: nsString = step.get_by_name("guid")?;
            let guid = Guid::from_utf16(&*raw_guid)?;

            let raw_parent_guid: nsString = step.get_by_name("parentGuid")?;
            let parent_guid = Guid::from_utf16(&*raw_parent_guid)?;

            builder.parent_for(&guid).by_children(&parent_guid)?;
        }

        let tree = Tree::try_from(builder)?;
        Ok(tree)
    }

    fn apply<'t>(&mut self, root: MergedRoot<'t>) -> Result<ApplyStatus> {
        self.controller.err_if_aborted()?;
        let ops = root.completion_ops();

        self.controller.err_if_aborted()?;
        let deletions = root.deletions().collect::<Vec<_>>();

        if ops.is_empty() && deletions.is_empty() && self.weak_uploads.is_empty() {
            // If we don't have any items to apply, upload, or delete,
            // no need to open a transaction at all.
            return Ok(ApplyStatus::Skipped);
        }

        // Apply the merged tree and stage outgoing items. This transaction
        // blocks writes from the main connection until it's committed, so we
        // try to do as little work as possible within it.
        let tx = self.db.transaction()?;
        if self.total_sync_changes != total_sync_changes() {
            return Err(Error::MergeConflict);
        }

        self.controller.err_if_aborted()?;
        debug!(self.driver, "Updating local items in Places");
        update_local_items_in_places(&tx, &self.driver, &ops, deletions)?;

        self.controller.err_if_aborted()?;
        debug!(self.driver, "Staging items to upload");
        stage_items_to_upload(&tx, &ops.upload, &self.weak_uploads)?;

        cleanup(&tx)?;
        tx.commit()?;

        Ok(ApplyStatus::Merged)
    }
}

/// Builds a temporary table with the merge states of all nodes in the merged
/// tree and updates Places to match the merged tree.
///
/// Conceptually, we examine the merge state of each item, and either leave the
/// item unchanged, upload the local side, apply the remote side, or apply and
/// then reupload the remote side with a new structure.
///
/// Note that we update Places and flag items *before* upload, while iOS
/// updates the mirror *after* a successful upload. This simplifies our
/// implementation, though we lose idempotent merges. If upload is interrupted,
/// the next sync won't distinguish between new merge states from the previous
/// sync, and local changes.
fn update_local_items_in_places<'t>(
    db: &Conn,
    driver: &Driver,
    ops: &CompletionOps<'t>,
    deletions: Vec<Deletion>,
) -> Result<()> {
    // Clean up any observer notifications left over from the last sync.
    db.exec(
        "DELETE FROM itemsAdded;
         DELETE FROM guidsChanged;
         DELETE FROM itemsChanged;
         DELETE FROM itemsRemoved;
         DELETE FROM itemsMoved;",
    )?;

    let now = rounded_now();

    // Insert URLs for new remote items into the `moz_places` table. We need to
    // do this before inserting new remote items, since we need Place IDs for
    // both old and new URLs.
    debug!(driver, "Inserting Places for new items");
    for chunk in ops.apply_remote_items.chunks(SQLITE_MAX_VARIABLE_NUMBER) {
        let mut statement = db.prepare(format!(
            "INSERT OR IGNORE INTO moz_places(url, url_hash, rev_host, hidden,
                                              frecency, guid)
             SELECT u.url, u.hash, u.revHost, 0,
                    (CASE v.kind WHEN {} THEN 0 ELSE -1 END),
                    IFNULL((SELECT h.guid FROM moz_places h
                            WHERE h.url_hash = u.hash AND
                                  h.url = u.url), u.guid)
             FROM items v
             JOIN urls u ON u.id = v.urlId
             WHERE v.guid IN ({})",
            mozISyncedBookmarksMerger::KIND_QUERY,
            repeat_sql_vars(chunk.len()),
        ))?;
        for (index, op) in chunk.iter().enumerate() {
            let remote_guid = nsString::from(&*op.remote_node().guid);
            statement.bind_by_index(index as u32, remote_guid)?;
        }
        statement.execute()?;
    }

    // Trigger frecency updates for all new origins.
    debug!(driver, "Updating origins for new URLs");
    db.exec("DELETE FROM moz_updateoriginsinsert_temp")?;

    // Build a table of new and updated items.
    debug!(driver, "Staging apply remote item ops");
    for chunk in ops
        .apply_remote_items
        .chunks(SQLITE_MAX_VARIABLE_NUMBER / 3)
    {
        // CTEs in `WITH` clauses aren't indexed, so this query needs a full
        // table scan on `ops`. But that's okay; a separate temp table for ops
        // would also need a full scan. Note that we need both the local _and_
        // remote GUIDs here, because we haven't changed the local GUIDs yet.
        let mut statement = db.prepare(format!(
            "WITH
             ops(mergedGuid, localGuid, remoteGuid, level) AS (VALUES {})
             INSERT INTO itemsToApply(mergedGuid, localId, remoteId,
                                      remoteGuid, newLevel,
                                      newType,
                                      localDateAddedMicroseconds,
                                      remoteDateAddedMicroseconds,
                                      lastModifiedMicroseconds,
                                      oldTitle, newTitle, oldPlaceId,
                                      newPlaceId,
                                      newKeyword)
             SELECT n.mergedGuid, b.id, v.id,
                    v.guid, n.level,
                    (CASE WHEN v.kind IN ({}, {}) THEN {}
                          WHEN v.kind IN ({}, {}) THEN {}
                          ELSE {}
                     END),
                    b.dateAdded,
                    v.dateAdded * 1000,
                    MAX(v.dateAdded * 1000, {}),
                    b.title, v.title, b.fk,
                    (SELECT h.id FROM moz_places h
                     JOIN urls u ON u.hash = h.url_hash
                     WHERE u.id = v.urlId AND
                           u.url = h.url),
                    v.keyword
             FROM ops n
             JOIN items v ON v.guid = n.remoteGuid
             LEFT JOIN moz_bookmarks b ON b.guid = n.localGuid",
            repeat_display(chunk.len(), ",", |index, f| {
                let op = &chunk[index];
                write!(f, "(?, ?, ?, {})", op.level)
            }),
            mozISyncedBookmarksMerger::KIND_BOOKMARK,
            mozISyncedBookmarksMerger::KIND_QUERY,
            nsINavBookmarksService::TYPE_BOOKMARK,
            mozISyncedBookmarksMerger::KIND_FOLDER,
            mozISyncedBookmarksMerger::KIND_LIVEMARK,
            nsINavBookmarksService::TYPE_FOLDER,
            nsINavBookmarksService::TYPE_SEPARATOR,
            now,
        ))?;
        for (index, op) in chunk.iter().enumerate() {
            let offset = (index * 3) as u32;

            // In most cases, the merged and remote GUIDs are the same for new
            // items. For updates, all three are typically the same. We could
            // try to avoid binding duplicates, but that complicates chunking,
            // and we don't expect many items to change after the first sync.
            let merged_guid = nsString::from(&*op.merged_node.guid);
            statement.bind_by_index(offset, merged_guid)?;

            let local_guid = op
                .merged_node
                .merge_state
                .local_node()
                .map(|node| nsString::from(&*node.guid));
            statement.bind_by_index(offset + 1, local_guid)?;

            let remote_guid = nsString::from(&*op.remote_node().guid);
            statement.bind_by_index(offset + 2, remote_guid)?;
        }
        statement.execute()?;
    }

    debug!(driver, "Staging change GUID ops");
    for chunk in ops.change_guids.chunks(SQLITE_MAX_VARIABLE_NUMBER / 2) {
        let mut statement = db.prepare(format!(
            "INSERT INTO changeGuidOps(localGuid, mergedGuid, syncStatus, level,
                                       lastModifiedMicroseconds)
             VALUES {}",
            repeat_display(chunk.len(), ",", |index, f| {
                let op = &chunk[index];
                // If only the local GUID changed, the item was deduped, so we
                // can mark it as syncing. Otherwise, we changed an invalid
                // GUID locally or remotely, so we leave its original sync
                // status in place until we've uploaded it.
                let sync_status = if op.merged_node.remote_guid_changed() {
                    None
                } else {
                    Some(nsINavBookmarksService::SYNC_STATUS_NORMAL)
                };
                write!(
                    f,
                    "(?, ?, {}, {}, {})",
                    NullableFragment(sync_status),
                    op.level,
                    now
                )
            })
        ))?;
        for (index, op) in chunk.iter().enumerate() {
            let offset = (index * 2) as u32;

            let local_guid = nsString::from(&*op.local_node().guid);
            statement.bind_by_index(offset, local_guid)?;

            let merged_guid = nsString::from(&*op.merged_node.guid);
            statement.bind_by_index(offset + 1, merged_guid)?;
        }
        statement.execute()?;
    }

    debug!(driver, "Staging apply new local structure ops");
    for chunk in ops
        .apply_new_local_structure
        .chunks(SQLITE_MAX_VARIABLE_NUMBER / 2)
    {
        let mut statement = db.prepare(format!(
            "INSERT INTO applyNewLocalStructureOps(mergedGuid, mergedParentGuid,
                                                   position, level,
                                                   lastModifiedMicroseconds)
             VALUES {}",
            repeat_display(chunk.len(), ",", |index, f| {
                let op = &chunk[index];
                write!(f, "(?, ?, {}, {}, {})", op.position, op.level, now)
            })
        ))?;
        for (index, op) in chunk.iter().enumerate() {
            let offset = (index * 2) as u32;

            let merged_guid = nsString::from(&*op.merged_node.guid);
            statement.bind_by_index(offset, merged_guid)?;

            let merged_parent_guid = nsString::from(&*op.merged_parent_node.guid);
            statement.bind_by_index(offset + 1, merged_parent_guid)?;
        }
        statement.execute()?;
    }

    debug!(driver, "Staging deletions");
    for chunk in deletions.chunks(SQLITE_MAX_VARIABLE_NUMBER) {
        // Inserting into `itemsToRemove` fires the `noteItemRemoved` trigger,
        // which records info for deleted item observer notifications. It's
        // important we do this before updating the structure, so that the
        // trigger captures the old parent and position.
        let mut statement = db.prepare(format!(
            "INSERT INTO itemsToRemove(guid, localLevel, shouldUploadTombstone,
                                       dateRemovedMicroseconds)
             VALUES {}",
            repeat_display(chunk.len(), ",", |index, f| {
                let d = &chunk[index];
                write!(
                    f,
                    "(?, {}, {}, {})",
                    d.local_level, d.should_upload_tombstone as i8, now,
                )
            })
        ))?;
        for (index, d) in chunk.iter().enumerate() {
            statement.bind_by_index(index as u32, nsString::from(d.guid.as_str()))?;
        }
        statement.execute()?;
    }

    // Remove tombstones for revived items. We intentionally use three `IN`
    // subqueries instead of one with `UNION ALL`s because SQLite's query
    // planner can't unroll the latter into a multi-index OR.
    debug!(driver, "Removing tombstones for revived items");
    db.exec(
        "DELETE FROM moz_bookmarks_deleted
         WHERE guid IN (SELECT mergedGuid FROM itemsToApply) OR
               guid IN (SELECT remoteGuid FROM itemsToApply) OR
               guid IN (SELECT localGuid FROM changeGuidOps)",
    )?;

    debug!(driver, "Removing local items");
    remove_local_items(&db)?;

    // Fires the `changeGuids` trigger.
    debug!(driver, "Changing GUIDs");
    db.exec("DELETE FROM changeGuidOps")?;

    debug!(driver, "Applying remote items");
    apply_remote_items(&db)?;

    // Trigger frecency updates for all affected origins.
    debug!(driver, "Updating origins for changed URLs");
    db.exec("DELETE FROM moz_updateoriginsupdate_temp")?;

    // Fires the `applyNewLocalStructure` trigger.
    debug!(driver, "Applying new local structure");
    db.exec("DELETE FROM applyNewLocalStructureOps")?;

    // Reset the change counter for items that we don't need to upload.
    debug!(driver, "Applying skip upload ops");
    for chunk in ops.skip_upload.chunks(SQLITE_MAX_VARIABLE_NUMBER) {
        let mut statement = db.prepare(format!(
            "UPDATE moz_bookmarks SET
               syncChangeCounter = 0
             WHERE guid IN ({})",
            repeat_sql_vars(chunk.len()),
        ))?;
        for (index, op) in chunk.iter().enumerate() {
            statement.bind_by_index(index as u32, nsString::from(&*op.merged_node.guid))?;
        }
        statement.execute()?;
    }

    // Bump the counter for items that we should upload.
    debug!(driver, "Applying flag for upload ops");
    for chunk in ops.flag_for_upload.chunks(SQLITE_MAX_VARIABLE_NUMBER) {
        let mut statement = db.prepare(format!(
            "UPDATE moz_bookmarks SET
               syncChangeCounter = 1
             WHERE guid IN ({})",
            repeat_sql_vars(chunk.len()),
        ))?;
        for (index, op) in chunk.iter().enumerate() {
            statement.bind_by_index(index as u32, nsString::from(&*op.merged_node.guid))?;
        }
        statement.execute()?;
    }

    // Flag applied remote items as merged in the mirror.
    debug!(driver, "Applying flag as merged ops");
    for chunk in ops.flag_as_merged.chunks(SQLITE_MAX_VARIABLE_NUMBER) {
        let mut statement = db.prepare(format!(
            "UPDATE items SET
               needsMerge = 0
             WHERE guid IN ({})",
            repeat_sql_vars(chunk.len()),
        ))?;
        for (index, op) in chunk.iter().enumerate() {
            statement.bind_by_index(index as u32, nsString::from(op.guid().as_str()))?;
        }
        statement.execute()?;
    }

    Ok(())
}

/// Upserts all new and updated items from the `itemsToApply` table into Places.
fn apply_remote_items(db: &Conn) -> Result<()> {
    // Record item added notifications for new items.
    db.exec(
        "INSERT INTO itemsAdded(guid, keywordChanged, level)
         SELECT n.mergedGuid, n.newKeyword NOT NULL OR
                              EXISTS(SELECT 1 FROM moz_keywords k
                                     WHERE k.place_id = n.newPlaceId OR
                                           k.keyword = n.newKeyword),
                n.newLevel
         FROM itemsToApply n
         WHERE n.localId IS NULL",
    )?;

    // Record item changed notifications for existing items.
    db.exec(
        "INSERT INTO itemsChanged(itemId, oldTitle, oldPlaceId, keywordChanged,
                                 level)
        SELECT n.localId, n.oldTitle, n.oldPlaceId,
               n.newKeyword NOT NULL OR EXISTS(
                 SELECT 1 FROM moz_keywords k
                 WHERE k.place_id IN (n.oldPlaceId, n.newPlaceId) OR
                       k.keyword = n.newKeyword
               ),
               n.newLevel
        FROM itemsToApply n
        WHERE n.localId NOT NULL",
    )?;

    // Remove all keywords from old and new URLs, and remove new keywords from
    // all existing URLs. The `NOT NULL` conditions are important; they ensure
    // that SQLite uses our partial indexes, instead of a table scan.
    db.exec(
        "DELETE FROM moz_keywords
         WHERE place_id IN (SELECT oldPlaceId FROM itemsToApply
                            WHERE oldPlaceId NOT NULL) OR
               place_id IN (SELECT newPlaceId FROM itemsToApply
                            WHERE newPlaceId NOT NULL) OR
               keyword IN (SELECT newKeyword FROM itemsToApply
                           WHERE newKeyword NOT NULL)
    ",
    )?;

    // Remove existing tags.
    db.exec(
        "DELETE FROM localTags
         WHERE placeId IN (SELECT oldPlaceId FROM itemsToApply
                           WHERE oldPlaceId NOT NULL) OR
               placeId IN (SELECT newPlaceId FROM itemsToApply
                           WHERE newPlaceId NOT NULL)",
    )?;

    // Insert new items, using -1 as a placeholder for the parent ID and
    // position. We'll update these later, when we apply the new local
    // structure. This is a full table scan on `itemsToApply`. The no-op
    // `WHERE` clause is necessary to avoid a parsing ambiguity.
    db.exec(format!(
        "INSERT INTO moz_bookmarks(id, guid, parent, position, type, fk, title,
                                   dateAdded,
                                   lastModified,
                                   syncStatus, syncChangeCounter)
         SELECT localId, mergedGuid, -1, -1, newType, newPlaceId, newTitle,
                /* Pick the older of the local and remote date added. We'll
                   weakly reupload any items with an older local date. */
                MIN(IFNULL(localDateAddedMicroseconds,
                           remoteDateAddedMicroseconds),
                    remoteDateAddedMicroseconds),
                /* The last modified date should always be newer than the date
                   added, so we pick the newer of the two here. */
                MAX(lastModifiedMicroseconds, remoteDateAddedMicroseconds),
                {}, 0
         FROM itemsToApply
         WHERE 1
         ON CONFLICT(id) DO UPDATE SET
           title = excluded.title,
           dateAdded = excluded.dateAdded,
           lastModified = excluded.lastModified,
           /* It's important that we update the URL *after* removing old keywords
              and *before* inserting new ones, so that the above DELETEs select
              the correct affected items. */
           fk = excluded.fk",
        nsINavBookmarksService::SYNC_STATUS_NORMAL
    ))?;

    // Flag frecencies for recalculation. This is a multi-index OR that uses the
    // `oldPlacesIds` and `newPlaceIds` partial indexes, since `<>` is only true
    // if both terms are not NULL. Without those constraints, the subqueries
    // would scan `itemsToApply` twice. The `oldPlaceId <> newPlaceId` and
    // `newPlaceId <> oldPlaceId` checks exclude items where the URL didn't
    // change; we don't need to recalculate their frecencies.
    db.exec(
        "UPDATE moz_places SET
           frecency = -frecency
         WHERE frecency > 0 AND (
           id IN (
             SELECT oldPlaceId FROM itemsToApply
             WHERE oldPlaceId <> newPlaceId
           ) OR id IN (
             SELECT newPlaceId FROM itemsToApply
             WHERE newPlaceId <> oldPlaceId
           )
         )",
    )?;

    // Insert new keywords for new URLs.
    db.exec(
        "INSERT OR IGNORE INTO moz_keywords(keyword, place_id, post_data)
         SELECT newKeyword, newPlaceId, ''
         FROM itemsToApply
         WHERE newKeyword NOT NULL",
    )?;

    // Insert new tags for new URLs.
    db.exec(
        "INSERT INTO localTags(tag, placeId, lastModifiedMicroseconds)
         SELECT t.tag, n.newPlaceId, n.lastModifiedMicroseconds
         FROM itemsToApply n
         JOIN tags t ON t.itemId = n.remoteId",
    )?;

    Ok(())
}

/// Removes items that are deleted on one or both sides from Places, and inserts
/// new tombstones for non-syncable items to delete remotely.
fn remove_local_items(db: &Conn) -> Result<()> {
    // Record observer notifications for removed items.
    db.exec(
        "INSERT INTO itemsRemoved(itemId, parentId, position, type, placeId,
                                  guid, parentGuid, level)
         SELECT b.id, b.parent, b.position, b.type, b.fk,
                b.guid, p.guid, d.localLevel
         FROM itemsToRemove d
         JOIN moz_bookmarks b ON b.guid = d.guid
         JOIN moz_bookmarks p ON p.id = b.parent",
    )?;

    // Flag URL frecency for recalculation.
    db.exec(
        "UPDATE moz_places SET
           frecency = -frecency
         WHERE id IN (SELECT b.fk FROM moz_bookmarks b
                      JOIN itemsToRemove d ON d.guid = b.guid) AND
               frecency > 0",
    )?;

    // Remove annos for the deleted items. This can be removed in bug 1460577.
    db.exec(
        "DELETE FROM moz_items_annos
         WHERE item_id = (SELECT b.id FROM moz_bookmarks b
                          JOIN itemsToRemove d ON d.guid = b.guid)",
    )?;

    // Don't reupload tombstones for items that are already deleted on the
    // server.
    db.exec(
        "DELETE FROM moz_bookmarks_deleted
         WHERE guid IN (SELECT guid FROM itemsToRemove
                        WHERE NOT shouldUploadTombstone)",
    )?;

    // Upload tombstones for non-syncable items. We can remove the
    // `shouldUploadTombstone` check and persist tombstones unconditionally in
    // bug 1343103.
    db.exec(
        "INSERT OR IGNORE INTO moz_bookmarks_deleted(guid, dateRemoved)
         SELECT guid, dateRemovedMicroseconds
         FROM itemsToRemove
         WHERE shouldUploadTombstone",
    )?;

    // Remove the item from Places.
    db.exec(
        "DELETE FROM moz_bookmarks
         WHERE guid IN (SELECT guid FROM itemsToRemove)",
    )?;

    Ok(())
}

/// Stores a snapshot of all locally changed items in a temporary table for
/// upload. This is called from within the merge transaction, to ensure that
/// changes made during the sync don't cause us to upload inconsistent records.
///
/// For an example of why we use a temporary table instead of reading directly
/// from Places, consider a user adding a bookmark, then changing its parent
/// folder. We first add the bookmark to the default folder, bump the change
/// counter of the new bookmark and the default folder, then trigger a sync.
/// Depending on how quickly the user picks the new parent, we might upload
/// a record for the default folder, commit the move, then upload the bookmark.
/// We'll still upload the new parent on the next sync, but, in the meantime,
/// we've introduced a parent-child disagreement. This can also happen if the
/// user moves many items between folders.
///
/// Conceptually, `itemsToUpload` is a transient "view" of locally changed
/// items. The change counter in Places is the persistent record of items that
/// we need to upload, so, if upload is interrupted or fails, we'll stage the
/// items again on the next sync.
fn stage_items_to_upload(db: &Conn, upload: &[Upload], weak_upload: &[nsString]) -> Result<()> {
    // Clean up staged items left over from the last sync.
    db.exec("DELETE FROM itemsToUpload")?;

    // Stage explicit weak uploads such as repair responses.
    for chunk in weak_upload.chunks(SQLITE_MAX_VARIABLE_NUMBER) {
        let mut statement = db.prepare(format!(
            "INSERT INTO itemsToUpload(id, guid, syncChangeCounter, parentGuid,
                                       parentTitle, dateAdded, type, title,
                                       placeId, isQuery, url, keyword, position,
                                       tagFolderName)
             {}
             WHERE b.guid IN ({})",
            UploadItemsFragment("b"),
            repeat_sql_vars(chunk.len()),
        ))?;
        for (index, guid) in chunk.iter().enumerate() {
            statement.bind_by_index(index as u32, nsString::from(guid.as_ref()))?;
        }
        statement.execute()?;
    }

    // Stage remotely changed items with older local creation dates. These are
    // tracked "weakly": if the upload is interrupted or fails, we won't
    // reupload the record on the next sync.
    db.exec(format!(
        "INSERT OR IGNORE INTO itemsToUpload(id, guid, syncChangeCounter,
                                             parentGuid, parentTitle, dateAdded,
                                             type, title, placeId, isQuery, url,
                                             keyword, position, tagFolderName)
         {}
         JOIN itemsToApply n ON n.mergedGuid = b.guid
         WHERE n.localDateAddedMicroseconds < n.remoteDateAddedMicroseconds",
        UploadItemsFragment("b"),
    ))?;

    // Stage remaining locally changed items for upload.
    for chunk in upload.chunks(SQLITE_MAX_VARIABLE_NUMBER) {
        let mut statement = db.prepare(format!(
            "INSERT OR IGNORE INTO itemsToUpload(id, guid, syncChangeCounter,
                                                 parentGuid, parentTitle,
                                                 dateAdded, type, title,
                                                 placeId, isQuery, url, keyword,
                                                 position, tagFolderName)
             {}
             WHERE b.guid IN ({})",
            UploadItemsFragment("b"),
            repeat_sql_vars(chunk.len()),
        ))?;
        for (index, op) in chunk.iter().enumerate() {
            statement.bind_by_index(index as u32, nsString::from(&*op.merged_node.guid))?;
        }
        statement.execute()?;
    }

    // Record the child GUIDs of locally changed folders, which we use to
    // populate the `children` array in the record.
    db.exec(
        "
        INSERT INTO structureToUpload(guid, parentId, position)
        SELECT b.guid, b.parent, b.position
        FROM moz_bookmarks b
        JOIN itemsToUpload o ON o.id = b.parent",
    )?;

    // Stage tags for outgoing bookmarks.
    db.exec(
        "
        INSERT INTO tagsToUpload(id, tag)
        SELECT o.id, t.tag
        FROM localTags t
        JOIN itemsToUpload o ON o.placeId = t.placeId",
    )?;

    // Finally, stage tombstones for deleted items. Ignore conflicts if we have
    // tombstones for undeleted items; Places Maintenance should clean these up.
    db.exec(
        "
        INSERT OR IGNORE INTO itemsToUpload(guid, syncChangeCounter, isDeleted)
        SELECT guid, 1, 1 FROM moz_bookmarks_deleted",
    )?;

    Ok(())
}

fn cleanup(db: &Conn) -> Result<()> {
    db.exec(
        "DELETE FROM itemsToApply;
         DELETE FROM itemsToRemove;",
    )?;
    Ok(())
}

/// Formats a list of binding parameters for inclusion in a SQL list.
#[inline]
fn repeat_sql_vars(count: usize) -> impl fmt::Display {
    repeat_display(count, ",", |_, f| write!(f, "?"))
}

/// Construct a `RepeatDisplay` that will repeatedly call `fmt_one` with a
/// formatter `count` times, separated by `sep`. This is copied from the
/// `sql_support` crate in `application-services`.
#[inline]
fn repeat_display<'a, F>(count: usize, sep: &'a str, fmt_one: F) -> RepeatDisplay<'a, F>
where
    F: Fn(usize, &mut fmt::Formatter) -> fmt::Result,
{
    RepeatDisplay {
        count,
        sep,
        fmt_one,
    }
}

/// Helper type for printing repeated strings more efficiently.
#[derive(Debug, Clone)]
struct RepeatDisplay<'a, F> {
    count: usize,
    sep: &'a str,
    fmt_one: F,
}

impl<'a, F> fmt::Display for RepeatDisplay<'a, F>
where
    F: Fn(usize, &mut fmt::Formatter) -> fmt::Result,
{
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        for i in 0..self.count {
            if i != 0 {
                f.write_str(self.sep)?;
            }
            (self.fmt_one)(i, f)?;
        }
        Ok(())
    }
}

/// Converts between a type `T` and its SQL representation.
trait Column<T> {
    fn from_column(raw: T) -> Result<Self>
    where
        Self: Sized;
    fn into_column(self) -> T;
}

impl Column<i64> for Kind {
    fn from_column(raw: i64) -> Result<Kind> {
        Ok(match raw {
            mozISyncedBookmarksMerger::KIND_BOOKMARK => Kind::Bookmark,
            mozISyncedBookmarksMerger::KIND_QUERY => Kind::Query,
            mozISyncedBookmarksMerger::KIND_FOLDER => Kind::Folder,
            mozISyncedBookmarksMerger::KIND_LIVEMARK => Kind::Livemark,
            mozISyncedBookmarksMerger::KIND_SEPARATOR => Kind::Separator,
            _ => return Err(Error::UnknownItemKind(raw)),
        })
    }

    fn into_column(self) -> i64 {
        match self {
            Kind::Bookmark => mozISyncedBookmarksMerger::KIND_BOOKMARK,
            Kind::Query => mozISyncedBookmarksMerger::KIND_QUERY,
            Kind::Folder => mozISyncedBookmarksMerger::KIND_FOLDER,
            Kind::Livemark => mozISyncedBookmarksMerger::KIND_LIVEMARK,
            Kind::Separator => mozISyncedBookmarksMerger::KIND_SEPARATOR,
        }
    }
}

impl Column<i64> for Validity {
    fn from_column(raw: i64) -> Result<Validity> {
        Ok(match raw {
            mozISyncedBookmarksMerger::VALIDITY_VALID => Validity::Valid,
            mozISyncedBookmarksMerger::VALIDITY_REUPLOAD => Validity::Reupload,
            mozISyncedBookmarksMerger::VALIDITY_REPLACE => Validity::Replace,
            _ => return Err(Error::UnknownItemValidity(raw).into()),
        })
    }

    fn into_column(self) -> i64 {
        match self {
            Validity::Valid => mozISyncedBookmarksMerger::VALIDITY_VALID,
            Validity::Reupload => mozISyncedBookmarksMerger::VALIDITY_REUPLOAD,
            Validity::Replace => mozISyncedBookmarksMerger::VALIDITY_REPLACE,
        }
    }
}

/// Formats an optional value so that it can be included in a SQL statement.
struct NullableFragment<T>(Option<T>);

impl<T> fmt::Display for NullableFragment<T>
where
    T: fmt::Display,
{
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match &self.0 {
            Some(v) => v.fmt(f),
            None => write!(f, "NULL"),
        }
    }
}

/// Formats a `SELECT` statement for staging local items in the `itemsToUpload`
/// table.
struct UploadItemsFragment(&'static str);

impl fmt::Display for UploadItemsFragment {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "SELECT {0}.id, {0}.guid, {}.syncChangeCounter,
                       p.guid AS parentGuid, p.title AS parentTitle,
                       {0}.dateAdded / 1000 AS dateAdded, {0}.type, {0}.title,
                       h.id AS placeId,
                       IFNULL(substr(h.url, 1, 6) = 'place:', 0) AS isQuery,
                       h.url,
                       (SELECT keyword FROM moz_keywords WHERE place_id = h.id),
                       {0}.position,
                       (SELECT get_query_param(substr(url, 7), 'tag')
                        WHERE substr(h.url, 1, 6) = 'place:') AS tagFolderName
                FROM moz_bookmarks {0}
                JOIN moz_bookmarks p ON p.id = {0}.parent
                LEFT JOIN moz_places h ON h.id = {0}.fk",
            self.0
        )
    }
}

/// Returns the current time, in microseconds, rounded to the nearest
/// millisecond.
fn rounded_now() -> u64 {
    SystemTime::now()
        .duration_since(SystemTime::UNIX_EPOCH)
        .map(|d| (d.as_secs() as u64) * 1_000_000 + u64::from(d.subsec_millis()) * 1000)
        .unwrap_or(0)
}

pub enum ApplyStatus {
    Merged,
    Skipped,
}

impl From<ApplyStatus> for bool {
    fn from(status: ApplyStatus) -> bool {
        match status {
            ApplyStatus::Merged => true,
            ApplyStatus::Skipped => false,
        }
    }
}
