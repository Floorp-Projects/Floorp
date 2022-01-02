//! A NaN-canonicalizing rewriting pass. Patch floating point arithmetic
//! instructions that may return a NaN result with a sequence of operations
//! that will replace nondeterministic NaN's with a single canonical NaN value.

use crate::cursor::{Cursor, FuncCursor};
use crate::ir::condcodes::FloatCC;
use crate::ir::immediates::{Ieee32, Ieee64};
use crate::ir::types;
use crate::ir::types::Type;
use crate::ir::{Function, Inst, InstBuilder, InstructionData, Opcode, Value};
use crate::timing;

// Canonical 32-bit and 64-bit NaN values.
static CANON_32BIT_NAN: u32 = 0b01111111110000000000000000000000;
static CANON_64BIT_NAN: u64 = 0b0111111111111000000000000000000000000000000000000000000000000000;

/// Perform the NaN canonicalization pass.
pub fn do_nan_canonicalization(func: &mut Function) {
    let _tt = timing::canonicalize_nans();
    let mut pos = FuncCursor::new(func);
    while let Some(_block) = pos.next_block() {
        while let Some(inst) = pos.next_inst() {
            if is_fp_arith(&mut pos, inst) {
                add_nan_canon_seq(&mut pos, inst);
            }
        }
    }
}

/// Returns true/false based on whether the instruction is a floating-point
/// arithmetic operation. This ignores operations like `fneg`, `fabs`, or
/// `fcopysign` that only operate on the sign bit of a floating point value.
fn is_fp_arith(pos: &mut FuncCursor, inst: Inst) -> bool {
    match pos.func.dfg[inst] {
        InstructionData::Unary { opcode, .. } => {
            opcode == Opcode::Ceil
                || opcode == Opcode::Floor
                || opcode == Opcode::Nearest
                || opcode == Opcode::Sqrt
                || opcode == Opcode::Trunc
        }
        InstructionData::Binary { opcode, .. } => {
            opcode == Opcode::Fadd
                || opcode == Opcode::Fdiv
                || opcode == Opcode::Fmax
                || opcode == Opcode::Fmin
                || opcode == Opcode::Fmul
                || opcode == Opcode::Fsub
        }
        InstructionData::Ternary { opcode, .. } => opcode == Opcode::Fma,
        _ => false,
    }
}

/// Append a sequence of canonicalizing instructions after the given instruction.
fn add_nan_canon_seq(pos: &mut FuncCursor, inst: Inst) {
    // Select the instruction result, result type. Replace the instruction
    // result and step forward before inserting the canonicalization sequence.
    let val = pos.func.dfg.first_result(inst);
    let val_type = pos.func.dfg.value_type(val);
    let new_res = pos.func.dfg.replace_result(val, val_type);
    let _next_inst = pos.next_inst().expect("block missing terminator!");

    // Insert a comparison instruction, to check if `inst_res` is NaN. Select
    // the canonical NaN value if `val` is NaN, assign the result to `inst`.
    let is_nan = pos.ins().fcmp(FloatCC::NotEqual, new_res, new_res);
    let canon_nan = insert_nan_const(pos, val_type);
    pos.ins()
        .with_result(val)
        .select(is_nan, canon_nan, new_res);

    pos.prev_inst(); // Step backwards so the pass does not skip instructions.
}

/// Insert a canonical 32-bit or 64-bit NaN constant at the current position.
fn insert_nan_const(pos: &mut FuncCursor, nan_type: Type) -> Value {
    match nan_type {
        types::F32 => pos.ins().f32const(Ieee32::with_bits(CANON_32BIT_NAN)),
        types::F64 => pos.ins().f64const(Ieee64::with_bits(CANON_64BIT_NAN)),
        _ => {
            // Panic if the type given was not an IEEE floating point type.
            panic!("Could not canonicalize NaN: Unexpected result type found.");
        }
    }
}
