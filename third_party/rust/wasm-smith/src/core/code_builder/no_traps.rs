use crate::core::*;
use wasm_encoder::Instruction;

use super::CodeBuilder;

// For loads, we dynamically check whether the load will
// trap, and if it will then we generate a dummy value to
// use instead.
pub(crate) fn load<'a>(
    inst: Instruction<'a>,
    module: &Module,
    builder: &mut CodeBuilder,
    insts: &mut Vec<Instruction<'a>>,
) {
    let memarg = get_memarg(&inst);
    let memory = &module.memories[memarg.memory_index as usize];
    let address_type = if memory.memory64 {
        ValType::I64
    } else {
        ValType::I32
    };
    // Add a temporary local to hold this load's address.
    let address_local = builder.alloc_local(address_type);

    // Add a temporary local to hold the result of this load.
    let load_type = type_of_memory_access(&inst);
    let result_local = builder.alloc_local(load_type);

    // [address:address_type]
    insts.push(Instruction::LocalSet(address_local));
    // []
    insts.push(Instruction::Block(wasm_encoder::BlockType::Empty));
    {
        // []
        insts.push(Instruction::Block(wasm_encoder::BlockType::Empty));
        {
            // []
            insts.push(Instruction::MemorySize(memarg.memory_index));
            // [mem_size_in_pages:address_type]
            insts.push(int_const_inst(
                address_type,
                crate::page_size(memory).into(),
            ));
            // [mem_size_in_pages:address_type wasm_page_size:address_type]
            insts.push(int_mul_inst(address_type));
            // [mem_size_in_bytes:address_type]
            insts.push(int_const_inst(
                address_type,
                (memarg.offset + size_of_type_in_memory(load_type)) as i64,
            ));
            // [mem_size_in_bytes:address_type offset_and_size:address_type]
            insts.push(Instruction::LocalGet(address_local));
            // [mem_size_in_bytes:address_type offset_and_size:address_type address:address_type]
            insts.push(int_add_inst(address_type));
            // [mem_size_in_bytes:address_type highest_byte_accessed:address_type]
            insts.push(int_le_u_inst(address_type));
            // [load_will_trap:i32]
            insts.push(Instruction::BrIf(0));
            // []
            insts.push(Instruction::LocalGet(address_local));
            // [address:address_type]
            insts.push(int_const_inst(address_type, 0));
            // [address:address_type 0:address_type]
            insts.push(int_le_s_inst(address_type));
            // [load_will_trap:i32]
            insts.push(Instruction::BrIf(0));
            // []
            insts.push(Instruction::LocalGet(address_local));
            // [address:address_type]
            insts.push(inst);
            // [result:load_type]
            insts.push(Instruction::LocalSet(result_local));
            // []
            insts.push(Instruction::Br(1));
            // <unreachable>
        }
        // []
        insts.push(Instruction::End);
        // []
        insts.push(dummy_value_inst(load_type));
        // [dummy_value:load_type]
        insts.push(Instruction::LocalSet(result_local));
        // []
    }
    // []
    insts.push(Instruction::End);
    // []
    insts.push(Instruction::LocalGet(result_local));
    // [result:load_type]
}

// Stores are similar to loads: we check whether the store
// will trap, and if it will then we just drop the value.
pub(crate) fn store<'a>(
    inst: Instruction<'a>,
    module: &Module,
    builder: &mut CodeBuilder,
    insts: &mut Vec<Instruction<'a>>,
) {
    let memarg = get_memarg(&inst);
    let memory = &module.memories[memarg.memory_index as usize];
    let address_type = if memory.memory64 {
        ValType::I64
    } else {
        ValType::I32
    };

    // Add a temporary local to hold this store's address.
    let address_local = builder.alloc_local(address_type);

    // Add a temporary local to hold the value to store.
    let store_type = type_of_memory_access(&inst);
    let value_local = builder.alloc_local(store_type);

    // [address:address_type value:store_type]
    insts.push(Instruction::LocalSet(value_local));
    // [address:address_type]
    insts.push(Instruction::LocalSet(address_local));
    // []
    insts.push(Instruction::MemorySize(memarg.memory_index));
    // [mem_size_in_pages:address_type]
    insts.push(int_const_inst(
        address_type,
        crate::page_size(memory).into(),
    ));
    // [mem_size_in_pages:address_type wasm_page_size:address_type]
    insts.push(int_mul_inst(address_type));
    // [mem_size_in_bytes:address_type]
    insts.push(int_const_inst(
        address_type,
        (memarg.offset + size_of_type_in_memory(store_type)) as i64,
    ));
    // [mem_size_in_bytes:address_type offset_and_size:address_type]
    insts.push(Instruction::LocalGet(address_local));
    // [mem_size_in_bytes:address_type offset_and_size:address_type address:address_type]
    insts.push(int_add_inst(address_type));
    // [mem_size_in_bytes:address_type highest_byte_accessed:address_type]
    insts.push(int_le_u_inst(address_type));
    // [store_will_trap:i32]
    insts.push(Instruction::If(BlockType::Empty));
    insts.push(Instruction::Else);
    {
        // []
        insts.push(Instruction::LocalGet(address_local));
        // [address:address_type]
        insts.push(int_const_inst(address_type, 0));
        // [address:address_type 0:address_type]
        insts.push(int_le_s_inst(address_type));
        // [load_will_trap:i32]
        insts.push(Instruction::If(BlockType::Empty));
        insts.push(Instruction::Else);
        {
            // []
            insts.push(Instruction::LocalGet(address_local));
            // [address:address_type]
            insts.push(Instruction::LocalGet(value_local));
            // [address:address_type value:store_type]
            insts.push(inst);
            // []
        }
        insts.push(Instruction::End);
    }
    // []
    insts.push(Instruction::End);
}

// Unsigned integer division and remainder will trap when
// the divisor is 0. To avoid the trap, we will set any 0
// divisors to 1 prior to the operation.
//
// The code below is equivalent to this expression:
//
//     local.set $temp_divisor
//     (select (i32.eqz (local.get $temp_divisor) (i32.const 1) (local.get $temp_divisor))
pub(crate) fn unsigned_div_rem<'a>(
    inst: Instruction<'a>,
    builder: &mut CodeBuilder,
    insts: &mut Vec<Instruction<'a>>,
) {
    let op_type = type_of_integer_operation(&inst);
    let temp_divisor = builder.alloc_local(op_type);

    // [dividend:op_type divisor:op_type]
    insts.push(Instruction::LocalSet(temp_divisor));
    // [dividend:op_type]
    insts.push(int_const_inst(op_type, 1));
    // [dividend:op_type 1:op_type]
    insts.push(Instruction::LocalGet(temp_divisor));
    // [dividend:op_type 1:op_type divisor:op_type]
    insts.push(Instruction::LocalGet(temp_divisor));
    // [dividend:op_type 1:op_type divisor:op_type divisor:op_type]
    insts.push(eqz_inst(op_type));
    // [dividend:op_type 1:op_type divisor:op_type is_zero:i32]
    insts.push(Instruction::Select);
    // [dividend:op_type divisor:op_type]
    insts.push(inst);
    // [result:op_type]
}

pub(crate) fn trunc<'a>(
    inst: Instruction<'a>,
    builder: &mut CodeBuilder,
    insts: &mut Vec<Instruction<'a>>,
) {
    // If NaN or ±inf, replace with dummy value. Our method of checking for NaN
    // is to use `ne` because NaN is the only value that is not equal to itself
    let conv_type = type_of_float_conversion(&inst);
    let temp_float = builder.alloc_local(conv_type);
    // [input:conv_type]
    insts.push(Instruction::LocalTee(temp_float));
    // [input:conv_type]
    insts.push(Instruction::LocalGet(temp_float));
    // [input:conv_type input:conv_type]
    insts.push(ne_inst(conv_type));
    // [is_nan:i32]
    insts.push(Instruction::LocalGet(temp_float));
    // [is_nan:i32 input:conv_type]
    insts.push(flt_inf_const_inst(conv_type));
    // [is_nan:i32 input:conv_type inf:conv_type]
    insts.push(eq_inst(conv_type));
    // [is_nan:i32 is_inf:i32]
    insts.push(Instruction::LocalGet(temp_float));
    // [is_nan:i32 is_inf:i32 input:conv_type]
    insts.push(flt_neg_inf_const_inst(conv_type));
    // [is_nan:i32 is_inf:i32 input:conv_type neg_inf:conv_type]
    insts.push(eq_inst(conv_type));
    // [is_nan:i32 is_inf:i32 is_neg_inf:i32]
    insts.push(Instruction::I32Or);
    // [is_nan:i32 is_±inf:i32]
    insts.push(Instruction::I32Or);
    // [is_nan_or_inf:i32]
    insts.push(Instruction::If(BlockType::Empty));
    {
        // []
        insts.push(dummy_value_inst(conv_type));
        // [0:conv_type]
        insts.push(Instruction::LocalSet(temp_float));
        // []
    }
    insts.push(Instruction::End);
    // []
    insts.push(Instruction::LocalGet(temp_float));
    // [input_or_0:conv_type]

    // first ensure that it is >= the min value of our target type
    insts.push(min_input_const_for_trunc(&inst));
    // [input_or_0:conv_type min_value_of_target_type:conv_type]
    insts.push(flt_lt_inst(conv_type));
    // [input_lt_min:i32]
    insts.push(Instruction::If(BlockType::Empty));
    {
        // []
        insts.push(min_input_const_for_trunc(&inst));
        // [min_value_of_target_type:conv_type]
        insts.push(Instruction::LocalSet(temp_float));
    }
    insts.push(Instruction::End);
    // []
    insts.push(Instruction::LocalGet(temp_float));
    // [coerced_input:conv_type]

    // next ensure that it is  <= the max value of our target type
    insts.push(max_input_const_for_trunc(&inst));
    // [input_or_0:conv_type max_value_of_target_type:conv_type]
    insts.push(flt_gt_inst(conv_type));
    // [input_gt_min:i32]
    insts.push(Instruction::If(BlockType::Empty));
    {
        // []
        insts.push(max_input_const_for_trunc(&inst));
        // [max_value_of_target_type:conv_type]
        insts.push(Instruction::LocalSet(temp_float));
    }
    insts.push(Instruction::End);
    // []
    insts.push(Instruction::LocalGet(temp_float));
    // [coerced_input:conv_type]
    insts.push(inst);
}

// Signed division and remainder will trap in the following instances:
//     - The divisor is 0
//     - The result of the division is 2^(n-1)
pub(crate) fn signed_div_rem<'a>(
    inst: Instruction<'a>,
    builder: &mut CodeBuilder,
    insts: &mut Vec<Instruction<'a>>,
) {
    // If divisor is 0, replace with 1
    let op_type = type_of_integer_operation(&inst);
    let temp_divisor = builder.alloc_local(op_type);
    // [dividend:op_type divisor:op_type]
    insts.push(Instruction::LocalSet(temp_divisor));
    // [dividend:op_type]
    insts.push(int_const_inst(op_type, 1));
    // [dividend:op_type 1:op_type]
    insts.push(Instruction::LocalGet(temp_divisor));
    // [dividend:op_type 1:op_type divisor:op_type]
    insts.push(Instruction::LocalGet(temp_divisor));
    // [dividend:op_type 1:op_type divisor:op_type divisor:op_type]
    insts.push(eqz_inst(op_type));
    // [dividend:op_type 1:op_type divisor:op_type is_zero:i32]
    insts.push(Instruction::Select);
    // [dividend:op_type divisor:op_type]
    // If dividend and divisor are -int.max and -1, replace
    // divisor with 1.
    let temp_dividend = builder.alloc_local(op_type);
    insts.push(Instruction::LocalSet(temp_divisor));
    // [dividend:op_type]
    insts.push(Instruction::LocalSet(temp_dividend));
    // []
    insts.push(Instruction::Block(wasm_encoder::BlockType::Empty));
    {
        insts.push(Instruction::Block(wasm_encoder::BlockType::Empty));
        {
            // []
            insts.push(Instruction::LocalGet(temp_dividend));
            // [dividend:op_type]
            insts.push(Instruction::LocalGet(temp_divisor));
            // [dividend:op_type divisor:op_type]
            insts.push(Instruction::LocalSet(temp_divisor));
            // [dividend:op_type]
            insts.push(Instruction::LocalTee(temp_dividend));
            // [dividend:op_type]
            insts.push(int_min_const_inst(op_type));
            // [dividend:op_type int_min:op_type]
            insts.push(ne_inst(op_type));
            // [not_int_min:i32]
            insts.push(Instruction::BrIf(0));
            // []
            insts.push(Instruction::LocalGet(temp_divisor));
            // [divisor:op_type]
            insts.push(int_const_inst(op_type, -1));
            // [divisor:op_type -1:op_type]
            insts.push(ne_inst(op_type));
            // [not_neg_one:i32]
            insts.push(Instruction::BrIf(0));
            // []
            insts.push(int_const_inst(op_type, 1));
            // [divisor:op_type]
            insts.push(Instruction::LocalSet(temp_divisor));
            // []
            insts.push(Instruction::Br(1));
        }
        // []
        insts.push(Instruction::End);
    }
    // []
    insts.push(Instruction::End);
    // []
    insts.push(Instruction::LocalGet(temp_dividend));
    // [dividend:op_type]
    insts.push(Instruction::LocalGet(temp_divisor));
    // [dividend:op_type divisor:op_type]
    insts.push(inst);
}

fn get_memarg(inst: &Instruction) -> wasm_encoder::MemArg {
    match *inst {
        Instruction::I32Load(memarg)
        | Instruction::I64Load(memarg)
        | Instruction::F32Load(memarg)
        | Instruction::F64Load(memarg)
        | Instruction::I32Load8S(memarg)
        | Instruction::I32Load8U(memarg)
        | Instruction::I32Load16S(memarg)
        | Instruction::I32Load16U(memarg)
        | Instruction::I64Load8S(memarg)
        | Instruction::I64Load8U(memarg)
        | Instruction::I64Load16S(memarg)
        | Instruction::I64Load16U(memarg)
        | Instruction::I64Load32S(memarg)
        | Instruction::I64Load32U(memarg)
        | Instruction::V128Load(memarg)
        | Instruction::V128Load8x8S(memarg)
        | Instruction::V128Load8x8U(memarg)
        | Instruction::V128Load16x4S(memarg)
        | Instruction::V128Load16x4U(memarg)
        | Instruction::V128Load32x2S(memarg)
        | Instruction::V128Load32x2U(memarg)
        | Instruction::V128Load8Splat(memarg)
        | Instruction::V128Load16Splat(memarg)
        | Instruction::V128Load32Splat(memarg)
        | Instruction::V128Load64Splat(memarg)
        | Instruction::V128Load32Zero(memarg)
        | Instruction::V128Load64Zero(memarg)
        | Instruction::I32Store(memarg)
        | Instruction::I64Store(memarg)
        | Instruction::F32Store(memarg)
        | Instruction::F64Store(memarg)
        | Instruction::I32Store8(memarg)
        | Instruction::I32Store16(memarg)
        | Instruction::I64Store8(memarg)
        | Instruction::I64Store16(memarg)
        | Instruction::I64Store32(memarg)
        | Instruction::V128Store(memarg) => memarg,
        _ => unreachable!(),
    }
}

fn dummy_value_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::I32 => Instruction::I32Const(0),
        ValType::I64 => Instruction::I64Const(0),
        ValType::F32 => Instruction::F32Const(0.0),
        ValType::F64 => Instruction::F64Const(0.0),
        ValType::V128 => Instruction::V128Const(0),
        ValType::Ref(ty) => {
            assert!(ty.nullable);
            Instruction::RefNull(ty.heap_type)
        }
    }
}

fn eq_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::F32 => Instruction::F32Eq,
        ValType::F64 => Instruction::F64Eq,
        ValType::I32 => Instruction::I32Eq,
        ValType::I64 => Instruction::I64Eq,
        _ => panic!("not a numeric type"),
    }
}

fn eqz_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::I32 => Instruction::I32Eqz,
        ValType::I64 => Instruction::I64Eqz,
        _ => panic!("not an integer type"),
    }
}

fn type_of_integer_operation(inst: &Instruction) -> ValType {
    match inst {
        Instruction::I32DivU
        | Instruction::I32DivS
        | Instruction::I32RemU
        | Instruction::I32RemS => ValType::I32,
        Instruction::I64RemU
        | Instruction::I64DivU
        | Instruction::I64DivS
        | Instruction::I64RemS => ValType::I64,
        _ => panic!("not integer division or remainder"),
    }
}

fn type_of_float_conversion(inst: &Instruction) -> ValType {
    match inst {
        Instruction::I32TruncF32S
        | Instruction::I32TruncF32U
        | Instruction::I64TruncF32S
        | Instruction::I64TruncF32U => ValType::F32,
        Instruction::I32TruncF64S
        | Instruction::I32TruncF64U
        | Instruction::I64TruncF64S
        | Instruction::I64TruncF64U => ValType::F64,
        _ => panic!("not a float -> integer conversion"),
    }
}

fn min_input_const_for_trunc<'a>(inst: &Instruction) -> Instruction<'a> {
    // This is the minimum float value that is representable as an i64
    let min_f64 = -9_223_372_036_854_775_000f64;
    let min_f32 = -9_223_372_000_000_000_000f32;

    // This is the minimum float value that is representable as as i32
    let min_f32_as_i32 = -2_147_483_500f32;
    match inst {
        Instruction::I32TruncF32S => Instruction::F32Const(min_f32_as_i32),
        Instruction::I32TruncF32U => Instruction::F32Const(0.0),
        Instruction::I64TruncF32S => Instruction::F32Const(min_f32),
        Instruction::I64TruncF32U => Instruction::F32Const(0.0),
        Instruction::I32TruncF64S => Instruction::F64Const(i32::MIN as f64),
        Instruction::I32TruncF64U => Instruction::F64Const(0.0),
        Instruction::I64TruncF64S => Instruction::F64Const(min_f64),
        Instruction::I64TruncF64U => Instruction::F64Const(0.0),
        _ => panic!("not a trunc instruction"),
    }
}

fn max_input_const_for_trunc<'a>(inst: &Instruction) -> Instruction<'a> {
    // This is the maximum float value that is representable as as i64
    let max_f64_as_i64 = 9_223_372_036_854_775_000f64;
    let max_f32_as_i64 = 9_223_371_500_000_000_000f32;

    // This is the maximum float value that is representable as as i32
    let max_f32_as_i32 = 2_147_483_500f32;
    match inst {
        Instruction::I32TruncF32S | Instruction::I32TruncF32U => {
            Instruction::F32Const(max_f32_as_i32)
        }
        Instruction::I64TruncF32S | Instruction::I64TruncF32U => {
            Instruction::F32Const(max_f32_as_i64)
        }
        Instruction::I32TruncF64S | Instruction::I32TruncF64U => {
            Instruction::F64Const(i32::MAX as f64)
        }
        Instruction::I64TruncF64S | Instruction::I64TruncF64U => {
            Instruction::F64Const(max_f64_as_i64)
        }
        _ => panic!("not a trunc instruction"),
    }
}

fn type_of_memory_access(inst: &Instruction) -> ValType {
    match inst {
        Instruction::I32Load(_)
        | Instruction::I32Load8S(_)
        | Instruction::I32Load8U(_)
        | Instruction::I32Load16S(_)
        | Instruction::I32Load16U(_)
        | Instruction::I32Store(_)
        | Instruction::I32Store8(_)
        | Instruction::I32Store16(_) => ValType::I32,

        Instruction::I64Load(_)
        | Instruction::I64Load8S(_)
        | Instruction::I64Load8U(_)
        | Instruction::I64Load16S(_)
        | Instruction::I64Load16U(_)
        | Instruction::I64Load32S(_)
        | Instruction::I64Load32U(_)
        | Instruction::I64Store(_)
        | Instruction::I64Store8(_)
        | Instruction::I64Store16(_)
        | Instruction::I64Store32(_) => ValType::I64,

        Instruction::F32Load(_) | Instruction::F32Store(_) => ValType::F32,

        Instruction::F64Load(_) | Instruction::F64Store(_) => ValType::F64,

        Instruction::V128Load { .. }
        | Instruction::V128Load8x8S { .. }
        | Instruction::V128Load8x8U { .. }
        | Instruction::V128Load16x4S { .. }
        | Instruction::V128Load16x4U { .. }
        | Instruction::V128Load32x2S { .. }
        | Instruction::V128Load32x2U { .. }
        | Instruction::V128Load8Splat { .. }
        | Instruction::V128Load16Splat { .. }
        | Instruction::V128Load32Splat { .. }
        | Instruction::V128Load64Splat { .. }
        | Instruction::V128Load32Zero { .. }
        | Instruction::V128Load64Zero { .. }
        | Instruction::V128Store { .. } => ValType::V128,

        _ => panic!("not a memory access instruction"),
    }
}

fn int_min_const_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::I32 => Instruction::I32Const(i32::MIN),
        ValType::I64 => Instruction::I64Const(i64::MIN),
        _ => panic!("not an int type"),
    }
}

fn int_const_inst<'a>(ty: ValType, x: i64) -> Instruction<'a> {
    match ty {
        ValType::I32 => Instruction::I32Const(x as i32),
        ValType::I64 => Instruction::I64Const(x),
        _ => panic!("not an int type"),
    }
}

fn int_mul_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::I32 => Instruction::I32Mul,
        ValType::I64 => Instruction::I64Mul,
        _ => panic!("not an int type"),
    }
}

fn int_add_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::I32 => Instruction::I32Add,
        ValType::I64 => Instruction::I64Add,
        _ => panic!("not an int type"),
    }
}

fn int_le_u_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::I32 => Instruction::I32LeU,
        ValType::I64 => Instruction::I64LeU,
        _ => panic!("not an int type"),
    }
}

fn int_le_s_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::I32 => Instruction::I32LeS,
        ValType::I64 => Instruction::I64LeS,
        _ => panic!("not an int type"),
    }
}

fn ne_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::I32 => Instruction::I32Ne,
        ValType::I64 => Instruction::I64Ne,
        ValType::F32 => Instruction::F32Ne,
        ValType::F64 => Instruction::F64Ne,
        _ => panic!("not a numeric type"),
    }
}

fn flt_lt_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::F32 => Instruction::F32Lt,
        ValType::F64 => Instruction::F64Lt,
        _ => panic!("not a float type"),
    }
}

fn flt_gt_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::F32 => Instruction::F32Gt,
        ValType::F64 => Instruction::F64Gt,
        _ => panic!("not a float type"),
    }
}

fn flt_inf_const_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::F32 => Instruction::F32Const(f32::INFINITY),
        ValType::F64 => Instruction::F64Const(f64::INFINITY),
        _ => panic!("not a float type"),
    }
}

fn flt_neg_inf_const_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::F32 => Instruction::F32Const(f32::NEG_INFINITY),
        ValType::F64 => Instruction::F64Const(f64::NEG_INFINITY),
        _ => panic!("not a float type"),
    }
}

fn size_of_type_in_memory(ty: ValType) -> u64 {
    match ty {
        ValType::I32 => 4,
        ValType::I64 => 8,
        ValType::F32 => 4,
        ValType::F64 => 8,
        ValType::V128 => 16,
        ValType::Ref(_) => panic!("not a memory type"),
    }
}
