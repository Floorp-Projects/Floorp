// Copyright 2018-2019 Mozilla

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use std::{
    collections::{hash_map::Entry, HashMap, HashSet, VecDeque},
    fmt, mem,
};

use crate::driver::{AbortSignal, DefaultAbortSignal, DefaultDriver, Driver};
use crate::error::{ErrorKind, Result};
use crate::guid::{Guid, IsValidGuid, TAGS_GUID};
use crate::tree::{Content, MergeState, MergedNode, Node, Tree, Validity};

/// Structure change types, used to indicate if a node on one side is moved
/// or deleted on the other.
#[derive(Eq, PartialEq)]
enum StructureChange {
    /// Node not deleted, or doesn't exist, on the other side.
    Unchanged,
    /// Node moved on the other side.
    Moved,
    /// Node deleted on the other side.
    Deleted,
}

/// Records structure change counters for telemetry.
#[derive(Clone, Copy, Default, Debug, Eq, Hash, PartialEq)]
pub struct StructureCounts {
    /// Remote non-folder change wins over local deletion.
    pub remote_revives: usize,
    /// Local folder deletion wins over remote change.
    pub local_deletes: usize,
    /// Local non-folder change wins over remote deletion.
    pub local_revives: usize,
    /// Remote folder deletion wins over local change.
    pub remote_deletes: usize,
    /// Deduped local items.
    pub dupes: usize,
    /// Total number of nodes in the merged tree, excluding the
    /// root.
    pub merged_nodes: usize,
}

/// Holds (matching remote dupes for local GUIDs, matching local dupes for
/// remote GUIDs).
type MatchingDupes<'t> = (HashMap<Guid, Node<'t>>, HashMap<Guid, Node<'t>>);

/// Indicates which side to take in case of a merge conflict.
#[derive(Clone, Copy, Debug)]
enum ConflictResolution {
    Local,
    Remote,
    Unchanged,
}

/// A hash key used to match dupes by content.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
enum DupeKey<'a> {
    /// Matches a dupe by content only. Used for bookmarks, queries, folders,
    /// and livemarks.
    WithoutPosition(&'a Content),
    /// Matches a dupe by content and position. Used for separators.
    WithPosition(&'a Content, usize),
}

/// A two-way merger that produces a complete merged tree from a complete local
/// tree and a complete remote tree with changes since the last sync.
///
/// This is ported almost directly from iOS. On iOS, the `ThreeWayMerger` takes
/// a complete "mirror" tree with the server state after the last sync, and two
/// incomplete trees with local and remote changes to the mirror: "local" and
/// "mirror", respectively. Overlaying buffer onto mirror yields the current
/// server tree; overlaying local onto mirror yields the complete local tree.
///
/// Dogear doesn't store the shared parent for changed items, so we can only
/// do two-way merges. Our local tree is the union of iOS's mirror and local,
/// and our remote tree is the union of iOS's mirror and buffer.
///
/// Unlike iOS, Dogear doesn't distinguish between structure and value changes.
/// The `needs_merge` flag notes *that* a bookmark changed, but not *how*. This
/// means we might detect conflicts, and revert changes on one side, for cases
/// that iOS can merge cleanly.
///
/// Fortunately, most of our users don't organize their bookmarks into deeply
/// nested hierarchies, or make conflicting changes on multiple devices
/// simultaneously. A simpler two-way tree merge strikes a good balance between
/// correctness and complexity.
pub struct Merger<'t, D = DefaultDriver, A = DefaultAbortSignal> {
    driver: &'t D,
    signal: &'t A,
    local_tree: &'t Tree,
    remote_tree: &'t Tree,
    matching_dupes_by_local_parent_guid: HashMap<Guid, MatchingDupes<'t>>,
    merged_guids: HashSet<Guid>,
    delete_locally: HashSet<Guid>,
    delete_remotely: HashSet<Guid>,
    structure_counts: StructureCounts,
}

impl<'t> Merger<'t, DefaultDriver, DefaultAbortSignal> {
    /// Creates a merger with the default merge driver.
    pub fn new(local_tree: &'t Tree, remote_tree: &'t Tree) -> Merger<'t> {
        Merger {
            driver: &DefaultDriver,
            signal: &DefaultAbortSignal,
            local_tree,
            remote_tree,
            matching_dupes_by_local_parent_guid: HashMap::new(),
            merged_guids: HashSet::new(),
            delete_locally: HashSet::new(),
            delete_remotely: HashSet::new(),
            structure_counts: StructureCounts::default(),
        }
    }
}

impl<'t, D: Driver, A: AbortSignal> Merger<'t, D, A> {
    /// Creates a merger with the given merge driver and contents.
    pub fn with_driver(
        driver: &'t D,
        signal: &'t A,
        local_tree: &'t Tree,
        remote_tree: &'t Tree,
    ) -> Merger<'t, D, A> {
        Merger {
            driver,
            signal,
            local_tree,
            remote_tree,
            matching_dupes_by_local_parent_guid: HashMap::new(),
            merged_guids: HashSet::new(),
            delete_locally: HashSet::new(),
            delete_remotely: HashSet::new(),
            structure_counts: StructureCounts::default(),
        }
    }

    /// Builds a merged tree from the local and remote trees.
    pub fn merge(mut self) -> Result<MergedRoot<'t>> {
        let merged_root_node = {
            let local_root_node = self.local_tree.root();
            let remote_root_node = self.remote_tree.root();
            self.two_way_merge(local_root_node, remote_root_node)?
        };

        // Any remaining deletions on one side should be deleted on the other side.
        // This happens when the remote tree has tombstones for items that don't
        // exist locally, or the local tree has tombstones for items that
        // aren't on the server.
        for guid in self.local_tree.deletions() {
            self.signal.err_if_aborted()?;
            if !self.mentions(guid) {
                self.delete_remotely.insert(guid.clone());
            }
        }
        for guid in self.remote_tree.deletions() {
            self.signal.err_if_aborted()?;
            if !self.mentions(guid) {
                self.delete_locally.insert(guid.clone());
            }
        }

        // The merged tree should know about all items mentioned in the local
        // and remote trees. Otherwise, it's incomplete, and we can't apply it.
        // This indicates a bug in the merger.
        for guid in self.local_tree.guids() {
            self.signal.err_if_aborted()?;
            if !self.mentions(guid) {
                return Err(ErrorKind::UnmergedLocalItems.into());
            }
        }
        for guid in self.remote_tree.guids() {
            self.signal.err_if_aborted()?;
            if !self.mentions(guid) {
                return Err(ErrorKind::UnmergedRemoteItems.into());
            }
        }

        Ok(MergedRoot {
            local_tree: self.local_tree,
            remote_tree: self.remote_tree,
            node: merged_root_node,
            merged_guids: self.merged_guids,
            delete_locally: self.delete_locally,
            delete_remotely: self.delete_remotely,
            structure_counts: self.structure_counts,
        })
    }

    #[inline]
    fn mentions(&self, guid: &Guid) -> bool {
        self.merged_guids.contains(guid)
            || self.delete_locally.contains(guid)
            || self.delete_remotely.contains(guid)
    }

    fn merge_local_only_node(&mut self, local_node: Node<'t>) -> Result<MergedNode<'t>> {
        trace!(self.driver, "Item {} only exists locally", local_node);

        self.merged_guids.insert(local_node.guid.clone());

        let merged_guid = if local_node.guid.is_valid_guid() {
            local_node.guid.clone()
        } else {
            warn!(
                self.driver,
                "Generating new GUID for local node {}", local_node
            );
            self.signal.err_if_aborted()?;
            let new_guid = self.driver.generate_new_guid(&local_node.guid)?;
            if new_guid != local_node.guid {
                if self.merged_guids.contains(&new_guid) {
                    return Err(ErrorKind::DuplicateItem(new_guid).into());
                }
                self.merged_guids.insert(new_guid.clone());
            }
            new_guid
        };

        let mut merged_node = MergedNode::new(merged_guid, MergeState::LocalOnly(local_node));
        // The local folder doesn't exist remotely, but its children might, so
        // we still need to recursively walk and merge them. This method will
        // change the merge state from local to new if any children were moved
        // or deleted.
        for local_child_node in local_node.children() {
            self.signal.err_if_aborted()?;
            self.merge_local_child_into_merged_node(
                &mut merged_node,
                local_node,
                None,
                local_child_node,
            )?;
        }

        if local_node.diverged() {
            merged_node.merge_state = merged_node.merge_state.with_new_local_structure();
        }

        Ok(merged_node)
    }

    fn merge_remote_only_node(&mut self, remote_node: Node<'t>) -> Result<MergedNode<'t>> {
        trace!(self.driver, "Item {} only exists remotely", remote_node);

        self.merged_guids.insert(remote_node.guid.clone());

        let merged_guid = if remote_node.guid.is_valid_guid() {
            remote_node.guid.clone()
        } else {
            warn!(
                self.driver,
                "Generating new GUID for remote node {}", remote_node
            );
            self.signal.err_if_aborted()?;
            let new_guid = self.driver.generate_new_guid(&remote_node.guid)?;
            if new_guid != remote_node.guid {
                if self.merged_guids.contains(&new_guid) {
                    return Err(ErrorKind::DuplicateItem(new_guid).into());
                }
                self.merged_guids.insert(new_guid.clone());
                // Upload tombstones for changed remote GUIDs.
                self.delete_remotely.insert(remote_node.guid.clone());
            }
            new_guid
        };
        let mut merged_node = MergedNode::new(merged_guid, MergeState::RemoteOnly(remote_node));
        // As above, a remote folder's children might still exist locally, so we
        // need to merge them and update the merge state from remote to new if
        // any children were moved or deleted.
        for remote_child_node in remote_node.children() {
            self.signal.err_if_aborted()?;
            self.merge_remote_child_into_merged_node(
                &mut merged_node,
                None,
                remote_node,
                remote_child_node,
            )?;
        }

        if remote_node.diverged()
            || merged_node.remote_guid_changed()
            || remote_node.validity != Validity::Valid
        {
            // If the remote structure diverged, the merged item's GUID changed,
            // or the item isn't valid, flag it for reupload.
            merged_node.merge_state = merged_node.merge_state.with_new_remote_structure();
        }

        Ok(merged_node)
    }

    /// Merges two nodes that exist locally and remotely.
    fn two_way_merge(
        &mut self,
        local_node: Node<'t>,
        remote_node: Node<'t>,
    ) -> Result<MergedNode<'t>> {
        trace!(
            self.driver,
            "Item exists locally as {} and remotely as {}",
            local_node,
            remote_node
        );

        if !local_node.has_compatible_kind(&remote_node) {
            error!(
                self.driver,
                "Merging local {} and remote {} with different kinds", local_node, remote_node
            );
            return Err(ErrorKind::MismatchedItemKind(local_node.kind, remote_node.kind).into());
        }

        self.merged_guids.insert(local_node.guid.clone());
        self.merged_guids.insert(remote_node.guid.clone());

        let merged_guid = if remote_node.guid.is_valid_guid() {
            remote_node.guid.clone()
        } else {
            warn!(
                self.driver,
                "Generating new valid GUID for node {}", remote_node
            );
            self.signal.err_if_aborted()?;
            let new_guid = self.driver.generate_new_guid(&remote_node.guid)?;
            if new_guid != remote_node.guid {
                if self.merged_guids.contains(&new_guid) {
                    return Err(ErrorKind::DuplicateItem(new_guid).into());
                }
                self.merged_guids.insert(new_guid.clone());
                // Upload tombstones for changed remote GUIDs.
                self.delete_remotely.insert(remote_node.guid.clone());
            }
            new_guid
        };

        let (item, children) = self.resolve_value_conflict(local_node, remote_node);

        let mut merged_node = MergedNode::new(
            merged_guid,
            match item {
                ConflictResolution::Local => MergeState::Local {
                    local_node,
                    remote_node,
                },
                ConflictResolution::Remote => MergeState::Remote {
                    local_node,
                    remote_node,
                },
                ConflictResolution::Unchanged => MergeState::Unchanged {
                    local_node,
                    remote_node,
                },
            },
        );

        match children {
            ConflictResolution::Local => {
                for local_child_node in local_node.children() {
                    self.signal.err_if_aborted()?;
                    self.merge_local_child_into_merged_node(
                        &mut merged_node,
                        local_node,
                        Some(remote_node),
                        local_child_node,
                    )?;
                }
                for remote_child_node in remote_node.children() {
                    self.signal.err_if_aborted()?;
                    self.merge_remote_child_into_merged_node(
                        &mut merged_node,
                        Some(local_node),
                        remote_node,
                        remote_child_node,
                    )?;
                }
            }

            ConflictResolution::Remote => {
                for remote_child_node in remote_node.children() {
                    self.signal.err_if_aborted()?;
                    self.merge_remote_child_into_merged_node(
                        &mut merged_node,
                        Some(local_node),
                        remote_node,
                        remote_child_node,
                    )?;
                }
                for local_child_node in local_node.children() {
                    self.signal.err_if_aborted()?;
                    self.merge_local_child_into_merged_node(
                        &mut merged_node,
                        local_node,
                        Some(remote_node),
                        local_child_node,
                    )?;
                }
            }

            ConflictResolution::Unchanged => {
                // The children are the same, so we only need to merge one side.
                for (local_child_node, remote_child_node) in
                    local_node.children().zip(remote_node.children())
                {
                    self.signal.err_if_aborted()?;
                    self.merge_unchanged_child_into_merged_node(
                        &mut merged_node,
                        local_node,
                        local_child_node,
                        remote_node,
                        remote_child_node,
                    )?;
                }
            }
        }

        if local_node.diverged() {
            merged_node.merge_state = merged_node.merge_state.with_new_local_structure();
        }

        if remote_node.diverged() || remote_node.validity != Validity::Valid {
            // Flag remotely diverged and invalid items for reupload.
            merged_node.merge_state = merged_node.merge_state.with_new_remote_structure();
        }

        Ok(merged_node)
    }

    /// Merges two nodes with the same parents and positions.
    ///
    /// Unlike items that have been moved, or exist only on one side, unchanged
    /// children can be merged directly.
    fn merge_unchanged_child_into_merged_node(
        &mut self,
        merged_node: &mut MergedNode<'t>,
        local_parent_node: Node<'t>,
        local_child_node: Node<'t>,
        remote_parent_node: Node<'t>,
        remote_child_node: Node<'t>,
    ) -> Result<()> {
        assert!(
            !self.merged_guids.contains(&local_child_node.guid),
            "Unchanged local child shouldn't have been merged"
        );
        assert!(
            !self.merged_guids.contains(&remote_child_node.guid),
            "Unchanged remote child shouldn't have been merged"
        );

        // Even though the child exists on both sides, it might still be
        // non-syncable or invalid, so we need to check for structure
        // changes.
        let local_structure_change = self.check_for_local_structure_change_of_remote_node(
            merged_node,
            remote_parent_node,
            remote_child_node,
        )?;
        let remote_structure_change = self.check_for_remote_structure_change_of_local_node(
            merged_node,
            local_parent_node,
            local_child_node,
        )?;
        match (local_structure_change, remote_structure_change) {
            (StructureChange::Deleted, StructureChange::Deleted) => {
                // The child is deleted on both sides. We'll need to reupload
                // and apply a new structure.
                merged_node.merge_state = merged_node
                    .merge_state
                    .with_new_local_structure()
                    .with_new_remote_structure();
            }
            (StructureChange::Deleted, _) => {
                // The child is deleted locally, but not remotely, so we only
                // need to reupload a new structure.
                merged_node.merge_state = merged_node.merge_state.with_new_remote_structure();
            }
            (_, StructureChange::Deleted) => {
                // The child is deleted remotely, so we only need to apply a
                // new local structure.
                merged_node.merge_state = merged_node.merge_state.with_new_local_structure();
            }
            (_, _) => {
                // The child exists on both sides, so merge it now. If the GUID
                // changes because it's invalid, we'll need to reapply the
                // child, and reupload the child and its parent.
                let mut merged_child_node =
                    self.two_way_merge(local_child_node, remote_child_node)?;
                if merged_child_node.local_guid_changed() {
                    merged_child_node.merge_state =
                        merged_child_node.merge_state.with_new_local_structure();
                }
                if merged_node.remote_guid_changed() {
                    // The merged parent's GUID changed; flag the child for
                    // reupload with a new `parentid`.
                    merged_child_node.merge_state =
                        merged_child_node.merge_state.with_new_remote_structure();
                }
                if merged_child_node.remote_guid_changed() {
                    // The merged child's GUID changed; flag the parent for
                    // reupload with new `children`.
                    merged_node.merge_state = merged_node.merge_state.with_new_remote_structure();
                }
                merged_node.merged_children.push(merged_child_node);
                self.structure_counts.merged_nodes += 1;
            }
        }

        Ok(())
    }

    /// Merges a remote child node into a merged folder node. This handles the
    /// following cases:
    ///
    /// - The remote child is locally deleted. We recursively move all of its
    ///   descendants that don't exist locally to the merged folder.
    /// - The remote child doesn't exist locally, but has a content match in the
    ///   corresponding local folder. We dedupe the local child to the remote
    ///   child.
    /// - The remote child exists locally, but in a different folder. We compare
    ///   merge flags and timestamps to decide where to keep the child.
    /// - The remote child exists locally, and in the same folder. We merge the
    ///   local and remote children.
    ///
    /// This is the inverse of `merge_local_child_into_merged_node`.
    fn merge_remote_child_into_merged_node(
        &mut self,
        merged_node: &mut MergedNode<'t>,
        local_parent_node: Option<Node<'t>>,
        remote_parent_node: Node<'t>,
        remote_child_node: Node<'t>,
    ) -> Result<()> {
        if self.merged_guids.contains(&remote_child_node.guid) {
            trace!(
                self.driver,
                "Remote child {} already seen in another folder and merged",
                remote_child_node
            );
            // Omitting a remote child that we already merged locally means we
            // have a new remote structure.
            merged_node.merge_state = merged_node.merge_state.with_new_remote_structure();
            return Ok(());
        }

        trace!(
            self.driver,
            "Merging remote child {} of {} into {}",
            remote_child_node,
            remote_parent_node,
            merged_node
        );

        // Check if the remote child is locally deleted. and move all
        // descendants that aren't also remotely deleted to the merged node.
        // This handles the case where a user deletes a folder on this device,
        // and adds a bookmark to the same folder on another device. We want to
        // keep the folder deleted, but we also don't want to lose the new
        // bookmark, so we move the bookmark to the deleted folder's parent.
        if self.check_for_local_structure_change_of_remote_node(
            merged_node,
            remote_parent_node,
            remote_child_node,
        )? == StructureChange::Deleted
        {
            // Flag the merged parent for reupload, since we deleted the
            // remote child.
            merged_node.merge_state = merged_node.merge_state.with_new_remote_structure();
            return Ok(());
        }

        // The remote child isn't locally deleted. Does it exist in the local tree?
        if let Some(local_child_node) = self.local_tree.node_for_guid(&remote_child_node.guid) {
            // The remote child exists in the local tree. Did it move?
            let local_parent_node = local_child_node
                .parent()
                .expect("Can't merge existing remote child without local parent");

            trace!(
                self.driver,
                "Remote child {} exists locally in {} and remotely in {}",
                remote_child_node,
                local_parent_node,
                remote_parent_node
            );

            if self.remote_tree.is_deleted(&local_parent_node.guid) {
                trace!(
                    self.driver,
                    "Unconditionally taking remote move for {} to {} because local parent {} is \
                     deleted remotely",
                    remote_child_node,
                    remote_parent_node,
                    local_parent_node
                );

                let mut merged_child_node =
                    self.two_way_merge(local_child_node, remote_child_node)?;
                merged_child_node.merge_state =
                    merged_child_node.merge_state.with_new_local_structure();
                if merged_node.remote_guid_changed() {
                    // If the parent's GUID changed, flag the child for reupload, so that
                    // its `parentid` is correct.
                    merged_child_node.merge_state =
                        merged_child_node.merge_state.with_new_remote_structure();
                }
                if merged_child_node.remote_guid_changed() {
                    // If the child's GUID changed, flag the parent for reupload, so that
                    // its `children` are correct.
                    merged_node.merge_state = merged_node.merge_state.with_new_remote_structure();
                }
                merged_node.merge_state = merged_node.merge_state.with_new_local_structure();
                merged_node.merged_children.push(merged_child_node);
                self.structure_counts.merged_nodes += 1;
                return Ok(());
            }

            match self.resolve_structure_conflict(
                local_parent_node,
                local_child_node,
                remote_parent_node,
                remote_child_node,
            ) {
                ConflictResolution::Local => {
                    // The local move is newer, so we ignore the remote move.
                    // We'll merge the remote child later, when we walk its new
                    // local parent.
                    trace!(
                        self.driver,
                        "Remote child {} moved locally to {} and remotely to {}; \
                         keeping child in newer local parent and position",
                        remote_child_node,
                        local_parent_node,
                        remote_parent_node
                    );

                    // Flag the old parent for reupload, since we're moving
                    // the remote child. Note that, since we only flag the
                    // remote parent here, we don't need to handle
                    // reparenting and repositioning separately.
                    merged_node.merge_state = merged_node.merge_state.with_new_remote_structure();
                }

                ConflictResolution::Remote | ConflictResolution::Unchanged => {
                    // The remote move is newer, so we merge the remote
                    // child now and ignore the local move.
                    let mut merged_child_node = if local_parent_node.guid != remote_parent_node.guid
                    {
                        trace!(
                            self.driver,
                            "Remote child {} reparented locally to {} and remotely to {}; \
                             keeping child in newer remote parent",
                            remote_child_node,
                            local_parent_node,
                            remote_parent_node
                        );
                        let mut merged_child_node =
                            self.two_way_merge(local_child_node, remote_child_node)?;
                        merged_child_node.merge_state =
                            merged_child_node.merge_state.with_new_local_structure();
                        merged_child_node
                    } else {
                        trace!(
                            self.driver,
                            "Remote child {} repositioned locally in {} and remotely in {}; \
                             keeping child in newer remote position",
                            remote_child_node,
                            local_parent_node,
                            remote_parent_node
                        );
                        self.two_way_merge(local_child_node, remote_child_node)?
                    };
                    if merged_child_node.local_guid_changed() {
                        merged_child_node.merge_state =
                            merged_child_node.merge_state.with_new_local_structure();
                    }
                    if merged_node.remote_guid_changed() {
                        // The merged parent's GUID changed; flag the child for
                        // reupload with a new `parentid`.
                        merged_child_node.merge_state =
                            merged_child_node.merge_state.with_new_remote_structure();
                    }
                    if merged_child_node.remote_guid_changed() {
                        // The merged child's GUID changed; flag the parent for
                        // reupload with new `children`.
                        merged_node.merge_state =
                            merged_node.merge_state.with_new_remote_structure();
                    }
                    merged_node.merge_state = merged_node.merge_state.with_new_local_structure();
                    merged_node.merged_children.push(merged_child_node);
                    self.structure_counts.merged_nodes += 1;
                }
            }

            return Ok(());
        }

        // Remote child is not a root, and doesn't exist locally. Try to find a
        // content match in the containing folder, and dedupe the local item if
        // we can.
        trace!(
            self.driver,
            "Remote child {} doesn't exist locally; looking for local content match",
            remote_child_node
        );

        let mut merged_child_node = if let Some(local_child_node_by_content) = self
            .find_local_node_matching_remote_node(
                merged_node,
                local_parent_node,
                remote_parent_node,
                remote_child_node,
            )? {
            self.two_way_merge(local_child_node_by_content, remote_child_node)
        } else {
            self.merge_remote_only_node(remote_child_node)
        }?;
        if merged_child_node.local_guid_changed() {
            merged_child_node.merge_state =
                merged_child_node.merge_state.with_new_local_structure();
        }
        if merged_node.remote_guid_changed() {
            merged_child_node.merge_state =
                merged_child_node.merge_state.with_new_remote_structure();
        }
        if merged_child_node.remote_guid_changed() {
            merged_node.merge_state = merged_node.merge_state.with_new_remote_structure();
        }
        merged_node.merge_state = merged_node.merge_state.with_new_local_structure();
        merged_node.merged_children.push(merged_child_node);
        self.structure_counts.merged_nodes += 1;
        Ok(())
    }

    /// Merges a local child node into a merged folder node.
    ///
    /// This is the inverse of `merge_remote_child_into_merged_node`.
    fn merge_local_child_into_merged_node(
        &mut self,
        merged_node: &mut MergedNode<'t>,
        local_parent_node: Node<'t>,
        remote_parent_node: Option<Node<'t>>,
        local_child_node: Node<'t>,
    ) -> Result<()> {
        if self.merged_guids.contains(&local_child_node.guid) {
            // We already merged the child when we walked another folder. Since
            // a tree can't have duplicate GUIDs, we must have merged the remote
            // child, so we have a new local structure.
            trace!(
                self.driver,
                "Local child {} already seen in another folder and merged",
                local_child_node
            );
            merged_node.merge_state = merged_node.merge_state.with_new_local_structure();
            return Ok(());
        }

        trace!(
            self.driver,
            "Merging local child {} of {} into {}",
            local_child_node,
            local_parent_node,
            merged_node
        );

        // Check if the local child is remotely deleted, and move any new local
        // descendants to the merged node if so.
        if self.check_for_remote_structure_change_of_local_node(
            merged_node,
            local_parent_node,
            local_child_node,
        )? == StructureChange::Deleted
        {
            // Since we're merging local nodes, we don't need to flag the merged
            // parent for reupload.
            merged_node.merge_state = merged_node.merge_state.with_new_local_structure();
            return Ok(());
        }

        // At this point, we know the local child isn't deleted. See if it
        // exists in the remote tree.
        if let Some(remote_child_node) = self.remote_tree.node_for_guid(&local_child_node.guid) {
            // The local child exists remotely. It must have moved; otherwise, we
            // would have seen it when we walked the remote children.
            let remote_parent_node = remote_child_node
                .parent()
                .expect("Can't merge existing local child without remote parent");

            trace!(
                self.driver,
                "Local child {} exists locally in {} and remotely in {}",
                local_child_node,
                local_parent_node,
                remote_parent_node
            );

            if self.local_tree.is_deleted(&remote_parent_node.guid) {
                trace!(
                    self.driver,
                    "Unconditionally taking local move for {} to {} because remote parent {} is \
                     deleted locally",
                    local_child_node,
                    local_parent_node,
                    remote_parent_node
                );

                // Merge and flag the new parent *and the locally moved child* for
                // reupload. The parent references the child in its `children`; the
                // child points back to the parent in its `parentid`.
                //
                // Reuploading the child isn't necessary for newer Desktops, which
                // ignore the child's `parentid` and use the parent's `children`.
                //
                // However, older Desktop and Android use the child's `parentid` as
                // canonical, while iOS is stricter and requires both to match.
                let mut merged_child_node =
                    self.two_way_merge(local_child_node, remote_child_node)?;
                if merged_child_node.local_guid_changed() {
                    merged_child_node.merge_state =
                        merged_child_node.merge_state.with_new_local_structure();
                }
                merged_node.merge_state = merged_node.merge_state.with_new_remote_structure();
                merged_child_node.merge_state =
                    merged_child_node.merge_state.with_new_remote_structure();
                merged_node.merged_children.push(merged_child_node);
                self.structure_counts.merged_nodes += 1;
                return Ok(());
            }

            match self.resolve_structure_conflict(
                local_parent_node,
                local_child_node,
                remote_parent_node,
                remote_child_node,
            ) {
                ConflictResolution::Local => {
                    // The local move is newer, so we merge the local child now
                    // and ignore the remote move.
                    if local_parent_node.guid != remote_parent_node.guid {
                        // The child moved to a different folder.
                        trace!(
                            self.driver,
                            "Local child {} reparented locally to {} and remotely to {}; \
                             keeping child in newer local parent",
                            local_child_node,
                            local_parent_node,
                            remote_parent_node
                        );

                        // Merge and flag both the new parent and child for
                        // reupload. See above for why.
                        let mut merged_child_node =
                            self.two_way_merge(local_child_node, remote_child_node)?;
                        if merged_child_node.local_guid_changed() {
                            merged_child_node.merge_state =
                                merged_child_node.merge_state.with_new_local_structure();
                        }
                        merged_node.merge_state =
                            merged_node.merge_state.with_new_remote_structure();
                        merged_child_node.merge_state =
                            merged_child_node.merge_state.with_new_remote_structure();
                        merged_node.merged_children.push(merged_child_node);
                        self.structure_counts.merged_nodes += 1;
                    } else {
                        trace!(
                            self.driver,
                            "Local child {} repositioned locally in {} and remotely in {}; \
                             keeping child in newer local position",
                            local_child_node,
                            local_parent_node,
                            remote_parent_node
                        );

                        // For position changes in the same folder, we only need to
                        // merge and flag the parent for reupload...
                        let mut merged_child_node =
                            self.two_way_merge(local_child_node, remote_child_node)?;
                        if merged_child_node.local_guid_changed() {
                            merged_child_node.merge_state =
                                merged_child_node.merge_state.with_new_local_structure();
                        }
                        merged_node.merge_state =
                            merged_node.merge_state.with_new_remote_structure();
                        if merged_node.remote_guid_changed() {
                            // ...Unless the merged parent's GUID also changed,
                            // in which case we also need to flag the
                            // repositioned child for reupload, so that its
                            // `parentid` is correct.
                            merged_child_node.merge_state =
                                merged_child_node.merge_state.with_new_remote_structure();
                        }

                        merged_node.merged_children.push(merged_child_node);
                        self.structure_counts.merged_nodes += 1;
                    }
                }

                ConflictResolution::Remote | ConflictResolution::Unchanged => {
                    // The remote move is newer, so we ignore the local
                    // move. We'll merge the local child later, when we
                    // walk its new remote parent.
                    if local_parent_node.guid != remote_parent_node.guid {
                        trace!(
                            self.driver,
                            "Local child {} reparented locally to {} and remotely to {}; \
                             keeping child in newer remote parent",
                            local_child_node,
                            local_parent_node,
                            remote_parent_node
                        );
                    } else {
                        trace!(
                            self.driver,
                            "Local child {} repositioned locally in {} and remotely in {}; \
                             keeping child in newer remote position",
                            local_child_node,
                            local_parent_node,
                            remote_parent_node
                        );
                    }
                    merged_node.merge_state = merged_node.merge_state.with_new_local_structure();
                }
            }

            return Ok(());
        }

        // Local child is not a root, and doesn't exist remotely. Try to find a
        // content match in the containing folder, and dedupe the local item if
        // we can.
        trace!(
            self.driver,
            "Local child {} doesn't exist remotely; looking for remote content match",
            local_child_node
        );

        let merged_child_node = if let Some(remote_child_node_by_content) = self
            .find_remote_node_matching_local_node(
                merged_node,
                local_parent_node,
                remote_parent_node,
                local_child_node,
            )? {
            // The local child has a remote content match, so take the remote GUID
            // and merge.
            let mut merged_child_node =
                self.two_way_merge(local_child_node, remote_child_node_by_content)?;
            if merged_child_node.local_guid_changed() {
                merged_child_node.merge_state =
                    merged_child_node.merge_state.with_new_local_structure();
            }
            if merged_node.remote_guid_changed() {
                merged_child_node.merge_state =
                    merged_child_node.merge_state.with_new_remote_structure();
            }
            if merged_child_node.remote_guid_changed() {
                merged_node.merge_state = merged_node.merge_state.with_new_remote_structure();
            }
            merged_node.merge_state = merged_node.merge_state.with_new_local_structure();
            merged_child_node
        } else {
            // The local child doesn't exist remotely, so flag the merged parent and
            // new child for upload, and walk its descendants.
            let mut merged_child_node = self.merge_local_only_node(local_child_node)?;
            if merged_child_node.local_guid_changed() {
                merged_child_node.merge_state =
                    merged_child_node.merge_state.with_new_local_structure();
            }
            merged_node.merge_state = merged_node.merge_state.with_new_remote_structure();
            merged_child_node.merge_state =
                merged_child_node.merge_state.with_new_remote_structure();
            merged_child_node
        };
        merged_node.merged_children.push(merged_child_node);
        self.structure_counts.merged_nodes += 1;
        Ok(())
    }

    /// Determines which side to prefer, and which children to merge first,
    /// for an item that exists on both sides.
    fn resolve_value_conflict(
        &self,
        local_node: Node<'t>,
        remote_node: Node<'t>,
    ) -> (ConflictResolution, ConflictResolution) {
        if remote_node.is_root() {
            // Don't touch the Places root; it's not synced, anyway.
            return (ConflictResolution::Unchanged, ConflictResolution::Local);
        }

        match (local_node.needs_merge, remote_node.needs_merge) {
            (true, true) => {
                // The item changed locally and remotely.
                let item = if local_node.is_built_in_root() {
                    // For roots, we always prefer the local side for item
                    // changes, like the title (bug 1432614).
                    ConflictResolution::Local
                } else {
                    // For other items, we check the validity to decide
                    // which side to take.
                    match (local_node.validity, remote_node.validity) {
                        // If both are invalid, it doesn't matter which side
                        // we pick; the item will be deleted, anyway.
                        (Validity::Replace, Validity::Replace) => ConflictResolution::Unchanged,
                        // If only one side is invalid, pick the other side.
                        // This loses changes from that side, but we can't
                        // apply or upload those changes, anyway.
                        (Validity::Replace, _) => ConflictResolution::Remote,
                        (_, Validity::Replace) => ConflictResolution::Local,
                        (_, _) => {
                            // Otherwise, the item is either valid, or valid
                            // but needs to be reuploaded or reapplied, so
                            // compare timestamps to decide which side is newer.
                            if local_node.age < remote_node.age {
                                ConflictResolution::Local
                            } else {
                                ConflictResolution::Remote
                            }
                        }
                    }
                };
                // For children, it's easier: we always use the newer side, even
                // if we're taking local changes for the item.
                let children = if local_node.has_matching_children(remote_node) {
                    ConflictResolution::Unchanged
                } else if local_node.age < remote_node.age {
                    // The local change is newer, so merge local children first,
                    // followed by remaining unmerged remote children.
                    ConflictResolution::Local
                } else {
                    // The remote change is newer, so walk and merge remote
                    // children first, then remaining local children.
                    ConflictResolution::Remote
                };
                (item, children)
            }

            (true, false) => {
                // The item changed locally, but not remotely. Prefer the local
                // item, then merge local children first, followed by remote
                // children.
                let item = match local_node.validity {
                    Validity::Valid | Validity::Reupload => ConflictResolution::Local,
                    Validity::Replace => ConflictResolution::Remote,
                };
                let children = if local_node.has_matching_children(remote_node) {
                    ConflictResolution::Unchanged
                } else {
                    ConflictResolution::Local
                };
                (item, children)
            }

            (false, true) => {
                // The item changed remotely, but not locally.
                let item = if local_node.is_built_in_root() {
                    // For roots, we ignore remote item changes.
                    ConflictResolution::Unchanged
                } else {
                    match remote_node.validity {
                        Validity::Valid | Validity::Reupload => ConflictResolution::Remote,
                        // And, for invalid remote items, we must reupload the
                        // local side. This _loses remote changes_, but we can't
                        // apply those changes, anyway.
                        Validity::Replace => ConflictResolution::Local,
                    }
                };
                let children = if local_node.has_matching_children(remote_node) {
                    ConflictResolution::Unchanged
                } else {
                    ConflictResolution::Remote
                };
                // For children, we always use the remote side.
                (item, children)
            }

            (false, false) => {
                let item = match (local_node.validity, remote_node.validity) {
                    (Validity::Replace, Validity::Replace) => ConflictResolution::Unchanged,
                    (_, Validity::Replace) => ConflictResolution::Local,
                    (Validity::Replace, _) => ConflictResolution::Remote,
                    (_, _) => ConflictResolution::Unchanged,
                };
                // If the child lists are identical, the structure is unchanged.
                // Otherwise, the children differ even though the items aren't
                // flagged as unmerged, so we prefer the newer side.
                let children = if local_node.has_matching_children(remote_node) {
                    ConflictResolution::Unchanged
                } else if local_node.age < remote_node.age {
                    ConflictResolution::Local
                } else {
                    ConflictResolution::Remote
                };
                (item, children)
            }
        }
    }

    /// Determines where to keep a child of a folder that exists on both sides.
    fn resolve_structure_conflict(
        &self,
        local_parent_node: Node<'t>,
        local_child_node: Node<'t>,
        remote_parent_node: Node<'t>,
        remote_child_node: Node<'t>,
    ) -> ConflictResolution {
        if remote_child_node.is_built_in_root() {
            // Always use the local parent and position for roots.
            return ConflictResolution::Local;
        }

        match (
            local_parent_node.needs_merge,
            remote_parent_node.needs_merge,
        ) {
            (true, true) => {
                // If both parents changed, compare timestamps to decide where
                // to keep the local child.
                let latest_local_age = local_child_node.age.min(local_parent_node.age);
                let latest_remote_age = remote_child_node.age.min(remote_parent_node.age);

                if latest_local_age < latest_remote_age {
                    ConflictResolution::Local
                } else {
                    ConflictResolution::Remote
                }
            }

            // If only the local or remote parent changed, keep the child in its
            // new parent.
            (true, false) => ConflictResolution::Local,
            (false, true) => ConflictResolution::Remote,

            (false, false) => ConflictResolution::Unchanged,
        }
    }

    /// Checks if a remote node is locally moved or deleted, and reparents any
    /// descendants that aren't also remotely deleted to the merged node.
    ///
    /// This is the inverse of
    /// `check_for_remote_structure_change_of_local_node`.
    fn check_for_local_structure_change_of_remote_node(
        &mut self,
        merged_node: &mut MergedNode<'t>,
        remote_parent_node: Node<'t>,
        remote_node: Node<'t>,
    ) -> Result<StructureChange> {
        if !remote_node.is_syncable() {
            // If the remote node is known to be non-syncable, we unconditionally
            // delete it, even if it's syncable or moved locally.
            trace!(
                self.driver,
                "Deleting non-syncable remote node {}",
                remote_node
            );
            return self.delete_remote_node(merged_node, remote_node);
        }

        if !self.local_tree.is_deleted(&remote_node.guid) {
            if let Some(local_node) = self.local_tree.node_for_guid(&remote_node.guid) {
                if !local_node.is_syncable() {
                    // The remote node is syncable, but the local node is
                    // non-syncable. Unconditionally delete it.
                    trace!(
                        self.driver,
                        "Remote node {} is syncable, but local node {} isn't; deleting",
                        remote_node,
                        local_node
                    );
                    return self.delete_remote_node(merged_node, remote_node);
                }
                if local_node.validity == Validity::Replace
                    && remote_node.validity == Validity::Replace
                {
                    // The nodes are invalid on both sides, so we can't apply
                    // or reupload a valid copy. Delete it.
                    return self.delete_remote_node(merged_node, remote_node);
                }
                let local_parent_node = local_node
                    .parent()
                    .expect("Can't check for structure changes without local parent");
                if local_parent_node.guid != remote_parent_node.guid {
                    return Ok(StructureChange::Moved);
                }
                return Ok(StructureChange::Unchanged);
            }
            if remote_node.validity == Validity::Replace {
                // The remote node is invalid and doesn't exist locally, so we
                // can't reupload a valid copy. We must delete it.
                return self.delete_remote_node(merged_node, remote_node);
            }
            return Ok(StructureChange::Unchanged);
        }

        if remote_node.validity == Validity::Replace {
            // The remote node is invalid and deleted locally, so we can't
            // reupload a valid copy. Delete it.
            return self.delete_remote_node(merged_node, remote_node);
        }

        if remote_node.is_built_in_root() {
            // If the remote node is a content root, don't delete it locally.
            return Ok(StructureChange::Unchanged);
        }

        if remote_node.needs_merge {
            if !remote_node.is_folder() {
                // If a non-folder child is deleted locally and changed remotely, we
                // ignore the local deletion and take the remote child.
                trace!(
                    self.driver,
                    "Remote non-folder {} deleted locally and changed remotely; \
                     taking remote change",
                    remote_node
                );
                self.structure_counts.remote_revives += 1;
                return Ok(StructureChange::Unchanged);
            }
            // For folders, we always take the local deletion and relocate remotely
            // changed grandchildren to the merged node. We could use the remote
            // tree to revive the child folder, but it's easier to relocate orphaned
            // grandchildren than to partially revive the child folder.
            trace!(
                self.driver,
                "Remote folder {} deleted locally and changed remotely; \
                 taking local deletion",
                remote_node
            );
            self.structure_counts.local_deletes += 1;
        } else {
            trace!(
                self.driver,
                "Remote node {} deleted locally and not changed remotely; \
                 taking local deletion",
                remote_node
            );
        }

        // Take the local deletion and relocate any new remote descendants to the
        // merged node.
        self.delete_remote_node(merged_node, remote_node)
    }

    /// Checks if a local node is remotely moved or deleted, and reparents any
    /// descendants that aren't also locally deleted to the merged node.
    ///
    /// This is the inverse of
    /// `check_for_local_structure_change_of_remote_node`.
    fn check_for_remote_structure_change_of_local_node(
        &mut self,
        merged_node: &mut MergedNode<'t>,
        local_parent_node: Node<'t>,
        local_node: Node<'t>,
    ) -> Result<StructureChange> {
        if !local_node.is_syncable() {
            // If the local node is known to be non-syncable, we unconditionally
            // delete it, even if it's syncable or moved remotely.
            trace!(
                self.driver,
                "Deleting non-syncable local node {}",
                local_node
            );
            return self.delete_local_node(merged_node, local_node);
        }

        if !self.remote_tree.is_deleted(&local_node.guid) {
            if let Some(remote_node) = self.remote_tree.node_for_guid(&local_node.guid) {
                if !remote_node.is_syncable() {
                    // The local node is syncable, but the remote node is not.
                    // This can happen if we applied an orphaned left pane
                    // query in a previous sync, and later saw the left pane
                    // root on the server. Since we now have the complete
                    // subtree, we can remove it.
                    trace!(
                        self.driver,
                        "Local node {} is syncable, but remote node {} isn't; deleting",
                        local_node,
                        remote_node
                    );
                    return self.delete_local_node(merged_node, local_node);
                }
                if remote_node.validity == Validity::Replace
                    && local_node.validity == Validity::Replace
                {
                    // The nodes are invalid on both sides, so we can't replace
                    // the local copy with a remote one. Delete it.
                    return self.delete_local_node(merged_node, local_node);
                }
                // Otherwise, either both nodes are valid; or the remote node
                // is invalid but the local node is valid, so we can reupload a
                // valid copy.
                let remote_parent_node = remote_node
                    .parent()
                    .expect("Can't check for structure changes without remote parent");
                if remote_parent_node.guid != local_parent_node.guid {
                    return Ok(StructureChange::Moved);
                }
                return Ok(StructureChange::Unchanged);
            }
            if local_node.validity == Validity::Replace {
                // The local node is invalid and doesn't exist remotely, so
                // we can't replace the local copy. Delete it.
                return self.delete_local_node(merged_node, local_node);
            }
            return Ok(StructureChange::Unchanged);
        }

        if local_node.validity == Validity::Replace {
            // The local node is invalid and deleted remotely, so we can't
            // replace the local copy. Delete it.
            return self.delete_local_node(merged_node, local_node);
        }

        if local_node.is_built_in_root() {
            // If the local node is a content root, don't delete it remotely.
            return Ok(StructureChange::Unchanged);
        }

        // See `check_for_local_structure_change_of_remote_node` for an
        // explanation of how we decide to take or ignore a deletion.
        if local_node.needs_merge {
            if !local_node.is_folder() {
                trace!(
                    self.driver,
                    "Local non-folder {} deleted remotely and changed locally; taking local change",
                    local_node
                );
                self.structure_counts.local_revives += 1;
                return Ok(StructureChange::Unchanged);
            }
            trace!(
                self.driver,
                "Local folder {} deleted remotely and changed locally; taking remote deletion",
                local_node
            );
            self.structure_counts.remote_deletes += 1;
        } else {
            trace!(
                self.driver,
                "Local node {} deleted remotely and not changed locally; taking remote deletion",
                local_node
            );
        }

        // Take the remote deletion and relocate any new local descendants to the
        // merged node.
        self.delete_local_node(merged_node, local_node)
    }

    /// Marks a remote node as deleted, and relocates all remote descendants
    /// that aren't also locally deleted to the merged node. This avoids data
    /// loss if the user adds a bookmark to a folder on another device, and
    /// deletes that folder locally.
    ///
    /// This is the inverse of `delete_local_node`.
    fn delete_remote_node(
        &mut self,
        merged_node: &mut MergedNode<'t>,
        remote_node: Node<'t>,
    ) -> Result<StructureChange> {
        self.delete_remotely.insert(remote_node.guid.clone());
        for remote_child_node in remote_node.children() {
            self.signal.err_if_aborted()?;
            if self.merged_guids.contains(&remote_child_node.guid) {
                trace!(
                    self.driver,
                    "Remote child {} can't be an orphan; already merged",
                    remote_child_node
                );
                continue;
            }
            match self.check_for_local_structure_change_of_remote_node(
                merged_node,
                remote_node,
                remote_child_node,
            )? {
                StructureChange::Moved | StructureChange::Deleted => {
                    // The remote child is already moved or deleted locally, so we should
                    // ignore it instead of treating it as a remote orphan.
                    continue;
                }
                StructureChange::Unchanged => {
                    trace!(
                        self.driver,
                        "Relocating remote orphan {} to {}",
                        remote_child_node,
                        merged_node
                    );

                    // Flag the new parent and moved remote orphan for reupload.
                    let mut merged_orphan_node = if let Some(local_child_node) =
                        self.local_tree.node_for_guid(&remote_child_node.guid)
                    {
                        self.two_way_merge(local_child_node, remote_child_node)
                    } else {
                        self.merge_remote_only_node(remote_child_node)
                    }?;
                    merged_node.merge_state = merged_node
                        .merge_state
                        .with_new_local_structure()
                        .with_new_remote_structure();
                    merged_orphan_node.merge_state = merged_orphan_node
                        .merge_state
                        .with_new_local_structure()
                        .with_new_remote_structure();
                    merged_node.merged_children.push(merged_orphan_node);
                    self.structure_counts.merged_nodes += 1;
                }
            }
        }
        Ok(StructureChange::Deleted)
    }

    /// Marks a local node as deleted, and relocates all local descendants
    /// that aren't also remotely deleted to the merged node.
    ///
    /// This is the inverse of `delete_remote_node`.
    fn delete_local_node(
        &mut self,
        merged_node: &mut MergedNode<'t>,
        local_node: Node<'t>,
    ) -> Result<StructureChange> {
        self.delete_locally.insert(local_node.guid.clone());
        for local_child_node in local_node.children() {
            self.signal.err_if_aborted()?;
            if self.merged_guids.contains(&local_child_node.guid) {
                trace!(
                    self.driver,
                    "Local child {} can't be an orphan; already merged",
                    local_child_node
                );
                continue;
            }
            match self.check_for_remote_structure_change_of_local_node(
                merged_node,
                local_node,
                local_child_node,
            )? {
                StructureChange::Moved | StructureChange::Deleted => {
                    // The local child is already moved or deleted remotely, so we should
                    // ignore it instead of treating it as a local orphan.
                    continue;
                }
                StructureChange::Unchanged => {
                    trace!(
                        self.driver,
                        "Relocating local orphan {} to {}",
                        local_child_node,
                        merged_node
                    );

                    // Flag the new parent and moved local orphan for reupload.
                    let mut merged_orphan_node = if let Some(remote_child_node) =
                        self.remote_tree.node_for_guid(&local_child_node.guid)
                    {
                        self.two_way_merge(local_child_node, remote_child_node)
                    } else {
                        self.merge_local_only_node(local_child_node)
                    }?;
                    merged_node.merge_state = merged_node
                        .merge_state
                        .with_new_local_structure()
                        .with_new_remote_structure();
                    merged_orphan_node.merge_state = merged_orphan_node
                        .merge_state
                        .with_new_local_structure()
                        .with_new_remote_structure();
                    merged_node.merged_children.push(merged_orphan_node);
                    self.structure_counts.merged_nodes += 1;
                }
            }
        }
        Ok(StructureChange::Deleted)
    }

    /// Finds all children of a local folder with similar content as children of
    /// the corresponding remote folder. This is used to dedupe local items that
    /// haven't been uploaded yet, to remote items that don't exist locally.
    ///
    /// Recall that we match items by GUID as we walk down the tree. If a GUID
    /// on one side doesn't exist on the other, we fall back to a content
    /// match in the same folder.
    ///
    /// This method is called the first time that
    /// `find_remote_node_matching_local_node` merges a local child that
    /// doesn't exist remotely, and
    /// the first time that `find_local_node_matching_remote_node` merges a
    /// remote child that doesn't exist locally.
    ///
    /// Finding all possible dupes is O(m + n) in the worst case, where `m` is
    /// the number of local children, and `n` is the number of remote
    /// children. We cache matches in
    /// `matching_dupes_by_local_parent_guid`, so deduping all
    /// remaining children of the same folder, on both sides, only needs two
    /// O(1) map lookups per child.
    fn find_all_matching_dupes_in_folders(
        &self,
        local_parent_node: Node<'t>,
        remote_parent_node: Node<'t>,
    ) -> Result<MatchingDupes<'t>> {
        let mut dupe_key_to_local_nodes: HashMap<DupeKey<'_>, VecDeque<_>> = HashMap::new();

        for (local_position, local_child_node) in local_parent_node.children().enumerate() {
            self.signal.err_if_aborted()?;
            if local_child_node.is_built_in_root() {
                trace!(
                    self.driver,
                    "Not deduping local built-in root {}",
                    local_child_node
                );
                continue;
            }
            if self.remote_tree.mentions(&local_child_node.guid) {
                trace!(
                    self.driver,
                    "Not deduping local child {}; already deleted or exists remotely",
                    local_child_node
                );
                continue;
            }
            match local_child_node.content() {
                Some(local_child_content) => {
                    // Store matching local children in an array, in case multiple children
                    // have the same dupe key (for example, a toolbar containing multiple
                    // empty folders, as in bug 1213369).
                    let dupe_key = match local_child_content {
                        Content::Bookmark { .. } | Content::Folder { .. } => {
                            DupeKey::WithoutPosition(local_child_content)
                        }
                        Content::Separator => {
                            DupeKey::WithPosition(local_child_content, local_position)
                        }
                    };
                    let local_nodes_for_key = dupe_key_to_local_nodes.entry(dupe_key).or_default();
                    local_nodes_for_key.push_back(local_child_node);
                }
                None => {
                    trace!(
                        self.driver,
                        "Not deduping local child {} without content info",
                        local_child_node
                    );
                }
            }
        }

        let mut local_to_remote = HashMap::new();
        let mut remote_to_local = HashMap::new();

        for (remote_position, remote_child_node) in remote_parent_node.children().enumerate() {
            self.signal.err_if_aborted()?;
            if remote_child_node.is_built_in_root() {
                trace!(
                    self.driver,
                    "Not deduping remote built-in root {}",
                    remote_child_node
                );
                continue;
            }
            if self.local_tree.mentions(&remote_child_node.guid) {
                trace!(
                    self.driver,
                    "Not deduping remote child {}; already deleted or exists locally",
                    remote_child_node
                );
                continue;
            }
            if remote_to_local.contains_key(&remote_child_node.guid) {
                trace!(
                    self.driver,
                    "Not deduping remote child {}; already deduped",
                    remote_child_node
                );
                continue;
            }
            // Note that we don't need to check if the remote node is deleted
            // locally, because it wouldn't have local content entries if it
            // were.
            match remote_child_node.content() {
                Some(remote_child_content) => {
                    let dupe_key = match remote_child_content {
                        Content::Bookmark { .. } | Content::Folder { .. } => {
                            DupeKey::WithoutPosition(remote_child_content)
                        }
                        Content::Separator => {
                            DupeKey::WithPosition(remote_child_content, remote_position)
                        }
                    };
                    if let Some(local_nodes_for_key) = dupe_key_to_local_nodes.get_mut(&dupe_key) {
                        if let Some(local_child_node) = local_nodes_for_key.pop_front() {
                            trace!(
                                self.driver,
                                "Deduping local child {} to remote child {}",
                                local_child_node,
                                remote_child_node
                            );
                            local_to_remote
                                .insert(local_child_node.guid.clone(), remote_child_node);
                            remote_to_local
                                .insert(remote_child_node.guid.clone(), local_child_node);
                        } else {
                            trace!(
                                self.driver,
                                "Not deduping remote child {}; no remaining local content matches",
                                remote_child_node
                            );
                            continue;
                        }
                    } else {
                        trace!(
                            self.driver,
                            "Not deduping remote child {}; no local content matches",
                            remote_child_node
                        );
                        continue;
                    }
                }
                None => {
                    trace!(
                        self.driver,
                        "Not deduping remote child {} without content info",
                        remote_child_node
                    );
                }
            }
        }

        Ok((local_to_remote, remote_to_local))
    }

    /// Finds a remote node with a different GUID that matches the content of a
    /// local node.
    ///
    /// This is the inverse of `find_local_node_matching_remote_node`.
    fn find_remote_node_matching_local_node(
        &mut self,
        merged_node: &MergedNode<'t>,
        local_parent_node: Node<'t>,
        remote_parent_node: Option<Node<'t>>,
        local_child_node: Node<'t>,
    ) -> Result<Option<Node<'t>>> {
        if let Some(remote_parent_node) = remote_parent_node {
            let mut matching_dupes_by_local_parent_guid = mem::replace(
                &mut self.matching_dupes_by_local_parent_guid,
                HashMap::new(),
            );
            let new_remote_node = {
                let (local_to_remote, _) = match matching_dupes_by_local_parent_guid
                    .entry(local_parent_node.guid.clone())
                {
                    Entry::Occupied(entry) => entry.into_mut(),
                    Entry::Vacant(entry) => {
                        trace!(
                            self.driver,
                            "First local child {} doesn't exist remotely; \
                             finding all matching dupes in local {} and remote {}",
                            local_child_node,
                            local_parent_node,
                            remote_parent_node
                        );
                        let matching_dupes = self.find_all_matching_dupes_in_folders(
                            local_parent_node,
                            remote_parent_node,
                        )?;
                        entry.insert(matching_dupes)
                    }
                };
                let new_remote_node = local_to_remote.get(&local_child_node.guid);
                new_remote_node.map(|node| {
                    self.structure_counts.dupes += 1;
                    *node
                })
            };
            mem::replace(
                &mut self.matching_dupes_by_local_parent_guid,
                matching_dupes_by_local_parent_guid,
            );
            Ok(new_remote_node)
        } else {
            trace!(
                self.driver,
                "Merged node {} doesn't exist remotely; no potential dupes for local child {}",
                merged_node,
                local_child_node
            );
            Ok(None)
        }
    }

    /// Finds a local node with a different GUID that matches the content of a
    /// remote node.
    ///
    /// This is the inverse of `find_remote_node_matching_local_node`.
    fn find_local_node_matching_remote_node(
        &mut self,
        merged_node: &MergedNode<'t>,
        local_parent_node: Option<Node<'t>>,
        remote_parent_node: Node<'t>,
        remote_child_node: Node<'t>,
    ) -> Result<Option<Node<'t>>> {
        if let Some(local_parent_node) = local_parent_node {
            let mut matching_dupes_by_local_parent_guid = mem::replace(
                &mut self.matching_dupes_by_local_parent_guid,
                HashMap::new(),
            );
            let new_local_node = {
                let (_, remote_to_local) = match matching_dupes_by_local_parent_guid
                    .entry(local_parent_node.guid.clone())
                {
                    Entry::Occupied(entry) => entry.into_mut(),
                    Entry::Vacant(entry) => {
                        trace!(
                            self.driver,
                            "First remote child {} doesn't exist locally; \
                             finding all matching dupes in local {} and remote {}",
                            remote_child_node,
                            local_parent_node,
                            remote_parent_node
                        );
                        let matching_dupes = self.find_all_matching_dupes_in_folders(
                            local_parent_node,
                            remote_parent_node,
                        )?;
                        entry.insert(matching_dupes)
                    }
                };
                let new_local_node = remote_to_local.get(&remote_child_node.guid);
                new_local_node.map(|node| {
                    self.structure_counts.dupes += 1;
                    *node
                })
            };
            mem::replace(
                &mut self.matching_dupes_by_local_parent_guid,
                matching_dupes_by_local_parent_guid,
            );
            Ok(new_local_node)
        } else {
            trace!(
                self.driver,
                "Merged node {} doesn't exist locally; no potential dupes for remote child {}",
                merged_node,
                remote_child_node
            );
            Ok(None)
        }
    }
}

/// The root of a merged tree, from which all merged nodes descend.
#[derive(Debug)]
pub struct MergedRoot<'t> {
    local_tree: &'t Tree,
    remote_tree: &'t Tree,
    node: MergedNode<'t>,
    merged_guids: HashSet<Guid>,
    delete_locally: HashSet<Guid>,
    delete_remotely: HashSet<Guid>,
    structure_counts: StructureCounts,
}

impl<'t> MergedRoot<'t> {
    /// Returns the root node.
    #[inline]
    pub fn node(&self) -> &MergedNode<'_> {
        &self.node
    }

    /// Returns a sequence of completion operations, or "completion ops", to
    /// apply to the local tree so that it matches the merged tree. The abort
    /// signal can be used to interrupt fetching the ops.
    pub fn completion_ops_with_signal(
        &self,
        signal: &impl AbortSignal,
    ) -> Result<CompletionOps<'_>> {
        let mut ops = CompletionOps::default();
        accumulate(signal, &mut ops, self.node(), 1, false)?;

        // Clean up tombstones for local and remote items that are revived on
        // the other side.
        for guid in self
            .local_tree
            .deletions()
            .difference(&self.delete_remotely)
        {
            // For ignored local deletions, we remove the local tombstone. If
            // the item is already deleted remotely, we also flag the remote
            // tombstone as merged.
            signal.err_if_aborted()?;
            ops.delete_local_tombstones.push(DeleteLocalTombstone(guid));
            if self.remote_tree.is_deleted(guid) {
                ops.set_remote_merged.push(SetRemoteMerged(guid));
            }
        }
        for guid in self
            .remote_tree
            .deletions()
            .difference(&self.delete_locally)
            .filter(|guid| !self.local_tree.exists(guid))
        {
            // Ignored remote deletions are handled a little differently. Unlike
            // local tombstones, which are stored separately from items, remote
            // tombstones and items are stored in the same table. This means we
            // only need to flag the remote tombstone as merged if it's for an
            // item that doesn't exist locally. If the local item does exist,
            // we can avoid an extra write to flag the tombstone that we'll
            // replace with the item, anyway. If the item is already deleted
            // locally, we also delete the local tombstone.
            signal.err_if_aborted()?;
            ops.set_remote_merged.push(SetRemoteMerged(guid));
            if self.local_tree.is_deleted(guid) {
                ops.delete_local_tombstones.push(DeleteLocalTombstone(guid));
            }
        }

        // Emit completion ops for deleted items.
        for guid in self.deletions() {
            signal.err_if_aborted()?;
            match (
                self.local_tree.node_for_guid(guid),
                self.remote_tree.node_for_guid(guid),
            ) {
                (Some(local_node), Some(remote_node)) => {
                    // Delete items that are non-syncable or invalid on both
                    // sides.
                    ops.delete_local_items.push(DeleteLocalItem(local_node));
                    ops.insert_local_tombstones
                        .push(InsertLocalTombstone(remote_node));
                    ops.upload_tombstones.push(UploadTombstone(guid));
                }
                (Some(local_node), None) => {
                    // Apply remote tombstones, or delete invalid local-only
                    // items. If the item is deleted remotely, flag the remote
                    // tombstone as merged. If not, we don't need to upload one,
                    // since the item is only known locally.
                    ops.delete_local_items.push(DeleteLocalItem(local_node));
                    if self.remote_tree.is_deleted(guid) {
                        ops.set_remote_merged.push(SetRemoteMerged(guid));
                    }
                }
                (None, Some(remote_node)) => {
                    // Take local tombstones, or delete invalid remote-only
                    // items. If it's not already deleted locally, insert a
                    // tombstone for the item.
                    if !self.local_tree.is_deleted(guid) {
                        ops.insert_local_tombstones
                            .push(InsertLocalTombstone(remote_node));
                    }
                    ops.upload_tombstones.push(UploadTombstone(guid));
                }
                (None, None) => {
                    // Clean up local tombstones, and flag remote tombstones as
                    // merged, for items deleted on both sides.
                    if self.local_tree.is_deleted(guid) {
                        ops.delete_local_tombstones.push(DeleteLocalTombstone(guid));
                    }
                    if self.remote_tree.is_deleted(guid) {
                        ops.set_remote_merged.push(SetRemoteMerged(guid));
                    }
                }
            }
        }

        Ok(ops)
    }

    /// Returns a sequence of completion ops, without interruption.
    #[inline]
    pub fn completion_ops(&self) -> CompletionOps<'_> {
        self.completion_ops_with_signal(&DefaultAbortSignal)
            .unwrap()
    }

    /// Returns an iterator for all accepted local and remote deletions.
    #[inline]
    pub fn deletions(&self) -> impl Iterator<Item = &Guid> {
        self.delete_locally.union(&self.delete_remotely)
    }

    /// Returns an iterator for all items that should be deleted from the
    /// local tree.
    #[inline]
    pub fn local_deletions(&self) -> impl Iterator<Item = &Guid> {
        self.delete_locally.difference(&self.delete_remotely)
    }

    /// Returns an iterator for all items that should be deleted from the
    /// remote tree.
    #[inline]
    pub fn remote_deletions(&self) -> impl Iterator<Item = &Guid> {
        self.delete_remotely.iter()
    }

    /// Returns structure change counts for this merged root.
    #[inline]
    pub fn counts(&self) -> &StructureCounts {
        &self.structure_counts
    }
}

/// Completion operations to apply to the local tree after a merge. These are
/// represented as separate structs in `Vec`s instead of enums yielded from an
/// iterator so that consumers can easily chunk them.
#[derive(Clone, Debug, Default)]
pub struct CompletionOps<'t> {
    pub change_guids: Vec<ChangeGuid<'t>>,
    pub apply_remote_items: Vec<ApplyRemoteItem<'t>>,
    pub apply_new_local_structure: Vec<ApplyNewLocalStructure<'t>>,
    pub set_local_unmerged: Vec<SetLocalUnmerged<'t>>,
    pub set_local_merged: Vec<SetLocalMerged<'t>>,
    pub set_remote_merged: Vec<SetRemoteMerged<'t>>,
    pub delete_local_tombstones: Vec<DeleteLocalTombstone<'t>>,
    pub insert_local_tombstones: Vec<InsertLocalTombstone<'t>>,
    pub delete_local_items: Vec<DeleteLocalItem<'t>>,
    pub upload_items: Vec<UploadItem<'t>>,
    pub upload_tombstones: Vec<UploadTombstone<'t>>,
}

impl<'t> CompletionOps<'t> {
    /// Returns `true` if there are no completion ops to apply.
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.change_guids.is_empty()
            && self.apply_remote_items.is_empty()
            && self.apply_new_local_structure.is_empty()
            && self.set_local_unmerged.is_empty()
            && self.set_local_merged.is_empty()
            && self.set_remote_merged.is_empty()
            && self.delete_local_tombstones.is_empty()
            && self.insert_local_tombstones.is_empty()
            && self.delete_local_items.is_empty()
            && self.upload_items.is_empty()
            && self.upload_tombstones.is_empty()
    }

    /// Returns a printable summary of all completion ops to apply.
    pub fn summarize(&self) -> Vec<String> {
        std::iter::empty()
            .chain(to_strings(&self.change_guids))
            .chain(to_strings(&self.apply_remote_items))
            .chain(to_strings(&self.apply_new_local_structure))
            .chain(to_strings(&self.set_local_unmerged))
            .chain(to_strings(&self.set_local_merged))
            .chain(to_strings(&self.set_remote_merged))
            .chain(to_strings(&self.delete_local_tombstones))
            .chain(to_strings(&self.insert_local_tombstones))
            .chain(to_strings(&self.delete_local_items))
            .chain(to_strings(&self.upload_items))
            .chain(to_strings(&self.upload_tombstones))
            .collect()
    }
}

/// A completion op to change the local GUID to the merged GUID. This is used
/// to dedupe new local items to remote ones, as well as to fix up invalid
/// GUIDs.
#[derive(Clone, Copy, Debug)]
pub struct ChangeGuid<'t> {
    /// The merged node to update.
    pub merged_node: &'t MergedNode<'t>,
    /// The level of the node in the merged tree. Desktop uses this to ensure
    /// that GUID change observers are notified in level order (parents before
    /// children).
    pub level: usize,
}

impl<'t> ChangeGuid<'t> {
    /// Returns the local node for this completion op. Panics if the local node
    /// isn't set, as we should never emit a `ChangeGuid` op in that case.
    #[inline]
    pub fn local_node(&self) -> &'t Node<'t> {
        self.merged_node
            .merge_state
            .local_node()
            .expect("Can't change local GUID without local node")
    }
}

impl<'t> fmt::Display for ChangeGuid<'t> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "Change {} to {}",
            self.local_node().guid,
            self.merged_node.guid
        )
    }
}

/// A completion op to insert a new remote item into the local tree, or apply
/// synced changes to an existing item.
#[derive(Clone, Copy, Debug)]
pub struct ApplyRemoteItem<'t> {
    pub merged_node: &'t MergedNode<'t>,
    pub level: usize,
}

impl<'t> ApplyRemoteItem<'t> {
    /// Returns the remote node for this completion op. Panics if the remote
    /// node isn't set, as we should never emit an `ApplyRemoteItem` op in
    /// that case.
    #[inline]
    pub fn remote_node(&self) -> &'t Node<'t> {
        self.merged_node
            .merge_state
            .remote_node()
            .expect("Can't apply remote item without remote node")
    }
}

impl<'t> fmt::Display for ApplyRemoteItem<'t> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.merged_node.remote_guid_changed() {
            write!(
                f,
                "Apply remote {} as {}",
                self.remote_node().guid,
                self.merged_node.guid
            )
        } else {
            write!(f, "Apply remote {}", self.merged_node.guid)
        }
    }
}

/// A completion op to update the parent and position of a local item.
#[derive(Clone, Copy, Debug)]
pub struct ApplyNewLocalStructure<'t> {
    pub merged_node: &'t MergedNode<'t>,
    pub merged_parent_node: &'t MergedNode<'t>,
    pub position: usize,
    pub level: usize,
}

impl<'t> fmt::Display for ApplyNewLocalStructure<'t> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "Move {} into {} at {}",
            self.merged_node.guid, self.merged_parent_node.guid, self.position
        )
    }
}

/// A completion op to flag a local item for upload.
#[derive(Clone, Copy, Debug)]
pub struct SetLocalUnmerged<'t> {
    pub merged_node: &'t MergedNode<'t>,
}

impl<'t> fmt::Display for SetLocalUnmerged<'t> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Flag local {} as unmerged", self.merged_node.guid)
    }
}

/// A completion op to skip uploading a local item after resolving merge
/// conflicts.
#[derive(Clone, Copy, Debug)]
pub struct SetLocalMerged<'t> {
    pub merged_node: &'t MergedNode<'t>,
}

impl<'t> fmt::Display for SetLocalMerged<'t> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Flag local {} as merged", self.merged_node.guid)
    }
}

/// A completion op to upload or reupload a merged item.
#[derive(Clone, Copy, Debug)]
pub struct UploadItem<'t> {
    pub merged_node: &'t MergedNode<'t>,
}

impl<'t> fmt::Display for UploadItem<'t> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Upload item {}", self.merged_node.guid)
    }
}

/// A completion op to upload a tombstone.
#[derive(Clone, Copy, Debug)]
pub struct UploadTombstone<'t>(&'t Guid);

impl<'t> UploadTombstone<'t> {
    /// Returns the GUID to use for the tombstone.
    #[inline]
    pub fn guid(self) -> &'t Guid {
        self.0
    }
}

impl<'t> fmt::Display for UploadTombstone<'t> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Upload tombstone {}", self.0)
    }
}

/// A completion op to flag a remote item as merged.
#[derive(Clone, Copy, Debug)]
pub struct SetRemoteMerged<'t>(&'t Guid);

impl<'t> SetRemoteMerged<'t> {
    /// Returns the remote GUID for the item to flag as merged.
    #[inline]
    pub fn guid(self) -> &'t Guid {
        self.0
    }
}

impl<'t> fmt::Display for SetRemoteMerged<'t> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Flag remote {} as merged", self.guid())
    }
}

/// A completion op to store a tombstone for a remote item.
#[derive(Clone, Copy, Debug)]
pub struct InsertLocalTombstone<'t>(Node<'t>);

impl<'t> InsertLocalTombstone<'t> {
    /// Returns the node for the item to delete remotely.
    #[inline]
    pub fn remote_node(&self) -> Node<'t> {
        self.0
    }
}

impl<'t> fmt::Display for InsertLocalTombstone<'t> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Insert local tombstone {}", self.0.guid)
    }
}

/// A completion op to delete a local tombstone.
#[derive(Clone, Copy, Debug)]
pub struct DeleteLocalTombstone<'t>(&'t Guid);

impl<'t> DeleteLocalTombstone<'t> {
    /// Returns the GUID of the tombstone.
    #[inline]
    pub fn guid(self) -> &'t Guid {
        self.0
    }
}

impl<'t> fmt::Display for DeleteLocalTombstone<'t> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Delete local tombstone {}", self.0)
    }
}

/// A completion op to delete an item from the local tree.
#[derive(Clone, Copy, Debug)]
pub struct DeleteLocalItem<'t>(Node<'t>);

impl<'t> DeleteLocalItem<'t> {
    // Returns the node for the item to delete locally.
    #[inline]
    pub fn local_node(&self) -> Node<'t> {
        self.0
    }
}

impl<'t> fmt::Display for DeleteLocalItem<'t> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Delete local item {}", self.0.guid)
    }
}

/// Recursively accumulates completion ops, starting at `merged_node` and
/// drilling down into all its descendants.
fn accumulate<'t, A: AbortSignal>(
    signal: &A,
    ops: &mut CompletionOps<'t>,
    merged_node: &'t MergedNode<'t>,
    level: usize,
    is_tagging: bool,
) -> Result<()> {
    for (position, merged_child_node) in merged_node.merged_children.iter().enumerate() {
        signal.err_if_aborted()?;
        let is_tagging = if merged_child_node.guid == TAGS_GUID {
            true
        } else {
            is_tagging
        };
        if merged_child_node.merge_state.should_apply_item() {
            let apply_remote_item = ApplyRemoteItem {
                merged_node: merged_child_node,
                level,
            };
            ops.apply_remote_items.push(apply_remote_item);
        }
        if merged_child_node.local_guid_changed() {
            let change_guid = ChangeGuid {
                merged_node: merged_child_node,
                level,
            };
            ops.change_guids.push(change_guid);
        }
        let local_child_node = merged_node
            .merge_state
            .local_node()
            .and_then(|local_parent_node| local_parent_node.child(position));
        let merged_local_child_node = merged_child_node.merge_state.local_node();
        if local_child_node
            .and_then(|m| merged_local_child_node.map(|n| m.guid != n.guid))
            .unwrap_or(true)
        {
            // As an optimization, we only emit ops to apply a new local
            // structure for items that actually moved. For example, if the
            // local children are (A B C D) and the merged children are
            // (A D C B), only (B D) need new structure.
            let apply_new_local_structure = ApplyNewLocalStructure {
                merged_node: merged_child_node,
                merged_parent_node: merged_node,
                position,
                level,
            };
            ops.apply_new_local_structure
                .push(apply_new_local_structure);
        }
        let local_needs_merge = merged_child_node
            .merge_state
            .local_node()
            .map(|node| node.needs_merge)
            .unwrap_or(false);
        let should_upload = merged_child_node.merge_state.should_upload();
        match (local_needs_merge, should_upload) {
            (false, true) => {
                // Local item isn't flagged for upload, but should be.
                let set_local_unmerged = SetLocalUnmerged {
                    merged_node: merged_child_node,
                };
                ops.set_local_unmerged.push(set_local_unmerged);
            }
            (true, false) => {
                // Local item flagged for upload when it doesn't need to be.
                let set_local_merged = SetLocalMerged {
                    merged_node: merged_child_node,
                };
                ops.set_local_merged.push(set_local_merged);
            }
            _ => {}
        }
        if should_upload && !is_tagging {
            // (Re)upload items. Ignore the tags root and its descendants:
            // they're part of the local tree on Desktop (and will be removed
            // in bug 424160), but aren't synced as part of the structure.
            ops.upload_items.push(UploadItem {
                merged_node: merged_child_node,
            });
        }
        if let Some(remote_child_node) = merged_child_node.merge_state.remote_node() {
            if remote_child_node.needs_merge && !should_upload {
                // If the remote item was merged, and doesn't need to be
                // reuploaded, flag it as merged in the remote tree. Note that
                // we _don't_ emit this for locally revived items, or items with
                // new remote structure.
                let set_remote_merged = SetRemoteMerged(&remote_child_node.guid);
                ops.set_remote_merged.push(set_remote_merged);
            }
        }
        accumulate(signal, ops, merged_child_node, level + 1, is_tagging)?;
    }
    Ok(())
}

/// Converts all items in the list to strings.
pub(crate) fn to_strings<'a, T: ToString>(items: &'a [T]) -> impl Iterator<Item = String> + 'a {
    items.iter().map(ToString::to_string)
}
