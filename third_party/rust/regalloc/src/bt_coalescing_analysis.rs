//! Analysis in support of copy coalescing for the backtracking allocator.
//!
//! This detects and collects information about all copy coalescing
//! opportunities in the incoming function.  It does not use that information
//! at all -- that is for the main allocation loop and the spill slot allocator
//! to do.
//!
//! Coalescing analysis creates 4 pieces of information:
//!
//! * a map from `VirtualRangeIx` to a set of `Hint`s (see below) which state a
//!   preference for which register that range would prefer to be allocated to.
//!
//! * equivalence class groupings for the virtual ranges.  Two virtual ranges
//!   will be assigned the same equivalence class if there is a move instruction
//!   that transfers a value from one range to the other.  The equivalence
//!   classes created are the transitive closure of this pairwise relation.
//!
//! * a simple mapping from instruction index to bool, indicating those
//! instructions that are moves between virtual registers, and that have been
//! used to construct the equivalence classes above.
//!
//! * a mapping from virtual registers to virtual ranges.  This is really
//!   produced as a side-effect of computing the above three elements, but is
//!   useful in its own right and so is also returned.

#![allow(non_snake_case)]
#![allow(non_camel_case_types)]

use log::{debug, info, log_enabled, Level};
use smallvec::{smallvec, SmallVec};

use crate::data_structures::{
    InstIx, InstPoint, MoveInfo, MoveInfoElem, RangeFrag, RangeFragIx, RealRange, RealRangeIx,
    RealReg, RealRegUniverse, RegToRangesMaps, SpillCost, TypedIxVec, VirtualRange, VirtualRangeIx,
    VirtualReg,
};
use crate::union_find::{ToFromU32, UnionFind, UnionFindEquivClasses};
use crate::Function;

//=============================================================================
//
// *** Some important comments about the interaction between this coalescing
// *** analysis, the main allocation loop and the spill slot allocator.
//
// The main allocation loop tries to assign the same register to all the
// VirtualRanges in an equivalence class.  Similarly, the spill slot allocator
// tries to allocate the same spill slot to all the VirtualRanges in an
// equivalence class.  In most cases they are successful, and so the moves
// between those VirtualRanges will later disappear.  However, the complete
// story is not quite so simple.
//
// It is only safe to assign the VirtualRanges in the same equivalence class
// to a single register or spill slot if those VirtualRanges are
// non-overlapping.  That is, if their overall collection of RangeFrags is
// disjoint.  If two such VirtualRanges overlapped, then they could be
// carrying different values, and so they would need separate registers or
// spill slots.
//
// Most of the time, these equivalence classes are indeed internally
// non-overlapping.  But that's just luck -- that's how the input VCode mostly
// is.  The coalescing analysis *doesn't* properly check for overlaps within an
// equivalence class, so it can be the case that the members of an equivalence
// class overlap.  The users of that information -- the main allocation loop
// and the spill slot allocator -- currently check for, and handle, such
// situations.  So the generated allocation is correct.
//
// It does, however, cause imprecision and unnecessary spilling, and, in the
// main allocation loop, slightly increased evictions.
//
// The "proper" fix for all this would be to fix the coalescing analysis so as
// only to build non-internally-overlapping VirtualRange equivalence classes.
// However, that sounds expensive.  Instead there is a half-hearted effort
// made to avoid creating equivalence classes whose elements (VirtualRanges)
// overlap.  This is done by doing an overlap check on two VirtualRanges
// connected by a move, and not merging their equivalence classes if they
// overlap.  That helps, but it doesn't completely avoid the problem because
// there might be overlaps between other members (VirtualRanges) of the
// about-to-be-merged equivalence classes.

//=============================================================================
// Coalescing analysis: Hints
//
// A coalescing hint for a virtual live range.  The u32 is an arbitrary
// "weight" value which indicates a relative strength-of-preference for the
// hint.  It exists because a VLR can have arbitrarily many copy
// instructions at its "boundary", and hence arbitrarily many hints.  Of
// course the allocator core can honour at most one of them, so it needs a
// way to choose between them.  In this implementation, the u32s are simply
// the estimated execution count of the associated copy instruction.
#[derive(Clone)]
pub enum Hint {
    // I would like to have the same real register as some other virtual range.
    SameAs(VirtualRangeIx, u32),
    // I would like to have exactly this real register.
    Exactly(RealReg, u32),
}
fn show_hint(h: &Hint, univ: &RealRegUniverse) -> String {
    match h {
        Hint::SameAs(vlrix, weight) => format!("(SameAs {:?}, weight={})", vlrix, weight),
        Hint::Exactly(rreg, weight) => format!(
            "(Exactly {}, weight={})",
            rreg.to_reg().show_with_rru(&univ),
            weight
        ),
    }
}
impl Hint {
    fn get_weight(&self) -> u32 {
        match self {
            Hint::SameAs(_vlrix, weight) => *weight,
            Hint::Exactly(_rreg, weight) => *weight,
        }
    }
}

// We need this in order to construct a UnionFind<VirtualRangeIx>.
impl ToFromU32 for VirtualRangeIx {
    fn to_u32(x: VirtualRangeIx) -> u32 {
        x.get()
    }
    fn from_u32(x: u32) -> VirtualRangeIx {
        VirtualRangeIx::new(x)
    }
}

//=============================================================================
// Coalescing analysis: top level function

// This performs coalescing analysis and returns info as a 4-tuple.  Note that
// it also may change the spill costs for some of the VLRs in `vlr_env` to
// better reflect the spill cost situation in the presence of coalescing.
#[inline(never)]
pub fn do_coalescing_analysis<F: Function>(
    func: &F,
    univ: &RealRegUniverse,
    rlr_env: &TypedIxVec<RealRangeIx, RealRange>,
    vlr_env: &mut TypedIxVec<VirtualRangeIx, VirtualRange>,
    frag_env: &TypedIxVec<RangeFragIx, RangeFrag>,
    reg_to_ranges_maps: &RegToRangesMaps,
    move_info: &MoveInfo,
) -> (
    TypedIxVec<VirtualRangeIx, SmallVec<[Hint; 8]>>,
    UnionFindEquivClasses<VirtualRangeIx>,
    TypedIxVec<InstIx, bool>,
) {
    info!("");
    info!("do_coalescing_analysis: begin");

    // There follow four closures, which are used to find out whether a real or virtual reg has
    // a last use or first def at some instruction.  This is the central activity of the
    // coalescing analysis -- finding move instructions that are the last def for the src reg
    // and the first def for the dst reg.

    // Range checks for VRegs -- last use.
    let doesVRegHaveLastUseAt = |vreg: VirtualReg, iix: InstIx| -> Option<VirtualRangeIx> {
        let vreg_no = vreg.get_index();
        let vlrixs = &reg_to_ranges_maps.vreg_to_vlrs_map[vreg_no];
        for vlrix in vlrixs {
            for frag in &vlr_env[*vlrix].sorted_frags.frags {
                // We're checking to see if `vreg` has a last use in this block
                // (well, technically, a fragment end in the block; we don't care if
                // it is later redefined in the same block) .. anyway ..
                // We're checking to see if `vreg` has a last use in this block
                // at `iix`.u
                if frag.last == InstPoint::new_use(iix) {
                    return Some(*vlrix);
                }
            }
        }
        None
    };

    // Range checks for VRegs -- first def.
    let doesVRegHaveFirstDefAt = |vreg: VirtualReg, iix: InstIx| -> Option<VirtualRangeIx> {
        let vreg_no = vreg.get_index();
        let vlrixs = &reg_to_ranges_maps.vreg_to_vlrs_map[vreg_no];
        for vlrix in vlrixs {
            for frag in &vlr_env[*vlrix].sorted_frags.frags {
                // We're checking to see if `vreg` has a first def in this block at `iix`.d
                if frag.first == InstPoint::new_def(iix) {
                    return Some(*vlrix);
                }
            }
        }
        None
    };

    // Range checks for RRegs -- last use.
    let doesRRegHaveLastUseAt = |rreg: RealReg, iix: InstIx| -> Option<RealRangeIx> {
        let rreg_no = rreg.get_index();
        let rlrixs = &reg_to_ranges_maps.rreg_to_rlrs_map[rreg_no];
        for rlrix in rlrixs {
            let frags = &rlr_env[*rlrix].sorted_frags;
            for fix in &frags.frag_ixs {
                let frag = &frag_env[*fix];
                // We're checking to see if `rreg` has a last use in this block at `iix`.u
                if frag.last == InstPoint::new_use(iix) {
                    return Some(*rlrix);
                }
            }
        }
        None
    };

    // Range checks for RRegs -- first def.
    let doesRRegHaveFirstDefAt = |rreg: RealReg, iix: InstIx| -> Option<RealRangeIx> {
        let rreg_no = rreg.get_index();
        let rlrixs = &reg_to_ranges_maps.rreg_to_rlrs_map[rreg_no];
        for rlrix in rlrixs {
            let frags = &rlr_env[*rlrix].sorted_frags;
            for fix in &frags.frag_ixs {
                let frag = &frag_env[*fix];
                // We're checking to see if `rreg` has a first def in this block at `iix`.d
                if frag.first == InstPoint::new_def(iix) {
                    return Some(*rlrix);
                }
            }
        }
        None
    };

    // RETURNED TO CALLER
    // Hints for each VirtualRange.  Note that the SmallVecs could contain duplicates, I
    // suppose, for example if there are two identical copy insns at different points on the
    // "boundary" for some VLR.  I don't think it matters though since we're going to rank the
    // hints by strength and then choose at most one.
    let mut hints = TypedIxVec::<VirtualRangeIx, SmallVec<[Hint; 8]>>::new();
    hints.resize(vlr_env.len(), smallvec![]);

    // RETURNED TO CALLER
    // A vector that simply records which insns are v-to-v boundary moves, as established by the
    // analysis below.  This info is collected here because (1) the caller (BT) needs to have it
    // and (2) this is the first point at which we can efficiently compute it.
    let mut is_vv_boundary_move = TypedIxVec::<InstIx, bool>::new();
    is_vv_boundary_move.resize(func.insns().len() as u32, false);

    // RETURNED TO CALLER (after finalisation)
    // The virtual-to-virtual equivalence classes we're collecting.
    let mut vlrEquivClassesUF = UnionFind::<VirtualRangeIx>::new(vlr_env.len() as usize);

    // Not returned to caller; for use only in this function.
    // A list of `VirtualRange`s for which the `total_cost` (hence also their
    // `spill_cost`) should be adjusted downwards by the supplied `u32`.  We
    // can't do this directly in the loop below due to borrowing constraints,
    // hence we collect the required info in this vector and do it in a second
    // loop.
    let mut decVLRcosts = Vec::<(VirtualRangeIx, VirtualRangeIx, u32)>::new();

    for MoveInfoElem {
        dst,
        src,
        iix,
        est_freq,
        ..
    } in &move_info.moves
    {
        debug!(
            "connected by moves: {:?} {:?} <- {:?} (est_freq {})",
            iix, dst, src, est_freq
        );
        match (dst.is_virtual(), src.is_virtual()) {
            (true, true) => {
                // Check for a V <- V hint.
                let srcV = src.to_virtual_reg();
                let dstV = dst.to_virtual_reg();
                let mb_vlrixSrc = doesVRegHaveLastUseAt(srcV, *iix);
                let mb_vlrixDst = doesVRegHaveFirstDefAt(dstV, *iix);
                if mb_vlrixSrc.is_some() && mb_vlrixDst.is_some() {
                    let vlrixSrc = mb_vlrixSrc.unwrap();
                    let vlrixDst = mb_vlrixDst.unwrap();
                    // Per block comment at top of file, make a half-hearted
                    // attempt to avoid creating equivalence classes with
                    // internal overlaps.  Note this can't be completely
                    // effective as presently implemented.
                    if !vlr_env[vlrixSrc].overlaps(&vlr_env[vlrixDst]) {
                        // Add hints for both VLRs, since we don't know which one will
                        // assign first.  Indeed, a VLR may be assigned and un-assigned
                        // arbitrarily many times.
                        hints[vlrixSrc].push(Hint::SameAs(vlrixDst, *est_freq));
                        hints[vlrixDst].push(Hint::SameAs(vlrixSrc, *est_freq));
                        vlrEquivClassesUF.union(vlrixDst, vlrixSrc);
                        is_vv_boundary_move[*iix] = true;
                        // Reduce the total cost, and hence the spill cost, of
                        // both `vlrixSrc` and `vlrixDst`.  This is so as to reduce to
                        // zero, the cost of a VLR whose only instructions are its
                        // v-v boundary copies.
                        debug!("reduce cost of {:?} and {:?}", vlrixSrc, vlrixDst);
                        decVLRcosts.push((vlrixSrc, vlrixDst, 1 * est_freq));
                    }
                }
            }
            (true, false) => {
                // Check for a V <- R hint.
                let srcR = src.to_real_reg();
                let dstV = dst.to_virtual_reg();
                let mb_rlrSrc = doesRRegHaveLastUseAt(srcR, *iix);
                let mb_vlrDst = doesVRegHaveFirstDefAt(dstV, *iix);
                if mb_rlrSrc.is_some() && mb_vlrDst.is_some() {
                    let vlrDst = mb_vlrDst.unwrap();
                    hints[vlrDst].push(Hint::Exactly(srcR, *est_freq));
                }
            }
            (false, true) => {
                // Check for a R <- V hint.
                let srcV = src.to_virtual_reg();
                let dstR = dst.to_real_reg();
                let mb_vlrSrc = doesVRegHaveLastUseAt(srcV, *iix);
                let mb_rlrDst = doesRRegHaveFirstDefAt(dstR, *iix);
                if mb_vlrSrc.is_some() && mb_rlrDst.is_some() {
                    let vlrSrc = mb_vlrSrc.unwrap();
                    hints[vlrSrc].push(Hint::Exactly(dstR, *est_freq));
                }
            }
            (false, false) => {
                // This is a real-to-real move.  There's nothing we can do.  Ignore it.
            }
        }
    }

    // Now decrease the `total_cost` and `spill_cost` fields of selected
    // `VirtualRange`s, as detected by the previous loop.  Don't decrease the
    // `spill_cost` literally to zero; doing that causes various assertion
    // failures and boundary problems later on, in the `CommitmentMap`s.  In
    // such a case, make the `spill_cost` be tiny but nonzero.
    fn decrease_vlr_total_cost_by(vlr: &mut VirtualRange, decrease_total_cost_by: u32) {
        // Adjust `total_cost`.
        if vlr.total_cost < decrease_total_cost_by {
            vlr.total_cost = 0;
        } else {
            vlr.total_cost -= decrease_total_cost_by;
        }
        // And recompute `spill_cost` accordingly.
        if vlr.total_cost == 0 {
            vlr.spill_cost = SpillCost::finite(1.0e-6);
        } else {
            assert!(vlr.size > 0);
            vlr.spill_cost = SpillCost::finite(vlr.total_cost as f32 / vlr.size as f32);
        }
    }

    for (vlrix1, vlrix2, decrease_total_cost_by) in decVLRcosts {
        decrease_vlr_total_cost_by(&mut vlr_env[vlrix1], decrease_total_cost_by);
        decrease_vlr_total_cost_by(&mut vlr_env[vlrix2], decrease_total_cost_by);
    }

    // For the convenience of the allocator core, sort the hints for each VLR so
    // as to move the most preferred to the front.
    for hints_for_one_vlr in hints.iter_mut() {
        hints_for_one_vlr.sort_by(|h1, h2| h2.get_weight().partial_cmp(&h1.get_weight()).unwrap());
    }

    let vlrEquivClasses: UnionFindEquivClasses<VirtualRangeIx> =
        vlrEquivClassesUF.get_equiv_classes();

    if log_enabled!(Level::Debug) {
        debug!("Revised VLRs:");
        let mut n = 0;
        for vlr in vlr_env.iter() {
            debug!("{:<4?}   {:?}", VirtualRangeIx::new(n), vlr);
            n += 1;
        }

        debug!("Coalescing hints:");
        n = 0;
        for hints_for_one_vlr in hints.iter() {
            let mut s = "".to_string();
            for hint in hints_for_one_vlr {
                s = s + &show_hint(hint, &univ) + &" ".to_string();
            }
            debug!("  hintsfor {:<4?} = {}", VirtualRangeIx::new(n), s);
            n += 1;
        }

        for n in 0..vlr_env.len() {
            let vlrix = VirtualRangeIx::new(n);
            let mut tmpvec = vec![];
            for elem in vlrEquivClasses.equiv_class_elems_iter(vlrix) {
                tmpvec.reverse();
                tmpvec.push(elem);
            }
            debug!("  eclassof {:?} = {:?}", vlrix, tmpvec);
        }

        for (b, i) in is_vv_boundary_move.iter().zip(0..) {
            if *b {
                debug!("  vv_boundary_move at {:?}", InstIx::new(i));
            }
        }
    }

    info!("do_coalescing_analysis: end");
    info!("");

    (hints, vlrEquivClasses, is_vv_boundary_move)
}
