#![allow(non_snake_case)]
#![allow(non_camel_case_types)]

//! Core implementation of the backtracking allocator.

use log::{debug, info, log_enabled, Level};
use smallvec::SmallVec;
use std::fmt;

use crate::analysis_control_flow::InstIxToBlockIxMap;
use crate::analysis_data_flow::{add_raw_reg_vecs_for_insn, does_inst_use_def_or_mod_reg};
use crate::analysis_main::run_analysis;
use crate::avl_tree::{AVLTag, AVLTree, AVL_NULL};
use crate::bt_coalescing_analysis::{do_coalescing_analysis, Hint};
use crate::bt_commitment_map::{CommitmentMap, CommitmentMapFAST, FIxAndVLRIx, CROSSCHECK_CM};
use crate::bt_spillslot_allocator::SpillSlotAllocator;
use crate::bt_vlr_priority_queue::VirtualRangePrioQ;
use crate::data_structures::{
    BlockIx, InstIx, InstPoint, Point, RangeFrag, RangeFragIx, RangeFragKind, RealRange,
    RealRangeIx, RealReg, RealRegUniverse, Reg, RegVecBounds, RegVecs, RegVecsAndBounds, Set,
    SortedRangeFragIxs, SpillCost, SpillSlot, TypedIxVec, VirtualRange, VirtualRangeIx, VirtualReg,
    Writable,
};
use crate::inst_stream::{edit_inst_stream, InstToInsert, InstToInsertAndPoint};
use crate::sparse_set::{SparseSet, SparseSetU};
use crate::union_find::UnionFindEquivClasses;
use crate::{Function, RegAllocError, RegAllocResult};

//=============================================================================
// The per-real-register state
//
// Relevant methods are expected to be parameterised by the same VirtualRange
// env as used in calls to `VirtualRangePrioQ`.

struct PerRealReg {
    // The current committed fragments for this RealReg.
    committed: CommitmentMap,
    committedFAST: CommitmentMapFAST,

    // The set of VirtualRanges which have been assigned to this RealReg.  The
    // union of their frags will be equal to `committed` only if this RealReg
    // has no RealRanges.  If this RealReg does have RealRanges the
    // aforementioned union will be exactly the subset of `committed` not used
    // by the RealRanges.
    vlrixs_assigned: Set<VirtualRangeIx>,
}
impl PerRealReg {
    fn new() -> Self {
        Self {
            committed: CommitmentMap::new(),
            committedFAST: CommitmentMapFAST::new(),
            vlrixs_assigned: Set::<VirtualRangeIx>::empty(),
        }
    }

    #[inline(never)]
    fn add_RealRange(
        &mut self,
        to_add: &RealRange,
        fenv: &TypedIxVec<RangeFragIx, RangeFrag>,
        vlr_env: &TypedIxVec<VirtualRangeIx, VirtualRange>,
    ) {
        // Commit this register to `to_add`, irrevocably.  Don't add it to
        // `vlrixs_assigned` since we will never want to later evict the
        // assignment.
        self.committedFAST
            .add(&to_add.sorted_frags, None, fenv, vlr_env);
        if CROSSCHECK_CM {
            self.committed
                .add(&to_add.sorted_frags, None, fenv, vlr_env);
        }
    }

    #[inline(never)]
    fn add_VirtualRange(
        &mut self,
        to_add_vlrix: VirtualRangeIx,
        fenv: &TypedIxVec<RangeFragIx, RangeFrag>,
        vlr_env: &TypedIxVec<VirtualRangeIx, VirtualRange>,
    ) {
        let to_add_vlr = &vlr_env[to_add_vlrix];
        self.committedFAST
            .add(&to_add_vlr.sorted_frags, Some(to_add_vlrix), fenv, vlr_env);
        if CROSSCHECK_CM {
            self.committed
                .add(&to_add_vlr.sorted_frags, Some(to_add_vlrix), fenv, vlr_env);
        }
        assert!(!self.vlrixs_assigned.contains(to_add_vlrix));
        self.vlrixs_assigned.insert(to_add_vlrix);
    }

    #[inline(never)]
    fn del_VirtualRange(
        &mut self,
        to_del_vlrix: VirtualRangeIx,
        fenv: &TypedIxVec<RangeFragIx, RangeFrag>,
        vlr_env: &TypedIxVec<VirtualRangeIx, VirtualRange>,
    ) {
        // Remove it from `vlrixs_assigned`
        if self.vlrixs_assigned.contains(to_del_vlrix) {
            self.vlrixs_assigned.delete(to_del_vlrix);
        } else {
            panic!("PerRealReg: del_VirtualRange on VR not in vlrixs_assigned");
        }
        // Remove it from `committed`
        let to_del_vlr = &vlr_env[to_del_vlrix];
        self.committedFAST
            .del(&to_del_vlr.sorted_frags, fenv, vlr_env);
        if CROSSCHECK_CM {
            self.committed.del(&to_del_vlr.sorted_frags, fenv, vlr_env);
        }
    }
}

// HELPER FUNCTION
// In order to allocate `would_like_to_add` for this real reg, we will
// need to evict `pairs[pairs_ix]`.  That may or may not be possible,
// given the rules of the game.  If it is possible, update `evict_set` and
// `evict_cost` accordingly, and return `true`.  If it isn't possible,
// return `false`; `evict_set` and `evict_cost` must not be changed.
fn handle_CM_entry(
    evict_set: &mut Set<VirtualRangeIx>,
    evict_cost: &mut SpillCost,
    pairs: &Vec<FIxAndVLRIx>,
    pairs_ix: usize,
    spill_cost_budget: SpillCost,
    do_not_evict: &SparseSetU<[VirtualRangeIx; 16]>,
    vlr_env: &TypedIxVec<VirtualRangeIx, VirtualRange>,
    _who: &'static str,
) -> bool {
    let FIxAndVLRIx {
        fix: _fix_to_evict,
        mb_vlrix: mb_vlrix_to_evict,
    } = pairs[pairs_ix];
    //debug!("handle_CM_entry({}): to_evict = {:?}, set = {:?}",
    //       who, mb_vlrix_to_evict, evict_set);
    // Work through our list of reasons why evicting this frag isn't
    // possible or allowed.
    if mb_vlrix_to_evict.is_none() {
        // This frag has no associated VirtualRangeIx, so it is part of a
        // RealRange, and hence not evictable.
        return false;
    }
    // The `unwrap` is guarded by the previous `if`.
    let vlrix_to_evict = mb_vlrix_to_evict.unwrap();
    if do_not_evict.contains(vlrix_to_evict) {
        // Our caller asks that we do not evict this one.
        return false;
    }
    let vlr_to_evict = &vlr_env[vlrix_to_evict];
    if vlr_to_evict.spill_cost.is_infinite() {
        // This is a spill/reload range, so we can't evict it.
        return false;
    }
    // It's at least plausible.  Check the costs.  Note that because a
    // VirtualRange may contain arbitrarily many RangeFrags, and we're
    // visiting RangeFrags here, the `evict_set` may well already contain
    // `vlrix_to_evict`, in the case where we've already visited a different
    // fragment that "belongs" to `vlrix_to_evict`.  Hence we must be sure
    // not to add it again -- if only as as not to mess up the evict cost
    // calculations.

    if !evict_set.contains(vlrix_to_evict) {
        let mut new_evict_cost = *evict_cost;
        new_evict_cost.add(&vlr_to_evict.spill_cost);
        // Are we still within our spill-cost "budget"?
        if !new_evict_cost.is_less_than(&spill_cost_budget) {
            // No.  Give up.
            return false;
        }
        // We're committing to this entry.  So update the running state.
        evict_set.insert(vlrix_to_evict);
        *evict_cost = new_evict_cost;
    }
    true
}

// HELPER FUNCTION
// For a given RangeFrag, traverse the commitment sub-tree rooted at `root`,
// adding to `running_set` the set of VLRIxs that the frag intersects, and
// adding their spill costs to `running_cost`.  Return false if, for one of
// the 4 reasons documented below, the traversal has been abandoned, and true
// if the search completed successfully.
fn rec_helper(
    // The running state, threaded through the tree traversal.  These accumulate
    // ranges and costs as we traverse the tree.
    running_set: &mut SparseSetU<[VirtualRangeIx; 4]>,
    running_cost: &mut SpillCost,
    // The root of the subtree to search.  This changes as we recurse down.
    root: u32,
    // === All the other args stay constant as we recurse ===
    tree: &AVLTree<FIxAndVLRIx>,
    // The FIxAndVLRIx we want to accommodate, in its components.
    pair_frag: &RangeFrag,
    spill_cost_budget: &SpillCost,
    do_not_evict: &SparseSetU<[VirtualRangeIx; 16]>,
    frag_env: &TypedIxVec<RangeFragIx, RangeFrag>,
    vlr_env: &TypedIxVec<VirtualRangeIx, VirtualRange>,
) -> bool {
    let root_node = &tree.pool[root as usize];
    let root_node_FnV = &root_node.item;
    assert!(root_node.tag != AVLTag::Free);
    let root_frag = &frag_env[root_node_FnV.fix];
    // Now figure out:
    // - whether we need to search the left subtree
    // - whether we need to search the right subtree
    // - whether `pair_frag` overlaps the root of the subtree
    let go_left = pair_frag.first < root_frag.first;
    let go_right = pair_frag.last > root_frag.last;
    let overlaps_root = pair_frag.last >= root_frag.first && pair_frag.first <= root_frag.last;

    // Let's first consider the root node.  If we need it but it's not
    // evictable, we might as well stop now.
    if overlaps_root {
        // This frag has no associated VirtualRangeIx, so it is part of a
        // RealRange, and hence not evictable.
        if root_node_FnV.mb_vlrix.is_none() {
            return false;
        }
        // Maybe this one is a spill range, in which case, it can't be evicted.
        let vlrix_to_evict = root_node_FnV.mb_vlrix.unwrap();
        let vlr_to_evict = &vlr_env[vlrix_to_evict];
        if vlr_to_evict.spill_cost.is_infinite() {
            return false;
        }
        // Check that this range alone doesn't exceed our total spill cost.
        // NB: given the check XXX below, I don't think this is necessary.
        if !vlr_to_evict.spill_cost.is_less_than(spill_cost_budget) {
            return false;
        }
        // Maybe our caller asked us not to evict this one.
        if do_not_evict.contains(vlrix_to_evict) {
            return false;
        }
        // Ok!  We can evict the root node.  Update the running state accordingly.
        // Note that we may be presented with the same VLRIx to evict multiple
        // times, so we must be careful to add the cost of it only once.
        if !running_set.contains(vlrix_to_evict) {
            let mut tmp_cost = *running_cost;
            tmp_cost.add(&vlr_to_evict.spill_cost);
            // See above XXX
            if !tmp_cost.is_less_than(spill_cost_budget) {
                return false;
            }
            *running_cost = tmp_cost;
            running_set.insert(vlrix_to_evict);
        }
    }

    // Now consider contributions from the left subtree.  Whether we visit the
    // left or right subtree first is unimportant.
    let left_root = tree.pool[root as usize].left;
    if go_left && left_root != AVL_NULL {
        let ok_left = rec_helper(
            running_set,
            running_cost,
            left_root,
            tree,
            pair_frag,
            spill_cost_budget,
            do_not_evict,
            frag_env,
            vlr_env,
        );
        if !ok_left {
            return false;
        }
    }

    let right_root = tree.pool[root as usize].right;
    if go_right && right_root != AVL_NULL {
        let ok_right = rec_helper(
            running_set,
            running_cost,
            right_root,
            tree,
            pair_frag,
            spill_cost_budget,
            do_not_evict,
            frag_env,
            vlr_env,
        );
        if !ok_right {
            return false;
        }
    }

    // If we get here, it means that `pair_frag` can be accommodated if we evict
    // all the frags it overlaps in the entire subtree rooted at `root`.
    // Propagate that (good) news upwards.
    //
    // Stay sane ..
    assert!(running_cost.is_finite());
    assert!(running_cost.is_less_than(spill_cost_budget));
    true
}

impl PerRealReg {
    // Find the set of VirtualRangeIxs that would need to be evicted in order to
    // allocate `would_like_to_add` to this register.  Virtual ranges mentioned
    // in `do_not_evict` must not be considered as candidates for eviction.
    // Also returns the total associated spill cost.  That spill cost cannot be
    // infinite.
    //
    // This can fail (return None) for four different reasons:
    //
    // - `would_like_to_add` interferes with a real-register-live-range
    //   commitment, so the register would be unavailable even if we evicted
    //   *all* virtual ranges assigned to it.
    //
    // - `would_like_to_add` interferes with a virtual range which is a spill
    //   range (has infinite cost).  We cannot evict those without risking
    //   non-termination of the overall allocation algorithm.
    //
    // - `would_like_to_add` interferes with a virtual range listed in
    //   `do_not_evict`.  Our caller uses this mechanism when trying to do
    //   coalesing, to avoid the nonsensicality of evicting some part of a
    //   virtual live range group in order to allocate a member of the same
    //   group.
    //
    // - The total spill cost of the candidate set exceeds the spill cost of
    //   `would_like_to_add`.  This means that spilling them would be a net loss
    //   per our cost model.  Note that `would_like_to_add` may have an infinite
    //   spill cost, in which case it will "win" over all other
    //   non-infinite-cost eviction candidates.  This is by design (so as to
    //   guarantee that we can always allocate spill/reload bridges).
    #[inline(never)]
    fn find_Evict_Set_FAST(
        &self,
        would_like_to_add: VirtualRangeIx,
        do_not_evict: &SparseSetU<[VirtualRangeIx; 16]>,
        vlr_env: &TypedIxVec<VirtualRangeIx, VirtualRange>,
        frag_env: &TypedIxVec<RangeFragIx, RangeFrag>,
    ) -> Option<(SparseSetU<[VirtualRangeIx; 4]>, SpillCost)> {
        // Firstly, if the commitment tree is for this reg is empty, we can
        // declare success immediately.
        if self.committedFAST.tree.root == AVL_NULL {
            let evict_set = SparseSetU::<[VirtualRangeIx; 4]>::empty();
            let evict_cost = SpillCost::zero();
            return Some((evict_set, evict_cost));
        }

        // The tree isn't empty, so we will have to do this the hard way: iterate
        // over all fragments in `would_like_to_add` and check them against the
        // tree.

        // Useful constants for the main loop
        let would_like_to_add_vlr = &vlr_env[would_like_to_add];
        let evict_cost_budget = would_like_to_add_vlr.spill_cost;
        // Note that `evict_cost_budget` can be infinite because
        // `would_like_to_add` might be a spill/reload range.

        // The overall evict set and cost so far.  These are updated as we iterate
        // over the fragments that make up `would_like_to_add`.
        let mut running_set = SparseSetU::<[VirtualRangeIx; 4]>::empty();
        let mut running_cost = SpillCost::zero();

        // "wlta" = would like to add
        for wlta_fix in &would_like_to_add_vlr.sorted_frags.frag_ixs {
            //debug!("fESF: considering {:?}", *wlta_fix);
            let wlta_frag = &frag_env[*wlta_fix];
            let wlta_frag_ok = rec_helper(
                &mut running_set,
                &mut running_cost,
                self.committedFAST.tree.root,
                &self.committedFAST.tree,
                &wlta_frag,
                &evict_cost_budget,
                do_not_evict,
                frag_env,
                vlr_env,
            );
            if !wlta_frag_ok {
                return None;
            }
            // And move on to the next fragment.
        }

        // If we got here, it means that `would_like_to_add` can be accommodated \o/
        assert!(running_cost.is_finite());
        assert!(running_cost.is_less_than(&evict_cost_budget));
        Some((running_set, running_cost))
    }

    // This should compute exactly the same results as find_Evict_Set_FAST,
    // using a slow but much-easier-to-get-correct algorithm.  It has been used
    // to debug find_Evict_Set_FAST.
    #[inline(never)]
    fn find_Evict_Set_CROSSCHECK(
        &self,
        would_like_to_add: VirtualRangeIx,
        do_not_evict: &SparseSetU<[VirtualRangeIx; 16]>,
        vlr_env: &TypedIxVec<VirtualRangeIx, VirtualRange>,
        frag_env: &TypedIxVec<RangeFragIx, RangeFrag>,
    ) -> Option<(Set<VirtualRangeIx>, SpillCost)> {
        // Useful constants for the loop
        let would_like_to_add_vlr = &vlr_env[would_like_to_add];
        let evict_cost_budget = would_like_to_add_vlr.spill_cost;
        // Note that `evict_cost_budget` can be infinite because
        // `would_like_to_add` might be a spill/reload range.

        let vr_ip_first = frag_env[would_like_to_add_vlr.sorted_frags.frag_ixs[0]].first;
        let vr_ip_last = frag_env[would_like_to_add_vlr.sorted_frags.frag_ixs
            [would_like_to_add_vlr.sorted_frags.frag_ixs.len() - 1]]
            .last;
        // assert that wlta is in order
        for fix in &would_like_to_add_vlr.sorted_frags.frag_ixs {
            let frag = &frag_env[*fix];
            assert!(vr_ip_first <= frag.first && frag.first <= vr_ip_last);
            assert!(vr_ip_first <= frag.last && frag.last <= vr_ip_last);
        }

        // if the CM is empty, we can always accept the assignment.  Otherwise:

        // State updated by the loop
        let mut evict_set = Set::<VirtualRangeIx>::empty();
        let mut evict_cost = SpillCost::zero();

        if self.committed.pairs.len() > 0 {
            // iterate over all points in wlta (the VR)
            let mut vr_ip = vr_ip_first;
            loop {
                if vr_ip > vr_ip_last {
                    break;
                }

                // Find out any vr entry contains `vr_ip`, if any.  If not, move on.
                let mut found_in_vr = false;
                for fix in &would_like_to_add_vlr.sorted_frags.frag_ixs {
                    let frag = &frag_env[*fix];
                    if frag.first <= vr_ip && vr_ip <= frag.last {
                        found_in_vr = true;
                        break;
                    }
                }
                if !found_in_vr {
                    vr_ip = vr_ip.step();
                    continue; // there can't be any intersection at `vr_ip`
                }

                // Find the cm entry containing `vr_ip`
                let mut pair_ix = 0;
                let mut found = false;
                for pair in &self.committed.pairs {
                    let FIxAndVLRIx { fix, mb_vlrix: _ } = pair;
                    let frag = &frag_env[*fix];
                    if frag.first <= vr_ip && vr_ip <= frag.last {
                        found = true;
                        break;
                    }
                    pair_ix += 1;
                }
                if found {
                    //debug!("findXX: must evict {:?}", &self.committed.pairs[pair_ix]);
                    let evict_possible = handle_CM_entry(
                        &mut evict_set,
                        &mut evict_cost,
                        &self.committed.pairs,
                        pair_ix,
                        evict_cost_budget,
                        &do_not_evict,
                        &vlr_env,
                        "CX",
                    );
                    if !evict_possible {
                        return None;
                    }
                }

                vr_ip = vr_ip.step();
            }
        } // if self.committed.pairs.len() > 0 {

        assert!(evict_cost.is_finite());
        assert!(evict_cost.is_less_than(&evict_cost_budget));
        Some((evict_set, evict_cost))
    }

    #[inline(never)]
    fn find_Evict_Set(
        &self,
        would_like_to_add: VirtualRangeIx,
        do_not_evict: &SparseSetU<[VirtualRangeIx; 16]>,
        vlr_env: &TypedIxVec<VirtualRangeIx, VirtualRange>,
        frag_env: &TypedIxVec<RangeFragIx, RangeFrag>,
    ) -> Option<(SparseSetU<[VirtualRangeIx; 4]>, SpillCost)> {
        //let s1 = format!("{:?}", self.committed);
        //let s2 = format!("{:?}", self.committedFAST);
        //debug!("fESF: self.cm  = {}", s1);
        //debug!("fESF: self.cmF = {}", s2);
        //assert!(s1 == s2);

        let result_fast =
            self.find_Evict_Set_FAST(would_like_to_add, do_not_evict, vlr_env, frag_env);

        if CROSSCHECK_CM {
            let result_crosscheck =
                self.find_Evict_Set_CROSSCHECK(would_like_to_add, do_not_evict, vlr_env, frag_env);
            // Big hack.  Note that this hack depends on <Debug for Set> printing
            // set elements in some fixed sequence that depends only on what is in
            // the set, and not on any other factors (eg, the history of how it
            // was constructed.)
            let str_fast: String = format!("{:?}", result_fast);
            let str_crosscheck: String = format!("{:?}", result_crosscheck);
            if str_fast != str_crosscheck {
                println!(
                    "QQQQ find_Evict_Set: fast {}, crosscheck {}",
                    str_fast, str_crosscheck
                );
                println!("");
                println!("self.commitments = {:?}", self.committed);
                println!("");
                println!("wlta = {:?}", vlr_env[would_like_to_add]);
                println!("");
                println!("");
                panic!("find_Evict_Set: crosscheck failed");
            }
        }

        result_fast
    }

    #[inline(never)]
    fn show1_with_envs(&self, fenv: &TypedIxVec<RangeFragIx, RangeFrag>) -> String {
        "in_use:   ".to_string() + &self.committed.show_with_fenv(&fenv)
    }
    #[inline(never)]
    fn show2_with_envs(&self, _fenv: &TypedIxVec<RangeFragIx, RangeFrag>) -> String {
        "assigned: ".to_string() + &format!("{:?}", &self.vlrixs_assigned)
    }
}

//=============================================================================
// Printing the allocator's top level state

#[inline(never)]
fn print_RA_state(
    who: &str,
    universe: &RealRegUniverse,
    // State components
    prioQ: &VirtualRangePrioQ,
    perRealReg: &Vec<PerRealReg>,
    edit_list_move: &Vec<EditListItem>,
    edit_list_other: &Vec<EditListItem>,
    // The context (environment)
    vlr_env: &TypedIxVec<VirtualRangeIx, VirtualRange>,
    frag_env: &TypedIxVec<RangeFragIx, RangeFrag>,
) {
    debug!("<<<<====---- RA state at '{}' ----====", who);
    for ix in 0..perRealReg.len() {
        if !&perRealReg[ix].committed.pairs.is_empty() {
            debug!(
                "{:<5}  {}",
                universe.regs[ix].1,
                &perRealReg[ix].show1_with_envs(&frag_env)
            );
            debug!("       {}", &perRealReg[ix].show2_with_envs(&frag_env));
            debug!("");
        }
    }
    if !prioQ.is_empty() {
        for s in prioQ.show_with_envs(&vlr_env) {
            debug!("{}", s);
        }
    }
    for eli in edit_list_move {
        debug!("ELI MOVE: {:?}", eli);
    }
    for eli in edit_list_other {
        debug!("ELI other: {:?}", eli);
    }
    debug!(">>>>");
}

//=============================================================================
// Frag compression

// Does `frag1` describe some range of instructions that is followed
// immediately by `frag2` ?  Note that this assumes (and checks) that there
// are no spill or reload ranges in play at this point; there should not be.
fn frags_are_mergeable(frag1: &RangeFrag, frag2: &RangeFrag) -> bool {
    assert!(frag1.first.pt.is_use_or_def());
    assert!(frag1.last.pt.is_use_or_def());
    assert!(frag2.first.pt.is_use_or_def());
    assert!(frag2.last.pt.is_use_or_def());

    if frag1.bix != frag2.bix
        && frag1.last.iix.plus(1) == frag2.first.iix
        && frag1.last.pt == Point::Def
        && frag2.first.pt == Point::Use
    {
        assert!(frag1.kind == RangeFragKind::LiveOut || frag1.kind == RangeFragKind::Thru);
        assert!(frag2.kind == RangeFragKind::LiveIn || frag2.kind == RangeFragKind::Thru);
        return true;
    }

    if frag1.last.iix == frag2.first.iix
        && frag1.last.pt == Point::Use
        && frag2.first.pt == Point::Def
    {
        assert!(frag1.kind == RangeFragKind::LiveIn || frag1.kind == RangeFragKind::Local);
        assert!(frag2.kind == RangeFragKind::Local || frag2.kind == RangeFragKind::LiveOut);
        return true;
    }

    false
}

const Z_INVALID_BLOCKIX: BlockIx = BlockIx::invalid_value();
const Z_INVALID_COUNT: u16 = 0xFFFF;

// Try and compress the fragments for each virtual range in `vlr_env`, adding
// new ones to `frag_env` as we go.  Note this is a big kludge, in the sense
// that the new `RangeFrags` have bogus `bix`, `kind` and `count` fields, and
// we rely on the fact that the backtracking core algorithm won't look at them
// after this point.  This should be fixed cleanly.
#[inline(never)]
fn do_vlr_frag_compression(
    vlr_env: &mut TypedIxVec<VirtualRangeIx, VirtualRange>,
    frag_env: &mut TypedIxVec<RangeFragIx, RangeFrag>,
) {
    let mut fragsIN = 0;
    let mut fragsOUT = 0;
    for vlr in vlr_env.iter_mut() {
        let frag_ixs = &mut vlr.sorted_frags.frag_ixs;
        let num_frags = frag_ixs.len();
        fragsIN += num_frags;

        if num_frags == 1 {
            // Nothing we can do.
            fragsOUT += 1;
            continue;
        }

        // BEGIN merge this frag sequence as much as possible
        assert!(num_frags > 1);

        let mut w = 0; // write point, for merged frags
        let mut s = 0; // start point of current group
        let mut e = 0; // end point of current group
        loop {
            if s >= num_frags {
                break;
            }
            while e + 1 < num_frags
                && frags_are_mergeable(&frag_env[frag_ixs[e]], &frag_env[frag_ixs[e + 1]])
            {
                e += 1;
            }
            // s to e inclusive is a maximal group
            // emit (s, e)
            if s == e {
                // Can't compress this one
                frag_ixs[w] = frag_ixs[s];
            } else {
                // Hack, kludge, hack, semantic muddyness, oh la la.  Let's hope
                // nobody looks at these kludged-out fields (bix, kind, count) after
                // this point.
                let zFrag = RangeFrag {
                    bix: Z_INVALID_BLOCKIX,
                    kind: RangeFragKind::Multi,
                    first: frag_env[frag_ixs[s]].first,
                    last: frag_env[frag_ixs[e]].last,
                    count: Z_INVALID_COUNT,
                };
                //print!("Compressed ");
                //for i in s ..= e {
                //  print!("{:?} ", frag_env[frag_ixs[i]]);
                //}
                //println!("  to  {:?}", zFrag);
                frag_env.push(zFrag);
                frag_ixs[w] = RangeFragIx::new(frag_env.len() - 1);
            }
            // move on
            w = w + 1;
            s = e + 1;
            e = s;
        }
        frag_ixs.truncate(w);
        fragsOUT += w;
        // END merge this frag sequence as much as possible
    }
    info!(
        "alloc_main:   compress frags: in {}, out {}",
        fragsIN, fragsOUT
    );
}

//=============================================================================
// Allocator top level

/* (const) For each virtual live range
   - its sorted RangeFrags
   - its total size
   - its spill cost
   - (eventually) its assigned real register
   New VirtualRanges are created as we go, but existing ones are unchanged,
   apart from being tagged with its assigned real register.

   (mut) For each real register
   - the sorted RangeFrags assigned to it (todo: rm the M)
   - the virtual LR indices assigned to it.  This is so we can eject
     a VirtualRange from the commitment, as needed

   (mut) the set of VirtualRanges not yet allocated, prioritised by total size

   (mut) the edit list that is produced
*/
/*
fn show_commit_tab(commit_tab: &Vec::<SortedRangeFragIxs>,
                   who: &str,
                   context: &TypedIxVec::<RangeFragIx, RangeFrag>) -> String {
    let mut res = "Commit Table at '".to_string()
                  + &who.to_string() + &"'\n".to_string();
    let mut rregNo = 0;
    for smf in commit_tab.iter() {
        res += &"  ".to_string();
        res += &mkRealReg(rregNo).show();
        res += &" ".to_string();
        res += &smf.show_with_fenv(&context);
        res += &"\n".to_string();
        rregNo += 1;
    }
    res
}
*/

// VirtualRanges created by spilling all pertain to a single InstIx.  But
// within that InstIx, there are three kinds of "bridges":
#[derive(Clone, Copy, PartialEq)]
pub(crate) enum BridgeKind {
    RtoU, // A bridge for a USE.  This connects the reload to the use.
    RtoS, // a bridge for a MOD.  This connects the reload, the use/def
    // and the spill, since the value must first be reloade, then
    // operated on, and finally re-spilled.
    DtoS, // A bridge for a DEF.  This connects the def to the spill.
}

impl fmt::Debug for BridgeKind {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match self {
            BridgeKind::RtoU => write!(fmt, "R->U"),
            BridgeKind::RtoS => write!(fmt, "R->S"),
            BridgeKind::DtoS => write!(fmt, "D->S"),
        }
    }
}

#[derive(Clone, Copy)]
struct EditListItem {
    // This holds enough information to create a spill or reload instruction,
    // or both, and also specifies where in the instruction stream it/they
    // should be added.  Note that if the edit list as a whole specifies
    // multiple items for the same location, then it is assumed that the order
    // in which they execute isn't important.
    //
    // Some of the relevant info can be found via the VirtualRangeIx link:
    // (1) the real reg involved
    // (2) the place where the insn should go, since the VirtualRange should
    //    only have one RangeFrag, and we can deduce the correct location
    //    from that.
    // Despite (2) we also carry here the InstIx of the affected instruction
    // (there should be only one) since computing it via (2) is expensive.
    // This however gives a redundancy in representation against (2).  Beware!
    slot: SpillSlot,
    vlrix: VirtualRangeIx,
    kind: BridgeKind,
    iix: InstIx,
}

impl fmt::Debug for EditListItem {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(
            fmt,
            "(ELI: at {:?} for {:?} add {:?}, slot={:?})",
            self.iix, self.vlrix, self.kind, self.slot
        )
    }
}

// Allocator top level.  This function returns a result struct that contains
// the final sequence of instructions, possibly with fills/spills/moves
// spliced in and redundant moves elided, and with all virtual registers
// replaced with real registers. Allocation can fail if there are insufficient
// registers to even generate spill/reload code, or if the function appears to
// have any undefined VirtualReg/RealReg uses.

#[inline(never)]
pub fn alloc_main<F: Function>(
    func: &mut F,
    reg_universe: &RealRegUniverse,
    use_checker: bool,
    request_block_annotations: bool,
) -> Result<RegAllocResult<F>, RegAllocError> {
    // -------- Perform initial liveness analysis --------
    // Note that the analysis phase can fail; hence we propagate any error.
    let analysis_info =
        run_analysis(func, reg_universe).map_err(|err| RegAllocError::Analysis(err))?;

    let reg_vecs_and_bounds: RegVecsAndBounds = analysis_info.0;
    let rlr_env: TypedIxVec<RealRangeIx, RealRange> = analysis_info.1;
    let mut vlr_env: TypedIxVec<VirtualRangeIx, VirtualRange> = analysis_info.2;
    let mut frag_env: TypedIxVec<RangeFragIx, RangeFrag> = analysis_info.3;
    let _liveouts: TypedIxVec<BlockIx, SparseSet<Reg>> = analysis_info.4;
    let est_freqs: TypedIxVec<BlockIx, u32> = analysis_info.5;
    let inst_to_block_map: InstIxToBlockIxMap = analysis_info.6;

    assert!(reg_vecs_and_bounds.is_sanitized());

    // Also perform analysis that finds all coalesing opportunities.
    let coalescing_info = do_coalescing_analysis(
        func,
        &reg_vecs_and_bounds,
        &rlr_env,
        &mut vlr_env,
        &frag_env,
        &est_freqs,
        &reg_universe,
    );
    let mut hints: TypedIxVec<VirtualRangeIx, SmallVec<[Hint; 8]>> = coalescing_info.0;
    let vlrEquivClasses: UnionFindEquivClasses<VirtualRangeIx> = coalescing_info.1;
    let is_vv_boundary_move: TypedIxVec<InstIx, bool> = coalescing_info.2;
    let vreg_to_vlrs_map: Vec</*vreg index,*/ SmallVec<[VirtualRangeIx; 3]>> = coalescing_info.3;
    assert!(hints.len() == vlr_env.len());

    // -------- Alloc main --------

    // Create initial state
    info!("alloc_main: begin");
    info!(
        "alloc_main:   in: {} insns in {} blocks",
        func.insns().len(),
        func.blocks().len()
    );
    let num_vlrs_initial = vlr_env.len();
    info!(
        "alloc_main:   in: {} VLRs, {} RLRs",
        num_vlrs_initial,
        rlr_env.len()
    );

    // This is fully populated by the ::new call.
    let mut prioQ = VirtualRangePrioQ::new(&vlr_env);

    // Whereas this is empty.  We have to populate it "by hand", by
    // effectively cloning the allocatable part (prefix) of the universe.
    let mut per_real_reg = Vec::<PerRealReg>::new();
    for _ in 0..reg_universe.allocable {
        // Doing this instead of simply .resize avoids needing Clone for
        // PerRealReg
        per_real_reg.push(PerRealReg::new());
    }
    for rlr in rlr_env.iter() {
        let rregIndex = rlr.rreg.get_index();
        // Ignore RealRanges for RealRegs that are not part of the allocatable
        // set.  As far as the allocator is concerned, such RealRegs simply
        // don't exist.
        if rregIndex >= reg_universe.allocable {
            continue;
        }
        per_real_reg[rregIndex].add_RealRange(&rlr, &frag_env, &vlr_env);
    }

    // Do RangeFrag compression.
    do_vlr_frag_compression(&mut vlr_env, &mut frag_env);

    let mut edit_list_move = Vec::<EditListItem>::new();
    let mut edit_list_other = Vec::<EditListItem>::new();
    if log_enabled!(Level::Debug) {
        debug!("");
        print_RA_state(
            "Initial",
            &reg_universe,
            &prioQ,
            &per_real_reg,
            &edit_list_move,
            &edit_list_other,
            &vlr_env,
            &frag_env,
        );
    }

    // This is also part of the running state.  `vlr_slot_env` tells us the
    // assigned spill slot for each VirtualRange, if any.
    // `spill_slot_allocator` decides on the assignments and writes them into
    // `vlr_slot_env`.
    let mut vlr_slot_env = TypedIxVec::<VirtualRangeIx, Option<SpillSlot>>::new();
    vlr_slot_env.resize(num_vlrs_initial, None);
    let mut spill_slot_allocator = SpillSlotAllocator::new();

    // Main allocation loop.  Each time round, pull out the longest
    // unallocated VirtualRange, and do one of three things:
    //
    // * allocate it to a RealReg, possibly by ejecting some existing
    //   allocation, but only one with a lower spill cost than this one, or
    //
    // * spill it.  This causes the VirtualRange to disappear.  It is replaced
    //   by a set of very short VirtualRanges to carry the spill and reload
    //   values.  Or,
    //
    // * split it.  This causes it to disappear but be replaced by two
    //   VirtualRanges which together constitute the original.
    debug!("");
    debug!("-- MAIN ALLOCATION LOOP (DI means 'direct', CO means 'coalesced'):");

    // A handy constant
    let empty_Set_VirtualRangeIx = SparseSetU::<[VirtualRangeIx; 16]>::empty();

    info!("alloc_main:   main allocation loop: begin");

    // ======== BEGIN Main allocation loop ========
    let mut num_vlrs_processed = 0; // stats only
    let mut num_vlrs_spilled = 0; // stats only
    let mut num_vlrs_evicted = 0; // stats only

    'main_allocation_loop: loop {
        debug!("-- still TODO          {}", prioQ.len());
        if false {
            if log_enabled!(Level::Debug) {
                debug!("");
                print_RA_state(
                    "Loop Top",
                    &reg_universe,
                    &prioQ,
                    &per_real_reg,
                    &edit_list_move,
                    &edit_list_other,
                    &vlr_env,
                    &frag_env,
                );
                debug!("");
            }
        }

        let mb_curr_vlrix = prioQ.get_longest_VirtualRange();
        if mb_curr_vlrix.is_none() {
            break 'main_allocation_loop;
        }

        num_vlrs_processed += 1;
        let curr_vlrix = mb_curr_vlrix.unwrap();
        let curr_vlr = &vlr_env[curr_vlrix];

        debug!("--   considering       {:?}:  {:?}", curr_vlrix, curr_vlr);

        assert!(curr_vlr.vreg.to_reg().is_virtual());
        assert!(curr_vlr.rreg.is_none());
        let curr_vlr_regclass = curr_vlr.vreg.get_class();
        let curr_vlr_rc = curr_vlr_regclass.rc_to_usize();

        // ====== BEGIN Try to do coalescing ======
        //
        // First, look through the hints for `curr_vlr`, collecting up candidate
        // real regs, in decreasing order of preference, in `hinted_regs`.  Note
        // that we don't have to consider the weights here, because the coalescing
        // analysis phase has already sorted the hints for the VLR so as to
        // present the most favoured (weighty) first, so we merely need to retain
        // that ordering when copying into `hinted_regs`.
        assert!(hints.len() == vlr_env.len());
        let mut hinted_regs = SmallVec::<[RealReg; 8]>::new();
        let mut mb_curr_vlr_eclass: Option<SparseSetU<[VirtualRangeIx; 16]>> = None;

        // === BEGIN collect all hints for `curr_vlr` ===
        // `hints` has one entry per VLR, but only for VLRs which existed
        // initially (viz, not for any created by spilling/splitting/whatever).
        // Similarly, `vlrEquivClasses` can only map VLRs that existed initially,
        // and will panic otherwise.  Hence the following check:
        if curr_vlrix.get() < hints.len() {
            for hint in &hints[curr_vlrix] {
                // BEGIN for each hint
                let mb_cand = match hint {
                    Hint::SameAs(other_vlrix, _weight) => {
                        // It wants the same reg as some other VLR, but we can only honour
                        // that if the other VLR actually *has* a reg at this point.  Its
                        // `rreg` field will tell us exactly that.
                        vlr_env[*other_vlrix].rreg
                    }
                    Hint::Exactly(rreg, _weight) => Some(*rreg),
                };
                // So now `mb_cand` might have a preferred real reg.  If so, add it to
                // the list of cands.  De-dup as we go, since that is way cheaper than
                // effectively doing the same via repeated lookups in the
                // CommitmentMaps.
                if let Some(rreg) = mb_cand {
                    if !hinted_regs.iter().any(|r| *r == rreg) {
                        hinted_regs.push(rreg);
                    }
                }
                // END for each hint
            }

            // At this point, we have in `hinted_regs`, the hint candidates that
            // arise from copies between `curr_vlr` and its immediate neighbouring
            // VLRs or RLRs, in order of declining preference.  And that is a good
            // start.  However, it may be the case that there is some other VLR
            // which is in the same equivalence class as `curr_vlr`, but is not a
            // direct neighbour, and which has already been assigned a register.  We
            // really ought to take those into account too, as the least-preferred
            // candidates.  Hence we need to iterate over the equivalence class and
            // "round up the secondary candidates."
            let n_primary_cands = hinted_regs.len();

            // Find the equivalence class set for `curr_vlrix`.  We'll need it
            // later.  Equivalence class info exists for only the "initial"
            // VLRs, though.
            if curr_vlrix.get() < num_vlrs_initial {
                let mut curr_vlr_eclass = SparseSetU::<[VirtualRangeIx; 16]>::empty();
                for vlrix in vlrEquivClasses.equiv_class_elems_iter(curr_vlrix) {
                    curr_vlr_eclass.insert(vlrix);
                }
                assert!(curr_vlr_eclass.contains(curr_vlrix));
                mb_curr_vlr_eclass = Some(curr_vlr_eclass);

                // And work through it to pick up any rreg hints now.
                for vlrix in vlrEquivClasses.equiv_class_elems_iter(curr_vlrix) {
                    if vlrix != curr_vlrix {
                        if let Some(rreg) = vlr_env[vlrix].rreg {
                            // Add `rreg` as a cand, if we don't already have it.
                            if !hinted_regs.iter().any(|r| *r == rreg) {
                                hinted_regs.push(rreg);
                            }
                        }
                    }
                }

                // Sort the secondary cands, so as to try and impose more consistency
                // across the group.  I don't know if this is worthwhile, but it seems
                // sensible.
                hinted_regs[n_primary_cands..].sort_by(|rreg1, rreg2| {
                    rreg1.get_index().partial_cmp(&rreg2.get_index()).unwrap()
                });
            }

            if log_enabled!(Level::Debug) {
                if !hinted_regs.is_empty() {
                    let mut candStr = "pri {".to_string();
                    for (rreg, n) in hinted_regs.iter().zip(0..) {
                        if n == n_primary_cands {
                            candStr = candStr + &" } sec {".to_string();
                        }
                        candStr =
                            candStr + &" ".to_string() + &reg_universe.regs[rreg.get_index()].1;
                    }
                    candStr = candStr + &" }";
                    debug!("--   CO candidates     {}", candStr);
                }
            }
        }
        // === END collect all hints for `curr_vlr` ===

        // === BEGIN try to use the hints for `curr_vlr` ===
        // Now work through the list of preferences, to see if we can honour any
        // of them.
        for rreg in &hinted_regs {
            let rregNo = rreg.get_index();

            // Find the set of ranges which we'd have to evict in order to honour
            // this hint.  In the best case the set will be empty.  In the worst
            // case, we will get None either because (1) it would require evicting a
            // spill range, which is disallowed so as to guarantee termination of
            // the algorithm, or (2) because it would require evicting a real reg
            // live range, which we can't do.
            //
            // We take care not to evict any range which is in the same equivalence
            // class as `curr_vlr` since those are (by definition) connected to
            // `curr_vlr` via V-V copies, and so evicting any of them would be
            // counterproductive from the point of view of removing copies.

            let do_not_evict = if let Some(ref curr_vlr_eclass) = mb_curr_vlr_eclass {
                curr_vlr_eclass
            } else {
                &empty_Set_VirtualRangeIx
            };
            let mb_evict_info: Option<(SparseSetU<[VirtualRangeIx; 4]>, SpillCost)> =
                per_real_reg[rregNo].find_Evict_Set(
                    curr_vlrix,
                    do_not_evict, // these are not to be considered for eviction
                    &vlr_env,
                    &frag_env,
                );
            if let Some((vlrixs_to_evict, total_evict_cost)) = mb_evict_info {
                // Stay sane #1
                assert!(curr_vlr.rreg.is_none());
                // Stay sane #2
                assert!(vlrixs_to_evict.is_empty() == total_evict_cost.is_zero());
                // Can't evict if any in the set are spill ranges
                assert!(total_evict_cost.is_finite());
                // Ensure forward progress
                assert!(total_evict_cost.is_less_than(&curr_vlr.spill_cost));
                // Evict all evictees in the set
                for vlrix_to_evict in vlrixs_to_evict.iter() {
                    // Ensure we're not evicting anything in `curr_vlrix`'s eclass.
                    // This should be guaranteed us by find_Evict_Set.
                    assert!(!do_not_evict.contains(*vlrix_to_evict));
                    // Evict ..
                    debug!(
                        "--   CO evict          {:?}:  {:?}",
                        *vlrix_to_evict, &vlr_env[*vlrix_to_evict]
                    );
                    per_real_reg[rregNo].del_VirtualRange(*vlrix_to_evict, &frag_env, &vlr_env);
                    prioQ.add_VirtualRange(&vlr_env, *vlrix_to_evict);
                    // Directly modify bits of vlr_env.  This means we have to abandon
                    // the immutable borrow for curr_vlr, but that's OK -- we won't need
                    // it again (in this loop iteration).
                    debug_assert!(vlr_env[*vlrix_to_evict].rreg.is_some());
                    vlr_env[*vlrix_to_evict].rreg = None;
                    num_vlrs_evicted += 1;
                }
                // .. and reassign.
                debug!("--   CO alloc to       {}", reg_universe.regs[rregNo].1);
                per_real_reg[rregNo].add_VirtualRange(curr_vlrix, &frag_env, &vlr_env);
                vlr_env[curr_vlrix].rreg = Some(*rreg);
                // We're done!
                continue 'main_allocation_loop;
            }
        } /* for rreg in hinted_regs */
        // === END try to use the hints for `curr_vlr` ===

        // ====== END Try to do coalescing ======

        // We get here if we failed to find a viable assignment by the process of
        // looking at the coalescing hints.
        //
        // So: do almost exactly as we did in the "try for coalescing" stage
        // above.  Except, instead of trying each coalescing candidate
        // individually, iterate over all the registers in the class, to find the
        // one (if any) that has the lowest total evict cost.  If we find one that
        // has zero cost -- that is, we can make the assignment without evicting
        // anything -- then stop the search at that point, since searching further
        // is pointless.

        let (first_in_rc, last_in_rc) = match &reg_universe.allocable_by_class[curr_vlr_rc] {
            &None => {
                return Err(RegAllocError::OutOfRegisters(curr_vlr_regclass));
            }
            &Some(ref info) => (info.first, info.last),
        };

        let mut best_so_far: Option<(
            /*rreg index*/ usize,
            SparseSetU<[VirtualRangeIx; 4]>,
            SpillCost,
        )> = None;

        'search_through_cand_rregs_loop: for rregNo in first_in_rc..last_in_rc + 1 {
            //debug!("--   Cand              {} ...",
            //       reg_universe.regs[rregNo].1);

            let mb_evict_info: Option<(SparseSetU<[VirtualRangeIx; 4]>, SpillCost)> = per_real_reg
                [rregNo]
                .find_Evict_Set(curr_vlrix, &empty_Set_VirtualRangeIx, &vlr_env, &frag_env);
            //
            //match mb_evict_info {
            //  None => debug!("--   Cand              {}: Unavail",
            //                 reg_universe.regs[rregNo].1),
            //  Some((ref evict_set, ref evict_cost)) =>
            //    debug!("--   Cand              {}: Avail, evict cost {:?}, set {:?}",
            //            reg_universe.regs[rregNo].1, evict_cost, evict_set)
            //}
            //
            if let Some((cand_vlrixs_to_evict, cand_total_evict_cost)) = mb_evict_info {
                // Stay sane ..
                assert!(cand_vlrixs_to_evict.is_empty() == cand_total_evict_cost.is_zero());
                // We can't evict if any in the set are spill ranges, and
                // find_Evict_Set should not offer us that possibility.
                assert!(cand_total_evict_cost.is_finite());
                // Ensure forward progress
                assert!(cand_total_evict_cost.is_less_than(&curr_vlr.spill_cost));
                // Update the "best so far".  First, if the evict set is empty, then
                // the candidate is by definition better than all others, and we will
                // quit searching just below.
                let mut cand_is_better = cand_vlrixs_to_evict.is_empty();
                if !cand_is_better {
                    if let Some((_, _, best_spill_cost)) = best_so_far {
                        // If we've already got a candidate, this one is better if it has
                        // a lower total spill cost.
                        if cand_total_evict_cost.is_less_than(&best_spill_cost) {
                            cand_is_better = true;
                        }
                    } else {
                        // We don't have any candidate so far, so what we just got is
                        // better (than nothing).
                        cand_is_better = true;
                    }
                }
                // Remember the candidate if required.
                let cand_vlrixs_to_evict_is_empty = cand_vlrixs_to_evict.is_empty();
                if cand_is_better {
                    best_so_far = Some((rregNo, cand_vlrixs_to_evict, cand_total_evict_cost));
                }
                // If we've found a no-evictions-necessary candidate, quit searching
                // immediately.  We won't find anything better.
                if cand_vlrixs_to_evict_is_empty {
                    break 'search_through_cand_rregs_loop;
                }
            }
        } // for rregNo in first_in_rc..last_in_rc + 1 {

        // Examine the results of the search.  Did we find any usable candidate?
        if let Some((rregNo, vlrixs_to_evict, total_spill_cost)) = best_so_far {
            // We are still Totally Paranoid (tm)
            // Stay sane #1
            debug_assert!(curr_vlr.rreg.is_none());
            // Can't spill a spill range
            assert!(total_spill_cost.is_finite());
            // Ensure forward progress
            assert!(total_spill_cost.is_less_than(&curr_vlr.spill_cost));
            // Now the same evict-reassign section as with the coalescing logic above.
            // Evict all evictees in the set
            for vlrix_to_evict in vlrixs_to_evict.iter() {
                // Evict ..
                debug!(
                    "--   DI evict          {:?}:  {:?}",
                    *vlrix_to_evict, &vlr_env[*vlrix_to_evict]
                );
                per_real_reg[rregNo].del_VirtualRange(*vlrix_to_evict, &frag_env, &vlr_env);
                prioQ.add_VirtualRange(&vlr_env, *vlrix_to_evict);
                debug_assert!(vlr_env[*vlrix_to_evict].rreg.is_some());
                vlr_env[*vlrix_to_evict].rreg = None;
                num_vlrs_evicted += 1;
            }
            // .. and reassign.
            debug!("--   DI alloc to       {}", reg_universe.regs[rregNo].1);
            per_real_reg[rregNo].add_VirtualRange(curr_vlrix, &frag_env, &vlr_env);
            let rreg = reg_universe.regs[rregNo].0;
            vlr_env[curr_vlrix].rreg = Some(rreg);
            // We're done!
            continue 'main_allocation_loop;
        }

        // Still no luck.  We can't find a register to put it in, so we'll
        // have to spill it, since splitting it isn't yet implemented.
        debug!("--   spill");

        // If the live range already pertains to a spill or restore, then
        // it's Game Over.  The constraints (availability of RealRegs vs
        // requirement for them) are impossible to fulfill, and so we cannot
        // generate any valid allocation for this function.
        if curr_vlr.spill_cost.is_infinite() {
            return Err(RegAllocError::OutOfRegisters(curr_vlr_regclass));
        }

        // Generate a new spill slot number, S
        /* Spilling vreg V with virtual live range VirtualRange to slot S:
              for each frag F in VirtualRange {
                 for each insn I in F.first.iix .. F.last.iix {
                    if I does not mention V
                       continue
                    if I mentions V in a Read role {
                       // invar: I cannot mention V in a Mod role
                       add new VirtualRange I.reload -> I.use,
                                            vreg V, spillcost Inf
                       add eli @ I.reload V SpillSlot
                    }
                    if I mentions V in a Mod role {
                       // invar: I cannot mention V in a Read or Write Role
                       add new VirtualRange I.reload -> I.spill,
                                            Vreg V, spillcost Inf
                       add eli @ I.reload V SpillSlot
                       add eli @ I.spill  SpillSlot V
                    }
                    if I mentions V in a Write role {
                       // invar: I cannot mention V in a Mod role
                       add new VirtualRange I.def -> I.spill,
                                            vreg V, spillcost Inf
                       add eli @ I.spill V SpillSlot
                    }
                 }
              }
        */

        // We will be spilling vreg `curr_vlr.reg` with VirtualRange `curr_vlr` to
        // a spill slot that the spill slot allocator will choose for us, just a
        // bit further down this function.

        // This holds enough info to create reload or spill (or both)
        // instructions around an instruction that references a VirtualReg
        // that has been spilled.
        struct SpillAndOrReloadInfo {
            bix: BlockIx,     // that `iix` is in
            iix: InstIx,      // this is the Inst we are spilling/reloading for
            kind: BridgeKind, // says whether to create a spill or reload or both
        }
        let mut sri_vec = Vec::<SpillAndOrReloadInfo>::new();
        let curr_vlr_vreg = curr_vlr.vreg;
        let curr_vlr_reg = curr_vlr_vreg.to_reg();

        for fix in &curr_vlr.sorted_frags.frag_ixs {
            let frag: &RangeFrag = &frag_env[*fix];
            for iix in frag.first.iix.dotdot(frag.last.iix.plus(1)) {
                let (iix_uses_curr_vlr_reg, iix_defs_curr_vlr_reg, iix_mods_curr_vlr_reg) =
                    does_inst_use_def_or_mod_reg(&reg_vecs_and_bounds, iix, curr_vlr_reg);
                // If this insn doesn't mention the vreg we're spilling for,
                // move on.
                if !iix_defs_curr_vlr_reg && !iix_mods_curr_vlr_reg && !iix_uses_curr_vlr_reg {
                    continue;
                }
                // USES: Do we need to create a reload-to-use bridge
                // (VirtualRange) ?
                if iix_uses_curr_vlr_reg && frag.contains(&InstPoint::new_use(iix)) {
                    debug_assert!(!iix_mods_curr_vlr_reg);
                    // Stash enough info that we can create a new VirtualRange
                    // and a new edit list entry for the reload.
                    let bix = inst_to_block_map.map(iix);
                    let sri = SpillAndOrReloadInfo {
                        bix,
                        iix,
                        kind: BridgeKind::RtoU,
                    };
                    sri_vec.push(sri);
                }
                // MODS: Do we need to create a reload-to-spill bridge?  This
                // can only happen for instructions which modify a register.
                // Note this has to be a single VirtualRange, since if it were
                // two (one for the reload, one for the spill) they could
                // later end up being assigned to different RealRegs, which is
                // obviously nonsensical.
                if iix_mods_curr_vlr_reg
                    && frag.contains(&InstPoint::new_use(iix))
                    && frag.contains(&InstPoint::new_def(iix))
                {
                    debug_assert!(!iix_uses_curr_vlr_reg);
                    debug_assert!(!iix_defs_curr_vlr_reg);
                    let bix = inst_to_block_map.map(iix);
                    let sri = SpillAndOrReloadInfo {
                        bix,
                        iix,
                        kind: BridgeKind::RtoS,
                    };
                    sri_vec.push(sri);
                }
                // DEFS: Do we need to create a def-to-spill bridge?
                if iix_defs_curr_vlr_reg && frag.contains(&InstPoint::new_def(iix)) {
                    debug_assert!(!iix_mods_curr_vlr_reg);
                    let bix = inst_to_block_map.map(iix);
                    let sri = SpillAndOrReloadInfo {
                        bix,
                        iix,
                        kind: BridgeKind::DtoS,
                    };
                    sri_vec.push(sri);
                }
            }
        }

        // Now that we no longer need to access `frag_env` or `vlr_env` for
        // the remainder of this iteration of the main allocation loop, we can
        // actually generate the required spill/reload artefacts.

        // First off, poke the spill slot allocator to get an intelligent choice
        // of slot.  Note that this will fail for "non-initial" VirtualRanges; but
        // the only non-initial ones will have been created by spilling anyway,
        // any we definitely shouldn't be trying to spill them again.  Hence we
        // can assert.
        assert!(vlr_slot_env.len() == num_vlrs_initial);
        assert!(curr_vlrix < VirtualRangeIx::new(num_vlrs_initial));
        if vlr_slot_env[curr_vlrix].is_none() {
            // It hasn't been decided yet.  Cause it to be so by asking for an
            // allocation for the entire eclass that `curr_vlrix` belongs to.
            spill_slot_allocator.alloc_spill_slots(
                &mut vlr_slot_env,
                func,
                &frag_env,
                &vlr_env,
                &vlrEquivClasses,
                curr_vlrix,
            );
            assert!(vlr_slot_env[curr_vlrix].is_some());
        }
        let spill_slot_to_use = vlr_slot_env[curr_vlrix].unwrap();

        for sri in sri_vec {
            // For a spill for a MOD use, the new value will be referenced
            // three times.  For DEF and USE uses, it'll only be ref'd twice.
            // (I think we don't care about metrics for the new RangeFrags,
            // though)
            let new_vlr_count = if sri.kind == BridgeKind::RtoS { 3 } else { 2 };
            let (new_vlr_first_pt, new_vlr_last_pt) = match sri.kind {
                BridgeKind::RtoU => (Point::Reload, Point::Use),
                BridgeKind::RtoS => (Point::Reload, Point::Spill),
                BridgeKind::DtoS => (Point::Def, Point::Spill),
            };
            let new_vlr_frag = RangeFrag {
                bix: sri.bix,
                kind: RangeFragKind::Local,
                first: InstPoint::new(sri.iix, new_vlr_first_pt),
                last: InstPoint::new(sri.iix, new_vlr_last_pt),
                count: new_vlr_count,
            };
            let new_vlr_fix = RangeFragIx::new(frag_env.len() as u32);
            frag_env.push(new_vlr_frag);
            debug!(
                "--     new RangeFrag    {:?}  :=  {:?}",
                &new_vlr_fix, &new_vlr_frag
            );
            let new_vlr_sfixs = SortedRangeFragIxs::unit(new_vlr_fix, &frag_env);
            let new_vlr = VirtualRange {
                vreg: curr_vlr_vreg,
                rreg: None,
                sorted_frags: new_vlr_sfixs,
                size: 1,
                // Effectively infinite.  We'll never look at this again anyway.
                total_cost: 0xFFFF_FFFFu32,
                spill_cost: SpillCost::infinite(),
            };
            let new_vlrix = VirtualRangeIx::new(vlr_env.len() as u32);
            debug!(
                "--     new VirtRange    {:?}  :=  {:?}",
                new_vlrix, &new_vlr
            );
            vlr_env.push(new_vlr);
            prioQ.add_VirtualRange(&vlr_env, new_vlrix);

            // BEGIN (optimisation only) see if we can create any coalescing hints
            // for this new VLR.
            let mut new_vlr_hint = SmallVec::<[Hint; 8]>::new();
            if is_vv_boundary_move[sri.iix] {
                // Collect the src and dst regs for the move.  It must be a
                // move because `is_vv_boundary_move` claims that to be true.
                let im = func.is_move(&func.get_insn(sri.iix));
                assert!(im.is_some());
                let (wdst_reg, src_reg): (Writable<Reg>, Reg) = im.unwrap();
                let dst_reg: Reg = wdst_reg.to_reg();
                assert!(src_reg.is_virtual() && dst_reg.is_virtual());
                let dst_vreg: VirtualReg = dst_reg.to_virtual_reg();
                let src_vreg: VirtualReg = src_reg.to_virtual_reg();
                let bridge_eef = est_freqs[sri.bix];
                match sri.kind {
                    BridgeKind::RtoU => {
                        // Reload-to-Use bridge.  Hint that we want to be
                        // allocated to the same reg as the destination of the
                        // move.  That means we have to find the VLR that owns
                        // the destination vreg.
                        for vlrix in &vreg_to_vlrs_map[dst_vreg.get_index()] {
                            if vlr_env[*vlrix].vreg == dst_vreg {
                                new_vlr_hint.push(Hint::SameAs(*vlrix, bridge_eef));
                                break;
                            }
                        }
                    }
                    BridgeKind::DtoS => {
                        // Def-to-Spill bridge.  Hint that we want to be
                        // allocated to the same reg as the source of the
                        // move.
                        for vlrix in &vreg_to_vlrs_map[src_vreg.get_index()] {
                            if vlr_env[*vlrix].vreg == src_vreg {
                                new_vlr_hint.push(Hint::SameAs(*vlrix, bridge_eef));
                                break;
                            }
                        }
                    }
                    BridgeKind::RtoS => {
                        // A Reload-to-Spill bridge.  This can't happen.  It
                        // implies that the instruction modifies (both reads
                        // and writes) one of its operands, but the client's
                        // `is_move` function claims this instruction is a
                        // plain "move" (reads source, writes dest).  These
                        // claims are mutually contradictory.
                        panic!("RtoS bridge for v-v boundary move");
                    }
                }
            }
            hints.push(new_vlr_hint);
            // END see if we can create any coalescing hints for this new VLR.

            // Finally, create a new EditListItem.  This holds enough
            // information that a suitable spill or reload instruction can
            // later be created.
            let new_eli = EditListItem {
                slot: spill_slot_to_use,
                vlrix: new_vlrix,
                kind: sri.kind,
                iix: sri.iix,
            };
            if is_vv_boundary_move[sri.iix] {
                debug!("--     new ELI MOVE     {:?}", &new_eli);
                edit_list_move.push(new_eli);
            } else {
                debug!("--     new ELI other    {:?}", &new_eli);
                edit_list_other.push(new_eli);
            }
        }

        num_vlrs_spilled += 1;
        // And implicitly "continue 'main_allocation_loop"
    }
    // ======== END Main allocation loop ========

    info!("alloc_main:   main allocation loop: end");

    if log_enabled!(Level::Debug) {
        debug!("");
        print_RA_state(
            "Final",
            &reg_universe,
            &prioQ,
            &per_real_reg,
            &edit_list_move,
            &edit_list_other,
            &vlr_env,
            &frag_env,
        );
    }

    // ======== BEGIN Do spill slot coalescing ========

    debug!("");
    info!("alloc_main:   create spills_n_reloads for MOVE insns");

    // Sort `edit_list_move` by the insn with which each item is associated.
    edit_list_move.sort_unstable_by(|eli1, eli2| eli1.iix.cmp(&eli2.iix));

    // Now go through `edit_list_move` and find pairs which constitute a
    // spillslot-to-the-same-spillslot move.  What we have in `edit_list_move` is
    // heavily constrained, as follows:
    //
    // * each entry should reference an InstIx which the coalescing analysis
    //   identified as a virtual-to-virtual copy which exists at the boundary
    //   between two VirtualRanges.  The "boundary" aspect is important; we
    //   can't coalesce out moves in which the source vreg is not the "last use"
    //   or for which the destination vreg is not the "first def".  (The same is
    //   true for coalescing of unspilled moves).
    //
    // * the each entry must reference a VirtualRange which has only a single
    //   RangeFrag, and that frag must exist entirely "within" the referenced
    //   instruction.  Specifically, it may only be a R->U frag (bridge) or a
    //   D->S frag.
    //
    // * For a referenced instruction, there may be at most two entries in this
    //   list: one that references the R->U frag and one that references the
    //   D->S frag.  Furthermore, the two entries must carry values of the same
    //   RegClass; if that isn't true, the client's `is_move` function is
    //   defective.
    //
    // For any such pair identified, if both frags mention the same spill slot,
    // we skip generating both the reload and the spill instruction.  We also
    // note that the instruction itself is to be deleted (converted to a
    // zero-len nop).  In a sense we have "cancelled out" a reload/spill pair.
    // Entries that can't be cancelled out are handled the same way as for
    // entries in `edit_list_other`, by simply copying them there.
    //
    // Since `edit_list_move` is sorted by insn ix, we can scan linearly over
    // it, looking for adjacent pairs.  We'll have to accept them in either
    // order though (first R->U then D->S, or the other way round).  There's no
    // fixed ordering since there is no particular ordering in the way
    // VirtualRanges are allocated.

    // As a result of spill slot coalescing, we'll need to delete the move
    // instructions leading to a mergable spill slot move.  The insn stream
    // editor can't delete instructions, so instead it'll replace them with zero
    // len nops obtained from the client.  `iixs_to_nop_out` records the insns
    // that this has to happen to.  It is in increasing order of InstIx (because
    // `edit_list_sorted` is), and the indices in it refer to the original
    // virtual-registerised code.
    let mut iixs_to_nop_out = Vec::<InstIx>::new();

    let n_edit_list_move = edit_list_move.len();
    let mut n_edit_list_move_processed = 0; // for assertions only
    let mut i_min = 0;
    loop {
        if i_min >= n_edit_list_move {
            break;
        }
        // Find the bounds of the current group.
        debug!("editlist entry (MOVE): min: {:?}", &edit_list_move[i_min]);
        let i_min_iix = edit_list_move[i_min].iix;
        let mut i_max = i_min;
        while i_max + 1 < n_edit_list_move && edit_list_move[i_max + 1].iix == i_min_iix {
            i_max += 1;
            debug!("editlist entry (MOVE): max: {:?}", &edit_list_move[i_max]);
        }
        // Current group is from i_min to i_max inclusive.  At most 2 entries are
        // allowed per group.
        assert!(i_max - i_min <= 1);
        // Check for a mergeable pair.
        if i_max - i_min == 1 {
            assert!(is_vv_boundary_move[i_min_iix]);
            let vlrix1 = edit_list_move[i_min].vlrix;
            let vlrix2 = edit_list_move[i_max].vlrix;
            assert!(vlrix1 != vlrix2);
            let vlr1 = &vlr_env[vlrix1];
            let vlr2 = &vlr_env[vlrix2];
            let fixs1 = &vlr1.sorted_frags;
            let fixs2 = &vlr2.sorted_frags;
            assert!(fixs1.frag_ixs.len() == 1);
            assert!(fixs2.frag_ixs.len() == 1);
            let frag1 = &frag_env[fixs1.frag_ixs[0]];
            let frag2 = &frag_env[fixs2.frag_ixs[0]];
            assert!(frag1.first.iix == i_min_iix);
            assert!(frag1.last.iix == i_min_iix);
            assert!(frag2.first.iix == i_min_iix);
            assert!(frag2.last.iix == i_min_iix);
            // frag1 must be R->U and frag2 must be D->S, or vice versa
            match (frag1.first.pt, frag1.last.pt, frag2.first.pt, frag2.last.pt) {
                (Point::Reload, Point::Use, Point::Def, Point::Spill)
                | (Point::Def, Point::Spill, Point::Reload, Point::Use) => {
                    let slot1 = edit_list_move[i_min].slot;
                    let slot2 = edit_list_move[i_max].slot;
                    if slot1 == slot2 {
                        // Yay.  We've found a coalescable pair.  We can just ignore the
                        // two entries and move on.  Except we have to mark the insn
                        // itself for deletion.
                        debug!("editlist entry (MOVE): delete {:?}", i_min_iix);
                        iixs_to_nop_out.push(i_min_iix);
                        i_min = i_max + 1;
                        n_edit_list_move_processed += 2;
                        continue;
                    }
                }
                (_, _, _, _) => {
                    panic!("spill slot coalescing, edit_list_move: unexpected frags");
                }
            }
        }
        // If we get here for whatever reason, this group is uninteresting.  Copy
        // in to `edit_list_other` so that it gets processed in the normal way.
        for i in i_min..=i_max {
            edit_list_other.push(edit_list_move[i]);
            n_edit_list_move_processed += 1;
        }
        i_min = i_max + 1;
    }
    assert!(n_edit_list_move_processed == n_edit_list_move);

    // ======== END Do spill slot coalescing ========

    // ======== BEGIN Create all other spills and reloads ========

    debug!("");
    info!("alloc_main:   create spills_n_reloads for other insns");

    // Reload and spill instructions are missing.  To generate them, go through
    // the "edit list", which contains info on both how to generate the
    // instructions, and where to insert them.
    let mut spills_n_reloads = Vec::<InstToInsertAndPoint>::new();
    let mut num_spills = 0; // stats only
    let mut num_reloads = 0; // stats only
    for eli in &edit_list_other {
        debug!("editlist entry (other): {:?}", eli);
        let vlr = &vlr_env[eli.vlrix];
        let vlr_sfrags = &vlr.sorted_frags;
        debug_assert!(vlr.sorted_frags.frag_ixs.len() == 1);
        let vlr_frag = frag_env[vlr_sfrags.frag_ixs[0]];
        let rreg = vlr.rreg.expect("Gen of spill/reload: reg not assigned?!");
        let vreg = vlr.vreg;
        match eli.kind {
            BridgeKind::RtoU => {
                debug_assert!(vlr_frag.first.pt.is_reload());
                debug_assert!(vlr_frag.last.pt.is_use());
                debug_assert!(vlr_frag.first.iix == vlr_frag.last.iix);
                let insnR = InstToInsert::Reload {
                    to_reg: Writable::from_reg(rreg),
                    from_slot: eli.slot,
                    for_vreg: vreg,
                };
                let whereToR = vlr_frag.first;
                spills_n_reloads.push(InstToInsertAndPoint::new(insnR, whereToR));
                num_reloads += 1;
            }
            BridgeKind::RtoS => {
                debug_assert!(vlr_frag.first.pt.is_reload());
                debug_assert!(vlr_frag.last.pt.is_spill());
                debug_assert!(vlr_frag.first.iix == vlr_frag.last.iix);
                let insnR = InstToInsert::Reload {
                    to_reg: Writable::from_reg(rreg),
                    from_slot: eli.slot,
                    for_vreg: vreg,
                };
                let whereToR = vlr_frag.first;
                let insnS = InstToInsert::Spill {
                    to_slot: eli.slot,
                    from_reg: rreg,
                    for_vreg: vreg,
                };
                let whereToS = vlr_frag.last;
                spills_n_reloads.push(InstToInsertAndPoint::new(insnR, whereToR));
                spills_n_reloads.push(InstToInsertAndPoint::new(insnS, whereToS));
                num_reloads += 1;
                num_spills += 1;
            }
            BridgeKind::DtoS => {
                debug_assert!(vlr_frag.first.pt.is_def());
                debug_assert!(vlr_frag.last.pt.is_spill());
                debug_assert!(vlr_frag.first.iix == vlr_frag.last.iix);
                let insnS = InstToInsert::Spill {
                    to_slot: eli.slot,
                    from_reg: rreg,
                    for_vreg: vreg,
                };
                let whereToS = vlr_frag.last;
                spills_n_reloads.push(InstToInsertAndPoint::new(insnS, whereToS));
                num_spills += 1;
            }
        }
    }

    // ======== END Create all other spills and reloads ========

    // ======== BEGIN Create final instruction stream ========

    // Gather up a vector of (RangeFrag, VirtualReg, RealReg) resulting from
    // the previous phase.  This fundamentally is the result of the allocation
    // and tells us how the instruction stream must be edited.  Note it does
    // not take account of spill or reload instructions.  Dealing with those
    // is relatively simple and happens later.

    info!("alloc_main:   create frag_map");

    let mut frag_map = Vec::<(RangeFragIx, VirtualReg, RealReg)>::new();
    // For each real register under our control ..
    for i in 0..reg_universe.allocable {
        let rreg = reg_universe.regs[i].0;
        // .. look at all the VirtualRanges assigned to it.  And for each such
        // VirtualRange ..
        for vlrix_assigned in per_real_reg[i].vlrixs_assigned.iter() {
            let VirtualRange {
                vreg, sorted_frags, ..
            } = &vlr_env[*vlrix_assigned];
            // All the RangeFrags in `vlr_assigned` require `vlr_assigned.reg`
            // to be mapped to the real reg `i`
            // .. collect up all its constituent RangeFrags.
            for fix in &sorted_frags.frag_ixs {
                frag_map.push((*fix, *vreg, rreg));
            }
        }
    }

    info!("alloc_main:   edit_inst_stream");

    let final_insns_and_targetmap__or_err = edit_inst_stream(
        func,
        spills_n_reloads,
        &iixs_to_nop_out,
        frag_map,
        &frag_env,
        &reg_universe,
        use_checker,
    );

    // ======== END Create final instruction stream ========

    // ======== BEGIN Create the RegAllocResult ========

    match final_insns_and_targetmap__or_err {
        Ok((ref final_insns, ..)) => {
            info!(
                "alloc_main:   out: VLRs: {} initially, {} processed",
                num_vlrs_initial, num_vlrs_processed
            );
            info!(
                "alloc_main:   out: VLRs: {} evicted, {} spilled",
                num_vlrs_evicted, num_vlrs_spilled
            );
            info!(
                "alloc_main:   out: insns: {} total, {} spills, {} reloads, {} nopzs",
                final_insns.len(),
                num_spills,
                num_reloads,
                iixs_to_nop_out.len()
            );
            info!(
                "alloc_main:   out: spill slots: {} used",
                spill_slot_allocator.num_slots_in_use()
            );
        }
        Err(_) => {
            info!("alloc_main:   allocation failed!");
        }
    }

    let (final_insns, target_map, orig_insn_map) = match final_insns_and_targetmap__or_err {
        Err(e) => {
            info!("alloc_main: fail");
            return Err(e);
        }
        Ok(pair) => {
            info!("alloc_main:   creating RegAllocResult");
            pair
        }
    };

    // Compute clobbered registers with one final, quick pass.
    //
    // FIXME: derive this information directly from the allocation data
    // structures used above.
    //
    // NB at this point, the `san_reg_uses` that was computed in the analysis
    // phase is no longer valid, because we've added and removed instructions to
    // the function relative to the one that `san_reg_uses` was computed from,
    // so we have to re-visit all insns with `add_raw_reg_vecs_for_insn`.
    // That's inefficient, but we don't care .. this should only be a temporary
    // fix.

    let mut clobbered_registers: Set<RealReg> = Set::empty();

    // We'll dump all the reg uses in here.  We don't care the bounds, so just
    // pass a dummy one in the loop.
    let mut reg_vecs = RegVecs::new(/*sanitized=*/ false);
    let mut dummy_bounds = RegVecBounds::new();
    for insn in &final_insns {
        add_raw_reg_vecs_for_insn::<F>(insn, &mut reg_vecs, &mut dummy_bounds);
    }
    for reg in reg_vecs.defs.iter().chain(reg_vecs.mods.iter()) {
        assert!(reg.is_real());
        clobbered_registers.insert(reg.to_real_reg());
    }

    // And now remove from the set, all those not available to the allocator.
    // But not removing the reserved regs, since we might have modified those.
    clobbered_registers.filter_map(|&reg| {
        if reg.get_index() >= reg_universe.allocable {
            None
        } else {
            Some(reg)
        }
    });

    assert!(est_freqs.len() as usize == func.blocks().len());
    let mut block_annotations = None;
    if request_block_annotations {
        let mut anns = TypedIxVec::<BlockIx, Vec<String>>::new();
        for (estFreq, i) in est_freqs.iter().zip(0..) {
            let bix = BlockIx::new(i);
            let ef_str = format!("RA: bix {:?}, estFreq {}", bix, estFreq);
            anns.push(vec![ef_str]);
        }
        block_annotations = Some(anns);
    }

    let ra_res = RegAllocResult {
        insns: final_insns,
        target_map,
        orig_insn_map,
        clobbered_registers,
        num_spill_slots: spill_slot_allocator.num_slots_in_use() as u32,
        block_annotations,
    };

    info!("alloc_main: end");

    // ======== END Create the RegAllocResult ========

    Ok(ra_res)
}
