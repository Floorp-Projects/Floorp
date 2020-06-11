#![allow(non_snake_case)]
#![allow(non_camel_case_types)]

//! Allocation of spill slots for the backtracking allocator.

use log::debug;
use std::cmp::Ordering;

use crate::avl_tree::{AVLTree, AVL_NULL};
use crate::data_structures::{
    cmp_range_frags, RangeFrag, RangeFragIx, SortedRangeFragIxs, SpillSlot, TypedIxVec,
    VirtualRange, VirtualRangeIx,
};
use crate::union_find::UnionFindEquivClasses;
use crate::Function;

//=============================================================================
// A spill slot allocator.  This could be implemented more simply than it is.
// The reason for the extra complexity is to support copy-coalescing at the
// spill-slot level.  That is, it tries make it possible to allocate all
// members of a VirtualRange group to the same spill slot, so that moves
// between two spilled members of the same group can be turned into no-ops.
//
// All of the `size` metrics in this bit are in terms of "logical spill slot
// units", per the interface's description for `get_spillslot_size`.

// We keep one of these for every "logical spill slot" in use.
enum LogicalSpillSlot {
    // This slot is in use and can hold values of size `size` (only).  Note that
    // `InUse` may only appear in `SpillSlotAllocator::slots` positions that
    // have indices that are 0 % `size`.  Furthermore, after such an entry in
    // `SpillSlotAllocator::slots`, the next `size` - 1 entries must be
    // `Unavail`.  This is a hard invariant, violation of which will cause
    // overlapping spill slots and potential chaos.
    InUse {
        size: u32,
        tree: AVLTree<RangeFragIx>,
    },
    // This slot is unavailable, as described above.  It's unavailable because
    // it holds some part of the values associated with the nearest lower
    // numbered entry which isn't `Unavail`, and that entry must be an `InUse`
    // entry.
    Unavail,
}
impl LogicalSpillSlot {
    fn is_Unavail(&self) -> bool {
        match self {
            LogicalSpillSlot::Unavail => true,
            _ => false,
        }
    }
    fn is_InUse(&self) -> bool {
        !self.is_Unavail()
    }
    fn get_tree(&self) -> &AVLTree<RangeFragIx> {
        match self {
            LogicalSpillSlot::InUse { ref tree, .. } => tree,
            LogicalSpillSlot::Unavail => panic!("LogicalSpillSlot::get_tree"),
        }
    }
    fn get_mut_tree(&mut self) -> &mut AVLTree<RangeFragIx> {
        match self {
            LogicalSpillSlot::InUse { ref mut tree, .. } => tree,
            LogicalSpillSlot::Unavail => panic!("LogicalSpillSlot::get_mut_tree"),
        }
    }
    fn get_size(&self) -> u32 {
        match self {
            LogicalSpillSlot::InUse { size, .. } => *size,
            LogicalSpillSlot::Unavail => panic!("LogicalSpillSlot::get_size"),
        }
    }
}

// HELPER FUNCTION
// The custom comparator
fn cmp_tree_entries_for_SpillSlotAllocator(
    fix1: RangeFragIx,
    fix2: RangeFragIx,
    frag_env: &TypedIxVec<RangeFragIx, RangeFrag>,
) -> Option<Ordering> {
    cmp_range_frags(&frag_env[fix1], &frag_env[fix2])
}

// HELPER FUNCTION
// Find out whether it is possible to add all of `frags` to `tree`.  Returns
// true if possible, false if not.  This routine relies on the fact that
// SortedFrags is non-overlapping.  However, this is a bit subtle.  We know
// that both `tree` and `frags` individually are non-overlapping, but there's
// no guarantee that elements of `frags` don't overlap `tree`.  Hence we have
// to do a custom walk of `tree` to check for overlap; we can't just use
// `AVLTree::contains`.
fn ssal_is_add_possible(
    tree: &AVLTree<RangeFragIx>,
    frag_env: &TypedIxVec<RangeFragIx, RangeFrag>,
    frags: &SortedRangeFragIxs,
) -> bool {
    // Figure out whether all the frags will go in.
    for fix in &frags.frag_ixs {
        // BEGIN check `fix` for any overlap against `tree`.
        let frag = &frag_env[*fix];
        let mut root = tree.root;
        while root != AVL_NULL {
            let root_node = &tree.pool[root as usize];
            let root_fix = root_node.item;
            let root_frag = &frag_env[root_fix];
            if frag.last < root_frag.first {
                // `frag` is entirely to the left of the `root`.  So there's no
                // overlap with root.  Continue by inspecting the left subtree.
                root = root_node.left;
            } else if root_frag.last < frag.first {
                // Ditto for the right subtree.
                root = root_node.right;
            } else {
                // `frag` overlaps the `root`.  Give up.
                return false;
            }
        }
        // END check `fix` for any overlap against `tree`.
        // `fix` doesn't overlap.  Move on to the next one.
    }
    true
}

// HELPER FUNCTION
// Try to add all of `frags` to `tree`.  Return `true` if possible, `false` if
// not possible.  If `false` is returned, `tree` is unchanged (this is
// important).  This routine relies on the fact that SortedFrags is
// non-overlapping.
fn ssal_add_if_possible(
    tree: &mut AVLTree<RangeFragIx>,
    frag_env: &TypedIxVec<RangeFragIx, RangeFrag>,
    frags: &SortedRangeFragIxs,
) -> bool {
    // Check if all the frags will go in.
    if !ssal_is_add_possible(tree, frag_env, frags) {
        return false;
    }
    // They will.  So now insert them.
    for fix in &frags.frag_ixs {
        let inserted = tree.insert(
            *fix,
            Some(&|fix1, fix2| cmp_tree_entries_for_SpillSlotAllocator(fix1, fix2, frag_env)),
        );
        // This can't fail
        assert!(inserted);
    }
    true
}

//=============================================================================
// SpillSlotAllocator: public interface

pub struct SpillSlotAllocator {
    slots: Vec<LogicalSpillSlot>,
}
impl SpillSlotAllocator {
    pub fn new() -> Self {
        Self { slots: vec![] }
    }

    pub fn num_slots_in_use(&self) -> usize {
        self.slots.len()
    }

    // This adds a new, empty slot, for items of the given size, and returns
    // its index.  This isn't clever, in the sense that it fails to use some
    // slots that it could use, but at least it's simple.  Note, this is a
    // private method.
    fn add_new_slot(&mut self, req_size: u32) -> u32 {
        assert!(req_size == 1 || req_size == 2 || req_size == 4 || req_size == 8);
        // Satisfy alignment constraints.  These entries will unfortunately be
        // wasted (never used).
        while self.slots.len() % (req_size as usize) != 0 {
            self.slots.push(LogicalSpillSlot::Unavail);
        }
        // And now the new slot.
        let dflt = RangeFragIx::invalid_value();
        let tree = AVLTree::<RangeFragIx>::new(dflt);
        let res = self.slots.len() as u32;
        self.slots.push(LogicalSpillSlot::InUse {
            size: req_size,
            tree,
        });
        // And now "block out subsequent slots that `req_size` implies.
        // viz: req_size == 1  ->  block out 0 more
        // viz: req_size == 2  ->  block out 1 more
        // viz: req_size == 4  ->  block out 3 more
        // viz: req_size == 8  ->  block out 7 more
        for _ in 1..req_size {
            self.slots.push(LogicalSpillSlot::Unavail);
        }
        assert!(self.slots.len() % (req_size as usize) == 0);
        //println!("QQQQ   add_new_slot {} for size {}", res, req_size);
        res
    }

    // Allocate spill slots for all the VirtualRanges in `vlrix`s eclass,
    // including `vlrix` itself.  Since we are allocating spill slots for
    // complete eclasses at once, none of the members of the class should
    // currently have any allocation.  This routine will try to allocate all
    // class members the same slot, but it can only guarantee to do so if the
    // class members are mutually non-overlapping.  Hence it can't guarantee that
    // in general.
    pub fn alloc_spill_slots<F: Function>(
        &mut self,
        vlr_slot_env: &mut TypedIxVec<VirtualRangeIx, Option<SpillSlot>>,
        func: &F,
        frag_env: &TypedIxVec<RangeFragIx, RangeFrag>,
        vlr_env: &TypedIxVec<VirtualRangeIx, VirtualRange>,
        vlrEquivClasses: &UnionFindEquivClasses<VirtualRangeIx>,
        vlrix: VirtualRangeIx,
    ) {
        //println!("QQQQ alloc_spill_slots: BEGIN");
        for cand_vlrix in vlrEquivClasses.equiv_class_elems_iter(vlrix) {
            assert!(vlr_slot_env[cand_vlrix].is_none());
        }
        // Do this in two passes.  It's a bit cumbersome.
        //
        // In the first pass, find a spill slot which can take all of the
        // candidates when we try them *individually*, but don't update the tree
        // yet.  We will always find such a slot, because if none of the existing
        // slots can do it, we can always start a new one.
        //
        // Now, that doesn't guarantee that all the candidates can *together* can
        // be assigned to the chosen slot.  That's only possible when they are
        // non-overlapping.  Rather than laboriously try to determine that, simply
        // proceed with the second pass, the assignment pass, as follows.  For
        // each candidate, try to allocate it to our chosen slot.  If it goes in
        // without interference, fine.  If not, that means it overlaps with some
        // other member of the class -- in which case we must find some other slot
        // for it.  It's too bad.
        //
        // The result is: all members will get a valid spill slot.  And if they
        // were all non overlapping then we are guaranteed that they all get the
        // same slot.  Which is as good as we can hope for.

        // We need to know what regclass, and hence what slot size, we're looking
        // for.  Just look at the representative; all VirtualRanges in the eclass
        // must have the same regclass.  (If they don't, the client's is_move
        // function has been giving us wrong information.)
        let vlrix_vreg = vlr_env[vlrix].vreg;
        let req_size = func.get_spillslot_size(vlrix_vreg.get_class(), vlrix_vreg);
        assert!(req_size == 1 || req_size == 2 || req_size == 4 || req_size == 8);

        // Pass 1: find a slot which can take VirtualRange when tested
        // individually.
        //
        // 1a: search existing slots
        let mut mb_chosen_slotno: Option<u32> = None;
        'pass1_search_existing_slots: for cand_slot_no in 0..self.slots.len() as u32 {
            let cand_slot = &self.slots[cand_slot_no as usize];
            if !cand_slot.is_InUse() {
                continue 'pass1_search_existing_slots;
            }
            if cand_slot.get_size() != req_size {
                continue 'pass1_search_existing_slots;
            }
            let tree = &cand_slot.get_tree();
            assert!(mb_chosen_slotno.is_none());

            // BEGIN see if `cand_slot` can hold all eclass members individually
            let mut all_cands_fit_individually = true;
            for cand_vlrix in vlrEquivClasses.equiv_class_elems_iter(vlrix) {
                let cand_vlr = &vlr_env[cand_vlrix];
                let this_cand_fits = ssal_is_add_possible(&tree, &frag_env, &cand_vlr.sorted_frags);
                if !this_cand_fits {
                    all_cands_fit_individually = false;
                    break;
                }
            }
            // END see if `cand_slot` can hold all eclass members individually
            if !all_cands_fit_individually {
                continue 'pass1_search_existing_slots;
            }

            // Ok.  All eclass members will fit individually in `cand_slot_no`.
            mb_chosen_slotno = Some(cand_slot_no);
            break;
        } /* 'pass1_search_existing_slots */
        // 1b. If we didn't find a usable slot, allocate a new one.
        let chosen_slotno: u32 = if mb_chosen_slotno.is_none() {
            //println!(
            //    "QQQQ   alloc_spill_slots: no existing slot works.  Alloc new.");
            self.add_new_slot(req_size)
        } else {
            //println!("QQQQ   alloc_spill_slots: use existing {} (for eclass)",
            //         mb_chosen_slotno.unwrap());
            mb_chosen_slotno.unwrap()
        };
        //println!("QQQQ   alloc_spill_slots: chosen = {}", chosen_slotno);

        // Pass 2.  Try to allocate each eclass member individually to the chosen
        // slot.  If that fails, just allocate them anywhere.
        let mut _all_in_chosen = true;
        'pass2_per_equiv_class: for cand_vlrix in vlrEquivClasses.equiv_class_elems_iter(vlrix) {
            let cand_vlr = &vlr_env[cand_vlrix];
            let mut tree = self.slots[chosen_slotno as usize].get_mut_tree();
            let added = ssal_add_if_possible(&mut tree, &frag_env, &cand_vlr.sorted_frags);
            if added {
                vlr_slot_env[cand_vlrix] = Some(SpillSlot::new(chosen_slotno));
                continue 'pass2_per_equiv_class;
            }
            // It won't fit in `chosen_slotno`, so try somewhere (anywhere) else.
            for alt_slotno in 0..self.slots.len() as u32 {
                let alt_slot = &self.slots[alt_slotno as usize];
                if !alt_slot.is_InUse() {
                    continue;
                }
                if alt_slot.get_size() != req_size {
                    continue;
                }
                if alt_slotno == chosen_slotno {
                    // We already know this won't work.
                    continue;
                }
                let mut tree = self.slots[alt_slotno as usize].get_mut_tree();
                let added = ssal_add_if_possible(&mut tree, &frag_env, &cand_vlr.sorted_frags);
                if added {
                    vlr_slot_env[cand_vlrix] = Some(SpillSlot::new(alt_slotno));
                    continue 'pass2_per_equiv_class;
                }
            }
            // If we get here, it means it won't fit in any slot we currently have.
            // So allocate a new one and use that.
            _all_in_chosen = false;
            let new_slotno = self.add_new_slot(req_size);
            let mut tree = self.slots[new_slotno as usize].get_mut_tree();
            let added = ssal_add_if_possible(&mut tree, &frag_env, &cand_vlr.sorted_frags);
            if added {
                vlr_slot_env[cand_vlrix] = Some(SpillSlot::new(new_slotno));
                continue 'pass2_per_equiv_class;
            }
            // We failed to allocate it to any empty slot!  This can't happen.
            panic!("SpillSlotAllocator: alloc_spill_slots: failed?!?!");
            /*NOTREACHED*/
        } /* 'pass2_per_equiv_class */

        for eclass_vlrix in vlrEquivClasses.equiv_class_elems_iter(vlrix) {
            assert!(vlr_slot_env[eclass_vlrix].is_some());
            debug!(
                "--     alloc_spill_sls  {:?} -> {:?}",
                eclass_vlrix,
                vlr_slot_env[eclass_vlrix].unwrap()
            );
        }
        //println!("QQQQ alloc_spill_slots: END, all in same = {}",
        //         if all_in_chosen { "YES" } else { "no" });
        //println!("QQQQ");
    }
}
