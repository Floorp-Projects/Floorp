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
    cell::Cell,
    collections::HashMap,
    convert::{TryFrom, TryInto},
    sync::Once,
};

use env_logger;

use crate::driver::{DefaultAbortSignal, Driver};
use crate::error::{Error, ErrorKind, Result};
use crate::guid::{Guid, ROOT_GUID, UNFILED_GUID};
use crate::merge::{Merger, StructureCounts};
use crate::tree::{
    Builder, Content, DivergedParent, DivergedParentGuid, Item, Kind, Problem, Problems, Tree,
    Validity,
};

#[derive(Debug)]
struct Node {
    item: Item,
    children: Vec<Node>,
}

impl Node {
    fn new(item: Item) -> Node {
        Node {
            item,
            children: Vec::new(),
        }
    }
    /// For convenience.
    fn into_tree(self) -> Result<Tree> {
        self.try_into()
    }
}

impl TryFrom<Node> for Builder {
    type Error = Error;

    fn try_from(node: Node) -> Result<Builder> {
        fn inflate(b: &mut Builder, parent_guid: &Guid, node: Node) -> Result<()> {
            let guid = node.item.guid.clone();
            b.item(node.item)
                .map(|_| ())
                .or_else(|err| match err.kind() {
                    ErrorKind::DuplicateItem(_) => Ok(()),
                    _ => Err(err),
                })?;
            b.parent_for(&guid).by_structure(&parent_guid)?;
            for child in node.children {
                inflate(b, &guid, child)?;
            }
            Ok(())
        }

        let guid = node.item.guid.clone();
        let mut builder = Tree::with_root(node.item);
        builder.reparent_orphans_to(&UNFILED_GUID);
        for child in node.children {
            inflate(&mut builder, &guid, child)?;
        }
        Ok(builder)
    }
}

impl TryFrom<Node> for Tree {
    type Error = Error;
    fn try_from(node: Node) -> Result<Tree> {
        Builder::try_from(node)?.try_into()
    }
}

macro_rules! nodes {
    ($children:tt) => { nodes!(ROOT_GUID, Folder[needs_merge = true], $children) };
    ($guid:expr, $kind:ident) => { nodes!(Guid::from($guid), $kind[]) };
    ($guid:expr, $kind:ident [ $( $name:ident = $value:expr ),* ]) => {{
        #[allow(unused_mut)]
        let mut item = Item::new(Guid::from($guid), Kind::$kind);
        $({ item.$name = $value; })*
        Node::new(item)
    }};
    ($guid:expr, $kind:ident, $children:tt) => { nodes!($guid, $kind[], $children) };
    ($guid:expr, $kind:ident [ $( $name:ident = $value:expr ),* ], { $(( $($children:tt)+ )),* }) => {{
        #[allow(unused_mut)]
        let mut node = nodes!($guid, $kind [ $( $name = $value ),* ]);
        $({
            let child = nodes!($($children)*);
            node.children.push(child.into());
        })*
        node
    }};
}

fn before_each() {
    static ONCE: Once = Once::new();
    ONCE.call_once(|| {
        env_logger::init();
    });
}

#[test]
fn reparent_and_reposition() {
    before_each();

    let local_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("folderAAAAAA", Folder[needs_merge = true], {
                ("bookmarkAAAA", Bookmark[needs_merge = true]),
                ("folderBBBBBB", Folder[needs_merge = true], {
                    ("bookmarkCCCC", Bookmark[needs_merge = true]),
                    ("bookmarkDDDD", Bookmark[needs_merge = true])
                }),
                ("bookmarkEEEE", Bookmark[needs_merge = true])
            }),
            ("bookmarkFFFF", Bookmark[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();

    let remote_tree = nodes!({
        ("unfiled_____", Folder[needs_merge = true], {
            ("folderBBBBBB", Folder[needs_merge = true], {
                ("bookmarkDDDD", Bookmark[needs_merge = true]),
                ("bookmarkAAAA", Bookmark[needs_merge = true]),
                ("bookmarkCCCC", Bookmark[needs_merge = true])
            })
        }),
        ("toolbar_____", Folder[needs_merge = true], {
            ("folderAAAAAA", Folder, {
                ("bookmarkFFFF", Bookmark[needs_merge = true]),
                ("bookmarkEEEE", Bookmark[needs_merge = true])
            })
        })
    })
    .into_tree()
    .unwrap();

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!(ROOT_GUID, Folder[needs_merge = true], {
        ("menu________", Folder[needs_merge = true], {
            ("bookmarkFFFF", Bookmark[needs_merge = true])
        }),
        ("unfiled_____", Folder, {
            ("folderBBBBBB", Folder, {
                ("bookmarkDDDD", Bookmark),
                ("bookmarkAAAA", Bookmark),
                ("bookmarkCCCC", Bookmark)
            })
        }),
        ("toolbar_____", Folder, {
            ("folderAAAAAA", Folder[needs_merge = true], {
                ("bookmarkEEEE", Bookmark)
            })
        })
    })
    .into_tree()
    .unwrap();
    let expected_telem = StructureCounts {
        merged_nodes: 10,
        ..StructureCounts::default()
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    assert_eq!(merger.deletions().count(), 0);

    assert_eq!(merger.counts(), &expected_telem);
}

// This test moves a bookmark that exists locally into a new folder that only
// exists remotely, and is a later sibling of the local parent.
#[test]
fn move_into_parent_sibling() {
    before_each();

    let local_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("folderAAAAAA", Folder[needs_merge = true], {
                ("bookmarkBBBB", Bookmark[needs_merge = true])
            })
        })
    })
    .into_tree()
    .unwrap();

    let remote_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("folderAAAAAA", Folder[needs_merge = true]),
            ("folderCCCCCC", Folder[needs_merge = true], {
                ("bookmarkBBBB", Bookmark[needs_merge = true])
            })
        })
    })
    .into_tree()
    .unwrap();

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("folderAAAAAA", Folder),
            ("folderCCCCCC", Folder, {
                ("bookmarkBBBB", Bookmark)
            })
        })
    })
    .into_tree()
    .unwrap();
    let expected_telem = StructureCounts {
        merged_nodes: 4,
        ..StructureCounts::default()
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    assert_eq!(merger.deletions().count(), 0);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn reorder_and_insert() {
    before_each();

    let _shared_tree = nodes!({
        ("menu________", Folder, {
            ("bookmarkAAAA", Bookmark),
            ("bookmarkBBBB", Bookmark),
            ("bookmarkCCCC", Bookmark)
        }),
        ("toolbar_____", Folder, {
            ("bookmarkDDDD", Bookmark),
            ("bookmarkEEEE", Bookmark),
            ("bookmarkFFFF", Bookmark)
        })
    })
    .into_tree()
    .unwrap();

    let local_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("bookmarkCCCC", Bookmark),
            ("bookmarkAAAA", Bookmark),
            ("bookmarkBBBB", Bookmark)
        }),
        ("toolbar_____", Folder[needs_merge = true, age = 5], {
            ("bookmarkDDDD", Bookmark),
            ("bookmarkEEEE", Bookmark),
            ("bookmarkFFFF", Bookmark),
            ("bookmarkGGGG", Bookmark[needs_merge = true]),
            ("bookmarkHHHH", Bookmark[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();

    let remote_tree = nodes!({
        ("menu________", Folder[needs_merge = true, age = 5], {
            ("bookmarkAAAA", Bookmark[age = 5]),
            ("bookmarkBBBB", Bookmark[age = 5]),
            ("bookmarkCCCC", Bookmark[age = 5]),
            ("bookmarkIIII", Bookmark[needs_merge = true]),
            ("bookmarkJJJJ", Bookmark[needs_merge = true])
        }),
        ("toolbar_____", Folder[needs_merge = true], {
            ("bookmarkFFFF", Bookmark),
            ("bookmarkDDDD", Bookmark),
            ("bookmarkEEEE", Bookmark)
        })
    })
    .into_tree()
    .unwrap();

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            // The server has an older menu, so we should use the local order (C A B)
            // as the base, then append (I J).
            ("bookmarkCCCC", Bookmark),
            ("bookmarkAAAA", Bookmark),
            ("bookmarkBBBB", Bookmark),
            ("bookmarkIIII", Bookmark),
            ("bookmarkJJJJ", Bookmark)
        }),
        ("toolbar_____", Folder[needs_merge = true, age = 5], {
            // The server has a newer toolbar, so we should use the remote order (F D E)
            // as the base, then append (G H).
            ("bookmarkFFFF", Bookmark),
            ("bookmarkDDDD", Bookmark),
            ("bookmarkEEEE", Bookmark),
            ("bookmarkGGGG", Bookmark[needs_merge = true]),
            ("bookmarkHHHH", Bookmark[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    let expected_telem = StructureCounts {
        merged_nodes: 12,
        ..StructureCounts::default()
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    assert_eq!(merger.deletions().count(), 0);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn unchanged_newer_changed_older() {
    before_each();

    let _shared_tree = nodes!({
        ("menu________", Folder[age = 5], {
            ("folderAAAAAA", Folder[age = 5]),
            ("bookmarkBBBB", Bookmark[age = 5])
        }),
        ("toolbar_____", Folder[age = 5], {
            ("folderCCCCCC", Folder[age = 5]),
            ("bookmarkDDDD", Bookmark[age = 5])
        })
    })
    .into_tree()
    .unwrap();

    let mut local_tree = nodes!({
        // Even though the local menu is newer (local = 5s, remote = 9s;
        // adding E updated the modified times of A and the menu), it's
        // not *changed* locally, so we should merge remote children first.
        ("menu________", Folder, {
            ("folderAAAAAA", Folder[needs_merge = true], {
                ("bookmarkEEEE", Bookmark[needs_merge = true])
            }),
            ("bookmarkBBBB", Bookmark[age = 5])
        }),
        ("toolbar_____", Folder[needs_merge = true, age = 5], {
            ("bookmarkDDDD", Bookmark[age = 5])
        })
    })
    .into_tree()
    .unwrap();
    local_tree.note_deleted("folderCCCCCC".into());

    let mut remote_tree = nodes!({
        ("menu________", Folder[needs_merge = true, age = 5], {
            ("bookmarkBBBB", Bookmark[age = 5])
        }),
        // Even though the remote toolbar is newer (local = 15s, remote = 10s), it's
        // not changed remotely, so we should merge local children first.
        ("toolbar_____", Folder[age = 5], {
            ("folderCCCCCC", Folder[needs_merge = true], {
                ("bookmarkFFFF", Bookmark[needs_merge = true])
            }),
            ("bookmarkDDDD", Bookmark[age = 5])
        })
    })
    .into_tree()
    .unwrap();
    remote_tree.note_deleted("folderAAAAAA".into());

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("bookmarkBBBB", Bookmark[age = 5]),
            ("bookmarkEEEE", Bookmark[needs_merge = true])
        }),
        ("toolbar_____", Folder[needs_merge = true, age = 5], {
            ("bookmarkDDDD", Bookmark[age = 5]),
            ("bookmarkFFFF", Bookmark[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    let expected_deletions = vec!["folderAAAAAA", "folderCCCCCC"];
    let expected_telem = StructureCounts {
        remote_revives: 0,
        local_deletes: 1,
        local_revives: 0,
        remote_deletes: 1,
        dupes: 0,
        merged_nodes: 6,
        merged_deletions: 2,
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    let mut deletions = merger.deletions().map(|d| d.guid).collect::<Vec<_>>();
    deletions.sort();
    assert_eq!(deletions, expected_deletions);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn newer_local_moves() {
    before_each();

    let _shared_tree = nodes!({
        ("menu________", Folder[age = 10], {
            ("bookmarkAAAA", Bookmark[age = 10]),
            ("folderBBBBBB", Folder[age = 10], {
                ("bookmarkCCCC", Bookmark[age = 10])
            }),
            ("folderDDDDDD", Folder[age = 10])
        }),
        ("toolbar_____", Folder[age = 10], {
            ("bookmarkEEEE", Bookmark[age = 10]),
            ("folderFFFFFF", Folder[age = 10], {
                ("bookmarkGGGG", Bookmark[age = 10])
            }),
            ("folderHHHHHH", Folder[age = 10])
        })
    })
    .into_tree()
    .unwrap();

    let local_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("folderDDDDDD", Folder[needs_merge = true], {
                ("bookmarkCCCC", Bookmark[needs_merge = true])
            })
        }),
        ("toolbar_____", Folder[needs_merge = true], {
            ("folderHHHHHH", Folder[needs_merge = true], {
                ("bookmarkGGGG", Bookmark[needs_merge = true])
            }),
            ("folderFFFFFF", Folder[needs_merge = true]),
            ("bookmarkEEEE", Bookmark[age = 10])
        }),
        ("unfiled_____", Folder[needs_merge = true], {
            ("bookmarkAAAA", Bookmark[needs_merge = true])
        }),
        ("mobile______", Folder[needs_merge = true], {
            ("folderBBBBBB", Folder[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();

    let remote_tree = nodes!({
        ("mobile______", Folder[needs_merge = true, age = 5], {
            ("bookmarkAAAA", Bookmark[needs_merge = true, age = 5])
        }),
        ("unfiled_____", Folder[needs_merge = true, age = 5], {
            ("folderBBBBBB", Folder[needs_merge = true, age = 5])
        }),
        ("menu________", Folder[needs_merge = true, age = 5], {
            ("folderDDDDDD", Folder[needs_merge = true, age = 5], {
                ("bookmarkGGGG", Bookmark[needs_merge = true, age = 5])
            })
        }),
        ("toolbar_____", Folder[needs_merge = true, age = 5], {
            ("folderFFFFFF", Folder[needs_merge = true, age = 5]),
            ("bookmarkEEEE", Bookmark[age = 10]),
            ("folderHHHHHH", Folder[needs_merge = true, age = 5], {
                ("bookmarkCCCC", Bookmark[needs_merge = true, age = 5])
            })
        })
    })
    .into_tree()
    .unwrap();

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("folderDDDDDD", Folder[needs_merge = true], {
                ("bookmarkCCCC", Bookmark[needs_merge = true])
            })
        }),
        ("toolbar_____", Folder[needs_merge = true], {
            ("folderHHHHHH", Folder[needs_merge = true], {
                ("bookmarkGGGG", Bookmark[needs_merge = true])
            }),
            ("folderFFFFFF", Folder[needs_merge = true]),
            ("bookmarkEEEE", Bookmark[age = 10])
        }),
        ("unfiled_____", Folder[needs_merge = true], {
            ("bookmarkAAAA", Bookmark[needs_merge = true])
        }),
        ("mobile______", Folder[needs_merge = true], {
            ("folderBBBBBB", Folder[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    let expected_telem = StructureCounts {
        merged_nodes: 12,
        ..StructureCounts::default()
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    assert_eq!(merger.deletions().count(), 0);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn newer_remote_moves() {
    before_each();

    let _shared_tree = nodes!({
        ("menu________", Folder[age = 10], {
            ("bookmarkAAAA", Bookmark[age = 10]),
            ("folderBBBBBB", Folder[age = 10], {
                ("bookmarkCCCC", Bookmark[age = 10])
            }),
            ("folderDDDDDD", Folder[age = 10])
        }),
        ("toolbar_____", Folder[age = 10], {
            ("bookmarkEEEE", Bookmark[age = 10]),
            ("folderFFFFFF", Folder[age = 10], {
                ("bookmarkGGGG", Bookmark[age = 10])
            }),
            ("folderHHHHHH", Folder[age = 10])
        })
    })
    .into_tree()
    .unwrap();

    let local_tree = nodes!({
        ("menu________", Folder[needs_merge = true, age = 5], {
            ("folderDDDDDD", Folder[needs_merge = true, age = 5], {
                ("bookmarkCCCC", Bookmark[needs_merge = true, age = 5])
            })
        }),
        ("toolbar_____", Folder[needs_merge = true, age = 5], {
            ("folderHHHHHH", Folder[needs_merge = true, age = 5], {
                ("bookmarkGGGG", Bookmark[needs_merge = true, age = 5])
            }),
            ("folderFFFFFF", Folder[needs_merge = true, age = 5]),
            ("bookmarkEEEE", Bookmark[age = 10])
        }),
        ("unfiled_____", Folder[needs_merge = true, age = 5], {
            ("bookmarkAAAA", Bookmark[needs_merge = true, age = 5])
        }),
        ("mobile______", Folder[needs_merge = true, age = 5], {
            ("folderBBBBBB", Folder[needs_merge = true, age = 5])
        })
    })
    .into_tree()
    .unwrap();

    let remote_tree = nodes!({
        ("mobile______", Folder[needs_merge = true], {
            ("bookmarkAAAA", Bookmark[needs_merge = true])
        }),
        ("unfiled_____", Folder[needs_merge = true], {
            ("folderBBBBBB", Folder[needs_merge = true])
        }),
        ("menu________", Folder[needs_merge = true], {
            ("folderDDDDDD", Folder[needs_merge = true], {
                ("bookmarkGGGG", Bookmark[needs_merge = true])
            })
        }),
        ("toolbar_____", Folder[needs_merge = true], {
            ("folderFFFFFF", Folder[needs_merge = true]),
            ("bookmarkEEEE", Bookmark[age = 10]),
            ("folderHHHHHH", Folder[needs_merge = true], {
                ("bookmarkCCCC", Bookmark[needs_merge = true])
            })
        })
    })
    .into_tree()
    .unwrap();

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("menu________", Folder[needs_merge = true, age = 5], {
            ("folderDDDDDD", Folder, {
                ("bookmarkGGGG", Bookmark)
            })
        }),
        ("toolbar_____", Folder[needs_merge = true, age = 5], {
            ("folderFFFFFF", Folder),
            ("bookmarkEEEE", Bookmark[age = 10]),
            ("folderHHHHHH", Folder, {
                ("bookmarkCCCC", Bookmark)
            })
        }),
        ("unfiled_____", Folder[needs_merge = true, age = 5], {
            ("folderBBBBBB", Folder)
        }),
        ("mobile______", Folder[needs_merge = true, age = 5], {
            ("bookmarkAAAA", Bookmark)
        })
    })
    .into_tree()
    .unwrap();
    let expected_telem = StructureCounts {
        merged_nodes: 12,
        ..StructureCounts::default()
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    assert_eq!(merger.deletions().count(), 0);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn value_structure_conflict() {
    before_each();

    let _shared_tree = nodes!({
        ("menu________", Folder, {
            ("folderAAAAAA", Folder, {
                ("bookmarkBBBB", Bookmark),
                ("bookmarkCCCC", Bookmark)
            }),
            ("folderDDDDDD", Folder, {
                ("bookmarkEEEE", Bookmark)
            })
        })
    })
    .into_tree()
    .unwrap();

    let local_tree = nodes!({
        ("menu________", Folder, {
            ("folderAAAAAA", Folder[needs_merge = true, age = 10], {
                ("bookmarkCCCC", Bookmark)
            }),
            ("folderDDDDDD", Folder[needs_merge = true, age = 10], {
                ("bookmarkBBBB", Bookmark),
                ("bookmarkEEEE", Bookmark[age = 10])
            })
        })
    })
    .into_tree()
    .unwrap();

    let remote_tree = nodes!({
        ("menu________", Folder, {
            ("folderAAAAAA", Folder, {
                ("bookmarkBBBB", Bookmark),
                ("bookmarkCCCC", Bookmark)
            }),
            ("folderDDDDDD", Folder[needs_merge = true, age = 5], {
                ("bookmarkEEEE", Bookmark[needs_merge = true, age = 5])
            })
        })
    })
    .into_tree()
    .unwrap();

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("menu________", Folder, {
            ("folderAAAAAA", Folder[needs_merge = true, age = 10], {
                ("bookmarkCCCC", Bookmark)
            }),
            ("folderDDDDDD", Folder[needs_merge = true, age = 5], {
                ("bookmarkEEEE", Bookmark[age = 5]),
                ("bookmarkBBBB", Bookmark[needs_merge = true])
            })
        })
    })
    .into_tree()
    .unwrap();
    let expected_telem = StructureCounts {
        merged_nodes: 6,
        ..StructureCounts::default()
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    assert_eq!(merger.deletions().count(), 0);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn complex_move_with_additions() {
    before_each();

    let _shared_tree = nodes!({
        ("menu________", Folder, {
            ("folderAAAAAA", Folder, {
                ("bookmarkBBBB", Bookmark),
                ("bookmarkCCCC", Bookmark)
            })
        })
    })
    .into_tree()
    .unwrap();

    let local_tree = nodes!({
        ("menu________", Folder, {
            ("folderAAAAAA", Folder[needs_merge = true], {
                ("bookmarkBBBB", Bookmark),
                ("bookmarkCCCC", Bookmark),
                ("bookmarkDDDD", Bookmark[needs_merge = true])
            })
        })
    })
    .into_tree()
    .unwrap();

    let remote_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("bookmarkCCCC", Bookmark[needs_merge = true])
        }),
        ("toolbar_____", Folder[needs_merge = true], {
            ("folderAAAAAA", Folder[needs_merge = true], {
                ("bookmarkBBBB", Bookmark),
                ("bookmarkEEEE", Bookmark[needs_merge = true])
            })
        })
    })
    .into_tree()
    .unwrap();

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("menu________", Folder, {
            ("bookmarkCCCC", Bookmark)
        }),
        ("toolbar_____", Folder, {
            ("folderAAAAAA", Folder[needs_merge = true], {
                // We can guarantee child order (B E D), since we always walk remote
                // children first, and the remote folder A record is newer than the
                // local folder. If the local folder were newer, the order would be
                // (D B E).
                ("bookmarkBBBB", Bookmark),
                ("bookmarkEEEE", Bookmark),
                ("bookmarkDDDD", Bookmark[needs_merge = true])
            })
        })
    })
    .into_tree()
    .unwrap();
    let expected_telem = StructureCounts {
        merged_nodes: 7,
        ..StructureCounts::default()
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    assert_eq!(merger.deletions().count(), 0);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn complex_orphaning() {
    before_each();

    let _shared_tree = nodes!({
        ("toolbar_____", Folder, {
            ("folderAAAAAA", Folder, {
                ("folderBBBBBB", Folder)
            })
        }),
        ("menu________", Folder, {
            ("folderCCCCCC", Folder, {
                ("folderDDDDDD", Folder, {
                    ("folderEEEEEE", Folder)
                })
            })
        })
    })
    .into_tree()
    .unwrap();

    // Locally: delete E, add B > F.
    let mut local_tree = nodes!({
        ("toolbar_____", Folder[needs_merge = false], {
            ("folderAAAAAA", Folder, {
                ("folderBBBBBB", Folder[needs_merge = true], {
                    ("bookmarkFFFF", Bookmark[needs_merge = true])
                })
            })
        }),
        ("menu________", Folder, {
            ("folderCCCCCC", Folder, {
                ("folderDDDDDD", Folder[needs_merge = true])
            })
        })
    })
    .into_tree()
    .unwrap();
    local_tree.note_deleted("folderEEEEEE".into());

    // Remotely: delete B, add E > G.
    let mut remote_tree = nodes!({
        ("toolbar_____", Folder, {
            ("folderAAAAAA", Folder[needs_merge = true])
        }),
        ("menu________", Folder, {
            ("folderCCCCCC", Folder, {
                ("folderDDDDDD", Folder, {
                    ("folderEEEEEE", Folder[needs_merge = true], {
                        ("bookmarkGGGG", Bookmark[needs_merge = true])
                    })
                })
            })
        })
    })
    .into_tree()
    .unwrap();
    remote_tree.note_deleted("folderBBBBBB".into());

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("toolbar_____", Folder, {
            ("folderAAAAAA", Folder[needs_merge = true], {
                // B was deleted remotely, so F moved to A, the closest
                // surviving parent.
                ("bookmarkFFFF", Bookmark[needs_merge = true])
            })
        }),
        ("menu________", Folder, {
            ("folderCCCCCC", Folder, {
                ("folderDDDDDD", Folder[needs_merge = true], {
                    // E was deleted locally, so G moved to D.
                    ("bookmarkGGGG", Bookmark[needs_merge = true])
                })
            })
        })
    })
    .into_tree()
    .unwrap();
    let expected_deletions = vec!["folderBBBBBB", "folderEEEEEE"];
    let expected_telem = StructureCounts {
        remote_revives: 0,
        local_deletes: 1,
        local_revives: 0,
        remote_deletes: 1,
        dupes: 0,
        merged_nodes: 7,
        merged_deletions: 2,
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    let mut deletions = merger.deletions().map(|d| d.guid).collect::<Vec<_>>();
    deletions.sort();
    assert_eq!(deletions, expected_deletions);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn locally_modified_remotely_deleted() {
    before_each();

    let _shared_tree = nodes!({
        ("toolbar_____", Folder, {
            ("folderAAAAAA", Folder, {
                ("folderBBBBBB", Folder)
            })
        }),
        ("menu________", Folder, {
            ("folderCCCCCC", Folder, {
                ("folderDDDDDD", Folder, {
                    ("folderEEEEEE", Folder)
                })
            })
        })
    })
    .into_tree()
    .unwrap();

    let mut local_tree = nodes!({
        ("toolbar_____", Folder, {
            ("folderAAAAAA", Folder, {
                ("folderBBBBBB", Folder[needs_merge = true], {
                    ("bookmarkFFFF", Bookmark[needs_merge = true])
                })
            })
        }),
        ("menu________", Folder, {
            ("folderCCCCCC", Folder, {
                ("folderDDDDDD", Folder[needs_merge = true])
            })
        })
    })
    .into_tree()
    .unwrap();
    local_tree.note_deleted("folderEEEEEE".into());

    let mut remote_tree = nodes!({
        ("toolbar_____", Folder, {
            ("folderAAAAAA", Folder[needs_merge = true])
        }),
        ("menu________", Folder, {
            ("folderCCCCCC", Folder, {
                ("folderDDDDDD", Folder, {
                    ("folderEEEEEE", Folder[needs_merge = true], {
                        ("bookmarkGGGG", Bookmark[needs_merge = true])
                    })
                })
            })
        })
    })
    .into_tree()
    .unwrap();
    remote_tree.note_deleted("folderBBBBBB".into());

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("toolbar_____", Folder, {
            ("folderAAAAAA", Folder[needs_merge = true], {
                ("bookmarkFFFF", Bookmark[needs_merge = true])
            })
        }),
        ("menu________", Folder, {
            ("folderCCCCCC", Folder, {
                ("folderDDDDDD", Folder[needs_merge = true], {
                    ("bookmarkGGGG", Bookmark[needs_merge = true])
                })
            })
        })
    })
    .into_tree()
    .unwrap();
    let expected_deletions = vec!["folderBBBBBB", "folderEEEEEE"];
    let expected_telem = StructureCounts {
        remote_revives: 0,
        local_deletes: 1,
        local_revives: 0,
        remote_deletes: 1,
        dupes: 0,
        merged_nodes: 7,
        merged_deletions: 2,
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    let mut deletions = merger.deletions().map(|d| d.guid).collect::<Vec<_>>();
    deletions.sort();
    assert_eq!(deletions, expected_deletions);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn locally_deleted_remotely_modified() {
    before_each();

    let _shared_tree = nodes!({
        ("menu________", Folder, {
            ("bookmarkAAAA", Bookmark),
            ("folderBBBBBB", Folder, {
                ("bookmarkCCCC", Bookmark),
                ("folderDDDDDD", Folder, {
                    ("bookmarkEEEE", Bookmark)
                })
            })
        })
    })
    .into_tree()
    .unwrap();

    let mut local_tree = nodes!({ ("menu________", Folder[needs_merge = true]) })
        .into_tree()
        .unwrap();
    for guid in &[
        "bookmarkAAAA",
        "folderBBBBBB",
        "bookmarkCCCC",
        "folderDDDDDD",
        "bookmarkEEEE",
    ] {
        local_tree.note_deleted(guid.to_owned().into());
    }

    let remote_tree = nodes!({
        ("menu________", Folder, {
            ("bookmarkAAAA", Bookmark[needs_merge = true]),
            ("folderBBBBBB", Folder[needs_merge = true], {
                ("bookmarkCCCC", Bookmark),
                ("folderDDDDDD", Folder[needs_merge = true], {
                    ("bookmarkEEEE", Bookmark),
                    ("bookmarkFFFF", Bookmark[needs_merge = true])
                }),
                ("bookmarkGGGG", Bookmark[needs_merge = true])
            })
        })
    })
    .into_tree()
    .unwrap();

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("bookmarkAAAA", Bookmark),
            ("bookmarkFFFF", Bookmark[needs_merge = true]),
            ("bookmarkGGGG", Bookmark[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    let expected_deletions = vec![
        "bookmarkCCCC",
        "bookmarkEEEE",
        "folderBBBBBB",
        "folderDDDDDD",
    ];
    let expected_telem = StructureCounts {
        remote_revives: 1,
        local_deletes: 2,
        local_revives: 0,
        remote_deletes: 0,
        dupes: 0,
        merged_nodes: 4,
        merged_deletions: 4,
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    let mut deletions = merger.deletions().map(|d| d.guid).collect::<Vec<_>>();
    deletions.sort();
    assert_eq!(deletions, expected_deletions);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn nonexistent_on_one_side() {
    before_each();

    let mut local_tree = Tree::with_root(Item::new(ROOT_GUID, Kind::Folder))
        .into_tree()
        .unwrap();
    // A doesn't exist remotely.
    local_tree.note_deleted("bookmarkAAAA".into());

    let mut remote_tree = Tree::with_root(Item::new(ROOT_GUID, Kind::Folder))
        .into_tree()
        .unwrap();
    // B doesn't exist locally.
    remote_tree.note_deleted("bookmarkBBBB".into());

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let mut expected_root = Item::new(ROOT_GUID, Kind::Folder);
    expected_root.needs_merge = true;
    let expected_tree = Tree::with_root(expected_root).into_tree().unwrap();
    let expected_deletions = vec!["bookmarkAAAA", "bookmarkBBBB"];
    let expected_telem = StructureCounts {
        merged_deletions: 2,
        ..StructureCounts::default()
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    let mut deletions = merger.deletions().map(|d| d.guid).collect::<Vec<_>>();
    deletions.sort();
    assert_eq!(deletions, expected_deletions);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn clear_folder_then_delete() {
    before_each();

    let _shared_tree = nodes!({
        ("menu________", Folder, {
            ("folderAAAAAA", Folder, {
                ("bookmarkBBBB", Bookmark),
                ("bookmarkCCCC", Bookmark)
            }),
            ("folderDDDDDD", Folder, {
                ("bookmarkEEEE", Bookmark),
                ("bookmarkFFFF", Bookmark)
            })
        })
    })
    .into_tree()
    .unwrap();

    let mut local_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("folderAAAAAA", Folder, {
                ("bookmarkBBBB", Bookmark),
                ("bookmarkCCCC", Bookmark)
            }),
            ("bookmarkEEEE", Bookmark[needs_merge = true])
        }),
        ("mobile______", Folder[needs_merge = true], {
            ("bookmarkFFFF", Bookmark[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    local_tree.note_deleted("folderDDDDDD".into());

    let mut remote_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("bookmarkBBBB", Bookmark[needs_merge = true]),
            ("folderDDDDDD", Folder, {
                ("bookmarkEEEE", Bookmark),
                ("bookmarkFFFF", Bookmark)
            })
        }),
        ("unfiled_____", Folder[needs_merge = true], {
            ("bookmarkCCCC", Bookmark[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    remote_tree.note_deleted("folderAAAAAA".into());

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!(ROOT_GUID, Folder[needs_merge = true], {
        ("menu________", Folder[needs_merge = true], {
            ("bookmarkBBBB", Bookmark),
            ("bookmarkEEEE", Bookmark[needs_merge = true])
        }),
        ("mobile______", Folder[needs_merge = true], {
            ("bookmarkFFFF", Bookmark[needs_merge = true])
        }),
        ("unfiled_____", Folder, {
            ("bookmarkCCCC", Bookmark)
        })
    })
    .into_tree()
    .unwrap();
    let expected_deletions = vec!["folderAAAAAA", "folderDDDDDD"];
    let expected_telem = StructureCounts {
        merged_nodes: 7,
        merged_deletions: 2,
        ..StructureCounts::default()
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    let mut deletions = merger.deletions().map(|d| d.guid).collect::<Vec<_>>();
    deletions.sort();
    assert_eq!(deletions, expected_deletions);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn newer_move_to_deleted() {
    before_each();

    let _shared_tree = nodes!({
        ("menu________", Folder, {
            ("folderAAAAAA", Folder, {
                ("bookmarkBBBB", Bookmark)
            }),
            ("folderCCCCCC", Folder, {
                ("bookmarkDDDD", Bookmark)
            })
        })
    })
    .into_tree()
    .unwrap();

    let mut local_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            // A is younger locally. However, we should *not* revert
            // remotely moving B to the toolbar. (Locally, B exists in A,
            // but we deleted the now-empty A remotely).
            ("folderAAAAAA", Folder[needs_merge = true], {
                ("bookmarkBBBB", Bookmark[age = 5]),
                ("bookmarkEEEE", Bookmark[needs_merge = true])
            })
        }),
        ("toolbar_____", Folder[needs_merge = true], {
            ("bookmarkDDDD", Bookmark[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    local_tree.note_deleted("folderCCCCCC".into());

    let mut remote_tree = nodes!({
        ("menu________", Folder[needs_merge = true, age = 5], {
            // C is younger remotely. However, we should *not* revert
            // locally moving D to the toolbar. (Locally, D exists in C,
            // but we deleted the now-empty C locally).
            ("folderCCCCCC", Folder[needs_merge = true], {
                ("bookmarkDDDD", Bookmark[age = 5]),
                ("bookmarkFFFF", Bookmark[needs_merge = true])
            })
        }),
        ("toolbar_____", Folder[needs_merge = true, age = 5], {
            ("bookmarkBBBB", Bookmark[needs_merge = true, age = 5])
        })
    })
    .into_tree()
    .unwrap();
    remote_tree.note_deleted("folderAAAAAA".into());

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("bookmarkEEEE", Bookmark[needs_merge = true]),
            ("bookmarkFFFF", Bookmark[needs_merge = true])
        }),
        ("toolbar_____", Folder[needs_merge = true], {
            ("bookmarkDDDD", Bookmark[needs_merge = true]),
            ("bookmarkBBBB", Bookmark[age = 5])
        })
    })
    .into_tree()
    .unwrap();
    let expected_deletions = vec!["folderAAAAAA", "folderCCCCCC"];
    let expected_telem = StructureCounts {
        remote_revives: 0,
        local_deletes: 1,
        local_revives: 0,
        remote_deletes: 1,
        dupes: 0,
        merged_nodes: 6,
        merged_deletions: 2,
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    let mut deletions = merger.deletions().map(|d| d.guid).collect::<Vec<_>>();
    deletions.sort();
    assert_eq!(deletions, expected_deletions);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn deduping_local_newer() {
    before_each();

    let local_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("bookmarkAAA1", Bookmark[needs_merge = true]),
            ("bookmarkAAA2", Bookmark[needs_merge = true]),
            ("bookmarkAAA3", Bookmark[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    let mut new_local_contents: HashMap<Guid, Content> = HashMap::new();
    new_local_contents.insert(
        "bookmarkAAA1".into(),
        Content::Bookmark {
            title: "A".into(),
            url_href: "http://example.com/a".into(),
        },
    );
    new_local_contents.insert(
        "bookmarkAAA2".into(),
        Content::Bookmark {
            title: "A".into(),
            url_href: "http://example.com/a".into(),
        },
    );
    new_local_contents.insert(
        "bookmarkAAA3".into(),
        Content::Bookmark {
            title: "A".into(),
            url_href: "http://example.com/a".into(),
        },
    );

    let remote_tree = nodes!({
        ("menu________", Folder[needs_merge = true, age = 5], {
            ("bookmarkAAAA", Bookmark[needs_merge = true, age = 5]),
            ("bookmarkAAA4", Bookmark[needs_merge = true, age = 5]),
            ("bookmarkAAA5", Bookmark)
        })
    })
    .into_tree()
    .unwrap();
    let mut new_remote_contents: HashMap<Guid, Content> = HashMap::new();
    new_remote_contents.insert(
        "bookmarkAAAA".into(),
        Content::Bookmark {
            title: "A".into(),
            url_href: "http://example.com/a".into(),
        },
    );
    new_remote_contents.insert(
        "bookmarkAAA4".into(),
        Content::Bookmark {
            title: "A".into(),
            url_href: "http://example.com/a".into(),
        },
    );

    let mut merger = Merger::with_contents(
        &local_tree,
        &new_local_contents,
        &remote_tree,
        &new_remote_contents,
    );
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("bookmarkAAAA", Bookmark[needs_merge = true]),
            ("bookmarkAAA4", Bookmark[needs_merge = true]),
            ("bookmarkAAA3", Bookmark[needs_merge = true]),
            ("bookmarkAAA5", Bookmark)
        })
    })
    .into_tree()
    .unwrap();
    let expected_telem = StructureCounts {
        remote_revives: 0,
        local_deletes: 0,
        local_revives: 0,
        remote_deletes: 0,
        dupes: 2,
        merged_nodes: 5,
        merged_deletions: 0,
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    assert_eq!(merger.deletions().count(), 0);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn deduping_remote_newer() {
    before_each();

    let local_tree = nodes!({
        ("menu________", Folder[needs_merge = true, age = 5], {
            // Shouldn't dedupe to `folderAAAAA1` because it's not in
            // `new_local_contents`.
            ("folderAAAAAA", Folder[needs_merge = true, age = 5], {
                // Shouldn't dedupe to `bookmarkBBB1`. (bookmarkG111)
                ("bookmarkBBBB", Bookmark[age = 10]),
                // Not a candidate for `bookmarkCCC1` because the URLs are
                // different. (bookmarkH111)
                ("bookmarkCCCC", Bookmark[needs_merge = true, age = 5])
            }),
            // Should dedupe to `folderDDDDD1`. (folderB11111)
            ("folderDDDDDD", Folder[needs_merge = true, age = 5], {
                // Should dedupe to `bookmarkEEE1`. (bookmarkC222)
                ("bookmarkEEEE", Bookmark[needs_merge = true, age = 5]),
                // Should dedupe to `separatorFF1` because the positions are
                // the same. (separatorF11)
                ("separatorFFF", Separator[needs_merge = true, age = 5])
            }),
            // Shouldn't dedupe to `separatorGG1`, because the positions are
            // different. (separatorE11)
            ("separatorGGG", Separator[needs_merge = true, age = 5]),
            // Shouldn't dedupe to `bookmarkHHH1` because the parents are
            // different. (bookmarkC222)
            ("bookmarkHHHH", Bookmark[needs_merge = true, age = 5]),
            // Should dedupe to `queryIIIIII1`.
            ("queryIIIIIII", Query[needs_merge = true, age = 5])
        })
    })
    .into_tree()
    .unwrap();
    let mut new_local_contents: HashMap<Guid, Content> = HashMap::new();
    new_local_contents.insert(
        "bookmarkCCCC".into(),
        Content::Bookmark {
            title: "C".into(),
            url_href: "http://example.com/c".into(),
        },
    );
    new_local_contents.insert("folderDDDDDD".into(), Content::Folder { title: "D".into() });
    new_local_contents.insert(
        "bookmarkEEEE".into(),
        Content::Bookmark {
            title: "E".into(),
            url_href: "http://example.com/e".into(),
        },
    );
    new_local_contents.insert("separatorFFF".into(), Content::Separator { position: 1 });
    new_local_contents.insert("separatorGGG".into(), Content::Separator { position: 2 });
    new_local_contents.insert(
        "bookmarkHHHH".into(),
        Content::Bookmark {
            title: "H".into(),
            url_href: "http://example.com/h".into(),
        },
    );
    new_local_contents.insert(
        "queryIIIIIII".into(),
        Content::Bookmark {
            title: "I".into(),
            url_href: "place:maxResults=10&sort=8".into(),
        },
    );

    let remote_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("folderAAAAAA", Folder[needs_merge = true], {
                ("bookmarkBBBB", Bookmark[age = 10]),
                ("bookmarkCCC1", Bookmark[needs_merge = true])
            }),
            ("folderDDDDD1", Folder[needs_merge = true], {
                ("bookmarkEEE1", Bookmark[needs_merge = true]),
                ("separatorFF1", Separator[needs_merge = true])
            }),
            ("separatorGG1", Separator[needs_merge = true]),
            ("bookmarkHHH1", Bookmark[needs_merge = true]),
            ("queryIIIIII1", Query[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    let mut new_remote_contents: HashMap<Guid, Content> = HashMap::new();
    new_remote_contents.insert(
        "bookmarkCCC1".into(),
        Content::Bookmark {
            title: "C".into(),
            url_href: "http://example.com/c1".into(),
        },
    );
    new_remote_contents.insert("folderDDDDD1".into(), Content::Folder { title: "D".into() });
    new_remote_contents.insert(
        "bookmarkEEE1".into(),
        Content::Bookmark {
            title: "E".into(),
            url_href: "http://example.com/e".into(),
        },
    );
    new_remote_contents.insert("separatorFF1".into(), Content::Separator { position: 1 });
    new_remote_contents.insert("separatorGG1".into(), Content::Separator { position: 2 });
    new_remote_contents.insert(
        "bookmarkHHH1".into(),
        Content::Bookmark {
            title: "H".into(),
            url_href: "http://example.com/h".into(),
        },
    );
    new_remote_contents.insert(
        "queryIIIIII1".into(),
        Content::Bookmark {
            title: "I".into(),
            url_href: "place:maxResults=10&sort=8".into(),
        },
    );

    let mut merger = Merger::with_contents(
        &local_tree,
        &new_local_contents,
        &remote_tree,
        &new_remote_contents,
    );
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("menu________", Folder[needs_merge = true, age = 5], {
            ("folderAAAAAA", Folder[needs_merge = true], {
                ("bookmarkBBBB", Bookmark[age = 10]),
                ("bookmarkCCC1", Bookmark),
                ("bookmarkCCCC", Bookmark[needs_merge = true, age = 5])
            }),
            ("folderDDDDD1", Folder, {
                ("bookmarkEEE1", Bookmark),
                ("separatorFF1", Separator)
            }),
            ("separatorGG1", Separator),
            ("bookmarkHHH1", Bookmark),
            ("queryIIIIII1", Query)
        })
    })
    .into_tree()
    .unwrap();
    let expected_telem = StructureCounts {
        remote_revives: 0,
        local_deletes: 0,
        local_revives: 0,
        remote_deletes: 0,
        dupes: 6,
        merged_nodes: 11,
        merged_deletions: 0,
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    assert_eq!(merger.deletions().count(), 0);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn complex_deduping() {
    before_each();

    let local_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("folderAAAAAA", Folder[needs_merge = true, age = 10], {
                ("bookmarkBBBB", Bookmark[needs_merge = true, age = 10]),
                ("bookmarkCCCC", Bookmark[needs_merge = true, age = 10])
            }),
            ("folderDDDDDD", Folder[needs_merge = true], {
                ("bookmarkEEEE", Bookmark[needs_merge = true, age = 5])
            }),
            ("folderFFFFFF", Folder[needs_merge = true, age = 5], {
                ("bookmarkGGGG", Bookmark[needs_merge = true, age = 5])
            })
        })
    })
    .into_tree()
    .unwrap();
    let mut new_local_contents: HashMap<Guid, Content> = HashMap::new();
    new_local_contents.insert("folderAAAAAA".into(), Content::Folder { title: "A".into() });
    new_local_contents.insert(
        "bookmarkBBBB".into(),
        Content::Bookmark {
            title: "B".into(),
            url_href: "http://example.com/b".into(),
        },
    );
    new_local_contents.insert(
        "bookmarkCCCC".into(),
        Content::Bookmark {
            title: "C".into(),
            url_href: "http://example.com/c".into(),
        },
    );
    new_local_contents.insert("folderDDDDDD".into(), Content::Folder { title: "D".into() });
    new_local_contents.insert(
        "bookmarkEEEE".into(),
        Content::Bookmark {
            title: "E".into(),
            url_href: "http://example.com/e".into(),
        },
    );
    new_local_contents.insert("folderFFFFFF".into(), Content::Folder { title: "F".into() });
    new_local_contents.insert(
        "bookmarkGGGG".into(),
        Content::Bookmark {
            title: "G".into(),
            url_href: "http://example.com/g".into(),
        },
    );

    let remote_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("folderAAAAA1", Folder[needs_merge = true], {
                ("bookmarkBBB1", Bookmark[needs_merge = true])
            }),
            ("folderDDDDD1", Folder[needs_merge = true, age = 5], {
                ("bookmarkEEE1", Bookmark[needs_merge = true])
            }),
            ("folderFFFFF1", Folder[needs_merge = true], {
                ("bookmarkGGG1", Bookmark[needs_merge = true, age = 5]),
                ("bookmarkHHH1", Bookmark[needs_merge = true])
            })
        })
    })
    .into_tree()
    .unwrap();
    let mut new_remote_contents: HashMap<Guid, Content> = HashMap::new();
    new_remote_contents.insert("folderAAAAA1".into(), Content::Folder { title: "A".into() });
    new_remote_contents.insert(
        "bookmarkBBB1".into(),
        Content::Bookmark {
            title: "B".into(),
            url_href: "http://example.com/b".into(),
        },
    );
    new_remote_contents.insert("folderDDDDD1".into(), Content::Folder { title: "D".into() });
    new_remote_contents.insert(
        "bookmarkEEE1".into(),
        Content::Bookmark {
            title: "E".into(),
            url_href: "http://example.com/e".into(),
        },
    );
    new_remote_contents.insert("folderFFFFF1".into(), Content::Folder { title: "F".into() });
    new_remote_contents.insert(
        "bookmarkGGG1".into(),
        Content::Bookmark {
            title: "G".into(),
            url_href: "http://example.com/g".into(),
        },
    );
    new_remote_contents.insert(
        "bookmarkHHH1".into(),
        Content::Bookmark {
            title: "H".into(),
            url_href: "http://example.com/h".into(),
        },
    );

    let mut merger = Merger::with_contents(
        &local_tree,
        &new_local_contents,
        &remote_tree,
        &new_remote_contents,
    );
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("folderAAAAA1", Folder[needs_merge = true], {
                ("bookmarkBBB1", Bookmark),
                ("bookmarkCCCC", Bookmark[needs_merge = true, age = 10])
            }),
            ("folderDDDDD1", Folder[needs_merge = true], {
                ("bookmarkEEE1", Bookmark)
            }),
            ("folderFFFFF1", Folder, {
                ("bookmarkGGG1", Bookmark[age = 5]),
                ("bookmarkHHH1", Bookmark)
            })
        })
    })
    .into_tree()
    .unwrap();
    let expected_telem = StructureCounts {
        remote_revives: 0,
        local_deletes: 0,
        local_revives: 0,
        remote_deletes: 0,
        dupes: 6,
        merged_nodes: 9,
        merged_deletions: 0,
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    assert_eq!(merger.deletions().count(), 0);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn left_pane_root() {
    before_each();

    let local_tree = Tree::with_root(Item::new(ROOT_GUID, Kind::Folder))
        .into_tree()
        .unwrap();

    let remote_tree = nodes!({
        ("folderLEFTPR", Folder[needs_merge = true], {
            ("folderLEFTPQ", Query[needs_merge = true]),
            ("folderLEFTPF", Folder[needs_merge = true], {
                ("folderLEFTPC", Query[needs_merge = true])
            })
        })
    })
    .into_tree()
    .unwrap();

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!(ROOT_GUID, Folder[needs_merge = true])
        .into_tree()
        .unwrap();
    let expected_deletions = vec![
        "folderLEFTPC",
        "folderLEFTPF",
        "folderLEFTPQ",
        "folderLEFTPR",
    ];
    let expected_telem = StructureCounts {
        merged_deletions: 4,
        ..StructureCounts::default()
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    let mut deletions = merger.deletions().map(|d| d.guid).collect::<Vec<_>>();
    deletions.sort();
    assert_eq!(deletions, expected_deletions);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn livemarks() {
    before_each();

    let local_tree = nodes!({
        ("menu________", Folder, {
            ("livemarkAAAA", Livemark),
            ("livemarkBBBB", Folder),
            ("livemarkCCCC", Livemark)
        }),
        ("toolbar_____", Folder, {
            ("livemarkDDDD", Livemark)
        })
    })
    .into_tree()
    .unwrap();

    let remote_tree = nodes!({
        ("menu________", Folder, {
            ("livemarkAAAA", Livemark),
            ("livemarkBBBB", Livemark),
            ("livemarkCCCC", Folder)
        }),
        ("unfiled_____", Folder, {
            ("livemarkEEEE", Livemark)
        })
    })
    .into_tree()
    .unwrap();

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("menu________", Folder[needs_merge = true]),
        ("toolbar_____", Folder[needs_merge = true]),
        ("unfiled_____", Folder[needs_merge = true])
    })
    .into_tree()
    .unwrap();
    let expected_deletions = vec![
        "livemarkAAAA",
        "livemarkBBBB",
        "livemarkCCCC",
        "livemarkDDDD",
        "livemarkEEEE",
    ];
    let expected_telem = StructureCounts {
        merged_nodes: 3,
        // A, B, and C are counted twice, since they exist on both sides.
        merged_deletions: 8,
        ..StructureCounts::default()
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    let mut deletions = merger.deletions().map(|d| d.guid).collect::<Vec<_>>();
    deletions.sort();
    assert_eq!(deletions, expected_deletions);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn non_syncable_items() {
    before_each();

    let local_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            // A is non-syncable remotely, but B doesn't exist remotely, so
            // we'll remove A from the merged structure, and move B to the
            // menu.
            ("folderAAAAAA", Folder[needs_merge = true], {
                ("bookmarkBBBB", Bookmark[needs_merge = true])
            })
        }),
        ("unfiled_____", Folder, {
            // Orphaned left pane queries.
            ("folderLEFTPQ", Query),
            ("folderLEFTPC", Query)
        }),
        ("rootCCCCCCCC", Folder, {
            // Non-syncable local root and children.
            ("folderDDDDDD", Folder, {
                ("bookmarkEEEE", Bookmark)
            }),
            ("bookmarkFFFF", Bookmark)
        })
    })
    .into_tree()
    .unwrap();

    let remote_tree = nodes!({
        ("unfiled_____", Folder[needs_merge = true], {
            // D, J, and G are syncable remotely, but D is non-syncable
            // locally. Since J and G don't exist locally, and are syncable
            // remotely, we'll remove D, and move J and G to unfiled.
            ("folderDDDDDD", Folder[needs_merge = true], {
                ("bookmarkJJJJ", Bookmark[needs_merge = true])
            }),
            ("bookmarkGGGG", Bookmark)
        }),
        ("rootHHHHHHHH", Folder[needs_merge = true], {
            // H is a non-syncable root that only exists remotely. A is
            // non-syncable remotely, and syncable locally. We should
            // remove A and its descendants locally, since its parent
            // H is known to be non-syncable remotely.
            ("folderAAAAAA", Folder, {
                // F exists in two different non-syncable folders: C
                // locally, and A remotely.
                ("bookmarkFFFF", Bookmark),
                ("bookmarkIIII", Bookmark)
            })
        }),
        ("folderLEFTPR", Folder[needs_merge = true], {
            // The complete left pane root. We should remove all left pane
            // queries locally, even though they're syncable, since the left
            // pane root is known to be non-syncable.
            ("folderLEFTPQ", Query[needs_merge = true]),
            ("folderLEFTPF", Folder[needs_merge = true], {
                ("folderLEFTPC", Query[needs_merge = true])
            })
        })
    })
    .into_tree()
    .unwrap();

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!(ROOT_GUID, Folder[needs_merge = true], {
        ("menu________", Folder[needs_merge = true], {
            ("bookmarkBBBB", Bookmark[needs_merge = true])
        }),
        ("unfiled_____", Folder[needs_merge = true], {
            ("bookmarkJJJJ", Bookmark[needs_merge = true]),
            ("bookmarkGGGG", Bookmark)
        })
    })
    .into_tree()
    .unwrap();
    let expected_deletions = vec![
        "bookmarkEEEE", // Non-syncable locally.
        "bookmarkFFFF", // Non-syncable locally.
        "bookmarkIIII", // Non-syncable remotely.
        "folderAAAAAA", // Non-syncable remotely.
        "folderDDDDDD", // Non-syncable locally.
        "folderLEFTPC", // Non-syncable remotely.
        "folderLEFTPF", // Non-syncable remotely.
        "folderLEFTPQ", // Non-syncable remotely.
        "folderLEFTPR", // Non-syncable remotely.
        "rootCCCCCCCC", // Non-syncable locally.
        "rootHHHHHHHH", // Non-syncable remotely.
    ];
    let expected_telem = StructureCounts {
        merged_nodes: 5,
        merged_deletions: 16,
        ..StructureCounts::default()
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    let mut deletions = merger.deletions().map(|d| d.guid).collect::<Vec<_>>();
    deletions.sort();
    assert_eq!(deletions, expected_deletions);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn applying_two_empty_folders_doesnt_smush() {
    before_each();

    let local_tree = Tree::with_root(Item::new(ROOT_GUID, Kind::Folder))
        .into_tree()
        .unwrap();

    let remote_tree = nodes!({
        ("mobile______", Folder[needs_merge = true], {
            ("emptyempty01", Folder[needs_merge = true]),
            ("emptyempty02", Folder[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("mobile______", Folder, {
            ("emptyempty01", Folder),
            ("emptyempty02", Folder)
        })
    })
    .into_tree()
    .unwrap();
    let expected_telem = StructureCounts {
        merged_nodes: 3,
        ..StructureCounts::default()
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    assert_eq!(merger.deletions().count(), 0);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn applying_two_empty_folders_matches_only_one() {
    before_each();

    let local_tree = nodes!({
        ("mobile______", Folder[needs_merge = true], {
            ("emptyempty02", Folder[needs_merge = true]),
            ("emptyemptyL0", Folder[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    let mut new_local_contents: HashMap<Guid, Content> = HashMap::new();
    new_local_contents.insert(
        "emptyempty02".into(),
        Content::Folder {
            title: "Empty".into(),
        },
    );
    new_local_contents.insert(
        "emptyemptyL0".into(),
        Content::Folder {
            title: "Empty".into(),
        },
    );

    let remote_tree = nodes!({
        ("mobile______", Folder[needs_merge = true], {
            ("emptyempty01", Folder[needs_merge = true]),
            ("emptyempty02", Folder[needs_merge = true]),
            ("emptyempty03", Folder[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    let mut new_remote_contents: HashMap<Guid, Content> = HashMap::new();
    new_remote_contents.insert(
        "emptyempty01".into(),
        Content::Folder {
            title: "Empty".into(),
        },
    );
    new_remote_contents.insert(
        "emptyempty02".into(),
        Content::Folder {
            title: "Empty".into(),
        },
    );
    new_remote_contents.insert(
        "emptyempty03".into(),
        Content::Folder {
            title: "Empty".into(),
        },
    );

    let mut merger = Merger::with_contents(
        &local_tree,
        &new_local_contents,
        &remote_tree,
        &new_remote_contents,
    );
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("mobile______", Folder[needs_merge = true], {
            ("emptyempty01", Folder),
            ("emptyempty02", Folder),
            ("emptyempty03", Folder)
        })
    })
    .into_tree()
    .unwrap();
    let expected_telem = StructureCounts {
        remote_revives: 0,
        local_deletes: 0,
        local_revives: 0,
        remote_deletes: 0,
        dupes: 1,
        merged_nodes: 4,
        merged_deletions: 0,
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    assert_eq!(merger.deletions().count(), 0);

    assert_eq!(merger.counts(), &expected_telem);
}

// Bug 747699: we should follow the hierarchy when merging, instead of
// deduping by parent title.
#[test]
fn deduping_ignores_parent_title() {
    before_each();

    let local_tree = nodes!({
        ("mobile______", Folder[needs_merge = true], {
            ("bookmarkAAA1", Bookmark[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    let mut new_local_contents: HashMap<Guid, Content> = HashMap::new();
    new_local_contents.insert(
        "mobile______".into(),
        Content::Folder {
            title: "Favoritos do celular".into(),
        },
    );
    new_local_contents.insert(
        "bookmarkAAA1".into(),
        Content::Bookmark {
            title: "A".into(),
            url_href: "http://example.com/a".into(),
        },
    );

    let remote_tree = nodes!({
        ("mobile______", Folder[needs_merge = true], {
            ("bookmarkAAAA", Bookmark[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    let mut new_remote_contents: HashMap<Guid, Content> = HashMap::new();
    new_remote_contents.insert(
        "mobile______".into(),
        Content::Folder {
            title: "Mobile Bookmarks".into(),
        },
    );
    new_remote_contents.insert(
        "bookmarkAAAA".into(),
        Content::Bookmark {
            title: "A".into(),
            url_href: "http://example.com/a".into(),
        },
    );

    let mut merger = Merger::with_contents(
        &local_tree,
        &new_local_contents,
        &remote_tree,
        &new_remote_contents,
    );
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("mobile______", Folder[needs_merge = true], {
            ("bookmarkAAAA", Bookmark)
        })
    })
    .into_tree()
    .unwrap();
    let expected_telem = StructureCounts {
        remote_revives: 0,
        local_deletes: 0,
        local_revives: 0,
        remote_deletes: 0,
        dupes: 1,
        merged_nodes: 2,
        merged_deletions: 0,
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    assert_eq!(merger.deletions().count(), 0);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn mismatched_compatible_bookmark_kinds() {
    before_each();

    let local_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("queryAAAAAAA", Query[needs_merge = true]),
            ("bookmarkBBBB", Bookmark[needs_merge = true, age = 5])
        })
    })
    .into_tree()
    .unwrap();

    let remote_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("queryAAAAAAA", Bookmark[needs_merge = true, age = 5]),
            ("bookmarkBBBB", Query[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("queryAAAAAAA", Query[needs_merge = true]),
            ("bookmarkBBBB", Query)
        })
    })
    .into_tree()
    .unwrap();
    let expected_telem = StructureCounts {
        merged_nodes: 3,
        ..StructureCounts::default()
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    assert_eq!(merger.deletions().count(), 0);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn mismatched_incompatible_bookmark_kinds() {
    before_each();

    let local_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("bookmarkAAAA", Bookmark[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();

    let remote_tree = nodes!({
        ("menu________", Folder[needs_merge = true], {
            ("bookmarkAAAA", Folder[needs_merge = true, age = 5])
        })
    })
    .into_tree()
    .unwrap();

    let mut merger = Merger::new(&local_tree, &remote_tree);
    match merger.merge() {
        Ok(_) => panic!("Should not merge trees with mismatched kinds"),
        Err(err) => {
            match err.kind() {
                ErrorKind::MismatchedItemKind { .. } => {}
                kind => panic!("Got {:?} merging trees with mismatched kinds", kind),
            };
        }
    };
}

#[test]
fn invalid_guids() {
    before_each();

    #[derive(Default)]
    struct GenerateNewGuid(Cell<usize>);

    impl Driver for GenerateNewGuid {
        fn generate_new_guid(&self, old_guid: &Guid) -> Result<Guid> {
            let count = self.0.get();
            self.0.set(count + 1);
            assert!(
                &[")(*&", "shortGUID", "loooooongGUID", "!@#$%^", "",].contains(&old_guid.as_str()),
                "Didn't expect to generate new GUID for {}",
                old_guid
            );
            Ok(format!("item{:0>8}", count).into())
        }
    }

    let local_tree = nodes!({
        ("toolbar_____", Folder[needs_merge = true, age = 5], {
            ("bookmarkAAAA", Bookmark[needs_merge = true, age = 5]),
            ("bookmarkBBBB", Bookmark[needs_merge = true, age = 5]),
            (")(*&", Bookmark[needs_merge = true, age = 5])
        }),
        ("menu________", Folder[needs_merge = true], {
            ("shortGUID", Bookmark[needs_merge = true]),
            ("loooooongGUID", Bookmark[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    let new_local_contents: HashMap<Guid, Content> = HashMap::new();

    let remote_tree = nodes!({
        ("toolbar_____", Folder[needs_merge = true, age = 5], {
            ("!@#$%^", Bookmark[needs_merge = true, age = 5]),
            ("shortGUID", Bookmark[needs_merge = true, age = 5]),
            ("", Bookmark[needs_merge = true, age = 5]),
            ("loooooongGUID", Bookmark[needs_merge = true, age = 5])
        }),
        ("menu________", Folder[needs_merge = true], {
            ("bookmarkAAAA", Bookmark[needs_merge = true]),
            ("bookmarkBBBB", Bookmark[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    let new_remote_contents: HashMap<Guid, Content> = HashMap::new();

    let driver = GenerateNewGuid::default();
    let mut merger = Merger::with_driver(
        &driver,
        &DefaultAbortSignal,
        &local_tree,
        &new_local_contents,
        &remote_tree,
        &new_remote_contents,
    );
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("toolbar_____", Folder[needs_merge = true, age = 5], {
            ("item00000000", Bookmark[needs_merge = true, age = 5]),
            ("item00000001", Bookmark[needs_merge = true, age = 5]),
            ("item00000002", Bookmark[needs_merge = true, age = 5])
        }),
        ("menu________", Folder[needs_merge = true], {
            ("bookmarkAAAA", Bookmark),
            ("bookmarkBBBB", Bookmark),
            ("item00000003", Bookmark[needs_merge = true]),
            ("item00000004", Bookmark[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    let expected_deletions = vec!["", "!@#$%^", "loooooongGUID", "shortGUID"];
    let expected_telem = StructureCounts {
        merged_nodes: 9,
        merged_deletions: 4,
        ..StructureCounts::default()
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    let mut deletions = merger.deletions().map(|d| d.guid).collect::<Vec<_>>();
    deletions.sort();
    assert_eq!(deletions, expected_deletions);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn multiple_parents() {
    before_each();

    let local_tree = Tree::with_root(Item::new(ROOT_GUID, Kind::Folder))
        .into_tree()
        .unwrap();

    let remote_tree = nodes!({
        ("toolbar_____", Folder[age = 5], {
            ("bookmarkAAAA", Bookmark),
            ("bookmarkBBBB", Bookmark),
            ("folderCCCCCC", Folder, {
                ("bookmarkDDDD", Bookmark),
                ("bookmarkEEEE", Bookmark),
                ("bookmarkFFFF", Bookmark)
            })
        }),
        ("menu________", Folder, {
            ("bookmarkGGGG", Bookmark),
            ("bookmarkAAAA", Bookmark),
            ("folderCCCCCC", Folder, {
                ("bookmarkHHHH", Bookmark),
                ("bookmarkDDDD", Bookmark)
            })
        })
    })
    .into_tree()
    .unwrap();

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("toolbar_____", Folder[age = 5, needs_merge = true], {
            ("bookmarkBBBB", Bookmark)
        }),
        ("menu________", Folder[needs_merge = true], {
            ("bookmarkGGGG", Bookmark),
            ("bookmarkAAAA", Bookmark[needs_merge = true]),
            ("folderCCCCCC", Folder[needs_merge = true], {
                ("bookmarkDDDD", Bookmark[needs_merge = true]),
                ("bookmarkEEEE", Bookmark),
                ("bookmarkFFFF", Bookmark),
                ("bookmarkHHHH", Bookmark)
            })
        })
    })
    .into_tree()
    .unwrap();
    let expected_telem = StructureCounts {
        merged_nodes: 10,
        ..StructureCounts::default()
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    assert_eq!(merger.deletions().count(), 0);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn reparent_orphans() {
    before_each();

    let local_tree = nodes!({
        ("toolbar_____", Folder, {
            ("bookmarkAAAA", Bookmark),
            ("bookmarkBBBB", Bookmark)
        }),
        ("unfiled_____", Folder, {
            ("bookmarkCCCC", Bookmark)
        })
    })
    .into_tree()
    .unwrap();

    let mut remote_tree_builder: Builder = nodes!({
        ("toolbar_____", Folder[needs_merge = true], {
            ("bookmarkBBBB", Bookmark),
            ("bookmarkAAAA", Bookmark)
        }),
        ("unfiled_____", Folder[needs_merge = true], {
            ("bookmarkDDDD", Bookmark[needs_merge = true]),
            ("bookmarkCCCC", Bookmark)
        })
    })
    .try_into()
    .unwrap();
    remote_tree_builder
        .item(Item {
            guid: "bookmarkEEEE".into(),
            kind: Kind::Bookmark,
            age: 0,
            needs_merge: true,
            validity: Validity::Valid,
        })
        .and_then(|p| p.by_parent_guid("toolbar_____".into()))
        .expect("Should insert orphan E");
    remote_tree_builder
        .item(Item {
            guid: "bookmarkFFFF".into(),
            kind: Kind::Bookmark,
            age: 0,
            needs_merge: true,
            validity: Validity::Valid,
        })
        .and_then(|p| p.by_parent_guid("nonexistent".into()))
        .expect("Should insert orphan F");
    let remote_tree = remote_tree_builder.into_tree().unwrap();

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("toolbar_____", Folder[needs_merge = true], {
            ("bookmarkBBBB", Bookmark),
            ("bookmarkAAAA", Bookmark),
            ("bookmarkEEEE", Bookmark[needs_merge = true])
        }),
        ("unfiled_____", Folder[needs_merge = true], {
            ("bookmarkDDDD", Bookmark),
            ("bookmarkCCCC", Bookmark),
            ("bookmarkFFFF", Bookmark[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    let expected_telem = StructureCounts {
        merged_nodes: 8,
        ..StructureCounts::default()
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    assert_eq!(merger.deletions().count(), 0);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn deleted_user_content_roots() {
    before_each();

    let mut local_tree = nodes!({
        ("unfiled_____", Folder[needs_merge = true], {
            ("bookmarkAAAA", Bookmark[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    local_tree.note_deleted("mobile______".into());
    local_tree.note_deleted("toolbar_____".into());

    let mut remote_tree = nodes!({
        ("mobile______", Folder[needs_merge = true], {
            ("bookmarkBBBB", Bookmark[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    remote_tree.note_deleted("unfiled_____".into());
    remote_tree.note_deleted("toolbar_____".into());

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("unfiled_____", Folder[needs_merge = true], {
            ("bookmarkAAAA", Bookmark[needs_merge = true])
        }),
        ("mobile______", Folder, {
            ("bookmarkBBBB", Bookmark)
        })
    })
    .into_tree()
    .unwrap();
    let expected_telem = StructureCounts {
        remote_revives: 0,
        local_deletes: 0,
        local_revives: 0,
        remote_deletes: 0,
        dupes: 0,
        merged_nodes: 4,
        merged_deletions: 1,
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    // TODO(lina): Remove invalid tombstones from both sides.
    let deletions = merger.deletions().map(|d| d.guid).collect::<Vec<_>>();
    assert_eq!(deletions, vec![Into::<Guid>::into("toolbar_____")]);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn moved_user_content_roots() {
    before_each();

    let local_tree = nodes!({
        ("unfiled_____", Folder[needs_merge = true], {
            ("bookmarkAAAA", Bookmark[needs_merge = true]),
            ("menu________", Folder[needs_merge = true], {
                ("bookmarkBBBB", Bookmark[needs_merge = true]),
                ("folderCCCCCC", Folder, {
                    ("bookmarkDDDD", Bookmark),
                    ("toolbar_____", Folder, {
                        ("bookmarkEEEE", Bookmark)
                    })
                })
            })
        }),
        ("mobile______", Folder, {
            ("bookmarkFFFF", Bookmark)
        })
    })
    .into_tree()
    .unwrap();

    let remote_tree = nodes!({
        ("mobile______", Folder[needs_merge = true], {
            ("toolbar_____", Folder[needs_merge = true], {
                ("bookmarkGGGG", Bookmark[needs_merge = true]),
                ("bookmarkEEEE", Bookmark)
            })
        }),
        ("menu________", Folder[needs_merge = true], {
            ("bookmarkHHHH", Bookmark[needs_merge = true]),
            ("unfiled_____", Folder[needs_merge = true], {
                ("bookmarkIIII", Bookmark[needs_merge = true])
            })
        })
    })
    .into_tree()
    .unwrap();

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("unfiled_____", Folder[needs_merge = true], {
            ("bookmarkIIII", Bookmark),
            ("bookmarkAAAA", Bookmark[needs_merge = true])
        }),
        ("mobile______", Folder[needs_merge = true], {
            ("bookmarkFFFF", Bookmark[needs_merge = true])
        }),
        ("menu________", Folder[needs_merge = true], {
           ("bookmarkHHHH", Bookmark),
           ("bookmarkBBBB", Bookmark[needs_merge = true]),
           ("folderCCCCCC", Folder[needs_merge = true], {
               ("bookmarkDDDD", Bookmark[needs_merge = true])
           })
        }),
        ("toolbar_____", Folder[needs_merge = true], {
            ("bookmarkGGGG", Bookmark),
            ("bookmarkEEEE", Bookmark)
        })
    })
    .into_tree()
    .unwrap();
    let expected_telem = StructureCounts {
        merged_nodes: 13,
        ..StructureCounts::default()
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    assert_eq!(merger.deletions().count(), 0);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn cycle() {
    before_each();

    // Try to create a cycle: move A into B, and B into the menu, but keep
    // B's parent by children as A.
    let mut b: Builder = nodes!({ ("menu________", Folder) }).try_into().unwrap();

    b.item(Item::new("folderAAAAAA".into(), Kind::Folder))
        .and_then(|p| p.by_parent_guid("folderBBBBBB".into()))
        .expect("Should insert A");

    b.item(Item::new("folderBBBBBB".into(), Kind::Folder))
        .and_then(|p| p.by_parent_guid("menu________".into()))
        .and_then(|b| {
            b.parent_for(&"folderBBBBBB".into())
                .by_children(&"folderAAAAAA".into())
        })
        .expect("Should insert B");

    match b
        .into_tree()
        .expect_err("Should not build tree with cycles")
        .kind()
    {
        ErrorKind::Cycle(guid) => assert_eq!(guid, &Guid::from("folderAAAAAA")),
        err => panic!("Wrong error kind for cycle: {:?}", err),
    }
}

#[test]
fn reupload_replace() {
    before_each();

    let mut local_tree = nodes!({
        ("menu________", Folder, {
            ("bookmarkAAAA", Bookmark)
        }),
        ("toolbar_____", Folder, {
            ("folderBBBBBB", Folder, {
                ("bookmarkCCCC", Bookmark[validity = Validity::Replace])
            }),
            ("folderDDDDDD", Folder, {
                ("bookmarkEEEE", Bookmark[validity = Validity::Replace])
            })
        }),
        ("unfiled_____", Folder),
        ("mobile______", Folder, {
            ("bookmarkFFFF", Bookmark[validity = Validity::Replace]),
            ("folderGGGGGG", Folder),
            ("bookmarkHHHH", Bookmark[validity = Validity::Replace])
        })
    })
    .into_tree()
    .unwrap();
    local_tree.note_deleted("bookmarkIIII".into());

    let mut remote_tree = nodes!({
        ("menu________", Folder, {
            ("bookmarkAAAA", Bookmark[validity = Validity::Replace])
        }),
        ("toolbar_____", Folder, {
            ("bookmarkJJJJ", Bookmark[validity = Validity::Replace]),
            ("folderBBBBBB", Folder, {
                ("bookmarkCCCC", Bookmark[validity = Validity::Replace])
            }),
            ("folderDDDDDD", Folder)
        }),
        ("unfiled_____", Folder, {
            ("bookmarkKKKK", Bookmark[validity = Validity::Reupload])
        }),
        ("mobile______", Folder, {
            ("bookmarkFFFF", Bookmark),
            ("folderGGGGGG", Folder, {
                ("bookmarkIIII", Bookmark[validity = Validity::Replace])
            })
        })
    })
    .into_tree()
    .unwrap();
    remote_tree.note_deleted("bookmarkEEEE".into());

    let mut merger = Merger::new(&local_tree, &remote_tree);
    let merged_root = merger.merge().unwrap();
    assert!(merger.subsumes(&local_tree));
    assert!(merger.subsumes(&remote_tree));

    let expected_tree = nodes!({
        ("menu________", Folder, {
            // A is invalid remotely and valid locally, so replace.
            ("bookmarkAAAA", Bookmark[needs_merge = true])
        }),
        // Toolbar has new children.
        ("toolbar_____", Folder[needs_merge = true], {
            // B has new children.
            ("folderBBBBBB", Folder[needs_merge = true]),
            ("folderDDDDDD", Folder)
        }),
        ("unfiled_____", Folder, {
            // K was flagged for reupload.
           ("bookmarkKKKK", Bookmark[needs_merge = true])
        }),
        ("mobile______", Folder, {
            // F is invalid locally, so replace with remote. This isn't
            // possible in Firefox Desktop or Rust Places, where the local
            // tree is always valid, but we handle it for symmetry.
            ("bookmarkFFFF", Bookmark),
            ("folderGGGGGG", Folder[needs_merge = true])
        })
    })
    .into_tree()
    .unwrap();
    let expected_deletions = vec![
        // C is invalid on both sides, so we need to upload a tombstone.
        ("bookmarkCCCC", true),
        // E is invalid locally and deleted remotely, so doesn't need a
        // tombstone.
        ("bookmarkEEEE", false),
        // H is invalid locally and doesn't exist remotely, so doesn't need a
        // tombstone.
        ("bookmarkHHHH", false),
        // I is deleted locally and invalid remotely, so needs a tombstone.
        ("bookmarkIIII", true),
        // J doesn't exist locally and invalid remotely, so needs a tombstone.
        ("bookmarkJJJJ", true),
    ];
    let expected_telem = StructureCounts {
        merged_nodes: 10,
        // C is double-counted: it's deleted on both sides, so
        // `merged_deletions` is 6, even though we only have 5 expected
        // deletions.
        merged_deletions: 6,
        ..StructureCounts::default()
    };

    let merged_tree = merged_root.into_tree().unwrap();
    assert_eq!(merged_tree, expected_tree);

    let mut deletions = merger
        .deletions()
        .map(|d| (d.guid.as_ref(), d.should_upload_tombstone))
        .collect::<Vec<(&str, bool)>>();
    deletions.sort_by(|a, b| a.0.cmp(&b.0));
    assert_eq!(deletions, expected_deletions);

    assert_eq!(merger.counts(), &expected_telem);
}

#[test]
fn problems() {
    let mut problems = Problems::default();

    problems
        .note(&"bookmarkAAAA".into(), Problem::Orphan)
        .note(
            &"menu________".into(),
            Problem::MisparentedRoot(vec![DivergedParent::ByChildren("unfiled_____".into())]),
        )
        .note(&"toolbar_____".into(), Problem::MisparentedRoot(Vec::new()))
        .note(
            &"bookmarkBBBB".into(),
            Problem::DivergedParents(vec![
                DivergedParent::ByChildren("folderCCCCCC".into()),
                DivergedParentGuid::Folder("folderDDDDDD".into()).into(),
            ]),
        )
        .note(
            &"bookmarkEEEE".into(),
            Problem::DivergedParents(vec![
                DivergedParent::ByChildren("folderFFFFFF".into()),
                DivergedParentGuid::NonFolder("bookmarkGGGG".into()).into(),
            ]),
        )
        .note(
            &"bookmarkHHHH".into(),
            Problem::DivergedParents(vec![
                DivergedParent::ByChildren("folderIIIIII".into()),
                DivergedParent::ByChildren("folderJJJJJJ".into()),
                DivergedParentGuid::Missing("folderKKKKKK".into()).into(),
            ]),
        )
        .note(&"bookmarkLLLL".into(), Problem::DivergedParents(Vec::new()));

    let mut summary = problems.summarize().collect::<Vec<_>>();
    summary.sort_by(|a, b| a.guid().cmp(b.guid()));
    assert_eq!(
        summary
            .into_iter()
            .map(|s| s.to_string())
            .collect::<Vec<String>>(),
        &[
            "bookmarkAAAA is an orphan",
            "bookmarkBBBB is in children of folderCCCCCC and has parent folderDDDDDD",
            "bookmarkEEEE is in children of folderFFFFFF and has non-folder parent bookmarkGGGG",
            "bookmarkHHHH is in children of folderIIIIII, is in children of folderJJJJJJ, and has \
             nonexistent parent folderKKKKKK",
            "bookmarkLLLL has diverged parents",
            "menu________ is a user content root, but is in children of unfiled_____",
            "toolbar_____ is a user content root",
        ]
    );
}
