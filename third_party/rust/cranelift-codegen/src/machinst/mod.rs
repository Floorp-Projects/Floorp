//! This module exposes the machine-specific backend definition pieces.
//!
//! The MachInst infrastructure is the compiler backend, from CLIF
//! (ir::Function) to machine code. The purpose of this infrastructure is, at a
//! high level, to do instruction selection/lowering (to machine instructions),
//! register allocation, and then perform all the fixups to branches, constant
//! data references, etc., needed to actually generate machine code.
//!
//! The container for machine instructions, at various stages of construction,
//! is the `VCode` struct. We refer to a sequence of machine instructions organized
//! into basic blocks as "vcode". This is short for "virtual-register code", though
//! it's a bit of a misnomer because near the end of the pipeline, vcode has all
//! real registers. Nevertheless, the name is catchy and we like it.
//!
//! The compilation pipeline, from an `ir::Function` (already optimized as much as
//! you like by machine-independent optimization passes) onward, is as follows.
//! (N.B.: though we show the VCode separately at each stage, the passes
//! mutate the VCode in place; these are not separate copies of the code.)
//!
//! ```plain
//!
//!     ir::Function                (SSA IR, machine-independent opcodes)
//!         |
//!         |  [lower]
//!         |
//!     VCode<arch_backend::Inst>   (machine instructions:
//!         |                        - mostly virtual registers.
//!         |                        - cond branches in two-target form.
//!         |                        - branch targets are block indices.
//!         |                        - in-memory constants held by insns,
//!         |                          with unknown offsets.
//!         |                        - critical edges (actually all edges)
//!         |                          are split.)
//!         | [regalloc]
//!         |
//!     VCode<arch_backend::Inst>   (machine instructions:
//!         |                        - all real registers.
//!         |                        - new instruction sequence returned
//!         |                          out-of-band in RegAllocResult.
//!         |                        - instruction sequence has spills,
//!         |                          reloads, and moves inserted.
//!         |                        - other invariants same as above.)
//!         |
//!         | [preamble/postamble]
//!         |
//!     VCode<arch_backend::Inst>   (machine instructions:
//!         |                        - stack-frame size known.
//!         |                        - out-of-band instruction sequence
//!         |                          has preamble prepended to entry
//!         |                          block, and postamble injected before
//!         |                          every return instruction.
//!         |                        - all symbolic stack references to
//!         |                          stackslots and spillslots are resolved
//!         |                          to concrete FP-offset mem addresses.)
//!         |
//!         | [binary emission via MachBuffer
//!         |  with streaming branch resolution/simplification]
//!         |
//!     Vec<u8>                     (machine code!)
//!
//! ```

use crate::binemit::{CodeInfo, CodeOffset, StackMap};
use crate::ir::condcodes::IntCC;
use crate::ir::{Function, SourceLoc, StackSlot, Type, ValueLabel};
use crate::result::CodegenResult;
use crate::settings::{self, Flags};
use crate::value_label::ValueLabelsRanges;
use alloc::boxed::Box;
use alloc::vec::Vec;
use core::fmt::Debug;
use core::hash::Hasher;
use cranelift_entity::PrimaryMap;
use regalloc::RegUsageCollector;
use regalloc::{
    RealReg, RealRegUniverse, Reg, RegClass, RegUsageMapper, SpillSlot, VirtualReg, Writable,
};
use smallvec::{smallvec, SmallVec};
use std::string::String;
use target_lexicon::Triple;

#[cfg(feature = "unwind")]
use crate::isa::unwind::systemv::RegisterMappingError;

pub mod lower;
pub use lower::*;
pub mod vcode;
pub use vcode::*;
pub mod compile;
pub use compile::*;
pub mod blockorder;
pub use blockorder::*;
pub mod abi;
pub use abi::*;
pub mod abi_impl;
pub use abi_impl::*;
pub mod buffer;
pub use buffer::*;
pub mod adapter;
pub use adapter::*;
pub mod helpers;
pub use helpers::*;
pub mod inst_common;
pub use inst_common::*;
pub mod valueregs;
pub use valueregs::*;
pub mod debug;

/// A machine instruction.
pub trait MachInst: Clone + Debug {
    /// Return the registers referenced by this machine instruction along with
    /// the modes of reference (use, def, modify).
    fn get_regs(&self, collector: &mut RegUsageCollector);

    /// Map virtual registers to physical registers using the given virt->phys
    /// maps corresponding to the program points prior to, and after, this instruction.
    fn map_regs<RUM: RegUsageMapper>(&mut self, maps: &RUM);

    /// If this is a simple move, return the (source, destination) tuple of registers.
    fn is_move(&self) -> Option<(Writable<Reg>, Reg)>;

    /// Is this a terminator (branch or ret)? If so, return its type
    /// (ret/uncond/cond) and target if applicable.
    fn is_term<'a>(&'a self) -> MachTerminator<'a>;

    /// Returns true if the instruction is an epilogue placeholder.
    fn is_epilogue_placeholder(&self) -> bool;

    /// Should this instruction be included in the clobber-set?
    fn is_included_in_clobbers(&self) -> bool {
        true
    }

    /// If this is a load or store to the stack, return that info.
    fn stack_op_info(&self) -> Option<MachInstStackOpInfo> {
        None
    }

    /// Generate a move.
    fn gen_move(to_reg: Writable<Reg>, from_reg: Reg, ty: Type) -> Self;

    /// Generate a constant into a reg.
    fn gen_constant<F: FnMut(Type) -> Writable<Reg>>(
        to_regs: ValueRegs<Writable<Reg>>,
        value: u128,
        ty: Type,
        alloc_tmp: F,
    ) -> SmallVec<[Self; 4]>;

    /// Possibly operate on a value directly in a spill-slot rather than a
    /// register. Useful if the machine has register-memory instruction forms
    /// (e.g., add directly from or directly to memory), like x86.
    fn maybe_direct_reload(&self, reg: VirtualReg, slot: SpillSlot) -> Option<Self>;

    /// Determine register class(es) to store the given Cranelift type, and the
    /// Cranelift type actually stored in the underlying register(s).  May return
    /// an error if the type isn't supported by this backend.
    ///
    /// If the type requires multiple registers, then the list of registers is
    /// returned in little-endian order.
    ///
    /// Note that the type actually stored in the register(s) may differ in the
    /// case that a value is split across registers: for example, on a 32-bit
    /// target, an I64 may be stored in two registers, each of which holds an
    /// I32. The actually-stored types are used only to inform the backend when
    /// generating spills and reloads for individual registers.
    fn rc_for_type(ty: Type) -> CodegenResult<(&'static [RegClass], &'static [Type])>;

    /// Generate a jump to another target. Used during lowering of
    /// control flow.
    fn gen_jump(target: MachLabel) -> Self;

    /// Generate a NOP. The `preferred_size` parameter allows the caller to
    /// request a NOP of that size, or as close to it as possible. The machine
    /// backend may return a NOP whose binary encoding is smaller than the
    /// preferred size, but must not return a NOP that is larger. However,
    /// the instruction must have a nonzero size if preferred_size is nonzero.
    fn gen_nop(preferred_size: usize) -> Self;

    /// Get the register universe for this backend.
    fn reg_universe(flags: &Flags) -> RealRegUniverse;

    /// Align a basic block offset (from start of function).  By default, no
    /// alignment occurs.
    fn align_basic_block(offset: CodeOffset) -> CodeOffset {
        offset
    }

    /// What is the worst-case instruction size emitted by this instruction type?
    fn worst_case_size() -> CodeOffset;

    /// What is the register class used for reference types (GC-observable pointers)? Can
    /// be dependent on compilation flags.
    fn ref_type_regclass(_flags: &Flags) -> RegClass;

    /// Does this instruction define a ValueLabel? Returns the `Reg` whose value
    /// becomes the new value of the `ValueLabel` after this instruction.
    fn defines_value_label(&self) -> Option<(ValueLabel, Reg)> {
        None
    }

    /// Create a marker instruction that defines a value label.
    fn gen_value_label_marker(_label: ValueLabel, _reg: Reg) -> Self {
        Self::gen_nop(0)
    }

    /// A label-use kind: a type that describes the types of label references that
    /// can occur in an instruction.
    type LabelUse: MachInstLabelUse;
}

/// A descriptor of a label reference (use) in an instruction set.
pub trait MachInstLabelUse: Clone + Copy + Debug + Eq {
    /// Required alignment for any veneer. Usually the required instruction
    /// alignment (e.g., 4 for a RISC with 32-bit instructions, or 1 for x86).
    const ALIGN: CodeOffset;

    /// What is the maximum PC-relative range (positive)? E.g., if `1024`, a
    /// label-reference fixup at offset `x` is valid if the label resolves to `x
    /// + 1024`.
    fn max_pos_range(self) -> CodeOffset;
    /// What is the maximum PC-relative range (negative)? This is the absolute
    /// value; i.e., if `1024`, then a label-reference fixup at offset `x` is
    /// valid if the label resolves to `x - 1024`.
    fn max_neg_range(self) -> CodeOffset;
    /// What is the size of code-buffer slice this label-use needs to patch in
    /// the label's value?
    fn patch_size(self) -> CodeOffset;
    /// Perform a code-patch, given the offset into the buffer of this label use
    /// and the offset into the buffer of the label's definition.
    /// It is guaranteed that, given `delta = offset - label_offset`, we will
    /// have `offset >= -self.max_neg_range()` and `offset <=
    /// self.max_pos_range()`.
    fn patch(self, buffer: &mut [u8], use_offset: CodeOffset, label_offset: CodeOffset);
    /// Can the label-use be patched to a veneer that supports a longer range?
    /// Usually valid for jumps (a short-range jump can jump to a longer-range
    /// jump), but not for e.g. constant pool references, because the constant
    /// load would require different code (one more level of indirection).
    fn supports_veneer(self) -> bool;
    /// How many bytes are needed for a veneer?
    fn veneer_size(self) -> CodeOffset;
    /// Generate a veneer. The given code-buffer slice is `self.veneer_size()`
    /// bytes long at offset `veneer_offset` in the buffer. The original
    /// label-use will be patched to refer to this veneer's offset.  A new
    /// (offset, LabelUse) is returned that allows the veneer to use the actual
    /// label. For veneers to work properly, it is expected that the new veneer
    /// has a larger range; on most platforms this probably means either a
    /// "long-range jump" (e.g., on ARM, the 26-bit form), or if already at that
    /// stage, a jump that supports a full 32-bit range, for example.
    fn generate_veneer(self, buffer: &mut [u8], veneer_offset: CodeOffset) -> (CodeOffset, Self);
}

/// Describes a block terminator (not call) in the vcode, when its branches
/// have not yet been finalized (so a branch may have two targets).
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum MachTerminator<'a> {
    /// Not a terminator.
    None,
    /// A return instruction.
    Ret,
    /// An unconditional branch to another block.
    Uncond(MachLabel),
    /// A conditional branch to one of two other blocks.
    Cond(MachLabel, MachLabel),
    /// An indirect branch with known possible targets.
    Indirect(&'a [MachLabel]),
}

impl<'a> MachTerminator<'a> {
    /// Get the successor labels named in a `MachTerminator`.
    pub fn get_succs(&self) -> SmallVec<[MachLabel; 2]> {
        let mut ret = smallvec![];
        match self {
            &MachTerminator::Uncond(l) => {
                ret.push(l);
            }
            &MachTerminator::Cond(l1, l2) => {
                ret.push(l1);
                ret.push(l2);
            }
            &MachTerminator::Indirect(ls) => {
                ret.extend(ls.iter().cloned());
            }
            _ => {}
        }
        ret
    }

    /// Is this a terminator?
    pub fn is_term(&self) -> bool {
        match self {
            MachTerminator::None => false,
            _ => true,
        }
    }
}

/// A trait describing the ability to encode a MachInst into binary machine code.
pub trait MachInstEmit: MachInst {
    /// Persistent state carried across `emit` invocations.
    type State: MachInstEmitState<Self>;
    /// Constant information used in `emit` invocations.
    type Info: MachInstEmitInfo;
    /// Emit the instruction.
    fn emit(&self, code: &mut MachBuffer<Self>, info: &Self::Info, state: &mut Self::State);
    /// Pretty-print the instruction.
    fn pretty_print(&self, mb_rru: Option<&RealRegUniverse>, state: &mut Self::State) -> String;
}

/// Constant information used to emit an instruction.
pub trait MachInstEmitInfo {
    /// Return the target-independent settings used for the compilation of this
    /// particular function.
    fn flags(&self) -> &Flags;
}

/// A trait describing the emission state carried between MachInsts when
/// emitting a function body.
pub trait MachInstEmitState<I: MachInst>: Default + Clone + Debug {
    /// Create a new emission state given the ABI object.
    fn new(abi: &dyn ABICallee<I = I>) -> Self;
    /// Update the emission state before emitting an instruction that is a
    /// safepoint.
    fn pre_safepoint(&mut self, _stack_map: StackMap) {}
    /// Update the emission state to indicate instructions are associated with a
    /// particular SourceLoc.
    fn pre_sourceloc(&mut self, _srcloc: SourceLoc) {}
}

/// The result of a `MachBackend::compile_function()` call. Contains machine
/// code (as bytes) and a disassembly, if requested.
pub struct MachCompileResult {
    /// Machine code.
    pub buffer: MachBufferFinalized,
    /// Size of stack frame, in bytes.
    pub frame_size: u32,
    /// Disassembly, if requested.
    pub disasm: Option<String>,
    /// Debug info: value labels to registers/stackslots at code offsets.
    pub value_labels_ranges: ValueLabelsRanges,
    /// Debug info: stackslots to stack pointer offsets.
    pub stackslot_offsets: PrimaryMap<StackSlot, u32>,
}

impl MachCompileResult {
    /// Get a `CodeInfo` describing section sizes from this compilation result.
    pub fn code_info(&self) -> CodeInfo {
        let code_size = self.buffer.total_size();
        CodeInfo {
            code_size,
            jumptables_size: 0,
            rodata_size: 0,
            total_size: code_size,
        }
    }
}

/// Top-level machine backend trait, which wraps all monomorphized code and
/// allows a virtual call from the machine-independent `Function::compile()`.
pub trait MachBackend {
    /// Compile the given function.
    fn compile_function(
        &self,
        func: &Function,
        want_disasm: bool,
    ) -> CodegenResult<MachCompileResult>;

    /// Return flags for this backend.
    fn flags(&self) -> &Flags;

    /// Get the ISA-dependent flag values that were used to make this trait object.
    fn isa_flags(&self) -> Vec<settings::Value>;

    /// Hashes all flags, both ISA-independent and ISA-dependent, into the specified hasher.
    fn hash_all_flags(&self, hasher: &mut dyn Hasher);

    /// Return triple for this backend.
    fn triple(&self) -> Triple;

    /// Return name for this backend.
    fn name(&self) -> &'static str;

    /// Return the register universe for this backend.
    fn reg_universe(&self) -> &RealRegUniverse;

    /// Machine-specific condcode info needed by TargetIsa.
    /// Condition that will be true when an IaddIfcout overflows.
    fn unsigned_add_overflow_condition(&self) -> IntCC;

    /// Machine-specific condcode info needed by TargetIsa.
    /// Condition that will be true when an IsubIfcout overflows.
    fn unsigned_sub_overflow_condition(&self) -> IntCC;

    /// Produces unwind info based on backend results.
    #[cfg(feature = "unwind")]
    fn emit_unwind_info(
        &self,
        _result: &MachCompileResult,
        _kind: UnwindInfoKind,
    ) -> CodegenResult<Option<crate::isa::unwind::UnwindInfo>> {
        // By default, an backend cannot produce unwind info.
        Ok(None)
    }

    /// Creates a new System V Common Information Entry for the ISA.
    #[cfg(feature = "unwind")]
    fn create_systemv_cie(&self) -> Option<gimli::write::CommonInformationEntry> {
        // By default, an ISA cannot create a System V CIE
        None
    }
    /// Maps a regalloc::Reg to a DWARF register number.
    #[cfg(feature = "unwind")]
    fn map_reg_to_dwarf(&self, _: Reg) -> Result<u16, RegisterMappingError> {
        Err(RegisterMappingError::UnsupportedArchitecture)
    }
}

/// Expected unwind info type.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[non_exhaustive]
pub enum UnwindInfoKind {
    /// No unwind info.
    None,
    /// SystemV CIE/FDE unwind info.
    #[cfg(feature = "unwind")]
    SystemV,
    /// Windows X64 Unwind info
    #[cfg(feature = "unwind")]
    Windows,
}

/// Info about an operation that loads or stores from/to the stack.
#[derive(Clone, Copy, Debug)]
pub enum MachInstStackOpInfo {
    /// Load from an offset from the nominal stack pointer into the given reg.
    LoadNomSPOff(Reg, i64),
    /// Store to an offset from the nominal stack pointer from the given reg.
    StoreNomSPOff(Reg, i64),
    /// Adjustment of nominal-SP up or down. This value is added to subsequent
    /// offsets in loads/stores above to produce real-SP offsets.
    NomSPAdj(i64),
}
