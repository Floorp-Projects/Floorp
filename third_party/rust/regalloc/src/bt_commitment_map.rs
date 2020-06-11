#![allow(non_snake_case)]
#![allow(non_camel_case_types)]

//! Backtracking allocator: per-real-register commitment maps

use std::cmp::Ordering;
use std::fmt;

use crate::avl_tree::AVLTree;
use crate::data_structures::{
    cmp_range_frags, RangeFrag, RangeFragIx, SortedRangeFragIxs, SortedRangeFrags, TypedIxVec,
    VirtualRangeIx,
};

//=============================================================================
// Per-real-register commitment maps
//

// Something that pairs a fragment index with the index of the virtual range
// to which this fragment conceptually "belongs", at least for the purposes of
// this commitment map.  Alternatively, the `vlrix` field may be None, which
// indicates that the associated fragment belongs to a real-reg live range and
// is therefore non-evictable.
//
// (A fragment merely denotes a sequence of instruction (points), but within
// the context of a commitment map for a real register, obviously any
// particular fragment can't be part of two different virtual live ranges.)
//
// Note that we don't intend to actually use the PartialOrd methods for
// FIxAndVLRix.  However, they need to exist since we want to construct an
// AVLTree<FIxAndVLRix>, and that requires PartialOrd for its element type.
// For working with such trees we will supply our own comparison function;
// hence PartialOrd here serves only to placate the typechecker.  It should
// never actually be used.
#[derive(Clone)]
pub struct RangeFragAndVLRIx {
    pub frag: RangeFrag,
    pub mb_vlrix: Option<VirtualRangeIx>,
}
impl RangeFragAndVLRIx {
    fn new(frag: RangeFrag, mb_vlrix: Option<VirtualRangeIx>) -> Self {
        Self { frag, mb_vlrix }
    }
}
impl PartialEq for RangeFragAndVLRIx {
    fn eq(&self, _other: &Self) -> bool {
        // See comments above.
        panic!("impl PartialEq for RangeFragAndVLRIx: should never be used");
    }
}
impl PartialOrd for RangeFragAndVLRIx {
    fn partial_cmp(&self, _other: &Self) -> Option<Ordering> {
        // See comments above.
        panic!("impl PartialOrd for RangeFragAndVLRIx: should never be used");
    }
}
impl fmt::Debug for RangeFragAndVLRIx {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        let vlrix_string = match self.mb_vlrix {
            None => "NONE".to_string(),
            Some(vlrix) => format!("{:?}", vlrix),
        };
        write!(fmt, "(FnV {:?} {})", self.frag, vlrix_string)
    }
}

//=============================================================================
// Per-real-register commitment maps
//

// This indicates the current set of fragments to which some real register is
// currently "committed".  The fragments *must* be non-overlapping.  Hence
// they form a total order, and so they must appear in the vector sorted by
// that order.
//
// Overall this is identical to SortedRangeFragIxs, except extended so that
// each FragIx is tagged with an Option<VirtualRangeIx>.
pub struct CommitmentMap {
    pub tree: AVLTree<RangeFragAndVLRIx>,
}
impl fmt::Debug for CommitmentMap {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        let as_vec = self.tree.to_vec();
        as_vec.fmt(fmt)
    }
}

impl CommitmentMap {
    pub fn new() -> Self {
        // The AVL tree constructor needs a default value for the elements.  It
        // will never be used.  The not-present index value will show as
        // obviously bogus if we ever try to "dereference" any part of it.
        let dflt = RangeFragAndVLRIx::new(
            RangeFrag::invalid_value(),
            Some(VirtualRangeIx::invalid_value()),
        );
        Self {
            tree: AVLTree::<RangeFragAndVLRIx>::new(dflt),
        }
    }

    pub fn add(
        &mut self,
        to_add_frags: &SortedRangeFrags,
        to_add_mb_vlrix: Option<VirtualRangeIx>,
    ) {
        for frag in &to_add_frags.frags {
            let to_add = RangeFragAndVLRIx::new(frag.clone(), to_add_mb_vlrix);
            let added = self.tree.insert(
                to_add,
                Some(&|pair1: RangeFragAndVLRIx, pair2: RangeFragAndVLRIx| {
                    cmp_range_frags(&pair1.frag, &pair2.frag)
                }),
            );
            // If this fails, it means the fragment overlaps one that has already
            // been committed to.  That's a serious error.
            assert!(added);
        }
    }

    pub fn add_indirect(
        &mut self,
        to_add_frags: &SortedRangeFragIxs,
        to_add_mb_vlrix: Option<VirtualRangeIx>,
        frag_env: &TypedIxVec<RangeFragIx, RangeFrag>,
    ) {
        for fix in &to_add_frags.frag_ixs {
            let to_add = RangeFragAndVLRIx::new(frag_env[*fix].clone(), to_add_mb_vlrix);
            let added = self.tree.insert(
                to_add,
                Some(&|pair1: RangeFragAndVLRIx, pair2: RangeFragAndVLRIx| {
                    cmp_range_frags(&pair1.frag, &pair2.frag)
                }),
            );
            // If this fails, it means the fragment overlaps one that has already
            // been committed to.  That's a serious error.
            assert!(added);
        }
    }

    pub fn del(&mut self, to_del_frags: &SortedRangeFrags) {
        for frag in &to_del_frags.frags {
            // re None: we don't care what the VLRIx is, since we're deleting by
            // RangeFrags alone.
            let to_del = RangeFragAndVLRIx::new(frag.clone(), None);
            let deleted = self.tree.delete(
                to_del,
                Some(&|pair1: RangeFragAndVLRIx, pair2: RangeFragAndVLRIx| {
                    cmp_range_frags(&pair1.frag, &pair2.frag)
                }),
            );
            // If this fails, it means the fragment wasn't already committed to.
            // That's also a serious error.
            assert!(deleted);
        }
    }
}
