use super::{FixedInterval, IntId, Intervals, Mention, MentionMap, VirtualInterval};
use crate::{
    analysis_control_flow::{CFGInfo, InstIxToBlockIxMap},
    analysis_data_flow::{
        calc_def_and_use, calc_livein_and_liveout, get_sanitized_reg_uses_for_func, reg_ix_to_reg,
        reg_to_reg_ix,
    },
    data_structures::{BlockIx, InstPoint, RangeFragIx, RangeFragKind, Reg, RegVecsAndBounds},
    sparse_set::SparseSet,
    union_find::UnionFind,
    AnalysisError, Function, RealRegUniverse, RegClass, TypedIxVec,
};
use log::{debug, info, log_enabled, Level};
use smallvec::{smallvec, SmallVec};
use std::{fmt, mem};

#[derive(Clone, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) struct RangeFrag {
    pub(crate) first: InstPoint,
    pub(crate) last: InstPoint,
    pub(crate) mentions: MentionMap,
}

impl fmt::Debug for RangeFrag {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt, "[{:?}; {:?}]", self.first, self.last)
    }
}

impl RangeFrag {
    fn new<F: Function>(
        func: &F,
        bix: BlockIx,
        first: InstPoint,
        last: InstPoint,
        mentions: MentionMap,
    ) -> (Self, RangeFragMetrics) {
        debug_assert!(func.block_insns(bix).len() >= 1);
        debug_assert!(func.block_insns(bix).contains(first.iix()));
        debug_assert!(func.block_insns(bix).contains(last.iix()));
        debug_assert!(first <= last);

        let first_in_block = InstPoint::new_use(func.block_insns(bix).first());
        let last_in_block = InstPoint::new_def(func.block_insns(bix).last());
        let kind = match (first == first_in_block, last == last_in_block) {
            (false, false) => RangeFragKind::Local,
            (false, true) => RangeFragKind::LiveOut,
            (true, false) => RangeFragKind::LiveIn,
            (true, true) => RangeFragKind::Thru,
        };

        (
            RangeFrag {
                first,
                last,
                mentions,
            },
            RangeFragMetrics { bix, kind },
        )
    }

    #[inline(always)]
    #[cfg(debug_assertions)]
    pub(crate) fn contains(&self, inst: &InstPoint) -> bool {
        self.first <= *inst && *inst <= self.last
    }
}

struct RangeFragMetrics {
    bix: BlockIx,
    kind: RangeFragKind,
}

pub(crate) struct AnalysisInfo {
    /// The sanitized per-insn reg-use info.
    pub(crate) reg_vecs_and_bounds: RegVecsAndBounds,
    /// All the intervals, fixed or virtual.
    pub(crate) intervals: Intervals,
    /// Liveins per block.
    pub(crate) liveins: TypedIxVec<BlockIx, SparseSet<Reg>>,
    /// Liveouts per block.
    pub(crate) liveouts: TypedIxVec<BlockIx, SparseSet<Reg>>,
    /// Blocks's loop depths.
    pub(crate) _loop_depth: TypedIxVec<BlockIx, u32>,
    /// Maps InstIxs to BlockIxs.
    pub(crate) _inst_to_block_map: InstIxToBlockIxMap,
}

#[inline(never)]
pub(crate) fn run<F: Function>(
    func: &F,
    reg_universe: &RealRegUniverse,
) -> Result<AnalysisInfo, AnalysisError> {
    info!(
        "run_analysis: begin: {} blocks, {} insns",
        func.blocks().len(),
        func.insns().len()
    );

    // First do control flow analysis.  This is (relatively) simple.  Note that this can fail, for
    // various reasons; we propagate the failure if so.  Also create the InstIx-to-BlockIx map;
    // this isn't really control-flow analysis, but needs to be done at some point.

    info!("  run_analysis: begin control flow analysis");
    let cfg_info = CFGInfo::create(func)?;
    let inst_to_block_map = InstIxToBlockIxMap::new(func);
    info!("  run_analysis: end control flow analysis");

    info!("  run_analysis: begin data flow analysis");

    // See `get_sanitized_reg_uses_for_func` for the meaning of "sanitized".
    let reg_vecs_and_bounds = get_sanitized_reg_uses_for_func(func, reg_universe)
        .map_err(|reg| AnalysisError::IllegalRealReg(reg))?;
    assert!(reg_vecs_and_bounds.is_sanitized());

    // Calculate block-local def/use sets.
    let (def_sets_per_block, use_sets_per_block) =
        calc_def_and_use(func, &reg_vecs_and_bounds, &reg_universe);
    debug_assert!(def_sets_per_block.len() == func.blocks().len() as u32);
    debug_assert!(use_sets_per_block.len() == func.blocks().len() as u32);

    // Calculate live-in and live-out sets per block, using the traditional
    // iterate-to-a-fixed-point scheme.
    // `liveout_sets_per_block` is amended below for return blocks, hence `mut`.

    let (livein_sets_per_block, mut liveout_sets_per_block) = calc_livein_and_liveout(
        func,
        &def_sets_per_block,
        &use_sets_per_block,
        &cfg_info,
        &reg_universe,
    );
    debug_assert!(livein_sets_per_block.len() == func.blocks().len() as u32);
    debug_assert!(liveout_sets_per_block.len() == func.blocks().len() as u32);

    // Verify livein set of entry block against liveins specified by function (e.g., ABI params).
    let func_liveins = SparseSet::from_vec(
        func.func_liveins()
            .to_vec()
            .into_iter()
            .map(|rreg| rreg.to_reg())
            .collect(),
    );
    if !livein_sets_per_block[func.entry_block()].is_subset_of(&func_liveins) {
        let mut regs = livein_sets_per_block[func.entry_block()].clone();
        regs.remove(&func_liveins);
        return Err(AnalysisError::EntryLiveinValues(regs.to_vec()));
    }

    // Add function liveouts to every block ending in a return.
    let func_liveouts = SparseSet::from_vec(
        func.func_liveouts()
            .to_vec()
            .into_iter()
            .map(|rreg| rreg.to_reg())
            .collect(),
    );
    for block in func.blocks() {
        let last_iix = func.block_insns(block).last();
        if func.is_ret(last_iix) {
            liveout_sets_per_block[block].union(&func_liveouts);
        }
    }

    info!("  run_analysis: end data flow analysis");

    info!("  run_analysis: begin liveness analysis");
    let (frag_ixs_per_reg, mut frag_env, frag_metrics_env, vreg_classes) = get_range_frags(
        func,
        &reg_vecs_and_bounds,
        &reg_universe,
        &livein_sets_per_block,
        &liveout_sets_per_block,
    );

    let (mut fixed_intervals, virtual_intervals) = merge_range_frags(
        &reg_universe,
        &frag_ixs_per_reg,
        &mut frag_env,
        &frag_metrics_env,
        &cfg_info,
        &vreg_classes,
    );
    info!("  run_analysis: end liveness analysis");

    // Finalize interval construction by doing some last minute sort of the fixed intervals.
    for fixed in fixed_intervals.iter_mut() {
        fixed.frags.sort_unstable_by_key(|frag| frag.first);
    }
    let intervals = Intervals {
        virtuals: virtual_intervals,
        fixeds: fixed_intervals,
    };

    info!("run_analysis: end");

    Ok(AnalysisInfo {
        reg_vecs_and_bounds,
        intervals,
        liveins: livein_sets_per_block,
        liveouts: liveout_sets_per_block,
        _loop_depth: cfg_info.depth_map,
        _inst_to_block_map: inst_to_block_map,
    })
}

/// Calculate all the RangeFrags for `bix`.  Add them to `out_frags` and
/// corresponding metrics data to `out_frag_metrics`.  Add to `out_map`, the
/// associated RangeFragIxs, segregated by Reg.  `bix`, `livein`, `liveout` and
/// `rvb` are expected to be valid in the context of the Func `f` (duh!).
#[inline(never)]
fn get_range_frags_for_block<F: Function>(
    func: &F,
    rvb: &RegVecsAndBounds,
    reg_universe: &RealRegUniverse,
    vreg_classes: &Vec<RegClass>,
    bix: BlockIx,
    livein: &SparseSet<Reg>,
    liveout: &SparseSet<Reg>,
    // Temporary state reusable across function calls.
    visited: &mut Vec<u32>,
    state: &mut Vec</*rreg index, then vreg index, */ Option<RangeFrag>>,
    // Effectively results.
    out_map: &mut Vec<SmallVec<[RangeFragIx; 8]>>,
    out_frags: &mut Vec<RangeFrag>,
    out_frag_metrics: &mut Vec<RangeFragMetrics>,
) {
    let mut emit_range_frag =
        |r: Reg, frag: RangeFrag, frag_metrics: RangeFragMetrics, num_real_regs: u32| {
            let fix = RangeFragIx::new(out_frags.len() as u32);
            out_frags.push(frag);
            out_frag_metrics.push(frag_metrics);

            let out_map_index = reg_to_reg_ix(num_real_regs, r) as usize;
            out_map[out_map_index].push(fix);
        };

    // Some handy constants.
    debug_assert!(func.block_insns(bix).len() >= 1);
    let first_pt_in_block = InstPoint::new_use(func.block_insns(bix).first());
    let last_pt_in_block = InstPoint::new_def(func.block_insns(bix).last());

    // Clear the running state.
    visited.clear();

    let num_real_regs = reg_universe.regs.len() as u32;

    // First, set up `state` as if all of `livein` had been written just prior to the block.
    for r in livein.iter() {
        let r_state_ix = reg_to_reg_ix(num_real_regs, *r) as usize;
        debug_assert!(state[r_state_ix].is_none());
        state[r_state_ix] = Some(RangeFrag {
            mentions: MentionMap::new(),
            first: first_pt_in_block,
            last: first_pt_in_block,
        });
        visited.push(r_state_ix as u32);
    }

    // Now visit each instruction in turn, examining first the registers it reads, then those it
    // modifies, and finally those it writes.
    for iix in func.block_insns(bix) {
        let bounds_for_iix = &rvb.bounds[iix];

        // Examine reads: they extend an existing RangeFrag to the U point of the reading
        // insn.
        for i in bounds_for_iix.uses_start as usize
            ..bounds_for_iix.uses_start as usize + bounds_for_iix.uses_len as usize
        {
            let r = &rvb.vecs.uses[i];
            let r_state_ix = reg_to_reg_ix(num_real_regs, *r) as usize;

            // There has to be an entry, otherwise we'd do a read of a register not listed in
            // liveins.
            let pf = match &mut state[r_state_ix] {
                None => panic!("get_range_frags_for_block: fail #1"),
                Some(ref mut pf) => pf,
            };

            // This the first or subsequent read after a write.  Note that the "write" can be
            // either a real write, or due to the fact that `r` is listed in `livein`.  We don't
            // care here.
            let new_last = InstPoint::new_use(iix);
            debug_assert!(pf.last <= new_last);
            pf.last = new_last;

            // This first loop iterates over all the uses for the first time, so there shouldn't be
            // any duplicates.
            debug_assert!(!pf.mentions.iter().any(|tuple| tuple.0 == iix));
            let mut mention_set = Mention::new();
            mention_set.add_use();
            pf.mentions.push((iix, mention_set));
        }

        // Examine modifies.  These are handled almost identically to
        // reads, except that they extend an existing RangeFrag down to
        // the D point of the modifying insn.
        for i in bounds_for_iix.mods_start as usize
            ..bounds_for_iix.mods_start as usize + bounds_for_iix.mods_len as usize
        {
            let r = &rvb.vecs.mods[i];
            let r_state_ix = reg_to_reg_ix(num_real_regs, *r) as usize;

            // There has to be an entry here too.
            let pf = match &mut state[r_state_ix] {
                None => panic!("get_range_frags_for_block: fail #2"),
                Some(ref mut pf) => pf,
            };

            // This the first or subsequent modify after a write.
            let new_last = InstPoint::new_def(iix);
            debug_assert!(pf.last <= new_last);
            pf.last = new_last;

            pf.mentions.push((iix, {
                let mut mention_set = Mention::new();
                mention_set.add_mod();
                mention_set
            }));
        }

        // Examine writes (but not writes implied by modifies).  The general idea is that a write
        // causes us to terminate the existing RangeFrag, if any, add it to the results,
        // and start a new frag.
        for i in bounds_for_iix.defs_start as usize
            ..bounds_for_iix.defs_start as usize + bounds_for_iix.defs_len as usize
        {
            let r = &rvb.vecs.defs[i];
            let r_state_ix = reg_to_reg_ix(num_real_regs, *r) as usize;

            match &mut state[r_state_ix] {
                // First mention of a Reg we've never heard of before.
                // Start a new RangeFrag for it and keep going.
                None => {
                    let new_pt = InstPoint::new_def(iix);
                    let mut mention_set = Mention::new();
                    mention_set.add_def();
                    state[r_state_ix] = Some(RangeFrag {
                        first: new_pt,
                        last: new_pt,
                        mentions: smallvec![(iix, mention_set)],
                    })
                }

                // There's already a RangeFrag for `r`.  This write will start a new one, so
                // flush the existing one and note this write.
                Some(RangeFrag {
                    ref mut first,
                    ref mut last,
                    ref mut mentions,
                }) => {
                    // Steal the mentions and replace the mutable ref by an empty vector for reuse.
                    let stolen_mentions = mem::replace(mentions, MentionMap::new());

                    let (frag, frag_metrics) =
                        RangeFrag::new(func, bix, *first, *last, stolen_mentions);
                    emit_range_frag(*r, frag, frag_metrics, num_real_regs);

                    let mut mention_set = Mention::new();
                    mention_set.add_def();
                    mentions.push((iix, mention_set));

                    // Reuse the previous entry for this new definition of the same vreg.
                    let new_pt = InstPoint::new_def(iix);
                    *first = new_pt;
                    *last = new_pt;
                }
            }

            visited.push(r_state_ix as u32);
        }
    }

    // We are at the end of the block.  We still have to deal with live-out Regs.  We must also
    // deal with RangeFrag in `state` that are for registers not listed as live-out.

    // Deal with live-out Regs.  Treat each one as if it is read just after the block.
    for r in liveout.iter() {
        // Remove the entry from `state` so that the following loop doesn't process it again.
        let r_state_ix = reg_to_reg_ix(num_real_regs, *r) as usize;
        let entry = mem::replace(&mut state[r_state_ix], None);
        match entry {
            None => panic!("get_range_frags_for_block: fail #3"),
            Some(pf) => {
                let (frag, frag_metrics) =
                    RangeFrag::new(func, bix, pf.first, last_pt_in_block, pf.mentions);
                emit_range_frag(*r, frag, frag_metrics, num_real_regs);
            }
        }
    }

    // Finally, round up any remaining RangeFrag left in `state`.
    for r_state_ix in visited {
        if let Some(pf) = &mut state[*r_state_ix as usize] {
            let r = reg_ix_to_reg(reg_universe, vreg_classes, *r_state_ix);
            let (frag, frag_metrics) = RangeFrag::new(
                func,
                bix,
                pf.first,
                pf.last,
                mem::replace(&mut pf.mentions, MentionMap::new()),
            );
            emit_range_frag(r, frag, frag_metrics, num_real_regs);
            state[*r_state_ix as usize] = None;
        }
    }
}

#[inline(never)]
fn get_range_frags<F: Function>(
    func: &F,
    rvb: &RegVecsAndBounds,
    reg_universe: &RealRegUniverse,
    liveins: &TypedIxVec<BlockIx, SparseSet<Reg>>,
    liveouts: &TypedIxVec<BlockIx, SparseSet<Reg>>,
) -> (
    Vec</*rreg index, then vreg index, */ SmallVec<[RangeFragIx; 8]>>,
    Vec<RangeFrag>,
    Vec<RangeFragMetrics>,
    Vec</*vreg index,*/ RegClass>,
) {
    info!("    get_range_frags: begin");
    debug_assert!(liveins.len() == func.blocks().len() as u32);
    debug_assert!(liveouts.len() == func.blocks().len() as u32);
    debug_assert!(rvb.is_sanitized());

    let mut vreg_classes = vec![RegClass::INVALID; func.get_num_vregs()];
    for r in rvb
        .vecs
        .uses
        .iter()
        .chain(rvb.vecs.defs.iter())
        .chain(rvb.vecs.mods.iter())
    {
        if r.is_real() {
            continue;
        }
        let r_ix = r.get_index();
        let vreg_classes_ptr = &mut vreg_classes[r_ix];
        if *vreg_classes_ptr == RegClass::INVALID {
            *vreg_classes_ptr = r.get_class();
        } else {
            debug_assert_eq!(*vreg_classes_ptr, r.get_class());
        }
    }

    let num_real_regs = reg_universe.regs.len();
    let num_virtual_regs = vreg_classes.len();
    let num_regs = num_real_regs + num_virtual_regs;

    // Reused by the function below.
    let mut tmp_state = vec![None; num_regs];
    let mut tmp_visited = Vec::with_capacity(32);

    let mut result_map = vec![SmallVec::new(); num_regs];
    let mut result_frags = Vec::new();
    let mut result_frag_metrics = Vec::new();
    for bix in func.blocks() {
        get_range_frags_for_block(
            func,
            &rvb,
            reg_universe,
            &vreg_classes,
            bix,
            &liveins[bix],
            &liveouts[bix],
            &mut tmp_visited,
            &mut tmp_state,
            &mut result_map,
            &mut result_frags,
            &mut result_frag_metrics,
        );
    }

    assert!(tmp_state.len() == num_regs);
    assert!(result_map.len() == num_regs);
    assert!(vreg_classes.len() == num_virtual_regs);
    // This is pretty cheap (once per fn) and any failure will be catastrophic since it means we
    // may have forgotten some live range fragments.  Hence `assert!` and not `debug_assert!`.
    for state_elem in &tmp_state {
        assert!(state_elem.is_none());
    }

    if log_enabled!(Level::Debug) {
        debug!("");
        let mut n = 0;
        for frag in result_frags.iter() {
            debug!("{:<3?}   {:?}", RangeFragIx::new(n), frag);
            n += 1;
        }

        debug!("");
        for (reg_ix, frag_ixs) in result_map.iter().enumerate() {
            if frag_ixs.len() == 0 {
                continue;
            }
            let reg = reg_ix_to_reg(reg_universe, &vreg_classes, reg_ix as u32);
            debug!(
                "frags for {}   {:?}",
                reg.show_with_rru(reg_universe),
                frag_ixs
            );
        }
    }

    info!("    get_range_frags: end");
    assert!(result_frags.len() == result_frag_metrics.len());

    (result_map, result_frags, result_frag_metrics, vreg_classes)
}

#[inline(never)]
fn merge_range_frags(
    reg_universe: &RealRegUniverse,
    frag_ix_vec_per_reg: &[SmallVec<[RangeFragIx; 8]>],
    frag_env: &mut Vec<RangeFrag>,
    frag_metrics_env: &Vec<RangeFragMetrics>,
    cfg_info: &CFGInfo,
    vreg_classes: &Vec</*vreg index,*/ RegClass>,
) -> (Vec<FixedInterval>, Vec<VirtualInterval>) {
    info!("    merge_range_frags: begin");
    if log_enabled!(Level::Info) {
        let mut stats_num_total_incoming_frags = 0;
        for all_frag_ixs_for_reg in frag_ix_vec_per_reg.iter() {
            stats_num_total_incoming_frags += all_frag_ixs_for_reg.len();
        }
        info!("      in: {} in frag_env", frag_env.len());
        info!(
            "      in: {} regs containing in total {} frags",
            frag_ix_vec_per_reg.len(),
            stats_num_total_incoming_frags
        );
    }

    debug_assert!(frag_env.len() == frag_metrics_env.len());

    // Prefill fixed intervals, one per real register.
    let mut result_fixed = Vec::with_capacity(reg_universe.regs.len() as usize);
    for rreg in reg_universe.regs.iter() {
        result_fixed.push(FixedInterval {
            reg: rreg.0,
            frags: Vec::new(),
        });
    }

    let mut result_virtual = Vec::new();

    let mut triples = Vec::<(RangeFragIx, RangeFragKind, BlockIx)>::new();

    // BEGIN per_reg_loop
    for (reg_ix, all_frag_ixs_for_reg) in frag_ix_vec_per_reg.iter().enumerate() {
        let reg = reg_ix_to_reg(reg_universe, vreg_classes, reg_ix as u32);

        let num_reg_frags = all_frag_ixs_for_reg.len();

        // The reg might never have been mentioned at all, especially if it's a real reg.
        if num_reg_frags == 0 {
            continue;
        }

        // Do some shortcutting.  First off, if there's only one frag for this reg, we can directly
        // give it its own live range, and have done.
        if num_reg_frags == 1 {
            flush_interval(
                &mut result_fixed,
                &mut result_virtual,
                reg,
                all_frag_ixs_for_reg,
                frag_env,
            );
            continue;
        }

        // BEGIN merge `all_frag_ixs_for_reg` entries as much as possible.
        // but .. if we come across independents (RangeKind::Local), pull them out
        // immediately.
        triples.clear();

        // Create `triples`.  We will use it to guide the merging phase, but it is immutable there.
        for fix in all_frag_ixs_for_reg {
            let frag_metrics = &frag_metrics_env[fix.get() as usize];

            if frag_metrics.kind == RangeFragKind::Local {
                // This frag is Local (standalone).  Give it its own Range and move on.  This is an
                // optimisation, but it's also necessary: the main fragment-merging logic below
                // relies on the fact that the fragments it is presented with are all either
                // LiveIn, LiveOut or Thru.
                flush_interval(
                    &mut result_fixed,
                    &mut result_virtual,
                    reg,
                    &[*fix],
                    frag_env,
                );
                continue;
            }

            // This frag isn't Local (standalone) so we have to process it the slow way.
            triples.push((*fix, frag_metrics.kind, frag_metrics.bix));
        }

        let triples_len = triples.len();

        // This is the core of the merging algorithm.
        //
        // For each ix@(fix, kind, bix) in `triples` (order unimportant):
        //
        // (1) "Merge with blocks that are live 'downstream' from here":
        //     if fix is live-out or live-through:
        //        for b in succs[bix]
        //           for each ix2@(fix2, kind2, bix2) in `triples`
        //              if bix2 == b && kind2 is live-in or live-through:
        //                  merge(ix, ix2)
        //
        // (2) "Merge with blocks that are live 'upstream' from here":
        //     if fix is live-in or live-through:
        //        for b in preds[bix]
        //           for each ix2@(fix2, kind2, bix2) in `triples`
        //              if bix2 == b && kind2 is live-out or live-through:
        //                  merge(ix, ix2)
        //
        // `triples` remains unchanged.  The equivalence class info is accumulated
        // in `eclasses_uf` instead.  `eclasses_uf` entries are indices into
        // `triples`.
        //
        // Now, you might think it necessary to do both (1) and (2).  But no, they
        // are mutually redundant, since if two blocks are connected by a live
        // flow from one to the other, then they are also connected in the other
        // direction.  Hence checking one of the directions is enough.
        let mut eclasses_uf = UnionFind::<usize>::new(triples_len);

        // We have two schemes for group merging, one of which is N^2 in the
        // length of triples, the other is N-log-N, but with higher constant
        // factors.  Some experimentation with the bz2 test on a Cortex A57 puts
        // the optimal crossover point between 200 and 300; it's not critical.
        // Having this protects us against bad behaviour for huge inputs whilst
        // still being fast for small inputs.
        if triples_len <= 250 {
            // The simple way, which is N^2 in the length of `triples`.
            for (ix, (_fix, kind, bix)) in triples.iter().enumerate() {
                // Deal with liveness flows outbound from `fix`. Meaning, (1) above.
                if *kind == RangeFragKind::LiveOut || *kind == RangeFragKind::Thru {
                    for b in cfg_info.succ_map[*bix].iter() {
                        // Visit all entries in `triples` that are for `b`.
                        for (ix2, (_fix2, kind2, bix2)) in triples.iter().enumerate() {
                            if *bix2 != *b || *kind2 == RangeFragKind::LiveOut {
                                continue;
                            }
                            debug_assert!(
                                *kind2 == RangeFragKind::LiveIn || *kind2 == RangeFragKind::Thru
                            );
                            // Now we know that liveness for this reg "flows" from `triples[ix]` to
                            // `triples[ix2]`.  So those two frags must be part of the same live
                            // range.  Note this.
                            if ix != ix2 {
                                eclasses_uf.union(ix, ix2); // Order of args irrelevant
                            }
                        }
                    }
                }
            } // outermost iteration over `triples`
        } else {
            // The more complex way, which is N-log-N in the length of `triples`.  This is the same
            // as the simple way, except that the innermost loop, which is a linear search in
            // `triples` to find entries for some block `b`, is replaced by a binary search.  This
            // means that `triples` first needs to be sorted by block index.
            triples.sort_unstable_by_key(|(_, _, bix)| *bix);

            for (ix, (_fix, kind, bix)) in triples.iter().enumerate() {
                // Deal with liveness flows outbound from `fix`.  Meaning, (1) above.
                if *kind == RangeFragKind::LiveOut || *kind == RangeFragKind::Thru {
                    for b in cfg_info.succ_map[*bix].iter() {
                        // Visit all entries in `triples` that are for `b`.  Binary search
                        // `triples` to find the lowest-indexed entry for `b`.
                        let mut ix_left = 0;
                        let mut ix_right = triples_len;
                        while ix_left < ix_right {
                            let m = (ix_left + ix_right) >> 1;
                            if triples[m].2 < *b {
                                ix_left = m + 1;
                            } else {
                                ix_right = m;
                            }
                        }

                        // It might be that there is no block for `b` in the sequence.  That's
                        // legit; it just means that block `bix` jumps to a successor where the
                        // associated register isn't live-in/thru.  A failure to find `b` can be
                        // indicated one of two ways:
                        //
                        // * ix_left == triples_len
                        // * ix_left < triples_len and b < triples[ix_left].b
                        //
                        // In both cases I *think* the 'loop_over_entries_for_b below will not do
                        // anything.  But this is all a bit hairy, so let's convert the second
                        // variant into the first, so as to make it obvious that the loop won't do
                        // anything.

                        // ix_left now holds the lowest index of any `triples` entry for block `b`.
                        // Assert this.
                        if ix_left < triples_len && *b < triples[ix_left].2 {
                            ix_left = triples_len;
                        }
                        if ix_left < triples_len {
                            assert!(ix_left == 0 || triples[ix_left - 1].2 < *b);
                        }

                        // ix2 plays the same role as in the quadratic version.  ix_left and
                        // ix_right are not used after this point.
                        let mut ix2 = ix_left;
                        loop {
                            let (_fix2, kind2, bix2) = match triples.get(ix2) {
                                None => break,
                                Some(triple) => *triple,
                            };
                            if *b < bix2 {
                                // We've come to the end of the sequence of `b`-blocks.
                                break;
                            }
                            debug_assert!(*b == bix2);
                            if kind2 == RangeFragKind::LiveOut {
                                ix2 += 1;
                                continue;
                            }
                            // Now we know that liveness for this reg "flows" from `triples[ix]` to
                            // `triples[ix2]`.  So those two frags must be part of the same live
                            // range.  Note this.
                            eclasses_uf.union(ix, ix2);
                            ix2 += 1;
                        }

                        if ix2 + 1 < triples_len {
                            debug_assert!(*b < triples[ix2 + 1].2);
                        }
                    }
                }
            }
        }

        // Now `eclasses_uf` contains the results of the merging-search.  Visit each of its
        // equivalence classes in turn, and convert each into a virtual or real live range as
        // appropriate.
        let eclasses = eclasses_uf.get_equiv_classes();
        for leader_triple_ix in eclasses.equiv_class_leaders_iter() {
            // `leader_triple_ix` is an eclass leader.  Enumerate the whole eclass.
            let mut frag_ixs = SmallVec::<[RangeFragIx; 4]>::new();
            for triple_ix in eclasses.equiv_class_elems_iter(leader_triple_ix) {
                frag_ixs.push(triples[triple_ix].0 /*first field is frag ix*/);
            }
            flush_interval(
                &mut result_fixed,
                &mut result_virtual,
                reg,
                &frag_ixs,
                frag_env,
            );
        }
        // END merge `all_frag_ixs_for_reg` entries as much as possible
    } // END per reg loop

    info!("    merge_range_frags: end");

    (result_fixed, result_virtual)
}

#[inline(never)]
fn flush_interval(
    result_real: &mut Vec<FixedInterval>,
    result_virtual: &mut Vec<VirtualInterval>,
    reg: Reg,
    frag_ixs: &[RangeFragIx],
    frags: &mut Vec<RangeFrag>,
) {
    if reg.is_real() {
        // Append all the RangeFrags to this fixed interval. They'll get sorted later.
        result_real[reg.to_real_reg().get_index()]
            .frags
            .extend(frag_ixs.iter().map(|&i| {
                let frag = &mut frags[i.get() as usize];
                RangeFrag {
                    first: frag.first,
                    last: frag.last,
                    mentions: mem::replace(&mut frag.mentions, MentionMap::new()),
                }
            }));
        return;
    }

    debug_assert!(reg.is_virtual());

    let (start, end, mentions) = {
        // Merge all the mentions together.
        let capacity = frag_ixs
            .iter()
            .map(|fix| frags[fix.get() as usize].mentions.len())
            .fold(0, |a, b| a + b);

        let mut start = InstPoint::max_value();
        let mut end = InstPoint::min_value();

        // TODO rework this!
        let mut mentions = MentionMap::with_capacity(capacity);
        for frag in frag_ixs.iter().map(|fix| &frags[fix.get() as usize]) {
            mentions.extend(frag.mentions.iter().cloned());
            start = InstPoint::min(start, frag.first);
            end = InstPoint::max(end, frag.last);
        }
        mentions.sort_unstable_by_key(|tuple| tuple.0);

        // Merge mention set that are at the same instruction.
        let mut s = 0;
        let mut e;
        let mut to_remove = Vec::new();
        while s < mentions.len() {
            e = s;
            while e + 1 < mentions.len() && mentions[s].0 == mentions[e + 1].0 {
                e += 1;
            }
            if s != e {
                let mut i = s + 1;
                while i <= e {
                    if mentions[i].1.is_use() {
                        mentions[s].1.add_use();
                    }
                    if mentions[i].1.is_mod() {
                        mentions[s].1.add_mod();
                    }
                    if mentions[i].1.is_def() {
                        mentions[s].1.add_def();
                    }
                    i += 1;
                }
                for i in s + 1..=e {
                    to_remove.push(i);
                }
            }
            s = e + 1;
        }

        for &i in to_remove.iter().rev() {
            // TODO not efficient.
            mentions.remove(i);
        }

        (start, end, mentions)
    };

    let id = IntId(result_virtual.len());
    let mut int = VirtualInterval::new(id, reg.to_virtual_reg(), start, end, mentions);
    int.ancestor = Some(id);

    result_virtual.push(int);
}
