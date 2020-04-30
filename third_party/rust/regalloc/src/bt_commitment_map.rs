#![allow(non_snake_case)]
#![allow(non_camel_case_types)]

//! Backtracking allocator: per-real-register commitment maps

use std::cmp::Ordering;
use std::fmt;

use crate::avl_tree::AVLTree;
use crate::data_structures::{
    cmp_range_frags, RangeFrag, RangeFragIx, SortedRangeFragIxs, TypedIxVec, VirtualRange,
    VirtualRangeIx,
};

//=============================================================================
// Debugging config.  Set all these to `false` for normal operation.

// DEBUGGING: set to true to cross-check the CommitmentMap machinery.
pub const CROSSCHECK_CM: bool = false;

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
#[derive(Clone, Copy)]
pub struct FIxAndVLRIx {
    pub fix: RangeFragIx,
    pub mb_vlrix: Option<VirtualRangeIx>,
}
impl FIxAndVLRIx {
    fn new(fix: RangeFragIx, mb_vlrix: Option<VirtualRangeIx>) -> Self {
        Self { fix, mb_vlrix }
    }
}
impl PartialEq for FIxAndVLRIx {
    fn eq(&self, _other: &Self) -> bool {
        // See comments above.
        panic!("impl PartialEq for FIxAndVLRIx: should never be used");
    }
}
impl PartialOrd for FIxAndVLRIx {
    fn partial_cmp(&self, _other: &Self) -> Option<Ordering> {
        // See comments above.
        panic!("impl PartialOrd for FIxAndVLRIx: should never be used");
    }
}
impl fmt::Debug for FIxAndVLRIx {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        let vlrix_string = match self.mb_vlrix {
            None => "NONE".to_string(),
            Some(vlrix) => format!("{:?}", vlrix),
        };
        write!(fmt, "(FnV {:?} {})", self.fix, vlrix_string)
    }
}

// This indicates the current set of fragments to which some real register is
// currently "committed".  The fragments *must* be non-overlapping.  Hence
// they form a total order, and so they must appear in the vector sorted by
// that order.
//
// Overall this is identical to SortedRangeFragIxs, except extended so that
// each FragIx is tagged with an Option<VirtualRangeIx>.
#[derive(Clone)]
pub struct CommitmentMap {
    pub pairs: Vec<FIxAndVLRIx>,
}
impl fmt::Debug for CommitmentMap {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        self.pairs.fmt(fmt)
    }
}
impl CommitmentMap {
    pub fn show_with_fenv(&self, _fenv: &TypedIxVec<RangeFragIx, RangeFrag>) -> String {
        //let mut frags = TypedIxVec::<RangeFragIx, RangeFrag>::new();
        //for fix in &self.frag_ixs {
        //  frags.push(fenv[*fix]);
        //}
        format!("(CommitmentMap {:?})", &self.pairs)
    }
}

impl CommitmentMap {
    pub fn new() -> Self {
        CommitmentMap { pairs: vec![] }
    }

    fn check(
        &self,
        fenv: &TypedIxVec<RangeFragIx, RangeFrag>,
        vlr_env: &TypedIxVec<VirtualRangeIx, VirtualRange>,
    ) {
        let mut ok = true;
        for i in 1..self.pairs.len() {
            let prev_pair = &self.pairs[i - 1];
            let this_pair = &self.pairs[i - 0];
            let prev_fix = prev_pair.fix;
            let this_fix = this_pair.fix;
            let prev_frag = &fenv[prev_fix];
            let this_frag = &fenv[this_fix];
            // Check in-orderness
            if cmp_range_frags(prev_frag, this_frag) != Some(Ordering::Less) {
                ok = false;
                break;
            }
            // Check that, if this frag specifies an owner VirtualRange, that the
            // VirtualRange lists this frag as one of its components.
            let this_mb_vlrix = this_pair.mb_vlrix;
            if let Some(this_vlrix) = this_mb_vlrix {
                ok = vlr_env[this_vlrix]
                    .sorted_frags
                    .frag_ixs
                    .iter()
                    .any(|fix| *fix == this_fix);
                if !ok {
                    break;
                }
            }
        }
        // Also check the first entry for a correct back-link.
        if ok && self.pairs.len() > 0 {
            let this_pair = &self.pairs[0];
            let this_fix = this_pair.fix;
            let this_mb_vlrix = this_pair.mb_vlrix;
            if let Some(this_vlrix) = this_mb_vlrix {
                ok = vlr_env[this_vlrix]
                    .sorted_frags
                    .frag_ixs
                    .iter()
                    .any(|fix| *fix == this_fix);
            }
        }
        if !ok {
            panic!("CommitmentMap::check: vector not ok");
        }
    }

    pub fn add(
        &mut self,
        to_add_frags: &SortedRangeFragIxs,
        to_add_mb_vlrix: Option<VirtualRangeIx>,
        fenv: &TypedIxVec<RangeFragIx, RangeFrag>,
        vlr_env: &TypedIxVec<VirtualRangeIx, VirtualRange>,
    ) {
        self.check(fenv, vlr_env);
        to_add_frags.check(fenv);
        let pairs_x = &self;
        let frags_y = &to_add_frags;
        let mut ix = 0;
        let mut iy = 0;
        let mut res = Vec::<FIxAndVLRIx>::new();
        while ix < pairs_x.pairs.len() && iy < frags_y.frag_ixs.len() {
            let fx = fenv[pairs_x.pairs[ix].fix];
            let fy = fenv[frags_y.frag_ixs[iy]];
            match cmp_range_frags(&fx, &fy) {
                Some(Ordering::Less) => {
                    res.push(pairs_x.pairs[ix]);
                    ix += 1;
                }
                Some(Ordering::Greater) => {
                    res.push(FIxAndVLRIx::new(frags_y.frag_ixs[iy], to_add_mb_vlrix));
                    iy += 1;
                }
                Some(Ordering::Equal) | None => panic!("CommitmentMap::add: vectors intersect"),
            }
        }
        // At this point, one or the other or both vectors are empty.  Hence
        // it doesn't matter in which order the following two while-loops
        // appear.
        assert!(ix == pairs_x.pairs.len() || iy == frags_y.frag_ixs.len());
        while ix < pairs_x.pairs.len() {
            res.push(pairs_x.pairs[ix]);
            ix += 1;
        }
        while iy < frags_y.frag_ixs.len() {
            res.push(FIxAndVLRIx::new(frags_y.frag_ixs[iy], to_add_mb_vlrix));
            iy += 1;
        }
        self.pairs = res;
        self.check(fenv, vlr_env);
    }

    pub fn del(
        &mut self,
        to_del_frags: &SortedRangeFragIxs,
        fenv: &TypedIxVec<RangeFragIx, RangeFrag>,
        vlr_env: &TypedIxVec<VirtualRangeIx, VirtualRange>,
    ) {
        self.check(fenv, vlr_env);
        to_del_frags.check(fenv);
        let pairs_x = &self;
        let frags_y = &to_del_frags;
        let mut ix = 0;
        let mut iy = 0;
        let mut res = Vec::<FIxAndVLRIx>::new();
        while ix < pairs_x.pairs.len() && iy < frags_y.frag_ixs.len() {
            let fx = fenv[pairs_x.pairs[ix].fix];
            let fy = fenv[frags_y.frag_ixs[iy]];
            match cmp_range_frags(&fx, &fy) {
                Some(Ordering::Less) => {
                    res.push(pairs_x.pairs[ix]);
                    ix += 1;
                }
                Some(Ordering::Equal) => {
                    ix += 1;
                    iy += 1;
                }
                Some(Ordering::Greater) => {
                    iy += 1;
                }
                None => panic!("CommitmentMap::del: partial overlap"),
            }
        }
        assert!(ix == pairs_x.pairs.len() || iy == frags_y.frag_ixs.len());
        // Handle leftovers
        while ix < pairs_x.pairs.len() {
            res.push(pairs_x.pairs[ix]);
            ix += 1;
        }
        self.pairs = res;
        self.check(fenv, vlr_env);
    }
}

//=============================================================================
// Per-real-register commitment maps FAST
//

// This indicates the current set of fragments to which some real register is
// currently "committed".  The fragments *must* be non-overlapping.  Hence
// they form a total order, and so they must appear in the vector sorted by
// that order.
//
// Overall this is identical to SortedRangeFragIxs, except extended so that
// each FragIx is tagged with an Option<VirtualRangeIx>.
pub struct CommitmentMapFAST {
    pub tree: AVLTree<FIxAndVLRIx>,
}
impl fmt::Debug for CommitmentMapFAST {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        let as_vec = self.tree.to_vec();
        as_vec.fmt(fmt)
    }
}

// The custom comparator
fn cmp_tree_entries_for_CommitmentMapFAST(
    e1: FIxAndVLRIx,
    e2: FIxAndVLRIx,
    frag_env: &TypedIxVec<RangeFragIx, RangeFrag>,
) -> Option<Ordering> {
    cmp_range_frags(&frag_env[e1.fix], &frag_env[e2.fix])
}

impl CommitmentMapFAST {
    pub fn new() -> Self {
        // The AVL tree constructor needs a default value for the elements.  It
        // will never be used.  The not-present index value will show as
        // obviously bogus if we ever try to "dereference" any part of it.
        let dflt = FIxAndVLRIx::new(
            RangeFragIx::invalid_value(),
            Some(VirtualRangeIx::invalid_value()),
        );
        Self {
            tree: AVLTree::<FIxAndVLRIx>::new(dflt),
        }
    }

    pub fn add(
        &mut self,
        to_add_frags: &SortedRangeFragIxs,
        to_add_mb_vlrix: Option<VirtualRangeIx>,
        fenv: &TypedIxVec<RangeFragIx, RangeFrag>,
        _vlr_env: &TypedIxVec<VirtualRangeIx, VirtualRange>,
    ) {
        for fix in &to_add_frags.frag_ixs {
            let to_add = FIxAndVLRIx::new(*fix, to_add_mb_vlrix);
            let added = self.tree.insert(
                to_add,
                Some(&|pair1, pair2| cmp_tree_entries_for_CommitmentMapFAST(pair1, pair2, fenv)),
            );
            // If this fails, it means the fragment overlaps one that has already
            // been committed to.  That's a serious error.
            assert!(added);
        }
    }

    pub fn del(
        &mut self,
        to_del_frags: &SortedRangeFragIxs,
        fenv: &TypedIxVec<RangeFragIx, RangeFrag>,
        _vlr_env: &TypedIxVec<VirtualRangeIx, VirtualRange>,
    ) {
        for fix in &to_del_frags.frag_ixs {
            // re None: we don't care what the VLRIx is, since we're deleting by
            // RangeFrags alone.
            let to_del = FIxAndVLRIx::new(*fix, None);
            let deleted = self.tree.delete(
                to_del,
                Some(&|pair1, pair2| cmp_tree_entries_for_CommitmentMapFAST(pair1, pair2, fenv)),
            );
            // If this fails, it means the fragment wasn't already committed to.
            // That's also a serious error.
            assert!(deleted);
        }
    }
}
