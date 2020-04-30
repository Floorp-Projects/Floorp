//! Main file / top-level module for regalloc library.
//!
//! We have tried hard to make the library's interface as simple as possible,
//! yet flexible enough that the allocators it implements can provide good
//! quality allocations in reasonable time.  Nevertheless, there is still
//! significant semantic complexity in parts of the interface.  If you intend
//! to use this library in your own code, you would be well advised to read
//! the comments in this file very carefully.

// Make the analysis module public for fuzzing.
#[cfg(feature = "fuzzing")]
pub mod analysis_main;
#[cfg(not(feature = "fuzzing"))]
mod analysis_main;

mod analysis_control_flow;
mod analysis_data_flow;
mod avl_tree;
mod bt_coalescing_analysis;
mod bt_commitment_map;
mod bt_main;
mod bt_spillslot_allocator;
mod bt_vlr_priority_queue;
mod checker;
mod data_structures;
mod inst_stream;
mod linear_scan;
mod sparse_set;
mod union_find;

use log::{info, log_enabled, Level};
use std::fmt;

// Stuff that is defined by the library

// Sets and maps of things.  We can refine these later; but for now the
// interface needs some way to speak about them, so let's use the
// library-provided versions.

pub use crate::data_structures::Map;
pub use crate::data_structures::Set;

// Register classes

pub use crate::data_structures::RegClass;

// Registers, both real and virtual, and ways to create them

pub use crate::data_structures::Reg;

pub use crate::data_structures::RealReg;
pub use crate::data_structures::VirtualReg;

pub use crate::data_structures::Writable;

pub use crate::data_structures::NUM_REG_CLASSES;

// Spill slots

pub use crate::data_structures::SpillSlot;

// The "register universe".  This describes the registers available to the
// allocator.  There are very strict requirements on the structure of the
// universe.  If you fail to observe these requirements, either the allocator
// itself, or the resulting code, will fail in mysterious ways, and your life
// will be miserable while you try to figure out what happened.  There are
// lower level details on the definition of RealRegUniverse which you also
// need to take note of.  The overall contract is as follows.
//
// === (1) === Basic structure ===
//
// A "register universe" is a read-only structure that contains all
// information about real registers on a given host.  For each register class
// (RegClass) supported by the target, the universe must provide a vector of
// registers that the allocator may use.
//
// The universe may also list other registers that the incoming
// virtual-registerised code may use, but which are not available for use by
// the allocator.  Indeed, the universe *must* list *all* registers that will
// ever be mentioned in the incoming code.  Failure to do so will cause the
// allocator's analysis phase to return an error.
//
// === (2) === Ordering of registers within each class
//
// The ordering of available registers within these vectors does not affect
// the correctness of the final allocation.  However, it will affect the
// quality of final allocation.  Clients are recommended to list, for each
// class, the callee-saved registers first, and the caller-saved registers
// after that.  The currently supported allocation algorithms (Backtracking
// and LinearScan) will try to use the first available registers in each
// class, that is to say, callee-saved ones first.  The purpose of this is to
// try and minimise spilling around calls by avoiding use of caller-saved ones
// if possible.
//
// There is a twist here, however.  The abovementioned heuristic works well
// for non-leaf functions (functions that contain at least one call).  But for
// leaf functions, we would prefer to use the caller-saved registers first,
// since doing so has potential to minimise the number of registers that must
// be saved/restored in the prologue and epilogue.  Presently there is no way
// to tell this interface that the function is a leaf function, and so the
// only way to get optimal code in this case is to present a universe with the
// registers listed in the opposite order.
//
// This is of course inconvenient for the caller, since it requires
// maintenance of two separate universes.  In the future we will add a boolean
// parameter to the top level function `allocate_registers` that indicates
// that whether or not the function is a leaf function.
//
// === (3) === The "suggested scratch register" ===
//
// Some allocation algorithms, particularly linear-scan, may need to have a
// scratch register available for their own use.  The register universe must
// nominate a scratch register in each class, specified in
// RealRegUniverse::allocable_by_class[..]::Some(suggested_scratch).  The
// choice of scratch register is influenced by the architecture, the ABI, and
// client-side fixed-use register conventions.  There rules are as follows:
//
// (1) For each class, the universe must offer a reserved register.
//
// (2) The reserved register may not have any implied-by-the architecture
//     reads/modifies/writes for any instruction in the vcode.  Unfortunately
//     there is no easy way for this library to check that.
//
// (3) The reserved register must not have any reads or modifies by any
//     instruction in the vcode.  In other words, it must not be handed to
//     either the `add_use` or `add_mod` function of the `RegUsageCollector`
//     that is presented to the client's `get_regs` function.  If any such
//     mention is detected, the library will return an error.
//
// (4) The reserved reg may be mentioned as written by instructions in the
//     vcode, though -- in other words it may be handed to `add_def`.  The
//     library will tolerate and correctly handle that.  However, because no
//     vcode instruction may read or modify the reserved register, all such
//     writes are "dead".  This in turn guarantees that the allocator can, if
//     it wants, change the value in it at any time, without changing the
//     behaviour of the final generated code.
//
// Currently, the LinearScan algorithm may use the reserved registers.  The
// Backtracking algorithm will ignore the hints and treat them as "normal"
// allocatable registers.

pub use crate::data_structures::RealRegUniverse;
pub use crate::data_structures::RegClassInfo;

// A structure for collecting information about which registers each
// instruction uses.

pub use crate::data_structures::RegUsageCollector;

// A structure for providing mapping results for a given instruction.

pub use crate::data_structures::RegUsageMapper;

// TypedIxVector, so that the interface can speak about vectors of blocks and
// instructions.

pub use crate::data_structures::TypedIxVec;
pub use crate::data_structures::{BlockIx, InstIx, Range};

/// A trait defined by the regalloc client to provide access to its
/// machine-instruction / CFG representation.
pub trait Function {
    /// Regalloc is parameterized on F: Function and so can use the projected
    /// type F::Inst.
    type Inst: Clone + fmt::Debug;

    // -------------
    // CFG traversal
    // -------------

    /// Allow access to the underlying vector of instructions.
    fn insns(&self) -> &[Self::Inst];

    /// Get all instruction indices as an iterable range.
    fn insn_indices(&self) -> Range<InstIx> {
        Range::new(InstIx::new(0), self.insns().len())
    }

    /// Allow mutable access to the underlying vector of instructions.
    fn insns_mut(&mut self) -> &mut [Self::Inst];

    /// Get an instruction with a type-safe InstIx index.
    fn get_insn(&self, insn: InstIx) -> &Self::Inst;

    /// Get a mutable borrow of an instruction with the given type-safe InstIx
    /// index.
    fn get_insn_mut(&mut self, insn: InstIx) -> &mut Self::Inst;

    /// Allow iteration over basic blocks (in instruction order).
    fn blocks(&self) -> Range<BlockIx>;

    /// Get the index of the entry block.
    fn entry_block(&self) -> BlockIx;

    /// Provide the range of instruction indices contained in each block.
    fn block_insns(&self, block: BlockIx) -> Range<InstIx>;

    /// Get CFG successors for a given block.
    fn block_succs(&self, block: BlockIx) -> Vec<BlockIx>;

    /// Determine whether an instruction is a return instruction.
    fn is_ret(&self, insn: InstIx) -> bool;

    // --------------------------
    // Instruction register slots
    // --------------------------

    /// Add to `collector` the used, defined, and modified registers for an
    /// instruction.
    fn get_regs(insn: &Self::Inst, collector: &mut RegUsageCollector);

    /// Map each register slot through a virtual-to-real mapping indexed
    /// by virtual register. The two separate maps in `maps.pre` and
    /// `maps.post` provide the mapping to use for uses (which semantically
    /// occur just prior to the instruction's effect) and defs (which
    /// semantically occur just after the instruction's effect). Regs that were
    /// "modified" can use either map; the vreg should be the same in both.
    ///
    /// Note that this does not take a `self`, because we want to allow the
    /// regalloc to have a mutable borrow of an insn (which borrows the whole
    /// Function in turn) outstanding while calling this.
    fn map_regs(insn: &mut Self::Inst, maps: &RegUsageMapper);

    /// Allow the regalloc to query whether this is a move. Returns (dst, src).
    fn is_move(&self, insn: &Self::Inst) -> Option<(Writable<Reg>, Reg)>;

    /// Get an estimate of how many `VirtualReg` indices are used, if available, to allow
    /// preallocating data structures.
    fn get_vreg_count_estimate(&self) -> Option<usize> {
        None
    }

    // --------------
    // Spills/reloads
    // --------------

    /// How many logical spill slots does the given regclass require?  E.g., on a
    /// 64-bit machine, spill slots may nominally be 64-bit words, but a 128-bit
    /// vector value will require two slots.  The regalloc will always align on
    /// this size.
    ///
    /// This passes the associated virtual register to the client as well,
    /// because the way in which we spill a real register may depend on the
    /// value that we are using it for. E.g., if a machine has V128 registers
    /// but we also use them for F32 and F64 values, we may use a different
    /// store-slot size and smaller-operand store/load instructions for an F64
    /// than for a true V128.
    fn get_spillslot_size(&self, regclass: RegClass, for_vreg: VirtualReg) -> u32;

    /// Generate a spill instruction for insertion into the instruction
    /// sequence. The associated virtual register (whose value is being spilled)
    /// is passed so that the client may make decisions about the instruction to
    /// generate based on the type of value in question.  Because the register
    /// allocator will insert spill instructions at arbitrary points, the
    /// returned instruction here must not modify the machine's condition codes.
    fn gen_spill(&self, to_slot: SpillSlot, from_reg: RealReg, for_vreg: VirtualReg) -> Self::Inst;

    /// Generate a reload instruction for insertion into the instruction
    /// sequence. The associated virtual register (whose value is being loaded)
    /// is passed as well.  The returned instruction must not modify the
    /// machine's condition codes.
    fn gen_reload(
        &self,
        to_reg: Writable<RealReg>,
        from_slot: SpillSlot,
        for_vreg: VirtualReg,
    ) -> Self::Inst;

    /// Generate a register-to-register move for insertion into the instruction
    /// sequence. The associated virtual register is passed as well.  The
    /// returned instruction must not modify the machine's condition codes.
    fn gen_move(
        &self,
        to_reg: Writable<RealReg>,
        from_reg: RealReg,
        for_vreg: VirtualReg,
    ) -> Self::Inst;

    /// Generate an instruction which is a no-op and has zero length.
    fn gen_zero_len_nop(&self) -> Self::Inst;

    /// Try to alter an existing instruction to use a value directly in a
    /// spillslot (accessing memory directly) instead of the given register. May
    /// be useful on ISAs that have mem/reg ops, like x86.
    ///
    /// Note that this is not *quite* just fusing a load with the op; if the
    /// value is def'd or modified, it should be written back to the spill slot
    /// as well. In other words, it is just using the spillslot as if it were a
    /// real register, for reads and/or writes.
    ///
    /// FIXME JRS 2020Feb06: state precisely the constraints on condition code
    /// changes.
    fn maybe_direct_reload(
        &self,
        insn: &Self::Inst,
        reg: VirtualReg,
        slot: SpillSlot,
    ) -> Option<Self::Inst>;

    // ----------------------------------------------------------
    // Function liveins, liveouts, and direct-mode real registers
    // ----------------------------------------------------------

    /// Return the set of registers that should be considered live at the
    /// beginning of the function. This is semantically equivalent to an
    /// instruction at the top of the entry block def'ing all registers in this
    /// set.
    fn func_liveins(&self) -> Set<RealReg>;

    /// Return the set of registers that should be considered live at the
    /// end of the function (after every return instruction). This is
    /// semantically equivalent to an instruction at each block with no successors
    /// that uses each of these registers.
    fn func_liveouts(&self) -> Set<RealReg>;
}

/// The result of register allocation.  Note that allocation can fail!
pub struct RegAllocResult<F: Function> {
    /// A new sequence of instructions with all register slots filled with real
    /// registers, and spills/fills/moves possibly inserted (and unneeded moves
    /// elided).
    pub insns: Vec<F::Inst>,

    /// Basic-block start indices for the new instruction list, indexed by the
    /// original basic block indices. May be used by the client to, e.g., remap
    /// branch targets appropriately.
    pub target_map: TypedIxVec<BlockIx, InstIx>,

    /// Full mapping from new instruction indices to original instruction
    /// indices. May be needed by the client to, for example, update metadata
    /// such as debug/source-location info as the instructions are spliced
    /// and reordered.
    ///
    /// Each entry is an `InstIx`, but may be `InstIx::invalid_value()` if the
    /// new instruction at this new index was inserted by the allocator
    /// (i.e., if it is a load, spill or move instruction).
    pub orig_insn_map: TypedIxVec</* new */ InstIx, /* orig */ InstIx>,

    /// Which real registers were overwritten? This will contain all real regs
    /// that appear as defs or modifies in register slots of the output
    /// instruction list.  This will only list registers that are available to
    /// the allocator.  If one of the instructions clobbers a register which
    /// isn't available to the allocator, it won't be mentioned here.
    pub clobbered_registers: Set<RealReg>,

    /// How many spill slots were used?
    pub num_spill_slots: u32,

    /// Block annotation strings, for debugging.  Requires requesting in the
    /// call to `allocate_registers`.  Creating of these annotations is
    /// potentially expensive, so don't request them if you don't need them.
    pub block_annotations: Option<TypedIxVec<BlockIx, Vec<String>>>,
}

/// A choice of register allocation algorithm to run.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum RegAllocAlgorithm {
    Backtracking,
    BacktrackingChecked,
    LinearScan,
    LinearScanChecked,
}

pub use crate::analysis_main::AnalysisError;
pub use crate::checker::{CheckerError, CheckerErrors};

/// An error from the register allocator.
#[derive(Clone, Debug)]
pub enum RegAllocError {
    OutOfRegisters(RegClass),
    MissingSuggestedScratchReg(RegClass),
    Analysis(AnalysisError),
    RegChecker(CheckerErrors),
    Other(String),
}

impl fmt::Display for RegAllocError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:?}", self)
    }
}

/// Allocate registers for a function's code, given a universe of real
/// registers that we are allowed to use.
///
/// The control flow graph must not contain any critical edges, that is, any
/// edge coming from a block with multiple successors must not flow into a block
/// with multiple predecessors. The embedder must have split critical edges
/// before handing over the function to this function. Otherwise, an error will
/// be returned.
///
/// Allocate may succeed, returning a `RegAllocResult` with the new instruction
/// sequence, or it may fail, returning an error.
#[inline(never)]
pub fn allocate_registers<F: Function>(
    func: &mut F,
    algorithm: RegAllocAlgorithm,
    rreg_universe: &RealRegUniverse,
    request_block_annotations: bool,
) -> Result<RegAllocResult<F>, RegAllocError> {
    info!("");
    info!("================ regalloc.rs: BEGIN function ================");
    if log_enabled!(Level::Info) {
        let strs = rreg_universe.show();
        info!("using RealRegUniverse:");
        for s in strs {
            info!("  {}", s);
        }
    }
    let res = match algorithm {
        RegAllocAlgorithm::Backtracking | RegAllocAlgorithm::BacktrackingChecked => {
            let use_checker = algorithm == RegAllocAlgorithm::BacktrackingChecked;
            bt_main::alloc_main(func, rreg_universe, use_checker, request_block_annotations)
        }
        RegAllocAlgorithm::LinearScan | RegAllocAlgorithm::LinearScanChecked => {
            let use_checker = algorithm == RegAllocAlgorithm::LinearScanChecked;
            linear_scan::run(func, rreg_universe, use_checker)
        }
    };
    info!("================ regalloc.rs: END function ================");
    res
}
