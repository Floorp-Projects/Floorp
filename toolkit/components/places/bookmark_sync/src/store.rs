/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{collections::HashMap, convert::TryFrom, fmt};

use dogear::{
    AbortSignal, Content, Deletion, Guid, Item, Kind, MergedDescendant, MergedRoot, Tree,
    UploadReason, Validity,
};
use nsstring::{nsCString, nsString};
use storage::{Conn, Step};
use xpcom::interfaces::{mozISyncedBookmarksMerger, nsINavBookmarksService};

use crate::driver::AbortController;
use crate::error::{Error, Result};

pub const LMANNO_FEEDURI: &'static str = "livemark/feedURI";

pub const LOCAL_ITEMS_SQL_FRAGMENT: &str = "
  localItems(id, guid, parentId, parentGuid, position, type, title, parentTitle,
             placeId, dateAdded, lastModified, syncChangeCounter, level) AS (
  SELECT b.id, b.guid, 0, NULL, b.position, b.type, b.title, NULL,
         b.fk, b.dateAdded, b.lastModified, b.syncChangeCounter, 0
  FROM moz_bookmarks b
  WHERE b.guid = 'root________'
  UNION ALL
  SELECT b.id, b.guid, s.id, s.guid, b.position, b.type, b.title, s.title,
         b.fk, b.dateAdded, b.lastModified, b.syncChangeCounter, s.level + 1
  FROM moz_bookmarks b
  JOIN localItems s ON s.id = b.parent
  WHERE b.guid <> 'tags________')";

extern "C" {
    fn NS_NavBookmarksTotalSyncChanges() -> libc::int64_t;
}

fn total_sync_changes() -> i64 {
    unsafe { NS_NavBookmarksTotalSyncChanges() }
}

pub struct Store<'s> {
    db: &'s mut Conn,
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
        controller: &'s AbortController,
        local_time_millis: i64,
        remote_time_millis: i64,
        weak_uploads: &'s [nsString],
    ) -> Store<'s> {
        Store {
            db,
            controller,
            total_sync_changes: total_sync_changes(),
            local_time_millis,
            remote_time_millis,
            weak_uploads,
        }
    }

    /// Creates a local tree item from a row in the `localItems` CTE.
    fn local_row_to_item(&self, step: &Step) -> Result<Item> {
        let raw_guid: nsString = step.get_by_name("guid")?;
        let raw_kind: i64 = step.get_by_name("kind")?;

        let guid = Guid::from_utf16(&*raw_guid)?;
        let mut item = Item::new(guid, Kind::from_column(raw_kind)?);

        let local_modified: i64 = step.get_by_name_or_default("localModified");
        item.age = (self.local_time_millis - local_modified).max(0);

        let sync_change_counter: i64 = step.get_by_name("syncChangeCounter")?;
        item.needs_merge = sync_change_counter > 0;

        Ok(item)
    }

    /// Creates a remote tree item from a row in `mirror.items`.
    fn remote_row_to_item(&self, step: &Step) -> Result<Item> {
        let raw_guid: nsString = step.get_by_name("guid")?;
        let raw_kind: i64 = step.get_by_name("kind")?;

        let guid = Guid::from_utf16(&*raw_guid)?;
        let mut item = Item::new(guid, Kind::from_column(raw_kind)?);

        let remote_modified: i64 = step.get_by_name("serverModified")?;
        item.age = (self.remote_time_millis - remote_modified).max(0);

        let needs_merge: i32 = step.get_by_name("needsMerge")?;
        item.needs_merge = needs_merge == 1;

        let raw_validity: i64 = step.get_by_name("validity")?;
        item.validity = Validity::from_column(raw_validity)?;

        Ok(item)
    }
}

impl<'s> dogear::Store<Error> for Store<'s> {
    /// Builds a fully rooted, consistent tree from the items and tombstones in
    /// Places.
    fn fetch_local_tree(&self) -> Result<Tree> {
        let mut items_statement = self.db.prepare(format!(
            "WITH RECURSIVE
             {}
             SELECT s.id, s.guid, s.parentGuid,
                    /* Map Places item types to Sync record kinds. */
                    (CASE s.type
                       WHEN :bookmarkType THEN (
                         CASE SUBSTR((SELECT h.url FROM moz_places h
                                      WHERE h.id = s.placeId), 1, 6)
                         /* Queries are bookmarks with a `place:` URL scheme. */
                         WHEN 'place:' THEN :queryKind
                         ELSE :bookmarkKind END)
                       WHEN :folderType THEN (
                         CASE WHEN EXISTS(
                           /* Livemarks are folders with a feed URL annotation. */
                           SELECT 1 FROM moz_items_annos a
                           JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id
                           WHERE a.item_id = s.id AND
                                 n.name = :feedURLAnno
                         ) THEN :livemarkKind
                         ELSE :folderKind END)
                       ELSE :separatorKind END) AS kind,
                    s.lastModified / 1000 AS localModified, s.syncChangeCounter
             FROM localItems s
             ORDER BY s.level, s.parentId, s.position",
            LOCAL_ITEMS_SQL_FRAGMENT
        ))?;
        items_statement.bind_by_name("bookmarkType", nsINavBookmarksService::TYPE_BOOKMARK)?;
        items_statement.bind_by_name("queryKind", mozISyncedBookmarksMerger::KIND_QUERY)?;
        items_statement.bind_by_name("bookmarkKind", mozISyncedBookmarksMerger::KIND_BOOKMARK)?;
        items_statement.bind_by_name("folderType", nsINavBookmarksService::TYPE_FOLDER)?;
        items_statement.bind_by_name("feedURLAnno", nsCString::from(LMANNO_FEEDURI))?;
        items_statement.bind_by_name("livemarkKind", mozISyncedBookmarksMerger::KIND_LIVEMARK)?;
        items_statement.bind_by_name("folderKind", mozISyncedBookmarksMerger::KIND_FOLDER)?;
        items_statement.bind_by_name("separatorKind", mozISyncedBookmarksMerger::KIND_SEPARATOR)?;
        let mut builder = match items_statement.step()? {
            // The first row is always the root.
            Some(step) => Tree::with_root(self.local_row_to_item(&step)?),
            None => return Err(Error::InvalidLocalRoots.into()),
        };
        while let Some(step) = items_statement.step()? {
            // All subsequent rows are descendants.
            self.controller.err_if_aborted()?;
            let raw_parent_guid: nsString = step.get_by_name("parentGuid")?;
            let parent_guid = Guid::from_utf16(&*raw_parent_guid)?;
            builder
                .item(self.local_row_to_item(&step)?)?
                .by_structure(&parent_guid)?;
        }

        let mut tree = Tree::try_from(builder)?;

        let mut deletions_statement = self.db.prepare("SELECT guid FROM moz_bookmarks_deleted")?;
        while let Some(step) = deletions_statement.step()? {
            self.controller.err_if_aborted()?;
            let raw_guid: nsString = step.get_by_name("guid")?;
            let guid = Guid::from_utf16(&*raw_guid)?;
            tree.note_deleted(guid);
        }

        Ok(tree)
    }

    /// Fetches content info for all NEW and UNKNOWN local items that don't exist
    /// in the mirror. We'll try to dedupe them to changed items with similar
    /// contents and different GUIDs in the mirror.
    fn fetch_new_local_contents(&self) -> Result<HashMap<Guid, Content>> {
        let mut contents = HashMap::new();

        let mut statement = self.db.prepare(
            r#"SELECT b.guid, b.type, IFNULL(b.title, "") AS title, h.url,
                      b.position
               FROM moz_bookmarks b
               JOIN moz_bookmarks p ON p.id = b.parent
               LEFT JOIN moz_places h ON h.id = b.fk
               LEFT JOIN items v ON v.guid = b.guid
               WHERE v.guid IS NULL AND
                     p.guid <> :rootGuid AND
                     b.syncStatus <> :syncStatus"#,
        )?;
        statement.bind_by_name("rootGuid", nsCString::from(&*dogear::ROOT_GUID))?;
        statement.bind_by_name("syncStatus", nsINavBookmarksService::SYNC_STATUS_NORMAL)?;
        while let Some(step) = statement.step()? {
            self.controller.err_if_aborted()?;
            let typ: i64 = step.get_by_name("type")?;
            let content = match typ {
                nsINavBookmarksService::TYPE_BOOKMARK => {
                    let raw_title: nsString = step.get_by_name("title")?;
                    let title = String::from_utf16(&*raw_title)?;
                    let raw_url_href: nsString = step.get_by_name("url")?;
                    let url_href = String::from_utf16(&*raw_url_href)?;
                    Content::Bookmark { title, url_href }
                }
                nsINavBookmarksService::TYPE_FOLDER => {
                    let raw_title: nsString = step.get_by_name("title")?;
                    let title = String::from_utf16(&*raw_title)?;
                    Content::Folder { title }
                }
                nsINavBookmarksService::TYPE_SEPARATOR => {
                    let position: i64 = step.get_by_name("position")?;
                    Content::Separator { position }
                }
                _ => continue,
            };

            let raw_guid: nsString = step.get_by_name("guid")?;
            let guid = Guid::from_utf16(&*raw_guid)?;

            contents.insert(guid, content);
        }

        Ok(contents)
    }

    /// Builds a fully rooted, consistent tree from the items and tombstones in the
    /// mirror.
    fn fetch_remote_tree(&self) -> Result<Tree> {
        let mut root_statement = self.db.prepare(
            "SELECT guid, serverModified, kind, needsMerge, validity
             FROM items
             WHERE NOT isDeleted AND
                   guid = :rootGuid",
        )?;
        root_statement.bind_by_name("rootGuid", nsCString::from(&*dogear::ROOT_GUID))?;
        let mut builder = match root_statement.step()? {
            Some(step) => Tree::with_root(self.remote_row_to_item(&step)?),
            None => return Err(Error::InvalidRemoteRoots.into()),
        };
        builder.reparent_orphans_to(&dogear::UNFILED_GUID);

        let mut items_statement = self.db.prepare(
            "SELECT guid, parentGuid, serverModified, kind, needsMerge, validity
             FROM items
             WHERE NOT isDeleted AND
                   guid <> :rootGuid
             ORDER BY guid",
        )?;
        items_statement.bind_by_name("rootGuid", nsCString::from(&*dogear::ROOT_GUID))?;
        while let Some(step) = items_statement.step()? {
            self.controller.err_if_aborted()?;
            let p = builder.item(self.remote_row_to_item(&step)?)?;
            let raw_parent_guid: Option<nsString> = step.get_by_name("parentGuid")?;
            if let Some(raw_parent_guid) = raw_parent_guid {
                p.by_parent_guid(Guid::from_utf16(&*raw_parent_guid)?)?;
            }
        }

        let mut structure_statement = self.db.prepare(
            "SELECT guid, parentGuid FROM structure
             WHERE guid <> :rootGuid
             ORDER BY parentGuid, position",
        )?;
        structure_statement.bind_by_name("rootGuid", nsCString::from(&*dogear::ROOT_GUID))?;
        while let Some(step) = structure_statement.step()? {
            self.controller.err_if_aborted()?;
            let raw_guid: nsString = step.get_by_name("guid")?;
            let guid = Guid::from_utf16(&*raw_guid)?;

            let raw_parent_guid: nsString = step.get_by_name("parentGuid")?;
            let parent_guid = Guid::from_utf16(&*raw_parent_guid)?;

            builder.parent_for(&guid).by_children(&parent_guid)?;
        }

        let mut tree = Tree::try_from(builder)?;

        let mut deletions_statement = self.db.prepare(
            "SELECT guid FROM items
             WHERE isDeleted AND
                   needsMerge",
        )?;
        while let Some(step) = deletions_statement.step()? {
            self.controller.err_if_aborted()?;
            let raw_guid: nsString = step.get_by_name("guid")?;
            let guid = Guid::from_utf16(&*raw_guid)?;
            tree.note_deleted(guid);
        }

        Ok(tree)
    }

    /// Fetches content info for all items in the mirror that changed since the
    /// last sync and don't exist locally.
    fn fetch_new_remote_contents(&self) -> Result<HashMap<Guid, Content>> {
        let mut contents = HashMap::new();

        let mut statement = self.db.prepare(
            r#"SELECT v.guid, v.kind, IFNULL(v.title, "") AS title, u.url,
                      IFNULL(s.position, -1) AS position
               FROM items v
               LEFT JOIN urls u ON u.id = v.urlId
               LEFT JOIN structure s ON s.guid = v.guid
               LEFT JOIN moz_bookmarks b ON b.guid = v.guid
               WHERE NOT v.isDeleted AND
                     v.needsMerge AND
                     b.guid IS NULL AND
                     IFNULL(s.parentGuid, :unfiledGuid) <> :rootGuid"#,
        )?;
        statement.bind_by_name("unfiledGuid", nsCString::from(&*dogear::UNFILED_GUID))?;
        statement.bind_by_name("rootGuid", nsCString::from(&*dogear::ROOT_GUID))?;
        while let Some(step) = statement.step()? {
            self.controller.err_if_aborted()?;
            let kind: i64 = step.get_by_name("kind")?;
            let content = match kind {
                mozISyncedBookmarksMerger::KIND_BOOKMARK
                | mozISyncedBookmarksMerger::KIND_QUERY => {
                    let raw_title: nsString = step.get_by_name("title")?;
                    let title = String::from_utf16(&*raw_title)?;

                    let raw_url_href: nsString = step.get_by_name("url")?;
                    let url_href = String::from_utf16(&*raw_url_href)?;

                    Content::Bookmark { title, url_href }
                }
                mozISyncedBookmarksMerger::KIND_FOLDER
                | mozISyncedBookmarksMerger::KIND_LIVEMARK => {
                    let raw_title: nsString = step.get_by_name("title")?;
                    let title = String::from_utf16(&*raw_title)?;
                    Content::Folder { title }
                }
                mozISyncedBookmarksMerger::KIND_SEPARATOR => {
                    let position: i64 = step.get_by_name("position")?;
                    Content::Separator { position }
                }
                _ => continue,
            };

            let raw_guid: nsString = step.get_by_name("guid")?;
            let guid = Guid::from_utf16(&*raw_guid)?;

            contents.insert(guid, content);
        }

        Ok(contents)
    }

    fn apply<'t>(
        &mut self,
        root: MergedRoot<'t>,
        deletions: impl Iterator<Item = Deletion<'t>>,
    ) -> Result<()> {
        self.controller.err_if_aborted()?;
        let descendants = root.descendants();

        self.controller.err_if_aborted()?;
        let deletions = deletions.collect::<Vec<_>>();

        // Apply the merged tree and stage outgoing items. This transaction
        // blocks writes from the main connection until it's committed, so we
        // try to do as little work as possible within it.
        let tx = self.db.transaction()?;
        if self.total_sync_changes != total_sync_changes() {
            return Err(Error::MergeConflict);
        }

        self.controller.err_if_aborted()?;
        update_local_items_in_places(&tx, descendants, deletions)?;

        self.controller.err_if_aborted()?;
        stage_items_to_upload(&tx, &self.weak_uploads)?;

        cleanup(&tx)?;
        tx.commit()?;

        Ok(())
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
    descendants: Vec<MergedDescendant<'t>>,
    deletions: Vec<Deletion>,
) -> Result<()> {
    for chunk in descendants.chunks(999 / 4) {
        let mut statement = db.prepare(format!(
            "INSERT INTO mergeStates(localGuid, remoteGuid, mergedGuid, mergedParentGuid, level,
                                     position, useRemote, shouldUpload)
             VALUES {}",
            repeat_display(chunk.len(), ",", |index, f| {
                let d = &chunk[index];
                write!(
                    f,
                    "(?, ?, ?, ?, {}, {}, {}, {})",
                    d.level,
                    d.position,
                    d.merged_node.merge_state.should_apply() as i8,
                    (d.merged_node.merge_state.upload_reason() != UploadReason::None) as i8
                )
            })
        ))?;
        for (index, d) in chunk.iter().enumerate() {
            let offset = (index * 4) as u32;

            let local_guid = d
                .merged_node
                .merge_state
                .local_node()
                .map(|node| nsString::from(node.guid.as_str()));
            let remote_guid = d
                .merged_node
                .merge_state
                .remote_node()
                .map(|node| nsString::from(node.guid.as_str()));
            let merged_guid = nsString::from(d.merged_node.guid.as_str());
            let merged_parent_guid = nsString::from(d.merged_parent_node.guid.as_str());

            statement.bind_by_index(offset, local_guid)?;
            statement.bind_by_index(offset + 1, remote_guid)?;
            statement.bind_by_index(offset + 2, merged_guid)?;
            statement.bind_by_index(offset + 3, merged_parent_guid)?;
        }
        statement.execute()?;
    }

    for chunk in deletions.chunks(999) {
        // This fires the `noteItemRemoved` trigger, which records observer infos
        // for deletions. It's important we do this before updating the structure,
        // so that the trigger captures the old parent and position.
        let mut statement = db.prepare(format!(
            "INSERT INTO itemsToRemove(guid, localLevel, shouldUploadTombstone)
             VALUES {}",
            repeat_display(chunk.len(), ",", |index, f| {
                let d = &chunk[index];
                write!(
                    f,
                    "(?, {}, {})",
                    d.local_level, d.should_upload_tombstone as i8
                )
            })
        ))?;
        for (index, d) in chunk.iter().enumerate() {
            statement.bind_by_index(index as u32, nsString::from(d.guid.as_str()))?;
        }
        statement.execute()?;
    }

    insert_new_urls_into_places(&db)?;

    // "Deleting" from `itemsToMerge` fires the `insertNewLocalItems` and
    // `updateExistingLocalItems` triggers.
    db.exec("DELETE FROM itemsToMerge")?;

    // "Deleting" from `structureToMerge` fires the `updateLocalStructure`
    // trigger.
    db.exec("DELETE FROM structureToMerge")?;

    db.exec("DELETE FROM itemsToRemove")?;

    db.exec("DELETE FROM relatedIdsToReupload")?;

    Ok(())
}

/// Inserts URLs for new remote items from the mirror's `urls` table into the
/// `moz_places` table.
fn insert_new_urls_into_places(db: &Conn) -> Result<()> {
    let mut statement = db.prepare(
        "INSERT OR IGNORE INTO moz_places(url, url_hash, rev_host, hidden,
                                          frecency, guid)
         SELECT u.url, u.hash, u.revHost, 0,
                (CASE v.kind WHEN :queryKind THEN 0 ELSE -1 END),
                IFNULL((SELECT h.guid FROM moz_places h
                        WHERE h.url_hash = u.hash AND
                              h.url = u.url), u.guid)
         FROM items v
         JOIN urls u ON u.id = v.urlId
         JOIN mergeStates r ON r.remoteGuid = v.guid
         WHERE r.useRemote",
    )?;
    statement.bind_by_name("queryKind", mozISyncedBookmarksMerger::KIND_QUERY)?;
    statement.execute()?;
    db.exec("DELETE FROM moz_updateoriginsinsert_temp")?;
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
fn stage_items_to_upload(db: &Conn, weak_upload: &[nsString]) -> Result<()> {
    // Stage explicit weak uploads such as repair responses.
    for chunk in weak_upload.chunks(999) {
        let mut statement = db.prepare(format!(
            "INSERT INTO idsToWeaklyUpload(id)
             SELECT id
             FROM moz_bookmarks
             WHERE guid IN ({})",
            repeat_display(chunk.len(), ",", |_, f| f.write_str("?")),
        ))?;
        for (index, guid) in chunk.iter().enumerate() {
            statement.bind_by_index(index as u32, nsString::from(guid.as_ref()))?;
        }
        statement.execute()?;
    }

    // Stage remotely changed items with older local creation dates. These are
    // tracked "weakly": if the upload is interrupted or fails, we won't
    // reupload the record on the next sync.
    db.exec(
        r#"
        INSERT OR IGNORE INTO idsToWeaklyUpload(id)
        SELECT b.id
        FROM moz_bookmarks b
        JOIN mergeStates r ON r.mergedGuid = b.guid
        JOIN items v ON v.guid = r.remoteGuid
        WHERE r.useRemote AND
              /* "b.dateAdded" is in microseconds; "v.dateAdded" is in
                 milliseconds. */
              b.dateAdded / 1000 < v.dateAdded"#,
    )?;

    // Stage remaining locally changed items for upload.
    db.exec(format!(
        "
        WITH RECURSIVE
        {}
        INSERT INTO itemsToUpload(id, guid, syncChangeCounter, parentGuid,
                                  parentTitle, dateAdded, type, title, placeId,
                                  isQuery, url, keyword, position, tagFolderName)
        SELECT s.id, s.guid, s.syncChangeCounter, s.parentGuid, s.parentTitle,
               s.dateAdded / 1000, s.type, s.title, s.placeId,
               IFNULL(SUBSTR(h.url, 1, 6) = 'place:', 0) AS isQuery,
               h.url,
               (SELECT keyword FROM moz_keywords WHERE place_id = h.id),
               s.position,
               (SELECT get_query_param(substr(url, 7), 'tag')
                WHERE substr(h.url, 1, 6) = 'place:')
        FROM localItems s
        LEFT JOIN moz_places h ON h.id = s.placeId
        LEFT JOIN idsToWeaklyUpload w ON w.id = s.id
        WHERE s.guid <> '{}' AND (
          s.syncChangeCounter > 0 OR
          w.id NOT NULL
        )",
        LOCAL_ITEMS_SQL_FRAGMENT,
        dogear::ROOT_GUID,
    ))?;

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
    db.exec("DELETE FROM mergeStates")?;
    db.exec("DELETE FROM idsToWeaklyUpload")?;
    Ok(())
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
