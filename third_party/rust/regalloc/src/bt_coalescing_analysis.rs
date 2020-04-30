#![allow(non_snake_case)]
#![allow(non_camel_case_types)]

//! Copy-coalescing analysis for the backtracking allocator.

use log::{debug, info, log_enabled, Level};
use smallvec::{smallvec, SmallVec};

use crate::data_structures::{
    BlockIx, InstIx, InstPoint, RangeFrag, RangeFragIx, RealRange, RealRangeIx, RealReg,
    RealRegUniverse, Reg, RegVecsAndBounds, SpillCost, TypedIxVec, VirtualRange, VirtualRangeIx,
    VirtualReg,
};
use crate::union_find::{ToFromU32, UnionFind, UnionFindEquivClasses};
use crate::Function;

//=============================================================================
// Analysis in support of copy coalescing.
//
// This detects and collects information about all copy coalescing
// opportunities in the incoming function.  It does not use that information
// at all -- that is for the main allocation loop to do.

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
    reg_vecs_and_bounds: &RegVecsAndBounds,
    rlr_env: &TypedIxVec<RealRangeIx, RealRange>,
    vlr_env: &mut TypedIxVec<VirtualRangeIx, VirtualRange>,
    frag_env: &TypedIxVec<RangeFragIx, RangeFrag>,
    est_freqs: &TypedIxVec<BlockIx, u32>,
    univ: &RealRegUniverse,
) -> (
    TypedIxVec<VirtualRangeIx, SmallVec<[Hint; 8]>>,
    UnionFindEquivClasses<VirtualRangeIx>,
    TypedIxVec<InstIx, bool>,
    Vec</*vreg index,*/ SmallVec<[VirtualRangeIx; 3]>>,
) {
    info!("");
    info!("do_coalescing_analysis: begin");
    // We have in hand the virtual live ranges.  Each of these carries its
    // associated vreg.  So in effect we have a VLR -> VReg mapping.  We now
    // invert that, so as to generate a mapping from VRegs to their containing
    // VLRs.
    //
    // Note that multiple VLRs may map to the same VReg.  So the inverse mapping
    // will actually be from VRegs to a set of VLRs.  In most cases, we expect
    // the virtual-registerised-code given to this allocator to be derived from
    // SSA, in which case each VReg will have only one VLR.  So in this case,
    // the cost of first creating the mapping, and then looking up all the VRegs
    // in moves in it, will have cost linear in the size of the input function.
    //
    // It would be convenient here to know how many VRegs there are ahead of
    // time, but until then we'll discover it dynamically.
    // NB re the SmallVec.  That has set semantics (no dups)
    // FIXME use SmallVec for the VirtualRangeIxs.  Or even a sparse set.
    let mut vreg_to_vlrs_map = Vec::</*vreg index,*/ SmallVec<[VirtualRangeIx; 3]>>::new();

    for (vlr, n) in vlr_env.iter().zip(0..) {
        let vlrix = VirtualRangeIx::new(n);
        let vreg: VirtualReg = vlr.vreg;
        // Now we know that there's a VLR `vlr` that is for VReg `vreg`.  Update
        // the inverse mapping accordingly.  That may involve resizing it, since
        // we have no idea of the order in which we will first encounter VRegs.
        // By contrast, we know we are stepping sequentially through the VLR
        // (index) space, and we'll never see the same VLRIx twice.  So there's no
        // need to check for dups when adding a VLR index to an existing binding
        // for a VReg.
        let vreg_ix = vreg.get_index();

        while vreg_to_vlrs_map.len() <= vreg_ix {
            vreg_to_vlrs_map.push(smallvec![]); // This is very un-clever
        }

        vreg_to_vlrs_map[vreg_ix].push(vlrix);
    }

    // Same for the real live ranges
    let mut rreg_to_rlrs_map = Vec::</*rreg index,*/ Vec<RealRangeIx>>::new();

    for (rlr, n) in rlr_env.iter().zip(0..) {
        let rlrix = RealRangeIx::new(n);
        let rreg: RealReg = rlr.rreg;
        let rreg_ix = rreg.get_index();

        while rreg_to_rlrs_map.len() <= rreg_ix {
            rreg_to_rlrs_map.push(vec![]); // This is very un-clever
        }

        rreg_to_rlrs_map[rreg_ix].push(rlrix);
    }

    // And what do we got?
    //for (vlrixs, vreg) in vreg_to_vlrs_map.iter().zip(0..) {
    //  println!("QQQQ vreg v{:?} -> vlrixs {:?}", vreg, vlrixs);
    //}
    //for (rlrixs, rreg) in rreg_to_rlrs_map.iter().zip(0..) {
    //  println!("QQQQ rreg r{:?} -> rlrixs {:?}", rreg, rlrixs);
    //}

    // Range end checks for VRegs
    let doesVRegHaveXXat
    // `xxIsLastUse` is true means "XX is last use"
    // `xxIsLastUse` is false means "XX is first def"
    = |xxIsLastUse: bool, vreg: VirtualReg, iix: InstIx|
    -> Option<VirtualRangeIx> {
      let vreg_no = vreg.get_index();
      let vlrixs = &vreg_to_vlrs_map[vreg_no];
      for vlrix in vlrixs {
        let frags = &vlr_env[*vlrix].sorted_frags;
        for fix in &frags.frag_ixs {
          let frag = &frag_env[*fix];
          if xxIsLastUse {
            // We're checking to see if `vreg` has a last use in this block
            // (well, technically, a fragment end in the block; we don't care if
            // it is later redefined in the same block) .. anyway ..
            // We're checking to see if `vreg` has a last use in this block
            // at `iix`.u
            if frag.last == InstPoint::new_use(iix) {
              return Some(*vlrix);
            }
          } else {
            // We're checking to see if `vreg` has a first def in this block
            // at `iix`.d
            if frag.first == InstPoint::new_def(iix) {
              return Some(*vlrix);
            }
          }
        }
      }
      None
    };

    // Range end checks for RRegs
    let doesRRegHaveXXat
    // `xxIsLastUse` is true means "XX is last use"
    // `xxIsLastUse` is false means "XX is first def"
    = |xxIsLastUse: bool, rreg: RealReg, iix: InstIx|
    -> Option<RealRangeIx> {
      let rreg_no = rreg.get_index();
      let rlrixs = &rreg_to_rlrs_map[rreg_no];
      for rlrix in rlrixs {
        let frags = &rlr_env[*rlrix].sorted_frags;
        for fix in &frags.frag_ixs {
          let frag = &frag_env[*fix];
          if xxIsLastUse {
            // We're checking to see if `rreg` has a last use in this block
            // at `iix`.u
            if frag.last == InstPoint::new_use(iix) {
              return Some(*rlrix);
            }
          } else {
            // We're checking to see if `rreg` has a first def in this block
            // at `iix`.d
            if frag.first == InstPoint::new_def(iix) {
              return Some(*rlrix);
            }
          }
        }
      }
      None
    };

    // Make up a vector of registers that are connected by moves:
    //
    //    (dstReg, srcReg, transferring insn, estimated execution count of the
    //                                        containing block)
    //
    // This can contain real-to-real moves, which we obviously can't do anything
    // about.  We'll remove them in the next pass.
    let mut connectedByMoves = Vec::<(Reg, Reg, InstIx, u32)>::new();
    for b in func.blocks() {
        let block_eef = est_freqs[b];
        for iix in func.block_insns(b) {
            let insn = &func.get_insn(iix);
            let im = func.is_move(insn);
            match im {
                None => {}
                Some((wreg, reg)) => {
                    let iix_bounds = &reg_vecs_and_bounds.bounds[iix];
                    // It might seem strange to assert that `defs_len` and/or
                    // `uses_len` is <= 1 rather than == 1.  The reason is
                    // that either or even both registers might be ones which
                    // are not available to the allocator.  Hence they will
                    // have been removed by the sanitisation machinery before
                    // we get to this point.  If either is missing, we
                    // unfortunately can't coalesce the move away, and just
                    // have to live with it.
                    //
                    // If any of the following five assertions fail, the
                    // client's `is_move` is probably lying to us.
                    assert!(iix_bounds.uses_len <= 1);
                    assert!(iix_bounds.defs_len <= 1);
                    assert!(iix_bounds.mods_len == 0);
                    if iix_bounds.uses_len == 1 && iix_bounds.defs_len == 1 {
                        let reg_vecs = &reg_vecs_and_bounds.vecs;
                        assert!(reg_vecs.uses[iix_bounds.uses_start as usize] == reg);
                        assert!(reg_vecs.defs[iix_bounds.defs_start as usize] == wreg.to_reg());
                        connectedByMoves.push((wreg.to_reg(), reg, iix, block_eef));
                    }
                }
            }
        }
    }

    // XX these sub-vectors could contain duplicates, I suppose, for example if
    // there are two identical copy insns at different points on the "boundary"
    // for some VLR.  I don't think it matters though since we're going to rank
    // the hints by strength and then choose at most one.
    let mut hints = TypedIxVec::<VirtualRangeIx, SmallVec<[Hint; 8]>>::new();
    hints.resize(vlr_env.len(), smallvec![]);

    let mut is_vv_boundary_move = TypedIxVec::<InstIx, bool>::new();
    is_vv_boundary_move.resize(func.insns().len() as u32, false);

    // The virtual-to-virtual equivalence classes we're collecting.
    let mut vlrEquivClassesUF = UnionFind::<VirtualRangeIx>::new(vlr_env.len() as usize);

    // A list of `VirtualRange`s for which the `total_cost` (hence also their
    // `spill_cost`) should be adjusted downwards by the supplied `u32`.  We
    // can't do this directly in the loop below due to borrowing constraints,
    // hence we collect the required info in this vector and do it in a second
    // loop.
    let mut decVLRcosts = Vec::<(VirtualRangeIx, VirtualRangeIx, u32)>::new();

    for (rDst, rSrc, iix, block_eef) in connectedByMoves {
        debug!(
            "QQQQ connectedByMoves {:?} {:?} <- {:?} (block_eef {})",
            iix, rDst, rSrc, block_eef
        );
        match (rDst.is_virtual(), rSrc.is_virtual()) {
            (true, true) => {
                // Check for a V <- V hint.
                let rSrcV = rSrc.to_virtual_reg();
                let rDstV = rDst.to_virtual_reg();
                let mb_vlrSrc = doesVRegHaveXXat(/*xxIsLastUse=*/ true, rSrcV, iix);
                let mb_vlrDst = doesVRegHaveXXat(/*xxIsLastUse=*/ false, rDstV, iix);
                debug!("QQQQ mb_vlrSrc {:?} mb_vlrDst {:?}", mb_vlrSrc, mb_vlrDst);
                if mb_vlrSrc.is_some() && mb_vlrDst.is_some() {
                    let vlrSrc = mb_vlrSrc.unwrap();
                    let vlrDst = mb_vlrDst.unwrap();
                    // Add hints for both VLRs, since we don't know which one will
                    // assign first.  Indeed, a VLR may be assigned and un-assigned
                    // arbitrarily many times.
                    hints[vlrSrc].push(Hint::SameAs(vlrDst, block_eef));
                    hints[vlrDst].push(Hint::SameAs(vlrSrc, block_eef));
                    vlrEquivClassesUF.union(vlrDst, vlrSrc);
                    is_vv_boundary_move[iix] = true;
                    // Reduce the total cost, and hence the spill cost, of
                    // both `vlrSrc` and `vlrDst`.  This is so as to reduce to
                    // zero, the cost of a VLR whose only instructions are its
                    // v-v boundary copies.
                    debug!("QQQQ reduce cost of {:?} and {:?}", vlrSrc, vlrDst);
                    decVLRcosts.push((vlrSrc, vlrDst, 1 * block_eef));
                }
            }
            (true, false) => {
                // Check for a V <- R hint.
                let rSrcR = rSrc.to_real_reg();
                let rDstV = rDst.to_virtual_reg();
                let mb_rlrSrc = doesRRegHaveXXat(/*xxIsLastUse=*/ true, rSrcR, iix);
                let mb_vlrDst = doesVRegHaveXXat(/*xxIsLastUse=*/ false, rDstV, iix);
                if mb_rlrSrc.is_some() && mb_vlrDst.is_some() {
                    let vlrDst = mb_vlrDst.unwrap();
                    hints[vlrDst].push(Hint::Exactly(rSrcR, block_eef));
                }
            }
            (false, true) => {
                // Check for a R <- V hint.
                let rSrcV = rSrc.to_virtual_reg();
                let rDstR = rDst.to_real_reg();
                let mb_vlrSrc = doesVRegHaveXXat(/*xxIsLastUse=*/ true, rSrcV, iix);
                let mb_rlrDst = doesRRegHaveXXat(/*xxIsLastUse=*/ false, rDstR, iix);
                if mb_vlrSrc.is_some() && mb_rlrDst.is_some() {
                    let vlrSrc = mb_vlrSrc.unwrap();
                    hints[vlrSrc].push(Hint::Exactly(rDstR, block_eef));
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

    (
        hints,
        vlrEquivClasses,
        is_vv_boundary_move,
        vreg_to_vlrs_map,
    )
}
