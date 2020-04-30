//! Top level module for all analysis activities.

use log::{debug, info};

use crate::analysis_control_flow::{CFGInfo, InstIxToBlockIxMap};
use crate::analysis_data_flow::{
    calc_def_and_use, calc_livein_and_liveout, get_range_frags, get_sanitized_reg_uses_for_func,
    merge_range_frags, set_virtual_range_metrics,
};
use crate::data_structures::{
    BlockIx, RangeFrag, RangeFragIx, RealRange, RealRangeIx, RealReg, RealRegUniverse, Reg,
    RegVecsAndBounds, TypedIxVec, VirtualRange, VirtualRangeIx,
};
use crate::sparse_set::SparseSet;
use crate::Function;

//=============================================================================
// Overall analysis return results, for both control- and data-flow analyses.
// All of these failures refer to various problems with the code that the
// client (caller) supplied to us.

#[derive(Clone, Debug)]
pub enum AnalysisError {
    /// A critical edge from "from" to "to" has been found, and should have been
    /// removed by the caller in the first place.
    CriticalEdge { from: BlockIx, to: BlockIx },

    /// Some values in the entry block are live in to the function, but are not
    /// declared as such.
    EntryLiveinValues,

    /// The incoming code has an explicit or implicit mention (use, def or mod)
    /// of a real register, which either (1) isn't listed in the universe at
    /// all, or (2) is one of the `suggested_scratch` registers in the universe.
    /// (1) isn't allowed because the client must mention *all* real registers
    /// in the universe.  (2) isn't allowed because the client promises to us
    /// that the `suggested_scratch` registers really are completely unused in
    /// the incoming code, so that the allocator can use them at literally any
    /// point it wants.
    IllegalRealReg(RealReg),

    /// At least one block is dead.
    UnreachableBlocks,

    /// Implementation limits exceeded.  The incoming function is too big.  It
    /// may contain at most 1 million basic blocks and 16 million instructions.
    ImplementationLimitsExceeded,
}

impl ToString for AnalysisError {
    fn to_string(&self) -> String {
        match self {
      AnalysisError::CriticalEdge { from, to } => {
        format!("critical edge detected, from {:?} to {:?}", from, to)
      }
      AnalysisError::EntryLiveinValues => {
        "entry block has live-in value not present in function liveins".into()
      }
      AnalysisError::IllegalRealReg(reg) => {
        format!("instructions mention real register {:?}, which either isn't defined in the register universe, or is a 'suggested_scratch' register", reg)
      }
      AnalysisError::UnreachableBlocks => {
        "at least one block is unreachable".to_string()
      }
      AnalysisError::ImplementationLimitsExceeded => {
        "implementation limits exceeded (more than 1 million blocks or 16 million insns)".to_string()
      }
    }
    }
}

//=============================================================================
// Top level for all analysis activities.

#[inline(never)]
pub fn run_analysis<F: Function>(
    func: &F,
    reg_universe: &RealRegUniverse,
) -> Result<
    (
        // The sanitized per-insn reg-use info
        RegVecsAndBounds,
        // The real-reg live ranges
        TypedIxVec<RealRangeIx, RealRange>,
        // The virtual-reg live ranges
        TypedIxVec<VirtualRangeIx, VirtualRange>,
        // The fragment table
        TypedIxVec<RangeFragIx, RangeFrag>,
        // Liveouts per block
        TypedIxVec<BlockIx, SparseSet<Reg>>,
        // Estimated execution frequency per block
        TypedIxVec<BlockIx, u32>,
        // Maps InstIxs to BlockIxs
        InstIxToBlockIxMap,
    ),
    AnalysisError,
> {
    info!("run_analysis: begin");
    info!(
        "  run_analysis: {} blocks, {} insns",
        func.blocks().len(),
        func.insns().len()
    );
    info!("  run_analysis: begin control flow analysis");

    // First do control flow analysis.  This is (relatively) simple.  Note that
    // this can fail, for various reasons; we propagate the failure if so.
    let cfg_info = CFGInfo::create(func)?;

    // Create the InstIx-to-BlockIx map.  This isn't really control-flow
    // analysis, but needs to be done at some point.
    let inst_to_block_map = InstIxToBlockIxMap::new(func);

    // Annotate each Block with its estimated execution frequency
    let mut estimated_frequencies = TypedIxVec::new();
    for bix in func.blocks() {
        let mut estimated_frequency = 1;
        let mut depth = cfg_info.depth_map[bix];
        if depth > 3 {
            depth = 3;
        }
        for _ in 0..depth {
            estimated_frequency *= 10;
        }
        assert!(bix == BlockIx::new(estimated_frequencies.len()));
        estimated_frequencies.push(estimated_frequency);
    }

    info!("  run_analysis: end control flow analysis");

    // Now perform dataflow analysis.  This is somewhat more complex.
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

    // Verify livein set of entry block against liveins specified by function
    // (e.g., ABI params).
    let func_liveins = SparseSet::from_vec(
        func.func_liveins()
            .to_vec()
            .into_iter()
            .map(|rreg| rreg.to_reg())
            .collect(),
    );
    if !livein_sets_per_block[func.entry_block()].is_subset_of(&func_liveins) {
        return Err(AnalysisError::EntryLiveinValues);
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

    // Dataflow analysis is now complete.  Now compute the virtual and real live
    // ranges, in two steps: (1) compute RangeFrags, and (2) merge them
    // together, guided by flow and liveness info, so as to create the final
    // VirtualRanges and RealRanges.
    info!("  run_analysis: begin liveness analysis");

    let (frag_ixs_per_reg, frag_env) = get_range_frags(
        func,
        &livein_sets_per_block,
        &liveout_sets_per_block,
        &reg_vecs_and_bounds,
        &reg_universe,
    );

    let (rlr_env, mut vlr_env) = merge_range_frags(&frag_ixs_per_reg, &frag_env, &cfg_info);

    set_virtual_range_metrics(&mut vlr_env, &frag_env, &estimated_frequencies);

    debug_assert!(liveout_sets_per_block.len() == estimated_frequencies.len());

    debug!("");
    let mut n = 0;
    for rlr in rlr_env.iter() {
        debug!(
            "{:<4?}   {}",
            RealRangeIx::new(n),
            rlr.show_with_rru(&reg_universe)
        );
        n += 1;
    }

    debug!("");
    n = 0;
    for vlr in vlr_env.iter() {
        debug!("{:<4?}   {:?}", VirtualRangeIx::new(n), vlr);
        n += 1;
    }

    info!("  run_analysis: end liveness analysis");
    info!("run_analysis: end");

    Ok((
        reg_vecs_and_bounds,
        rlr_env,
        vlr_env,
        frag_env,
        liveout_sets_per_block,
        estimated_frequencies,
        inst_to_block_map,
    ))
}
