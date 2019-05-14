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
    borrow::Cow,
    cmp::Ordering,
    collections::{HashMap, HashSet},
    convert::{TryFrom, TryInto},
    fmt, mem,
    ops::Deref,
    ptr,
};

use smallbitvec::SmallBitVec;

use crate::error::{Error, ErrorKind, Result};
use crate::guid::Guid;

/// The type for entry indices in the tree.
type Index = usize;

/// A complete, rooted bookmark tree with tombstones.
///
/// The tree stores bookmark items in a vector, and uses indices in the vector
/// to identify parents and children. This makes traversal and lookup very
/// efficient. Retrieving a node's parent takes one indexing operation,
/// retrieving children takes one indexing operation per child, and retrieving
/// a node by random GUID takes one hash map lookup and one indexing operation.
#[derive(Debug)]
pub struct Tree {
    entry_index_by_guid: HashMap<Guid, Index>,
    entries: Vec<TreeEntry>,
    deleted_guids: HashSet<Guid>,
    problems: Problems,
}

impl Tree {
    /// Returns a builder for a rooted tree.
    pub fn with_root(root: Item) -> Builder {
        let mut entry_index_by_guid = HashMap::new();
        entry_index_by_guid.insert(root.guid.clone(), 0);

        Builder {
            entries: vec![BuilderEntry {
                item: root,
                parent: BuilderEntryParent::Root,
                children: Vec::new(),
            }],
            entry_index_by_guid,
            reparent_orphans_to: None,
        }
    }

    /// Returns the root node.
    #[inline]
    pub fn root(&self) -> Node<'_> {
        Node(self, &self.entries[0])
    }

    /// Returns an iterator for all tombstoned GUIDs.
    #[inline]
    pub fn deletions(&self) -> impl Iterator<Item = &Guid> {
        self.deleted_guids.iter()
    }

    /// Indicates if the GUID is known to be deleted. If `Tree::node_for_guid`
    /// returns `None` and `Tree::is_deleted` returns `false`, the item doesn't
    /// exist in the tree at all.
    #[inline]
    pub fn is_deleted(&self, guid: &Guid) -> bool {
        self.deleted_guids.contains(guid)
    }

    /// Notes a tombstone for a deleted item.
    #[inline]
    pub fn note_deleted(&mut self, guid: Guid) {
        self.deleted_guids.insert(guid);
    }

    /// Returns an iterator for all node and tombstone GUIDs.
    pub fn guids(&self) -> impl Iterator<Item = &Guid> {
        assert_eq!(self.entries.len(), self.entry_index_by_guid.len());
        self.entries
            .iter()
            .map(|entry| &entry.item.guid)
            .chain(self.deleted_guids.iter())
    }

    /// Returns the node for a given `guid`, or `None` if a node with the `guid`
    /// doesn't exist in the tree, or was deleted.
    pub fn node_for_guid(&self, guid: &Guid) -> Option<Node<'_>> {
        assert_eq!(self.entries.len(), self.entry_index_by_guid.len());
        self.entry_index_by_guid
            .get(guid)
            .map(|&index| Node(self, &self.entries[index]))
    }

    /// Returns the structure divergences found when building the tree.
    #[inline]
    pub fn problems(&self) -> &Problems {
        &self.problems
    }
}

impl fmt::Display for Tree {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let root = self.root();
        f.write_str(&root.to_ascii_string())?;
        if !self.deleted_guids.is_empty() {
            f.write_str("\nDeleted: [")?;
            for (i, guid) in self.deleted_guids.iter().enumerate() {
                if i != 0 {
                    f.write_str(", ")?;
                }
                f.write_str(guid.as_ref())?;
            }
        }
        if !self.problems.is_empty() {
            f.write_str("\nProblems:\n")?;
            for (i, summary) in self.problems.summarize().enumerate() {
                if i != 0 {
                    f.write_str("\n")?;
                }
                write!(f, "❗️ {}", summary)?;
            }
        }
        Ok(())
    }
}

#[cfg(test)]
impl PartialEq for Tree {
    fn eq(&self, other: &Tree) -> bool {
        self.root() == other.root() && self.deletions().eq(other.deletions())
    }
}

/// A tree builder builds a bookmark tree structure from a flat list of items
/// and parent-child associations.
///
/// # Tree structure
///
/// In a well-formed tree:
///
/// - Each item exists in exactly one folder. Two different folder's
///   `children` should never reference the same item.
/// - Each folder contains existing children. A folder's `children` should
///   never reference tombstones, or items that don't exist in the tree at all.
/// - Each item has a `parentid` that agrees with its parent's `children`. In
///   other words, if item B's `parentid` is A, then A's `children` should
///   contain B.
///
/// Because of Reasons, things are (a lot) messier in practice.
///
/// # Structure inconsistencies
///
/// Sync stores structure in two places: a `parentid` property on each item,
/// which points to its parent's GUID, and a list of ordered `children` on the
/// item's parent. They're duplicated because, historically, Sync clients didn't
/// stage incoming records. Instead, they applied records one at a time,
/// directly to the live local tree. This meant that, if a client saw a child
/// before its parent, it would first use the `parentid` to decide where to keep
/// the child, then fix up parents and positions using the parent's `children`.
///
/// This is also why moving an item into a different folder uploads records for
/// the item, old folder, and new folder. The item has a new `parentid`, and the
/// folders have new `children`. Similarly, deleting an item uploads a tombstone
/// for the item, and a record for the item's old parent.
///
/// Unfortunately, bugs (bug 1258127) and missing features (bug 1253051) in
/// older clients sometimes caused them to upload invalid or incomplete changes.
/// For example, a client might have:
///
/// - Uploaded a moved child, but not its parents. This means the child now
///   appears in multiple parents. In the most extreme case, an item might be
///   referenced in two different sets of `children`, _and_ have a third,
///   completely unrelated `parentid`.
/// - Deleted a child, and tracked the deletion, but didn't flag the parent for
///   reupload. The parent folder now has a tombstone child.
/// - Tracked and uploaded items that shouldn't exist on the server at all,
///   like the left pane or reading list roots (bug 1309255).
/// - Missed new folders created during a sync, creating holes in the tree.
///
/// Newer clients shouldn't do this, but we might still have inconsistent
/// records on the server that will confuse older clients. Additionally, Firefox
/// for iOS includes a much stricter bookmarks engine that refuses to sync if
/// it detects inconsistencies.
///
/// # Divergences
///
/// To work around this, the builder lets the structure _diverge_. This allows:
///
/// - Items with multiple parents.
/// - Items with missing `parentid`s.
/// - Folders with `children` whose `parentid`s don't match the folder.
/// - Items whose `parentid`s don't mention the item in their `children`.
/// - Items with `parentid`s that point to nonexistent or deleted folders.
/// - Folders with nonexistent `children`.
/// - Non-syncable items, like custom roots.
/// - Any combination of these.
///
/// # Resolving divergences
///
/// Building a tree using `std::convert::TryInto<Tree>::try_into` resolves
/// divergences using these rules:
///
/// 1. User content roots should always be children of the Places root. If
///    they appear in other parents, we move them.
/// 2. Items that appear in multiple `children`, and items with mismatched
///    `parentid`s, use the chronologically newer parent, based on the parent's
///    last modified time. We always prefer parents by `children` over
///    `parentid,` because `children` also gives us the item's position.
/// 3. Items that aren't mentioned in any parent's `children`, but have a
///    `parentid` that references an existing folder in the tree, are reparented
///    to the end of that folder, after the folder's `children`.
/// 4. Items that reference a nonexistent or non-folder `parentid`, or don't
///    have a `parentid` at all, are reparented to the default folder.
/// 5. If the default folder isn't set, or doesn't exist, items from rule 4 are
///    reparented to the root instead.
///
/// The result is a well-formed tree structure that can be merged. The merger
/// detects if the structure diverged, and flags affected items for reupload.
#[derive(Debug)]
pub struct Builder {
    entry_index_by_guid: HashMap<Guid, Index>,
    entries: Vec<BuilderEntry>,
    reparent_orphans_to: Option<Guid>,
}

impl Builder {
    /// Sets the default folder for reparented orphans. If not set, doesn't
    /// exist, or not a folder, orphans will be reparented to the root.
    #[inline]
    pub fn reparent_orphans_to(&mut self, guid: &Guid) -> &mut Builder {
        self.reparent_orphans_to = Some(guid.clone());
        self
    }

    /// Inserts an `item` into the tree. Returns an error if the item already
    /// exists.
    pub fn item(&mut self, item: Item) -> Result<ParentBuilder<'_>> {
        assert_eq!(self.entries.len(), self.entry_index_by_guid.len());
        if self.entry_index_by_guid.contains_key(&item.guid) {
            return Err(ErrorKind::DuplicateItem(item.guid.clone()).into());
        }
        self.entry_index_by_guid
            .insert(item.guid.clone(), self.entries.len());
        let entry_child = BuilderEntryChild::Exists(self.entries.len());
        self.entries.push(BuilderEntry {
            item,
            parent: BuilderEntryParent::None,
            children: Vec::new(),
        });
        Ok(ParentBuilder(self, entry_child))
    }

    /// Sets parents for a `child_guid`. Depending on where the parent comes
    /// from, `child_guid` may not need to exist in the tree.
    pub fn parent_for(&mut self, child_guid: &Guid) -> ParentBuilder<'_> {
        assert_eq!(self.entries.len(), self.entry_index_by_guid.len());
        let entry_child = match self.entry_index_by_guid.get(child_guid) {
            Some(&child_index) => BuilderEntryChild::Exists(child_index),
            None => BuilderEntryChild::Missing(child_guid.clone()),
        };
        ParentBuilder(self, entry_child)
    }

    /// Equivalent to using our implementation of`TryInto<Tree>::try_into`, but
    /// provided both for convenience when updating from previous versions of
    /// `dogear`, and for cases where a type hint would otherwise be needed to
    /// clarify the target type of the conversion.
    pub fn into_tree(self) -> Result<Tree> {
        self.try_into()
    }
}

impl TryFrom<Builder> for Tree {
    type Error = Error;
    /// Builds a tree from all stored items and parent-child associations,
    /// resolving inconsistencies like orphans, multiple parents, and
    /// parent-child disagreements.
    fn try_from(builder: Builder) -> Result<Tree> {
        let mut problems = Problems::default();

        // First, resolve parents for all entries, and build a lookup table for
        // items without a position.
        let mut parents = Vec::with_capacity(builder.entries.len());
        let mut reparented_child_indices_by_parent: HashMap<Index, Vec<Index>> = HashMap::new();
        for (entry_index, entry) in builder.entries.iter().enumerate() {
            let r = ResolveParent::new(&builder, entry, &mut problems);
            let resolved_parent = r.resolve();
            if let ResolvedParent::ByParentGuid(parent_index) = &resolved_parent {
                // Reparented items are special: since they aren't mentioned in
                // that parent's `children`, we don't know their positions. Note
                // them for when we resolve children. We also clone the GUID,
                // since we use it for sorting, but can't access it by
                // reference once we call `builder.entries.into_iter()` below.
                let reparented_child_indices = reparented_child_indices_by_parent
                    .entry(*parent_index)
                    .or_default();
                reparented_child_indices.push(entry_index);
            }
            parents.push(resolved_parent);
        }

        // If any parents form cycles, abort. We haven't seen cyclic trees in
        // the wild, and breaking cycles would add complexity.
        if let Some(index) = detect_cycles(&parents) {
            return Err(ErrorKind::Cycle(builder.entries[index].item.guid.clone()).into());
        }

        // Then, resolve children, and build a slab of entries for the tree.
        let mut entries = Vec::with_capacity(builder.entries.len());
        for (entry_index, entry) in builder.entries.into_iter().enumerate() {
            // Each entry is consistent, until proven otherwise!
            let mut divergence = Divergence::Consistent;

            let parent_index = match &parents[entry_index] {
                ResolvedParent::Root => {
                    // The Places root doesn't have a parent, and should always
                    // be the first entry.
                    assert_eq!(entry_index, 0);
                    None
                }
                ResolvedParent::ByStructure(index) => {
                    // The entry has a valid parent by structure, yay!
                    Some(*index)
                }
                ResolvedParent::ByChildren(index) | ResolvedParent::ByParentGuid(index) => {
                    // The entry has multiple parents, and we resolved one,
                    // so it's diverged.
                    divergence = Divergence::Diverged;
                    Some(*index)
                }
            };

            // Check if the entry's children exist and agree that this entry is
            // their parent.
            let mut child_indices = Vec::with_capacity(entry.children.len());
            for child in entry.children {
                match child {
                    BuilderEntryChild::Exists(child_index) => match &parents[child_index] {
                        ResolvedParent::Root => {
                            // The Places root can't be a child of another entry.
                            unreachable!("A child can't be a top-level root");
                        }
                        ResolvedParent::ByStructure(parent_index) => {
                            // If the child has a valid parent by structure, it
                            // must be the entry. If it's not, there's a bug
                            // in `ResolveParent` or `BuilderEntry`.
                            assert_eq!(*parent_index, entry_index);
                            child_indices.push(child_index);
                        }
                        ResolvedParent::ByChildren(parent_index) => {
                            // If the child has multiple parents, we may have
                            // resolved a different one, so check if we decided
                            // to keep the child in this entry.
                            divergence = Divergence::Diverged;
                            if *parent_index == entry_index {
                                child_indices.push(child_index);
                            }
                        }
                        ResolvedParent::ByParentGuid(parent_index) => {
                            // We should only ever prefer parents
                            // `by_parent_guid` over parents `by_children` for
                            // misparented user content roots. Otherwise,
                            // there's a bug in `ResolveParent`.
                            assert_eq!(*parent_index, 0);
                            divergence = Divergence::Diverged;
                        }
                    },
                    BuilderEntryChild::Missing(child_guid) => {
                        // If the entry's `children` mentions a GUID for which
                        // we don't have an entry, note it as a problem, and
                        // ignore the child.
                        divergence = Divergence::Diverged;
                        problems.note(
                            &entry.item.guid,
                            Problem::MissingChild {
                                child_guid: child_guid.clone(),
                            },
                        );
                    }
                }
            }

            // Reparented items don't appear in our `children`, so we move them
            // to the end, after existing children (rules 3-4).
            if let Some(reparented_child_indices) =
                reparented_child_indices_by_parent.get(&entry_index)
            {
                divergence = Divergence::Diverged;
                child_indices.extend_from_slice(reparented_child_indices);
            }

            entries.push(TreeEntry {
                item: entry.item,
                parent_index,
                child_indices,
                divergence,
            });
        }

        // Now we have a consistent tree.
        Ok(Tree {
            entry_index_by_guid: builder.entry_index_by_guid,
            entries,
            deleted_guids: HashSet::new(),
            problems,
        })
    }
}

/// Describes where an item's parent comes from.
pub struct ParentBuilder<'b>(&'b mut Builder, BuilderEntryChild);

impl<'b> ParentBuilder<'b> {
    /// Records a `parent_guid` from the item's parent's `children`. The
    /// `parent_guid` must refer to an existing folder in the tree, but
    /// the item itself doesn't need to exist. This handles folders with
    /// missing children.
    pub fn by_children(self, parent_guid: &Guid) -> Result<&'b mut Builder> {
        let parent_index = match self.0.entry_index_by_guid.get(parent_guid) {
            Some(&parent_index) if self.0.entries[parent_index].item.is_folder() => parent_index,
            _ => {
                return Err(ErrorKind::InvalidParent(
                    self.child_guid().clone(),
                    parent_guid.clone(),
                )
                .into());
            }
        };
        if let BuilderEntryChild::Exists(child_index) = &self.1 {
            self.0.entries[*child_index].parents_by(&[BuilderParentBy::Children(parent_index)])?;
        }
        self.0.entries[parent_index].children.push(self.1);
        Ok(self.0)
    }

    /// Records a `parent_guid` from the item's `parentid`. The item must
    /// exist in the tree, but the `parent_guid` doesn't need to exist,
    /// or even refer to a folder. The builder will reparent items with
    /// missing and non-folder `parentid`s to the default folder when it
    /// builds the tree.
    pub fn by_parent_guid(self, parent_guid: Guid) -> Result<&'b mut Builder> {
        match &self.1 {
            BuilderEntryChild::Exists(child_index) => {
                self.0.entries[*child_index]
                    .parents_by(&[BuilderParentBy::UnknownItem(parent_guid)])?;
            }
            BuilderEntryChild::Missing(child_guid) => {
                return Err(ErrorKind::MissingItem(child_guid.clone()).into());
            }
        }
        Ok(self.0)
    }

    /// Records a `parent_guid` from a valid tree structure. This is for
    /// callers who already know their structure is consistent, like
    /// `Store::fetch_local_tree()` on Desktop, and
    /// `std::convert::TryInto<Tree>` in the tests.
    ///
    /// Both the item and `parent_guid` must exist, and the `parent_guid` must
    /// refer to a folder.
    ///
    /// `by_structure(parent_guid)` is logically the same as:
    ///
    /// ```no_run
    /// # use dogear::{Item, Kind, Result, ROOT_GUID, Tree};
    /// # fn main() -> Result<()> {
    /// # let mut builder = Tree::with_root(Item::new(ROOT_GUID, Kind::Folder));
    /// # let child_guid = "bookmarkAAAA".into();
    /// # let parent_guid = "folderAAAAAA".into();
    /// builder.parent_for(&child_guid)
    ///     .by_children(&parent_guid)?
    /// .parent_for(&child_guid)
    ///     .by_parent_guid(parent_guid)?;
    /// # Ok(())
    /// # }
    /// ```
    ///
    /// ...But more convenient. It's also more efficient, because it avoids
    /// multiple lookups for the item and parent, as well as an extra heap
    /// allocation to store the parents.
    pub fn by_structure(self, parent_guid: &Guid) -> Result<&'b mut Builder> {
        let parent_index = match self.0.entry_index_by_guid.get(parent_guid) {
            Some(&parent_index) if self.0.entries[parent_index].item.is_folder() => parent_index,
            _ => {
                return Err(ErrorKind::InvalidParent(
                    self.child_guid().clone(),
                    parent_guid.clone(),
                )
                .into());
            }
        };
        match &self.1 {
            BuilderEntryChild::Exists(child_index) => {
                self.0.entries[*child_index].parents_by(&[
                    BuilderParentBy::Children(parent_index),
                    BuilderParentBy::KnownItem(parent_index),
                ])?;
            }
            BuilderEntryChild::Missing(child_guid) => {
                return Err(ErrorKind::MissingItem(child_guid.clone()).into());
            }
        }
        self.0.entries[parent_index].children.push(self.1);
        Ok(self.0)
    }

    fn child_guid(&self) -> &Guid {
        match &self.1 {
            BuilderEntryChild::Exists(index) => &self.0.entries[*index].item.guid,
            BuilderEntryChild::Missing(guid) => guid,
        }
    }
}

/// An entry wraps a tree item with references to its parents and children,
/// which index into the tree's `entries` vector. This indirection exists
/// because Rust is more strict about ownership of parents and children.
///
/// For example, we can't have entries own their children without sacrificing
/// fast random lookup: we'd need to store references to the entries in the
/// lookup map, but a struct can't hold references into itself.
///
/// Similarly, we can't have entries hold `Weak` pointers to `Rc` entries for
/// the parent and children, because we need to update the parent when we insert
/// a new node, but `Rc` won't hand us a mutable reference to the entry as long
/// as it has outstanding `Weak` pointers.
///
/// We *could* use GUIDs instead of indices, and store the entries in a
/// `HashMap<String, Entry>`, but that's inefficient: we'd need to store N
/// copies of the GUID for parent and child lookups, and retrieving children
/// would take one hash map lookup *per child*.
///
/// Note that we always compare references to entries, instead of deriving
/// `PartialEq`, because two entries with the same fields but in different
/// trees should never compare equal.
#[derive(Debug)]
struct TreeEntry {
    item: Item,
    divergence: Divergence,
    parent_index: Option<Index>,
    child_indices: Vec<Index>,
}

/// A builder entry holds an item and its structure. It's the builder's analog
/// of a `TreeEntry`.
#[derive(Debug)]
struct BuilderEntry {
    item: Item,
    parent: BuilderEntryParent,
    children: Vec<BuilderEntryChild>,
}

impl BuilderEntry {
    /// Adds `new_parents` for the entry.
    fn parents_by(&mut self, new_parents: &[BuilderParentBy]) -> Result<()> {
        let old_parent = mem::replace(&mut self.parent, BuilderEntryParent::None);
        let new_parent = match old_parent {
            BuilderEntryParent::Root => {
                mem::replace(&mut self.parent, BuilderEntryParent::Root);
                return Err(ErrorKind::DuplicateItem(self.item.guid.clone()).into());
            }
            BuilderEntryParent::None => match new_parents {
                [BuilderParentBy::Children(from_children), BuilderParentBy::KnownItem(from_item)]
                | [BuilderParentBy::KnownItem(from_item), BuilderParentBy::Children(from_children)]
                    if from_children == from_item =>
                {
                    // If the parent's `children` and item's `parentid` match,
                    // we have a complete structure, so we can avoid an extra
                    // allocation for the partial structure.
                    BuilderEntryParent::Complete(*from_children)
                }
                new_parents => BuilderEntryParent::Partial(new_parents.to_vec()),
            },
            BuilderEntryParent::Complete(index) => {
                let mut parents = vec![
                    BuilderParentBy::Children(index),
                    BuilderParentBy::KnownItem(index),
                ];
                parents.extend_from_slice(new_parents);
                BuilderEntryParent::Partial(parents)
            }
            BuilderEntryParent::Partial(mut parents) => {
                parents.extend_from_slice(new_parents);
                BuilderEntryParent::Partial(parents)
            }
        };
        mem::replace(&mut self.parent, new_parent);
        Ok(())
    }
}

/// Holds an existing child index, or missing child GUID, for a builder entry.
#[derive(Debug)]
enum BuilderEntryChild {
    Exists(Index),
    Missing(Guid),
}

/// Holds one or more parents for a builder entry.
#[derive(Clone, Debug)]
enum BuilderEntryParent {
    /// The entry is an orphan.
    None,

    /// The entry is a top-level root, from which all other entries descend.
    /// A tree can only have one root.
    Root,

    /// The entry has two matching parents from its structure. This is the fast
    /// path for local trees, which are always valid.
    Complete(Index),

    /// The entry has an incomplete or divergent structure. This is the path for
    /// all remote trees, valid and invalid, since we add structure from
    /// `parentid`s and `children` separately. This is also the path for
    /// mismatched and multiple parents.
    Partial(Vec<BuilderParentBy>),
}

/// Describes where a builder entry's parent comes from.
#[derive(Clone, Debug)]
enum BuilderParentBy {
    /// The entry's parent references the entry in its `children`.
    Children(Index),

    /// The entry's parent comes from its `parentid`, and will be resolved
    /// when we build the tree.
    UnknownItem(Guid),

    /// The entry's parent comes from its `parentid` and has been
    /// resolved.
    KnownItem(Index),
}

/// Resolves the parent for a builder entry.
struct ResolveParent<'a> {
    builder: &'a Builder,
    entry: &'a BuilderEntry,
    problems: &'a mut Problems,
}

impl<'a> ResolveParent<'a> {
    fn new(
        builder: &'a Builder,
        entry: &'a BuilderEntry,
        problems: &'a mut Problems,
    ) -> ResolveParent<'a> {
        ResolveParent {
            builder,
            entry,
            problems,
        }
    }

    fn resolve(self) -> ResolvedParent {
        if self.entry.item.guid.is_user_content_root() {
            self.user_content_root()
        } else {
            self.item()
        }
    }

    /// Returns the parent for this builder entry. This unifies parents
    /// `by_structure`, which are known to be consistent, and parents
    /// `by_children` and `by_parent_guid`, which are consistent if they match.
    fn parent(&self) -> Cow<'a, BuilderEntryParent> {
        let parents = match &self.entry.parent {
            // Roots and orphans pass through as-is.
            BuilderEntryParent::Root => return Cow::Owned(BuilderEntryParent::Root),
            BuilderEntryParent::None => return Cow::Owned(BuilderEntryParent::None),
            BuilderEntryParent::Complete(index) => {
                // The entry is known to have a valid parent by structure. This
                // is the fast path, used for local trees in Desktop.
                return Cow::Owned(BuilderEntryParent::Complete(*index));
            }
            BuilderEntryParent::Partial(parents) => parents,
        };
        // The entry has zero, one, or many parents, recorded separately. Check
        // if it has exactly two: one `by_parent_guid`, and one `by_children`.
        let (index_by_guid, index_by_children) = match parents.as_slice() {
            [BuilderParentBy::UnknownItem(guid), BuilderParentBy::Children(index_by_children)]
            | [BuilderParentBy::Children(index_by_children), BuilderParentBy::UnknownItem(guid)] => {
                match self.builder.entry_index_by_guid.get(guid) {
                    Some(&index_by_guid) => (index_by_guid, *index_by_children),
                    None => return Cow::Borrowed(&self.entry.parent),
                }
            }
            [BuilderParentBy::KnownItem(index_by_guid), BuilderParentBy::Children(index_by_children)]
            | [BuilderParentBy::Children(index_by_children), BuilderParentBy::KnownItem(index_by_guid)] => {
                (*index_by_guid, *index_by_children)
            }
            // In all other cases (missing `parentid`, missing from `children`,
            // multiple parents), return all possible parents. We'll pick one
            // when we resolve the parent.
            _ => return Cow::Borrowed(&self.entry.parent),
        };
        // If the entry has matching parents `by_children` and `by_parent_guid`,
        // it has a valid parent by structure. This is the "fast slow path",
        // used for remote trees in Desktop, because their structure is built in
        // two passes. In all other cases, we have a parent-child disagreement,
        // so return all possible parents.
        if index_by_guid == index_by_children {
            Cow::Owned(BuilderEntryParent::Complete(index_by_children))
        } else {
            Cow::Borrowed(&self.entry.parent)
        }
    }

    /// Resolves the parent for a user content root: menu, mobile, toolbar, and
    /// unfiled. These are simpler to resolve than non-roots because they must
    /// be children of the Places root (rule 1), which is always the first
    /// entry.
    fn user_content_root(self) -> ResolvedParent {
        match self.parent().as_ref() {
            BuilderEntryParent::None => {
                // Orphaned content root. This should only happen if the content
                // root doesn't have a parent `by_parent_guid`.
                self.problems.note(&self.entry.item.guid, Problem::Orphan);
                ResolvedParent::ByParentGuid(0)
            }
            BuilderEntryParent::Root => {
                unreachable!("A user content root can't be a top-level root")
            }
            BuilderEntryParent::Complete(index) => {
                if *index == 0 {
                    ResolvedParent::ByStructure(*index)
                } else {
                    // Move misparented content roots to the Places root.
                    let parent_guid = self.builder.entries[*index].item.guid.clone();
                    self.problems.note(
                        &self.entry.item.guid,
                        Problem::MisparentedRoot(vec![
                            DivergedParent::ByChildren(parent_guid.clone()),
                            DivergedParentGuid::Folder(parent_guid).into(),
                        ]),
                    );
                    ResolvedParent::ByParentGuid(0)
                }
            }
            BuilderEntryParent::Partial(parents_by) => {
                // Ditto for content roots with multiple parents or parent-child
                // disagreements.
                self.problems.note(
                    &self.entry.item.guid,
                    Problem::MisparentedRoot(
                        parents_by
                            .iter()
                            .map(|parent_by| {
                                PossibleParent::new(self.builder, parent_by).summarize()
                            })
                            .collect(),
                    ),
                );
                ResolvedParent::ByParentGuid(0)
            }
        }
    }

    /// Resolves the parent for a top-level Places root or other item, using
    /// rules 2-5.
    fn item(self) -> ResolvedParent {
        match self.parent().as_ref() {
            BuilderEntryParent::Root => ResolvedParent::Root,
            BuilderEntryParent::None => {
                // The item doesn't have a `parentid`, and isn't mentioned in
                // any `children`. Reparent to the default folder (rule 4) or
                // Places root (rule 5).
                let parent_index = self.reparent_orphans_to_default_index();
                self.problems.note(&self.entry.item.guid, Problem::Orphan);
                ResolvedParent::ByParentGuid(parent_index)
            }
            BuilderEntryParent::Complete(index) => {
                // The item's `parentid` and parent's `children` match, so keep
                // it in its current parent.
                ResolvedParent::ByStructure(*index)
            }
            BuilderEntryParent::Partial(parents) => {
                // For items with one or more than two parents, pick the
                // youngest (minimum age).
                let possible_parents = parents
                    .iter()
                    .map(|parent_by| PossibleParent::new(self.builder, parent_by))
                    .collect::<Vec<_>>();
                self.problems.note(
                    &self.entry.item.guid,
                    Problem::DivergedParents(
                        possible_parents
                            .iter()
                            .map(PossibleParent::summarize)
                            .collect(),
                    ),
                );
                possible_parents
                    .into_iter()
                    .min()
                    .and_then(|p| match p.parent_by {
                        BuilderParentBy::Children(index) => {
                            Some(ResolvedParent::ByChildren(*index))
                        }
                        BuilderParentBy::KnownItem(index) => {
                            Some(ResolvedParent::ByParentGuid(*index))
                        }
                        BuilderParentBy::UnknownItem(guid) => self
                            .builder
                            .entry_index_by_guid
                            .get(guid)
                            .filter(|&&index| self.builder.entries[index].item.is_folder())
                            .map(|&index| ResolvedParent::ByParentGuid(index)),
                    })
                    .unwrap_or_else(|| {
                        // Fall back to the default folder (rule 4) or root
                        // (rule 5) if we didn't find a parent.
                        let parent_index = self.reparent_orphans_to_default_index();
                        ResolvedParent::ByParentGuid(parent_index)
                    })
            }
        }
    }

    /// Returns the index of the default parent entry for reparented orphans.
    /// This is either the default folder (rule 4), or the root, if the
    /// default folder isn't set, doesn't exist, or isn't a folder (rule 5).
    fn reparent_orphans_to_default_index(&self) -> Index {
        self.builder
            .reparent_orphans_to
            .as_ref()
            .and_then(|guid| self.builder.entry_index_by_guid.get(guid))
            .cloned()
            .filter(|&parent_index| {
                let parent_entry = &self.builder.entries[parent_index];
                parent_entry.item.is_folder()
            })
            .unwrap_or(0)
    }
}

// A possible parent for an item with conflicting parents. We use this wrapper's
// `Ord` implementation to decide which parent is youngest.
#[derive(Clone, Copy, Debug)]
struct PossibleParent<'a> {
    builder: &'a Builder,
    parent_by: &'a BuilderParentBy,
}

impl<'a> PossibleParent<'a> {
    fn new(builder: &'a Builder, parent_by: &'a BuilderParentBy) -> PossibleParent<'a> {
        PossibleParent { builder, parent_by }
    }

    /// Returns the problem with this conflicting parent.
    fn summarize(&self) -> DivergedParent {
        let entry = match self.parent_by {
            BuilderParentBy::Children(index) => {
                return DivergedParent::ByChildren(self.builder.entries[*index].item.guid.clone());
            }
            BuilderParentBy::KnownItem(index) => &self.builder.entries[*index],
            BuilderParentBy::UnknownItem(guid) => {
                match self.builder.entry_index_by_guid.get(guid) {
                    Some(index) => &self.builder.entries[*index],
                    None => return DivergedParentGuid::Missing(guid.clone()).into(),
                }
            }
        };
        if entry.item.is_folder() {
            DivergedParentGuid::Folder(entry.item.guid.clone()).into()
        } else {
            DivergedParentGuid::NonFolder(entry.item.guid.clone()).into()
        }
    }
}

impl<'a> Ord for PossibleParent<'a> {
    /// Compares two possible parents to determine which is younger
    /// (`Ordering::Less`). Prefers parents from `children` over `parentid`
    /// (rule 2), and `parentid`s that reference folders over non-folders
    /// (rule 4).
    fn cmp(&self, other: &PossibleParent<'_>) -> Ordering {
        let (index, other_index) = match (&self.parent_by, &other.parent_by) {
            (BuilderParentBy::Children(index), BuilderParentBy::Children(other_index)) => {
                // Both `self` and `other` mention the item in their `children`.
                (*index, *other_index)
            }
            (BuilderParentBy::Children(_), BuilderParentBy::KnownItem(_)) => {
                // `self` mentions the item in its `children`, and the item's
                // `parentid` is `other`, so prefer `self`.
                return Ordering::Less;
            }
            (BuilderParentBy::Children(_), BuilderParentBy::UnknownItem(_)) => {
                // As above, except we don't know if `other` exists. We don't
                // need to look it up, though, because we can unconditionally
                // prefer `self`.
                return Ordering::Less;
            }
            (BuilderParentBy::KnownItem(_), BuilderParentBy::Children(_)) => {
                // The item's `parentid` is `self`, and `other` mentions the
                // item in its `children`, so prefer `other`.
                return Ordering::Greater;
            }
            (BuilderParentBy::UnknownItem(_), BuilderParentBy::Children(_)) => {
                // As above. We don't know if `self` exists, but we
                // unconditionally prefer `other`.
                return Ordering::Greater;
            }
            // Cases where `self` and `other` are `parentid`s, existing or not,
            // are academic, since it doesn't make sense for an item to have
            // multiple `parentid`s.
            _ => return Ordering::Equal,
        };
        // If both `self` and `other` are folders, compare timestamps. If one is
        // a folder, but the other isn't, we prefer the folder. If neither is a
        // folder, it doesn't matter.
        let entry = &self.builder.entries[index];
        let other_entry = &self.builder.entries[other_index];
        match (entry.item.is_folder(), other_entry.item.is_folder()) {
            (true, true) => entry.item.age.cmp(&other_entry.item.age),
            (false, true) => Ordering::Greater,
            (true, false) => Ordering::Less,
            (false, false) => Ordering::Equal,
        }
    }
}

impl<'a> PartialOrd for PossibleParent<'a> {
    fn partial_cmp(&self, other: &PossibleParent<'_>) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl<'a> PartialEq for PossibleParent<'a> {
    fn eq(&self, other: &PossibleParent<'_>) -> bool {
        self.cmp(other) == Ordering::Equal
    }
}

impl<'a> Eq for PossibleParent<'a> {}

/// Describes a resolved parent for an item.
#[derive(Debug)]
enum ResolvedParent {
    /// The item is a top-level root, and has no parent.
    Root,

    /// The item has a valid, consistent structure.
    ByStructure(Index),

    /// The item has multiple parents; this is the one we picked.
    ByChildren(Index),

    /// The item has a parent-child disagreement: the folder referenced by the
    /// item's `parentid` doesn't mention the item in its `children`, the
    /// `parentid` doesn't exist at all, or the item is a misparented content
    /// root.
    ByParentGuid(Index),
}

impl ResolvedParent {
    fn index(&self) -> Option<Index> {
        match self {
            ResolvedParent::Root => None,
            ResolvedParent::ByStructure(index)
            | ResolvedParent::ByChildren(index)
            | ResolvedParent::ByParentGuid(index) => Some(*index),
        }
    }
}

/// Detects cycles in resolved parents, using Floyd's tortoise and the hare
/// algorithm. Returns the index of the entry where the cycle was detected,
/// or `None` if there aren't any cycles.
fn detect_cycles(parents: &[ResolvedParent]) -> Option<Index> {
    let mut seen = SmallBitVec::from_elem(parents.len(), false);
    for (entry_index, parent) in parents.iter().enumerate() {
        if seen[entry_index] {
            continue;
        }
        let mut parent_index = parent.index();
        let mut grandparent_index = parent.index().and_then(|index| parents[index].index());
        while let (Some(i), Some(j)) = (parent_index, grandparent_index) {
            if i == j {
                return Some(i);
            }
            if seen[i] || seen[j] {
                break;
            }
            parent_index = parent_index.and_then(|index| parents[index].index());
            grandparent_index = grandparent_index
                .and_then(|index| parents[index].index())
                .and_then(|index| parents[index].index());
        }
        seen.set(entry_index, true);
    }
    None
}

/// Indicates if a tree entry's structure diverged.
#[derive(Debug)]
enum Divergence {
    /// The structure is already correct, and doesn't need to be reuploaded.
    Consistent,

    /// The node has structure problems, and should be flagged for reupload
    /// when merging.
    Diverged,
}

/// Describes a structure divergence for an item in a bookmark tree. These are
/// used for logging and validation telemetry.
#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub enum Problem {
    /// The item doesn't have a `parentid`, and isn't mentioned in any folders.
    Orphan,

    /// The item is a user content root (menu, mobile, toolbar, or unfiled),
    /// but `parent_guid` isn't the Places root.
    MisparentedRoot(Vec<DivergedParent>),

    /// The item has diverging parents. If the vector contains more than one
    /// `DivergedParent::ByChildren`, the item has multiple parents. If the
    /// vector contains a `DivergedParent::ByParentGuid`, with or without a
    /// `DivergedParent::ByChildren`, the item has a parent-child disagreement.
    DivergedParents(Vec<DivergedParent>),

    /// The item is mentioned in a folder's `children`, but doesn't exist or is
    /// deleted.
    MissingChild { child_guid: Guid },
}

/// Describes where an invalid parent comes from.
#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub enum DivergedParent {
    /// The item appears in this folder's `children`.
    ByChildren(Guid),
    /// The `parentid` references this folder.
    ByParentGuid(DivergedParentGuid),
}

impl From<DivergedParentGuid> for DivergedParent {
    fn from(d: DivergedParentGuid) -> DivergedParent {
        DivergedParent::ByParentGuid(d)
    }
}

impl fmt::Display for DivergedParent {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            DivergedParent::ByChildren(parent_guid) => {
                write!(f, "is in children of {}", parent_guid)
            }
            DivergedParent::ByParentGuid(p) => match p {
                DivergedParentGuid::Folder(parent_guid) => write!(f, "has parent {}", parent_guid),
                DivergedParentGuid::NonFolder(parent_guid) => {
                    write!(f, "has non-folder parent {}", parent_guid)
                }
                DivergedParentGuid::Missing(parent_guid) => {
                    write!(f, "has nonexistent parent {}", parent_guid)
                }
            },
        }
    }
}

/// Describes an invalid `parentid`.
#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub enum DivergedParentGuid {
    /// Exists and is a folder.
    Folder(Guid),
    /// Exists, but isn't a folder.
    NonFolder(Guid),
    /// Doesn't exist at all.
    Missing(Guid),
}

/// Records problems for all items in a tree.
#[derive(Debug, Default)]
pub struct Problems(HashMap<Guid, Vec<Problem>>);

impl Problems {
    /// Notes a problem for an item.
    pub fn note(&mut self, guid: &Guid, problem: Problem) -> &mut Problems {
        self.0.entry(guid.clone()).or_default().push(problem);
        self
    }

    /// Returns `true` if there are no problems.
    pub fn is_empty(&self) -> bool {
        self.0.is_empty()
    }

    /// Returns an iterator for all problems.
    pub fn summarize(&self) -> impl Iterator<Item = ProblemSummary<'_>> {
        self.0.iter().flat_map(|(guid, problems)| {
            problems
                .iter()
                .map(move |problem| ProblemSummary(guid, problem))
        })
    }
}

/// A printable summary of a problem for an item.
#[derive(Clone, Copy, Debug)]
pub struct ProblemSummary<'a>(&'a Guid, &'a Problem);

impl<'a> ProblemSummary<'a> {
    #[inline]
    pub fn guid(&self) -> &Guid {
        &self.0
    }

    #[inline]
    pub fn problem(&self) -> &Problem {
        &self.1
    }
}

impl<'a> fmt::Display for ProblemSummary<'a> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let parents = match self.problem() {
            Problem::Orphan => return write!(f, "{} is an orphan", self.guid()),
            Problem::MisparentedRoot(parents) => {
                write!(f, "{} is a user content root", self.guid())?;
                if parents.is_empty() {
                    return Ok(());
                }
                f.write_str(", but ")?;
                parents
            }
            Problem::DivergedParents(parents) => {
                if parents.is_empty() {
                    return write!(f, "{} has diverged parents", self.guid());
                }
                write!(f, "{} ", self.guid())?;
                parents
            }
            Problem::MissingChild { child_guid } => {
                return write!(f, "{} has nonexistent child {}", self.guid(), child_guid);
            }
        };
        match parents.as_slice() {
            [a] => write!(f, "{}", a)?,
            [a, b] => write!(f, "{} and {}", a, b)?,
            _ => {
                for (i, parent) in parents.iter().enumerate() {
                    if i != 0 {
                        f.write_str(", ")?;
                    }
                    if i == parents.len() - 1 {
                        f.write_str("and ")?;
                    }
                    write!(f, "{}", parent)?;
                }
            }
        }
        Ok(())
    }
}

/// A node in a bookmark tree that knows its parent and children, and
/// dereferences to its item.
#[derive(Clone, Copy, Debug)]
pub struct Node<'t>(&'t Tree, &'t TreeEntry);

impl<'t> Node<'t> {
    /// Returns an iterator for all children of this node.
    pub fn children<'n>(&'n self) -> impl Iterator<Item = Node<'t>> + 'n {
        self.1
            .child_indices
            .iter()
            .map(move |&child_index| Node(self.0, &self.0.entries[child_index]))
    }

    /// Returns the resolved parent of this node, or `None` if this is the
    /// root node.
    pub fn parent(&self) -> Option<Node<'_>> {
        self.1
            .parent_index
            .as_ref()
            .map(|&parent_index| Node(self.0, &self.0.entries[parent_index]))
    }

    /// Returns the level of this node in the tree.
    pub fn level(&self) -> i64 {
        if self.is_root() {
            return 0;
        }
        self.parent().map_or(-1, |parent| parent.level() + 1)
    }

    /// Indicates if this node is for a syncable item.
    ///
    /// Syncable items descend from the four user content roots. Any
    /// other roots and their descendants, like the left pane root,
    /// left pane queries, and custom roots, are non-syncable.
    ///
    /// Newer Desktops should never reupload non-syncable items
    /// (bug 1274496), and should have removed them in Places
    /// migrations (bug 1310295). However, these items may be
    /// reparented locally to unfiled, in which case they're seen as
    /// syncable. If the remote tree has the missing parents
    /// and roots, we'll determine that the items are non-syncable
    /// when merging, remove them locally, and mark them for deletion
    /// remotely.
    pub fn is_syncable(&self) -> bool {
        if self.is_root() {
            return false;
        }
        if self.is_user_content_root() {
            return true;
        }
        match self.kind {
            // Exclude livemarks (bug 1477671).
            Kind::Livemark => false,
            // Exclude orphaned Places queries (bug 1433182).
            Kind::Query if self.diverged() => false,
            _ => self.parent().map_or(false, |parent| parent.is_syncable()),
        }
    }

    /// Indicates if this node's structure diverged because it
    /// existed in multiple parents, or was reparented.
    #[inline]
    pub fn diverged(&self) -> bool {
        match &self.1.divergence {
            Divergence::Diverged => true,
            Divergence::Consistent => false,
        }
    }

    /// Returns an ASCII art (with emoji!) representation of this node and all
    /// its descendants. Handy for logging.
    pub fn to_ascii_string(&self) -> String {
        self.to_ascii_fragment("")
    }

    fn to_ascii_fragment(&self, prefix: &str) -> String {
        match self.1.item.kind {
            Kind::Folder => {
                let children_prefix = format!("{}| ", prefix);
                let children = self
                    .children()
                    .map(|n| n.to_ascii_fragment(&children_prefix))
                    .collect::<Vec<String>>();
                let kind = if self.diverged() {
                    "❗️📂"
                } else {
                    "📂"
                };
                if children.is_empty() {
                    format!("{}{} {}", prefix, kind, self.1.item)
                } else {
                    format!(
                        "{}{} {}\n{}",
                        prefix,
                        kind,
                        self.1.item,
                        children.join("\n")
                    )
                }
            }
            _ => {
                let kind = if self.diverged() {
                    "❗️🔖"
                } else {
                    "🔖"
                };
                format!("{}{} {}", prefix, kind, self.1.item)
            }
        }
    }

    /// Indicates if this node is the root node.
    #[inline]
    pub fn is_root(&self) -> bool {
        ptr::eq(self.1, &self.0.entries[0])
    }

    /// Indicates if this node is a user content root.
    #[inline]
    pub fn is_user_content_root(&self) -> bool {
        self.1.item.guid.is_user_content_root()
    }
}

impl<'t> Deref for Node<'t> {
    type Target = Item;

    fn deref(&self) -> &Item {
        &self.1.item
    }
}

impl<'t> fmt::Display for Node<'t> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.1.item.fmt(f)
    }
}

#[cfg(test)]
impl<'t> PartialEq for Node<'t> {
    fn eq(&self, other: &Node<'_>) -> bool {
        match (self.parent(), other.parent()) {
            (Some(parent), Some(other_parent)) => {
                if parent.1.item != other_parent.1.item {
                    return false;
                }
            }
            (Some(_), None) | (None, Some(_)) => return false,
            (None, None) => {}
        }
        if self.1.item != other.1.item {
            return false;
        }
        self.children().eq(other.children())
    }
}

/// An item in a local or remote bookmark tree.
#[derive(Debug, Eq, PartialEq)]
pub struct Item {
    pub guid: Guid,
    pub kind: Kind,
    pub age: i64,
    pub needs_merge: bool,
    pub validity: Validity,
}

impl Item {
    /// Creates an item with the given kind.
    #[inline]
    pub fn new(guid: Guid, kind: Kind) -> Item {
        Item {
            guid,
            kind,
            age: 0,
            needs_merge: false,
            validity: Validity::Valid,
        }
    }

    /// Indicates if the item is a folder. Only folders are allowed to have
    /// children.
    #[inline]
    pub fn is_folder(&self) -> bool {
        self.kind == Kind::Folder
    }

    /// Indicates if the item can be merged with another item. Only items with
    /// compatible kinds can be merged.
    #[inline]
    pub fn has_compatible_kind(&self, remote_node: &Item) -> bool {
        match (&self.kind, &remote_node.kind) {
            // Bookmarks and queries are interchangeable, as simply changing the URL
            // can cause it to flip kinds.
            (Kind::Bookmark, Kind::Query) => true,
            (Kind::Query, Kind::Bookmark) => true,
            (local_kind, remote_kind) => local_kind == remote_kind,
        }
    }
}

impl fmt::Display for Item {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let kind = match self.validity {
            Validity::Valid => format!("{}", self.kind),
            Validity::Reupload | Validity::Replace => format!("{} ({})", self.kind, self.validity),
        };
        let info = if self.needs_merge {
            format!("{}; Age = {}ms; Unmerged", kind, self.age)
        } else {
            format!("{}; Age = {}ms", kind, self.age)
        };
        write!(f, "{} ({})", self.guid, info)
    }
}

/// Synced item kinds. Each corresponds to a Sync record type.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum Kind {
    Bookmark,
    Query,
    Folder,
    Livemark,
    Separator,
}

impl fmt::Display for Kind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Debug::fmt(self, f)
    }
}

/// Synced item validity.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum Validity {
    /// The item is valid, and can be applied as-is.
    Valid,

    /// The item can be applied, but should also be flagged for reupload. Places
    /// uses this to rewrite legacy tag queries.
    Reupload,

    /// The item isn't valid at all, and should either be replaced with a valid
    /// local copy, or deleted if a valid local copy doesn't exist. Places uses
    /// this to flag bookmarks and queries without valid URLs.
    Replace,
}

impl fmt::Display for Validity {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Debug::fmt(self, f)
    }
}

/// The root of a merged tree, from which all merged nodes descend.
#[derive(Debug)]
pub struct MergedRoot<'t> {
    node: MergedNode<'t>,
    size_hint: usize,
}

impl<'t> MergedRoot<'t> {
    /// Returns a merged root for the given node. `size_hint` indicates the
    /// size of the tree, excluding the root, and is used to avoid extra
    /// allocations for the descendants.
    pub(crate) fn with_size(node: MergedNode<'t>, size_hint: usize) -> MergedRoot<'_> {
        MergedRoot { node, size_hint }
    }

    /// Returns the root node.
    pub fn node(&self) -> &MergedNode<'_> {
        &self.node
    }

    /// Returns a flattened `Vec` of the root node's descendants, excluding the
    /// root node itself.
    pub fn descendants(&self) -> Vec<MergedDescendant<'_>> {
        fn accumulate<'t>(
            results: &mut Vec<MergedDescendant<'t>>,
            merged_node: &'t MergedNode<'t>,
            level: usize,
        ) {
            results.reserve(merged_node.merged_children.len());
            for (position, merged_child_node) in merged_node.merged_children.iter().enumerate() {
                results.push(MergedDescendant {
                    merged_parent_node: &merged_node,
                    level: level + 1,
                    position,
                    merged_node: merged_child_node,
                });
                accumulate(results, merged_child_node, level + 1);
            }
        }
        let mut results = Vec::with_capacity(self.size_hint);
        accumulate(&mut results, &self.node, 0);
        results
    }

    /// Returns an ASCII art representation of the root and its descendants,
    /// similar to `Node::to_ascii_string`.
    pub fn to_ascii_string(&self) -> String {
        self.node.to_ascii_fragment("")
    }

    /// Lets us avoid needing to specify the target type in tests.
    #[cfg(test)]
    pub(crate) fn into_tree(self) -> Result<Tree> {
        self.try_into()
    }
}

#[cfg(test)]
impl<'t> TryFrom<MergedRoot<'t>> for Tree {
    type Error = Error;
    fn try_from(merged_root: MergedRoot<'t>) -> Result<Tree> {
        fn to_item(merged_node: &MergedNode<'_>) -> Item {
            let node = merged_node.merge_state.node();
            let mut item = Item::new(merged_node.guid.clone(), node.kind);
            item.age = node.age;
            item.needs_merge = merged_node.merge_state.upload_reason() != UploadReason::None;
            item
        }

        let mut b = Tree::with_root(to_item(&merged_root.node));
        for MergedDescendant {
            merged_parent_node,
            merged_node,
            ..
        } in merged_root.descendants()
        {
            b.item(to_item(merged_node))?
                .by_structure(&merged_parent_node.guid)?;
        }
        b.try_into()
    }
}

/// A merged bookmark node that indicates which side to prefer, and holds merged
/// child nodes.
#[derive(Debug)]
pub struct MergedNode<'t> {
    pub guid: Guid,
    pub merge_state: MergeState<'t>,
    pub merged_children: Vec<MergedNode<'t>>,
}

impl<'t> MergedNode<'t> {
    /// Creates a merged node from the given merge state.
    pub(crate) fn new(guid: Guid, merge_state: MergeState<'t>) -> MergedNode<'t> {
        MergedNode {
            guid,
            merge_state,
            merged_children: Vec::new(),
        }
    }

    /// Indicates if the merged node exists remotely and has a new GUID. The
    /// merger uses this to flag parents and children of remote nodes with
    /// invalid GUIDs for reupload.
    pub(crate) fn remote_guid_changed(&self) -> bool {
        self.merge_state
            .remote_node()
            .map_or(false, |remote_node| remote_node.guid != self.guid)
    }

    fn to_ascii_fragment(&self, prefix: &str) -> String {
        match self.merge_state.node().kind {
            Kind::Folder => {
                let children_prefix = format!("{}| ", prefix);
                let children = self
                    .merged_children
                    .iter()
                    .map(|n| n.to_ascii_fragment(&children_prefix))
                    .collect::<Vec<String>>();
                if children.is_empty() {
                    format!("{}📂 {}", prefix, &self)
                } else {
                    format!("{}📂 {}\n{}", prefix, &self, children.join("\n"))
                }
            }
            _ => format!("{}🔖 {}", prefix, &self),
        }
    }
}

impl<'t> fmt::Display for MergedNode<'t> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{} {}", self.guid, self.merge_state)
    }
}

/// A descendant holds a merged node, merged parent node, position in the
/// merged parent, and level in the merged tree.
#[derive(Clone, Copy, Debug)]
pub struct MergedDescendant<'t> {
    pub merged_parent_node: &'t MergedNode<'t>,
    pub level: usize,
    pub position: usize,
    pub merged_node: &'t MergedNode<'t>,
}

/// The merge state indicates which node we should prefer, local or remote, when
/// resolving conflicts.
#[derive(Clone, Copy, Debug)]
pub enum MergeState<'t> {
    /// A local-only merge state means the item only exists locally, and should
    /// be uploaded.
    LocalOnly(Node<'t>),

    /// A remote-only merge state means the item only exists remotely, and
    /// should be applied.
    RemoteOnly(Node<'t>),

    /// A local merge state means the item exists on both sides, and has newer
    /// local changes that should be uploaded.
    Local {
        local_node: Node<'t>,
        remote_node: Node<'t>,
    },

    /// A remote merge state means the item exists on both sides, and has newer
    /// remote changes that should be applied.
    Remote {
        local_node: Node<'t>,
        remote_node: Node<'t>,
    },

    /// A remote-only merge state with new structure means the item only exists
    /// remotely, and has a new merged structure that should be reuploaded. We
    /// use new structure states to resolve conflicts caused by moving local
    /// items out of a remotely deleted folder, moving remote items out of a
    /// locally deleted folder, or merging divergent items.
    RemoteOnlyWithNewStructure(Node<'t>),

    /// A remote merge state with new structure means the item exists on both
    /// sides, has newer remote changes, and new structure that should be
    /// reuploaded.
    RemoteWithNewStructure {
        local_node: Node<'t>,
        remote_node: Node<'t>,
    },

    /// An unchanged merge state means the item didn't change on either side,
    /// and doesn't need to be uploaded or applied.
    Unchanged {
        local_node: Node<'t>,
        remote_node: Node<'t>,
    },
}

/// The reason for uploading or reuploading a merged descendant.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum UploadReason {
    /// The item doesn't need to be uploaded.
    None,
    /// The item was added locally since the last sync.
    LocallyNew,
    /// The item has newer local changes.
    Merged,
    /// The item didn't change locally, but has new structure. Reuploading
    /// the same item with new structure on every sync may indicate a sync loop,
    /// where two or more clients clash trying to fix up the remote tree.
    NewStructure,
}

impl<'t> MergeState<'t> {
    /// Returns the local node for the item, or `None` if the item only exists
    /// remotely. The inverse of `remote_node()`.
    pub fn local_node(&self) -> Option<&Node<'t>> {
        match self {
            MergeState::LocalOnly(local_node)
            | MergeState::Local { local_node, .. }
            | MergeState::Remote { local_node, .. }
            | MergeState::RemoteWithNewStructure { local_node, .. }
            | MergeState::Unchanged { local_node, .. } => Some(local_node),

            MergeState::RemoteOnly(_) | MergeState::RemoteOnlyWithNewStructure(_) => None,
        }
    }

    /// Returns the remote node for the item, or `None` if the node only exists
    /// locally. The inverse of `local_node()`.
    pub fn remote_node(&self) -> Option<&Node<'t>> {
        match self {
            MergeState::RemoteOnly(remote_node)
            | MergeState::Local { remote_node, .. }
            | MergeState::Remote { remote_node, .. }
            | MergeState::RemoteOnlyWithNewStructure(remote_node)
            | MergeState::RemoteWithNewStructure { remote_node, .. }
            | MergeState::Unchanged { remote_node, .. } => Some(remote_node),

            MergeState::LocalOnly(_) => None,
        }
    }

    /// Returns `true` if the remote item should be inserted into or updated
    /// in the local tree. This is not necessarily the inverse of
    /// `should_upload()`, as remote items with new structure should be both
    /// applied and reuploaded, and unchanged items should be neither.
    pub fn should_apply(&self) -> bool {
        match self {
            MergeState::RemoteOnly(_)
            | MergeState::Remote { .. }
            | MergeState::RemoteOnlyWithNewStructure(_)
            | MergeState::RemoteWithNewStructure { .. } => true,

            MergeState::LocalOnly(_) | MergeState::Local { .. } | MergeState::Unchanged { .. } => {
                false
            }
        }
    }

    /// Returns the reason for (re)uploading this node.
    pub fn upload_reason(&self) -> UploadReason {
        match self {
            MergeState::LocalOnly(_) => UploadReason::LocallyNew,
            MergeState::RemoteOnly(_) => UploadReason::None,
            MergeState::Local { .. } => UploadReason::Merged,
            MergeState::Remote { .. } => UploadReason::None,
            MergeState::RemoteOnlyWithNewStructure(_) => {
                // We're reuploading an item that only exists remotely, so it
                // must have new structure. Otherwise, its merge state would
                // be remote only, without new structure.
                UploadReason::NewStructure
            }
            MergeState::RemoteWithNewStructure { local_node, .. } => {
                if local_node.needs_merge {
                    // The item exists on both sides, and changed locally, so
                    // we're uploading to resolve a merge conflict.
                    UploadReason::Merged
                } else {
                    // The item exists on both sides, and didn't change locally,
                    // so we must be uploading new structure to fix GUIDs or
                    // divergences.
                    UploadReason::NewStructure
                }
            }
            MergeState::Unchanged { .. } => UploadReason::None,
        }
    }

    /// Returns a new merge state, indicating that the item has a new merged
    /// structure that should be reuploaded to the server.
    pub(crate) fn with_new_structure(&self) -> MergeState<'t> {
        match *self {
            MergeState::LocalOnly(local_node) => MergeState::LocalOnly(local_node),
            MergeState::RemoteOnly(remote_node)
            | MergeState::RemoteOnlyWithNewStructure(remote_node) => {
                MergeState::RemoteOnlyWithNewStructure(remote_node)
            }
            MergeState::Local {
                local_node,
                remote_node,
            } => MergeState::Local {
                local_node,
                remote_node,
            },
            MergeState::Remote {
                local_node,
                remote_node,
            }
            | MergeState::RemoteWithNewStructure {
                local_node,
                remote_node,
            } => MergeState::RemoteWithNewStructure {
                local_node,
                remote_node,
            },
            MergeState::Unchanged {
                local_node,
                remote_node,
            } => {
                // Once the structure changes, it doesn't matter which side we
                // pick; we'll need to reupload the item to the server, anyway.
                MergeState::Local {
                    local_node,
                    remote_node,
                }
            }
        }
    }

    /// Returns the node from the preferred side. Unlike `local_node()` and
    /// `remote_node()`, this doesn't indicate which side, so it's only used
    /// for logging and `try_from()`.
    fn node(&self) -> &Node<'t> {
        match self {
            MergeState::LocalOnly(local_node) | MergeState::Local { local_node, .. } => local_node,

            MergeState::RemoteOnly(remote_node)
            | MergeState::Remote { remote_node, .. }
            | MergeState::RemoteOnlyWithNewStructure(remote_node)
            | MergeState::RemoteWithNewStructure { remote_node, .. } => remote_node,

            MergeState::Unchanged { local_node, .. } => local_node,
        }
    }
}

impl<'t> fmt::Display for MergeState<'t> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            MergeState::LocalOnly(_) | MergeState::Local { .. } => "(Local, Local)",

            MergeState::RemoteOnly(_) | MergeState::Remote { .. } => "(Remote, Remote)",

            MergeState::RemoteOnlyWithNewStructure(_)
            | MergeState::RemoteWithNewStructure { .. } => "(Remote, New)",

            MergeState::Unchanged { .. } => "(Unchanged, Unchanged)",
        })
    }
}

/// Content info for an item in the local or remote tree. This is used to dedupe
/// new local items to remote items that don't exist locally, with different
/// GUIDs and similar content.
///
/// - Bookmarks must have the same title and URL.
/// - Queries must have the same title and query URL.
/// - Folders and livemarks must have the same title.
/// - Separators must have the same position within their parents.
#[derive(Debug, Eq, Hash, PartialEq)]
pub enum Content {
    Bookmark { title: String, url_href: String },
    Folder { title: String },
    Separator { position: i64 },
}
