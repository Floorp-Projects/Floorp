use super::{
    CompositeType, Elements, FuncType, Instruction, InstructionKind::*, InstructionKinds, Module,
    ValType,
};
use crate::{unique_string, MemoryOffsetChoices};
use arbitrary::{Result, Unstructured};
use std::collections::{BTreeMap, BTreeSet};
use std::rc::Rc;
use wasm_encoder::{
    ArrayType, BlockType, Catch, ConstExpr, ExportKind, FieldType, GlobalType, HeapType, MemArg,
    RefType, StorageType, StructType,
};
mod no_traps;

macro_rules! instructions {
    (
        $(
            ($predicate:expr, $generator_fn:ident, $instruction_kind:ident $(, $cost:tt)?),
        )*
    ) => {
        static NUM_OPTIONS: usize = instructions!(
            @count;
            $( $generator_fn )*
        );

        fn choose_instruction(
            u: &mut Unstructured<'_>,
            module: &Module,
            allowed_instructions: InstructionKinds,
            builder: &mut CodeBuilder,
        ) -> Option<
            fn(&mut Unstructured<'_>, &Module, &mut CodeBuilder, &mut Vec<Instruction>) -> Result<()>
        > {
            builder.allocs.options.clear();
            let mut cost = 0;
            // Unroll the loop that checks whether each instruction is valid in
            // the current context and, if it is valid, pushes it onto our
            // options. Unrolling this loops lets us avoid dynamic calls through
            // function pointers and, furthermore, each call site can be branch
            // predicted and even inlined. This saved us about 30% of time in
            // the `corpus` benchmark.
            $(
                let predicate: Option<fn(&Module, &mut CodeBuilder) -> bool> = $predicate;
                if predicate.map_or(true, |f| f(module, builder))
                    && allowed_instructions.contains($instruction_kind) {
                    builder.allocs.options.push(($generator_fn, cost));
                    cost += 1000 $(- $cost)?;
                }
            )*

            // If there aren't actually any candidate instructions due to
            // various filters in place then return `None` to indicate the
            // situation.
            if cost == 0 {
                return None;
            }

            let i = u.int_in_range(0..=cost).ok()?;
            let idx = builder
                .allocs
                .options
                .binary_search_by_key(&i,|p| p.1)
                .unwrap_or_else(|i| i - 1);
            Some(builder.allocs.options[idx].0)
        }
    };

    ( @count; ) => {
        0
    };
    ( @count; $x:ident $( $xs:ident )* ) => {
        1 + instructions!( @count; $( $xs )* )
    };
}

// The static set of options of instruction to generate that could be valid at
// some given time. One entry per Wasm instruction.
//
// Each entry is made up of up to three parts:
//
// 1. A predicate for whether this is a valid choice, if any. `None` means that
//    the choice is always applicable.
//
// 2. The function to generate the instruction, given that we've made this
//    choice.
//
// 3. The `InstructionKind` the instruction belongs to; this allows filtering
//    out instructions by category.
//
// 4. An optional number used to weight how often this instruction is chosen.
//    Higher numbers are less likely to be chosen, and number specified must be
//    less than 1000.
instructions! {
    // Control instructions.
    (Some(unreachable_valid), unreachable, Control, 990),
    (None, nop, Control, 800),
    (None, block, Control),
    (None, r#loop, Control),
    (Some(try_table_valid), try_table, Control),
    (Some(if_valid), r#if, Control),
    (Some(else_valid), r#else, Control),
    (Some(end_valid), end, Control),
    (Some(br_valid), br, Control),
    (Some(br_if_valid), br_if, Control),
    (Some(br_table_valid), br_table, Control),
    (Some(return_valid), r#return, Control, 900),
    (Some(call_valid), call, Control),
    (Some(call_ref_valid), call_ref, Control),
    (Some(call_indirect_valid), call_indirect, Control),
    (Some(return_call_valid), return_call, Control),
    (Some(return_call_ref_valid), return_call_ref, Control),
    (Some(return_call_indirect_valid), return_call_indirect, Control),
    (Some(throw_valid), throw, Control, 850),
    (Some(throw_ref_valid), throw_ref, Control, 850),
    (Some(br_on_null_valid), br_on_null, Control),
    (Some(br_on_non_null_valid), br_on_non_null, Control),
    (Some(br_on_cast_valid), br_on_cast, Control),
    (Some(br_on_cast_fail_valid), br_on_cast_fail, Control),
    // Parametric instructions.
    (Some(drop_valid), drop, Parametric, 990),
    (Some(select_valid), select, Parametric),
    // Variable instructions.
    (Some(local_get_valid), local_get, Variable),
    (Some(local_set_valid), local_set, Variable),
    (Some(local_set_valid), local_tee, Variable),
    (Some(global_get_valid), global_get, Variable),
    (Some(global_set_valid), global_set, Variable),
    // Memory instructions.
    (Some(have_memory_and_offset), i32_load, Memory),
    (Some(have_memory_and_offset), i64_load, Memory),
    (Some(have_memory_and_offset), f32_load, Memory),
    (Some(have_memory_and_offset), f64_load, Memory),
    (Some(have_memory_and_offset), i32_load_8_s, Memory),
    (Some(have_memory_and_offset), i32_load_8_u, Memory),
    (Some(have_memory_and_offset), i32_load_16_s, Memory),
    (Some(have_memory_and_offset), i32_load_16_u, Memory),
    (Some(have_memory_and_offset), i64_load_8_s, Memory),
    (Some(have_memory_and_offset), i64_load_16_s, Memory),
    (Some(have_memory_and_offset), i64_load_32_s, Memory),
    (Some(have_memory_and_offset), i64_load_8_u, Memory),
    (Some(have_memory_and_offset), i64_load_16_u, Memory),
    (Some(have_memory_and_offset), i64_load_32_u, Memory),
    (Some(i32_store_valid), i32_store, Memory),
    (Some(i64_store_valid), i64_store, Memory),
    (Some(f32_store_valid), f32_store, Memory),
    (Some(f64_store_valid), f64_store, Memory),
    (Some(i32_store_valid), i32_store_8, Memory),
    (Some(i32_store_valid), i32_store_16, Memory),
    (Some(i64_store_valid), i64_store_8, Memory),
    (Some(i64_store_valid), i64_store_16, Memory),
    (Some(i64_store_valid), i64_store_32, Memory),
    (Some(have_memory), memory_size, Memory),
    (Some(memory_grow_valid), memory_grow, Memory),
    (Some(memory_init_valid), memory_init, Memory),
    (Some(data_drop_valid), data_drop, Memory),
    (Some(memory_copy_valid), memory_copy, Memory),
    (Some(memory_fill_valid), memory_fill, Memory),
    // Numeric instructions.
    (None, i32_const, Numeric),
    (None, i64_const, Numeric),
    (None, f32_const, Numeric),
    (None, f64_const, Numeric),
    (Some(i32_on_stack), i32_eqz, Numeric),
    (Some(i32_i32_on_stack), i32_eq, Numeric),
    (Some(i32_i32_on_stack), i32_ne, Numeric),
    (Some(i32_i32_on_stack), i32_lt_s, Numeric),
    (Some(i32_i32_on_stack), i32_lt_u, Numeric),
    (Some(i32_i32_on_stack), i32_gt_s, Numeric),
    (Some(i32_i32_on_stack), i32_gt_u, Numeric),
    (Some(i32_i32_on_stack), i32_le_s, Numeric),
    (Some(i32_i32_on_stack), i32_le_u, Numeric),
    (Some(i32_i32_on_stack), i32_ge_s, Numeric),
    (Some(i32_i32_on_stack), i32_ge_u, Numeric),
    (Some(i64_on_stack), i64_eqz, Numeric),
    (Some(i64_i64_on_stack), i64_eq, Numeric),
    (Some(i64_i64_on_stack), i64_ne, Numeric),
    (Some(i64_i64_on_stack), i64_lt_s, Numeric),
    (Some(i64_i64_on_stack), i64_lt_u, Numeric),
    (Some(i64_i64_on_stack), i64_gt_s, Numeric),
    (Some(i64_i64_on_stack), i64_gt_u, Numeric),
    (Some(i64_i64_on_stack), i64_le_s, Numeric),
    (Some(i64_i64_on_stack), i64_le_u, Numeric),
    (Some(i64_i64_on_stack), i64_ge_s, Numeric),
    (Some(i64_i64_on_stack), i64_ge_u, Numeric),
    (Some(f32_f32_on_stack), f32_eq, Numeric),
    (Some(f32_f32_on_stack), f32_ne, Numeric),
    (Some(f32_f32_on_stack), f32_lt, Numeric),
    (Some(f32_f32_on_stack), f32_gt, Numeric),
    (Some(f32_f32_on_stack), f32_le, Numeric),
    (Some(f32_f32_on_stack), f32_ge, Numeric),
    (Some(f64_f64_on_stack), f64_eq, Numeric),
    (Some(f64_f64_on_stack), f64_ne, Numeric),
    (Some(f64_f64_on_stack), f64_lt, Numeric),
    (Some(f64_f64_on_stack), f64_gt, Numeric),
    (Some(f64_f64_on_stack), f64_le, Numeric),
    (Some(f64_f64_on_stack), f64_ge, Numeric),
    (Some(i32_on_stack), i32_clz, Numeric),
    (Some(i32_on_stack), i32_ctz, Numeric),
    (Some(i32_on_stack), i32_popcnt, Numeric),
    (Some(i32_i32_on_stack), i32_add, Numeric),
    (Some(i32_i32_on_stack), i32_sub, Numeric),
    (Some(i32_i32_on_stack), i32_mul, Numeric),
    (Some(i32_i32_on_stack), i32_div_s, Numeric),
    (Some(i32_i32_on_stack), i32_div_u, Numeric),
    (Some(i32_i32_on_stack), i32_rem_s, Numeric),
    (Some(i32_i32_on_stack), i32_rem_u, Numeric),
    (Some(i32_i32_on_stack), i32_and, Numeric),
    (Some(i32_i32_on_stack), i32_or, Numeric),
    (Some(i32_i32_on_stack), i32_xor, Numeric),
    (Some(i32_i32_on_stack), i32_shl, Numeric),
    (Some(i32_i32_on_stack), i32_shr_s, Numeric),
    (Some(i32_i32_on_stack), i32_shr_u, Numeric),
    (Some(i32_i32_on_stack), i32_rotl, Numeric),
    (Some(i32_i32_on_stack), i32_rotr, Numeric),
    (Some(i64_on_stack), i64_clz, Numeric),
    (Some(i64_on_stack), i64_ctz, Numeric),
    (Some(i64_on_stack), i64_popcnt, Numeric),
    (Some(i64_i64_on_stack), i64_add, Numeric),
    (Some(i64_i64_on_stack), i64_sub, Numeric),
    (Some(i64_i64_on_stack), i64_mul, Numeric),
    (Some(i64_i64_on_stack), i64_div_s, Numeric),
    (Some(i64_i64_on_stack), i64_div_u, Numeric),
    (Some(i64_i64_on_stack), i64_rem_s, Numeric),
    (Some(i64_i64_on_stack), i64_rem_u, Numeric),
    (Some(i64_i64_on_stack), i64_and, Numeric),
    (Some(i64_i64_on_stack), i64_or, Numeric),
    (Some(i64_i64_on_stack), i64_xor, Numeric),
    (Some(i64_i64_on_stack), i64_shl, Numeric),
    (Some(i64_i64_on_stack), i64_shr_s, Numeric),
    (Some(i64_i64_on_stack), i64_shr_u, Numeric),
    (Some(i64_i64_on_stack), i64_rotl, Numeric),
    (Some(i64_i64_on_stack), i64_rotr, Numeric),
    (Some(f32_on_stack), f32_abs, Numeric),
    (Some(f32_on_stack), f32_neg, Numeric),
    (Some(f32_on_stack), f32_ceil, Numeric),
    (Some(f32_on_stack), f32_floor, Numeric),
    (Some(f32_on_stack), f32_trunc, Numeric),
    (Some(f32_on_stack), f32_nearest, Numeric),
    (Some(f32_on_stack), f32_sqrt, Numeric),
    (Some(f32_f32_on_stack), f32_add, Numeric),
    (Some(f32_f32_on_stack), f32_sub, Numeric),
    (Some(f32_f32_on_stack), f32_mul, Numeric),
    (Some(f32_f32_on_stack), f32_div, Numeric),
    (Some(f32_f32_on_stack), f32_min, Numeric),
    (Some(f32_f32_on_stack), f32_max, Numeric),
    (Some(f32_f32_on_stack), f32_copysign, Numeric),
    (Some(f64_on_stack), f64_abs, Numeric),
    (Some(f64_on_stack), f64_neg, Numeric),
    (Some(f64_on_stack), f64_ceil, Numeric),
    (Some(f64_on_stack), f64_floor, Numeric),
    (Some(f64_on_stack), f64_trunc, Numeric),
    (Some(f64_on_stack), f64_nearest, Numeric),
    (Some(f64_on_stack), f64_sqrt, Numeric),
    (Some(f64_f64_on_stack), f64_add, Numeric),
    (Some(f64_f64_on_stack), f64_sub, Numeric),
    (Some(f64_f64_on_stack), f64_mul, Numeric),
    (Some(f64_f64_on_stack), f64_div, Numeric),
    (Some(f64_f64_on_stack), f64_min, Numeric),
    (Some(f64_f64_on_stack), f64_max, Numeric),
    (Some(f64_f64_on_stack), f64_copysign, Numeric),
    (Some(i64_on_stack), i32_wrap_i64, Numeric),
    (Some(f32_on_stack), i32_trunc_f32_s, Numeric),
    (Some(f32_on_stack), i32_trunc_f32_u, Numeric),
    (Some(f64_on_stack), i32_trunc_f64_s, Numeric),
    (Some(f64_on_stack), i32_trunc_f64_u, Numeric),
    (Some(i32_on_stack), i64_extend_i32_s, Numeric),
    (Some(i32_on_stack), i64_extend_i32_u, Numeric),
    (Some(f32_on_stack), i64_trunc_f32_s, Numeric),
    (Some(f32_on_stack), i64_trunc_f32_u, Numeric),
    (Some(f64_on_stack), i64_trunc_f64_s, Numeric),
    (Some(f64_on_stack), i64_trunc_f64_u, Numeric),
    (Some(i32_on_stack), f32_convert_i32_s, Numeric),
    (Some(i32_on_stack), f32_convert_i32_u, Numeric),
    (Some(i64_on_stack), f32_convert_i64_s, Numeric),
    (Some(i64_on_stack), f32_convert_i64_u, Numeric),
    (Some(f64_on_stack), f32_demote_f64, Numeric),
    (Some(i32_on_stack), f64_convert_i32_s, Numeric),
    (Some(i32_on_stack), f64_convert_i32_u, Numeric),
    (Some(i64_on_stack), f64_convert_i64_s, Numeric),
    (Some(i64_on_stack), f64_convert_i64_u, Numeric),
    (Some(f32_on_stack), f64_promote_f32, Numeric),
    (Some(f32_on_stack), i32_reinterpret_f32, Numeric),
    (Some(f64_on_stack), i64_reinterpret_f64, Numeric),
    (Some(i32_on_stack), f32_reinterpret_i32, Numeric),
    (Some(i64_on_stack), f64_reinterpret_i64, Numeric),
    (Some(extendable_i32_on_stack), i32_extend_8_s, Numeric),
    (Some(extendable_i32_on_stack), i32_extend_16_s, Numeric),
    (Some(extendable_i64_on_stack), i64_extend_8_s, Numeric),
    (Some(extendable_i64_on_stack), i64_extend_16_s, Numeric),
    (Some(extendable_i64_on_stack), i64_extend_32_s, Numeric),
    (Some(nontrapping_f32_on_stack), i32_trunc_sat_f32_s, Numeric),
    (Some(nontrapping_f32_on_stack), i32_trunc_sat_f32_u, Numeric),
    (Some(nontrapping_f64_on_stack), i32_trunc_sat_f64_s, Numeric),
    (Some(nontrapping_f64_on_stack), i32_trunc_sat_f64_u, Numeric),
    (Some(nontrapping_f32_on_stack), i64_trunc_sat_f32_s, Numeric),
    (Some(nontrapping_f32_on_stack), i64_trunc_sat_f32_u, Numeric),
    (Some(nontrapping_f64_on_stack), i64_trunc_sat_f64_s, Numeric),
    (Some(nontrapping_f64_on_stack), i64_trunc_sat_f64_u, Numeric),
    // Reference instructions.
    (Some(ref_null_valid), ref_null, Reference),
    (Some(ref_func_valid), ref_func, Reference),
    (Some(ref_as_non_null_valid), ref_as_non_null, Reference),
    (Some(ref_eq_valid), ref_eq, Reference),
    (Some(ref_test_valid), ref_test, Reference),
    (Some(ref_cast_valid), ref_cast, Reference),
    (Some(ref_is_null_valid), ref_is_null, Reference),
    (Some(table_fill_valid), table_fill, Reference),
    (Some(table_set_valid), table_set, Reference),
    (Some(table_get_valid), table_get, Reference),
    (Some(table_size_valid), table_size, Reference),
    (Some(table_grow_valid), table_grow, Reference),
    (Some(table_copy_valid), table_copy, Reference),
    (Some(table_init_valid), table_init, Reference),
    (Some(elem_drop_valid), elem_drop, Reference),
    // Aggregate instructions.
    (Some(struct_new_valid), struct_new, Aggregate),
    (Some(struct_new_default_valid), struct_new_default, Aggregate),
    (Some(struct_get_valid), struct_get, Aggregate),
    (Some(struct_set_valid), struct_set, Aggregate),
    (Some(array_new_valid), array_new, Aggregate),
    (Some(array_new_fixed_valid), array_new_fixed, Aggregate),
    (Some(array_new_default_valid), array_new_default, Aggregate),
    (Some(array_new_data_valid), array_new_data, Aggregate),
    (Some(array_new_elem_valid), array_new_elem, Aggregate),
    (Some(array_get_valid), array_get, Aggregate),
    (Some(array_set_valid), array_set, Aggregate),
    (Some(array_len_valid), array_len, Aggregate),
    (Some(array_fill_valid), array_fill, Aggregate),
    (Some(array_copy_valid), array_copy, Aggregate),
    (Some(array_init_data_valid), array_init_data, Aggregate),
    (Some(array_init_elem_valid), array_init_elem, Aggregate),
    (Some(ref_i31_valid), ref_i31, Aggregate),
    (Some(i31_get_valid), i31_get, Aggregate),
    (Some(any_convert_extern_valid), any_convert_extern, Aggregate),
    (Some(extern_convert_any_valid), extern_convert_any, Aggregate),
    // SIMD instructions.
    (Some(simd_have_memory_and_offset), v128_load, Vector),
    (Some(simd_have_memory_and_offset), v128_load8x8s, Vector),
    (Some(simd_have_memory_and_offset), v128_load8x8u, Vector),
    (Some(simd_have_memory_and_offset), v128_load16x4s, Vector),
    (Some(simd_have_memory_and_offset), v128_load16x4u, Vector),
    (Some(simd_have_memory_and_offset), v128_load32x2s, Vector),
    (Some(simd_have_memory_and_offset), v128_load32x2u, Vector),
    (Some(simd_have_memory_and_offset), v128_load8_splat, Vector),
    (Some(simd_have_memory_and_offset), v128_load16_splat, Vector),
    (Some(simd_have_memory_and_offset), v128_load32_splat, Vector),
    (Some(simd_have_memory_and_offset), v128_load64_splat, Vector),
    (Some(simd_have_memory_and_offset), v128_load32_zero, Vector),
    (Some(simd_have_memory_and_offset), v128_load64_zero, Vector),
    (Some(simd_v128_store_valid), v128_store, Vector),
    (Some(simd_load_lane_valid), v128_load8_lane, Vector),
    (Some(simd_load_lane_valid), v128_load16_lane, Vector),
    (Some(simd_load_lane_valid), v128_load32_lane, Vector),
    (Some(simd_load_lane_valid), v128_load64_lane, Vector),
    (Some(simd_store_lane_valid), v128_store8_lane, Vector),
    (Some(simd_store_lane_valid), v128_store16_lane, Vector),
    (Some(simd_store_lane_valid), v128_store32_lane, Vector),
    (Some(simd_store_lane_valid), v128_store64_lane, Vector),
    (Some(simd_enabled), v128_const, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_shuffle, Vector),
    (Some(simd_v128_on_stack), i8x16_extract_lane_s, Vector),
    (Some(simd_v128_on_stack), i8x16_extract_lane_u, Vector),
    (Some(simd_v128_i32_on_stack), i8x16_replace_lane, Vector),
    (Some(simd_v128_on_stack), i16x8_extract_lane_s, Vector),
    (Some(simd_v128_on_stack), i16x8_extract_lane_u, Vector),
    (Some(simd_v128_i32_on_stack), i16x8_replace_lane, Vector),
    (Some(simd_v128_on_stack), i32x4_extract_lane, Vector),
    (Some(simd_v128_i32_on_stack), i32x4_replace_lane, Vector),
    (Some(simd_v128_on_stack), i64x2_extract_lane, Vector),
    (Some(simd_v128_i64_on_stack), i64x2_replace_lane, Vector),
    (Some(simd_v128_on_stack), f32x4_extract_lane, Vector),
    (Some(simd_v128_f32_on_stack), f32x4_replace_lane, Vector),
    (Some(simd_v128_on_stack), f64x2_extract_lane, Vector),
    (Some(simd_v128_f64_on_stack), f64x2_replace_lane, Vector),
    (Some(simd_i32_on_stack), i8x16_splat, Vector),
    (Some(simd_i32_on_stack), i16x8_splat, Vector),
    (Some(simd_i32_on_stack), i32x4_splat, Vector),
    (Some(simd_i64_on_stack), i64x2_splat, Vector),
    (Some(simd_f32_on_stack), f32x4_splat, Vector),
    (Some(simd_f64_on_stack), f64x2_splat, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_swizzle, Vector),
    (Some(simd_v128_v128_on_stack_relaxed), i8x16_relaxed_swizzle, Vector),
    (Some(simd_v128_v128_v128_on_stack), v128_bitselect, Vector),
    (Some(simd_v128_v128_v128_on_stack_relaxed), i8x16_relaxed_laneselect, Vector),
    (Some(simd_v128_v128_v128_on_stack_relaxed), i16x8_relaxed_laneselect, Vector),
    (Some(simd_v128_v128_v128_on_stack_relaxed), i32x4_relaxed_laneselect, Vector),
    (Some(simd_v128_v128_v128_on_stack_relaxed), i64x2_relaxed_laneselect, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_eq, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_ne, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_lt_s, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_lt_u, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_gt_s, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_gt_u, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_le_s, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_le_u, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_ge_s, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_ge_u, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_eq, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_ne, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_lt_s, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_lt_u, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_gt_s, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_gt_u, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_le_s, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_le_u, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_ge_s, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_ge_u, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_eq, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_ne, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_lt_s, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_lt_u, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_gt_s, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_gt_u, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_le_s, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_le_u, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_ge_s, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_ge_u, Vector),
    (Some(simd_v128_v128_on_stack), i64x2_eq, Vector),
    (Some(simd_v128_v128_on_stack), i64x2_ne, Vector),
    (Some(simd_v128_v128_on_stack), i64x2_lt_s, Vector),
    (Some(simd_v128_v128_on_stack), i64x2_gt_s, Vector),
    (Some(simd_v128_v128_on_stack), i64x2_le_s, Vector),
    (Some(simd_v128_v128_on_stack), i64x2_ge_s, Vector),
    (Some(simd_v128_v128_on_stack), f32x4_eq, Vector),
    (Some(simd_v128_v128_on_stack), f32x4_ne, Vector),
    (Some(simd_v128_v128_on_stack), f32x4_lt, Vector),
    (Some(simd_v128_v128_on_stack), f32x4_gt, Vector),
    (Some(simd_v128_v128_on_stack), f32x4_le, Vector),
    (Some(simd_v128_v128_on_stack), f32x4_ge, Vector),
    (Some(simd_v128_v128_on_stack), f64x2_eq, Vector),
    (Some(simd_v128_v128_on_stack), f64x2_ne, Vector),
    (Some(simd_v128_v128_on_stack), f64x2_lt, Vector),
    (Some(simd_v128_v128_on_stack), f64x2_gt, Vector),
    (Some(simd_v128_v128_on_stack), f64x2_le, Vector),
    (Some(simd_v128_v128_on_stack), f64x2_ge, Vector),
    (Some(simd_v128_on_stack), v128_not, Vector),
    (Some(simd_v128_v128_on_stack), v128_and, Vector),
    (Some(simd_v128_v128_on_stack), v128_and_not, Vector),
    (Some(simd_v128_v128_on_stack), v128_or, Vector),
    (Some(simd_v128_v128_on_stack), v128_xor, Vector),
    (Some(simd_v128_v128_on_stack), v128_any_true, Vector),
    (Some(simd_v128_on_stack), i8x16_abs, Vector),
    (Some(simd_v128_on_stack), i8x16_neg, Vector),
    (Some(simd_v128_on_stack), i8x16_popcnt, Vector),
    (Some(simd_v128_on_stack), i8x16_all_true, Vector),
    (Some(simd_v128_on_stack), i8x16_bitmask, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_narrow_i16x8s, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_narrow_i16x8u, Vector),
    (Some(simd_v128_i32_on_stack), i8x16_shl, Vector),
    (Some(simd_v128_i32_on_stack), i8x16_shr_s, Vector),
    (Some(simd_v128_i32_on_stack), i8x16_shr_u, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_add, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_add_sat_s, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_add_sat_u, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_sub, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_sub_sat_s, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_sub_sat_u, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_min_s, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_min_u, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_max_s, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_max_u, Vector),
    (Some(simd_v128_v128_on_stack), i8x16_avgr_u, Vector),
    (Some(simd_v128_on_stack), i16x8_extadd_pairwise_i8x16s, Vector),
    (Some(simd_v128_on_stack), i16x8_extadd_pairwise_i8x16u, Vector),
    (Some(simd_v128_on_stack), i16x8_abs, Vector),
    (Some(simd_v128_on_stack), i16x8_neg, Vector),
    (Some(simd_v128_v128_on_stack), i16x8q15_mulr_sat_s, Vector),
    (Some(simd_v128_on_stack), i16x8_all_true, Vector),
    (Some(simd_v128_on_stack), i16x8_bitmask, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_narrow_i32x4s, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_narrow_i32x4u, Vector),
    (Some(simd_v128_on_stack), i16x8_extend_low_i8x16s, Vector),
    (Some(simd_v128_on_stack), i16x8_extend_high_i8x16s, Vector),
    (Some(simd_v128_on_stack), i16x8_extend_low_i8x16u, Vector),
    (Some(simd_v128_on_stack), i16x8_extend_high_i8x16u, Vector),
    (Some(simd_v128_i32_on_stack), i16x8_shl, Vector),
    (Some(simd_v128_i32_on_stack), i16x8_shr_s, Vector),
    (Some(simd_v128_i32_on_stack), i16x8_shr_u, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_add, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_add_sat_s, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_add_sat_u, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_sub, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_sub_sat_s, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_sub_sat_u, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_mul, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_min_s, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_min_u, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_max_s, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_max_u, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_avgr_u, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_extmul_low_i8x16s, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_extmul_high_i8x16s, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_extmul_low_i8x16u, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_extmul_high_i8x16u, Vector),
    (Some(simd_v128_on_stack), i32x4_extadd_pairwise_i16x8s, Vector),
    (Some(simd_v128_on_stack), i32x4_extadd_pairwise_i16x8u, Vector),
    (Some(simd_v128_on_stack), i32x4_abs, Vector),
    (Some(simd_v128_on_stack), i32x4_neg, Vector),
    (Some(simd_v128_on_stack), i32x4_all_true, Vector),
    (Some(simd_v128_on_stack), i32x4_bitmask, Vector),
    (Some(simd_v128_on_stack), i32x4_extend_low_i16x8s, Vector),
    (Some(simd_v128_on_stack), i32x4_extend_high_i16x8s, Vector),
    (Some(simd_v128_on_stack), i32x4_extend_low_i16x8u, Vector),
    (Some(simd_v128_on_stack), i32x4_extend_high_i16x8u, Vector),
    (Some(simd_v128_i32_on_stack), i32x4_shl, Vector),
    (Some(simd_v128_i32_on_stack), i32x4_shr_s, Vector),
    (Some(simd_v128_i32_on_stack), i32x4_shr_u, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_add, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_sub, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_mul, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_min_s, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_min_u, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_max_s, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_max_u, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_dot_i16x8s, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_extmul_low_i16x8s, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_extmul_high_i16x8s, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_extmul_low_i16x8u, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_extmul_high_i16x8u, Vector),
    (Some(simd_v128_on_stack), i64x2_abs, Vector),
    (Some(simd_v128_on_stack), i64x2_neg, Vector),
    (Some(simd_v128_on_stack), i64x2_all_true, Vector),
    (Some(simd_v128_on_stack), i64x2_bitmask, Vector),
    (Some(simd_v128_on_stack), i64x2_extend_low_i32x4s, Vector),
    (Some(simd_v128_on_stack), i64x2_extend_high_i32x4s, Vector),
    (Some(simd_v128_on_stack), i64x2_extend_low_i32x4u, Vector),
    (Some(simd_v128_on_stack), i64x2_extend_high_i32x4u, Vector),
    (Some(simd_v128_i32_on_stack), i64x2_shl, Vector),
    (Some(simd_v128_i32_on_stack), i64x2_shr_s, Vector),
    (Some(simd_v128_i32_on_stack), i64x2_shr_u, Vector),
    (Some(simd_v128_v128_on_stack), i64x2_add, Vector),
    (Some(simd_v128_v128_on_stack), i64x2_sub, Vector),
    (Some(simd_v128_v128_on_stack), i64x2_mul, Vector),
    (Some(simd_v128_v128_on_stack), i64x2_extmul_low_i32x4s, Vector),
    (Some(simd_v128_v128_on_stack), i64x2_extmul_high_i32x4s, Vector),
    (Some(simd_v128_v128_on_stack), i64x2_extmul_low_i32x4u, Vector),
    (Some(simd_v128_v128_on_stack), i64x2_extmul_high_i32x4u, Vector),
    (Some(simd_v128_on_stack), f32x4_ceil, Vector),
    (Some(simd_v128_on_stack), f32x4_floor, Vector),
    (Some(simd_v128_on_stack), f32x4_trunc, Vector),
    (Some(simd_v128_on_stack), f32x4_nearest, Vector),
    (Some(simd_v128_on_stack), f32x4_abs, Vector),
    (Some(simd_v128_on_stack), f32x4_neg, Vector),
    (Some(simd_v128_on_stack), f32x4_sqrt, Vector),
    (Some(simd_v128_v128_on_stack), f32x4_add, Vector),
    (Some(simd_v128_v128_on_stack), f32x4_sub, Vector),
    (Some(simd_v128_v128_on_stack), f32x4_mul, Vector),
    (Some(simd_v128_v128_on_stack), f32x4_div, Vector),
    (Some(simd_v128_v128_on_stack), f32x4_min, Vector),
    (Some(simd_v128_v128_on_stack), f32x4_max, Vector),
    (Some(simd_v128_v128_on_stack), f32x4p_min, Vector),
    (Some(simd_v128_v128_on_stack), f32x4p_max, Vector),
    (Some(simd_v128_on_stack), f64x2_ceil, Vector),
    (Some(simd_v128_on_stack), f64x2_floor, Vector),
    (Some(simd_v128_on_stack), f64x2_trunc, Vector),
    (Some(simd_v128_on_stack), f64x2_nearest, Vector),
    (Some(simd_v128_on_stack), f64x2_abs, Vector),
    (Some(simd_v128_on_stack), f64x2_neg, Vector),
    (Some(simd_v128_on_stack), f64x2_sqrt, Vector),
    (Some(simd_v128_v128_on_stack), f64x2_add, Vector),
    (Some(simd_v128_v128_on_stack), f64x2_sub, Vector),
    (Some(simd_v128_v128_on_stack), f64x2_mul, Vector),
    (Some(simd_v128_v128_on_stack), f64x2_div, Vector),
    (Some(simd_v128_v128_on_stack), f64x2_min, Vector),
    (Some(simd_v128_v128_on_stack), f64x2_max, Vector),
    (Some(simd_v128_v128_on_stack), f64x2p_min, Vector),
    (Some(simd_v128_v128_on_stack), f64x2p_max, Vector),
    (Some(simd_v128_on_stack), i32x4_trunc_sat_f32x4s, Vector),
    (Some(simd_v128_on_stack), i32x4_trunc_sat_f32x4u, Vector),
    (Some(simd_v128_on_stack), f32x4_convert_i32x4s, Vector),
    (Some(simd_v128_on_stack), f32x4_convert_i32x4u, Vector),
    (Some(simd_v128_on_stack), i32x4_trunc_sat_f64x2s_zero, Vector),
    (Some(simd_v128_on_stack), i32x4_trunc_sat_f64x2u_zero, Vector),
    (Some(simd_v128_on_stack), f64x2_convert_low_i32x4s, Vector),
    (Some(simd_v128_on_stack), f64x2_convert_low_i32x4u, Vector),
    (Some(simd_v128_on_stack), f32x4_demote_f64x2_zero, Vector),
    (Some(simd_v128_on_stack), f64x2_promote_low_f32x4, Vector),
    (Some(simd_v128_on_stack_relaxed), i32x4_relaxed_trunc_f32x4s, Vector),
    (Some(simd_v128_on_stack_relaxed), i32x4_relaxed_trunc_f32x4u, Vector),
    (Some(simd_v128_on_stack_relaxed), i32x4_relaxed_trunc_f64x2s_zero, Vector),
    (Some(simd_v128_on_stack_relaxed), i32x4_relaxed_trunc_f64x2u_zero, Vector),
    (Some(simd_v128_v128_v128_on_stack_relaxed), f32x4_relaxed_madd, Vector),
    (Some(simd_v128_v128_v128_on_stack_relaxed), f32x4_relaxed_nmadd, Vector),
    (Some(simd_v128_v128_v128_on_stack_relaxed), f64x2_relaxed_madd, Vector),
    (Some(simd_v128_v128_v128_on_stack_relaxed), f64x2_relaxed_nmadd, Vector),
    (Some(simd_v128_v128_on_stack_relaxed), f32x4_relaxed_min, Vector),
    (Some(simd_v128_v128_on_stack_relaxed), f32x4_relaxed_max, Vector),
    (Some(simd_v128_v128_on_stack_relaxed), f64x2_relaxed_min, Vector),
    (Some(simd_v128_v128_on_stack_relaxed), f64x2_relaxed_max, Vector),
    (Some(simd_v128_v128_on_stack_relaxed), i16x8_relaxed_q15mulr_s, Vector),
    (Some(simd_v128_v128_on_stack_relaxed), i16x8_relaxed_dot_i8x16_i7x16_s, Vector),
    (Some(simd_v128_v128_v128_on_stack_relaxed), i32x4_relaxed_dot_i8x16_i7x16_add_s, Vector),
}

pub(crate) struct CodeBuilderAllocations {
    // The control labels in scope right now.
    controls: Vec<Control>,

    // The types on the operand stack right now.
    operands: Vec<Option<ValType>>,

    // Dynamic set of options of instruction we can generate that are known to
    // be valid right now.
    options: Vec<(
        fn(&mut Unstructured, &Module, &mut CodeBuilder, &mut Vec<Instruction>) -> Result<()>,
        u32,
    )>,

    // Cached information about the module that we're generating functions for,
    // used to speed up validity checks. The mutable globals map is a map of the
    // type of global to the global indices which have that type (and they're
    // all mutable).
    mutable_globals: BTreeMap<ValType, Vec<u32>>,

    // Like mutable globals above this is a map from function types to the list
    // of functions that have that function type.
    functions: BTreeMap<Rc<FuncType>, Vec<u32>>,

    // Like functions above this is a map from tag types to the list of tags
    // have that tag type.
    tags: BTreeMap<Vec<ValType>, Vec<u32>>,

    // Tables in this module which have a funcref element type.
    funcref_tables: Vec<u32>,

    // Functions that are referenced in the module through globals and segments.
    referenced_functions: Vec<u32>,

    // Flag that indicates if any element segments have the same type as any
    // table
    table_init_possible: bool,

    // Lists of memory indices which are either 32-bit or 64-bit. This is used
    // for faster lookup in validating instructions to know which memories have
    // which types. For example if there are no 64-bit memories then we
    // shouldn't ever look for i64 on the stack for `i32.load`.
    memory32: Vec<u32>,
    memory64: Vec<u32>,

    // State used when dropping operands to avoid dropping them into the ether
    // but instead folding their final values into module state, at this time
    // chosen to be exported globals.
    globals_cnt: u32,
    new_globals: Vec<(ValType, ConstExpr)>,
    global_dropped_i32: Option<u32>,
    global_dropped_i64: Option<u32>,
    global_dropped_f32: Option<u32>,
    global_dropped_f64: Option<u32>,
    global_dropped_v128: Option<u32>,

    // Indicates that additional exports cannot be generated. This will be true
    // if the `Config` specifies exactly which exports should be present.
    disallow_exporting: bool,
}

pub(crate) struct CodeBuilder<'a> {
    func_ty: &'a FuncType,
    locals: &'a mut Vec<ValType>,
    allocs: &'a mut CodeBuilderAllocations,

    // Temporary locals injected and used by nan canonicalization. Note that
    // this list of extra locals is appended to `self.locals` at the end of code
    // generation, and it's kept separate here to avoid using these locals in
    // `local.get` and similar instructions.
    extra_locals: Vec<ValType>,
    f32_scratch: Option<usize>,
    f64_scratch: Option<usize>,
    v128_scratch: Option<usize>,
}

/// A control frame.
#[derive(Debug, Clone)]
struct Control {
    kind: ControlKind,
    /// Value types that must be on the stack when entering this control frame.
    params: Vec<ValType>,
    /// Value types that are left on the stack when exiting this control frame.
    results: Vec<ValType>,
    /// How far down the operand stack instructions inside this control frame
    /// can reach.
    height: usize,
}

impl Control {
    fn label_types(&self) -> &[ValType] {
        if self.kind == ControlKind::Loop {
            &self.params
        } else {
            &self.results
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum ControlKind {
    Block,
    If,
    Loop,
    TryTable,
}

enum Float {
    F32,
    F64,
    F32x4,
    F64x2,
}

impl CodeBuilderAllocations {
    pub(crate) fn new(module: &Module, disallow_exporting: bool) -> Self {
        let mut mutable_globals = BTreeMap::new();
        for (i, global) in module.globals.iter().enumerate() {
            if global.mutable {
                mutable_globals
                    .entry(global.val_type)
                    .or_insert(Vec::new())
                    .push(i as u32);
            }
        }

        let mut tags = BTreeMap::new();
        for (idx, tag_type) in module.tags() {
            tags.entry(tag_type.func_type.params.to_vec())
                .or_insert(Vec::new())
                .push(idx);
        }

        let mut functions = BTreeMap::new();
        for (idx, func) in module.funcs() {
            functions
                .entry(func.clone())
                .or_insert(Vec::new())
                .push(idx);
        }

        let mut funcref_tables = Vec::new();
        let mut table_tys = Vec::new();
        for (i, table) in module.tables.iter().enumerate() {
            table_tys.push(table.element_type);
            if table.element_type == RefType::FUNCREF {
                funcref_tables.push(i as u32);
            }
        }

        let mut referenced_functions = BTreeSet::new();
        for (_, expr) in module.defined_globals.iter() {
            if let Some(i) = expr.get_ref_func() {
                referenced_functions.insert(i);
            }
        }
        for g in module.elems.iter() {
            match &g.items {
                Elements::Expressions(e) => {
                    let iter = e.iter().filter_map(|e| e.get_ref_func());
                    referenced_functions.extend(iter);
                }
                Elements::Functions(e) => {
                    referenced_functions.extend(e.iter().cloned());
                }
            }
        }

        let table_init_possible = module.elems.iter().any(|e| table_tys.contains(&e.ty));

        let mut memory32 = Vec::new();
        let mut memory64 = Vec::new();
        for (i, mem) in module.memories.iter().enumerate() {
            if mem.memory64 {
                memory64.push(i as u32);
            } else {
                memory32.push(i as u32);
            }
        }

        let mut global_dropped_i32 = None;
        let mut global_dropped_i64 = None;
        let mut global_dropped_f32 = None;
        let mut global_dropped_f64 = None;
        let mut global_dropped_v128 = None;

        // If we can't export additional globals, try to use existing exported
        // mutable globals for dropped values.
        if disallow_exporting {
            for (_, kind, index) in module.exports.iter() {
                if *kind == ExportKind::Global {
                    let ty = module.globals[*index as usize];
                    if ty.mutable {
                        match ty.val_type {
                            ValType::I32 => {
                                if global_dropped_i32.is_none() {
                                    global_dropped_i32 = Some(*index)
                                } else {
                                    global_dropped_f32 = Some(*index)
                                }
                            }
                            ValType::I64 => {
                                if global_dropped_i64.is_none() {
                                    global_dropped_i64 = Some(*index)
                                } else {
                                    global_dropped_f64 = Some(*index)
                                }
                            }
                            ValType::V128 => global_dropped_v128 = Some(*index),
                            _ => {}
                        }
                    }
                }
            }
        }

        CodeBuilderAllocations {
            controls: Vec::with_capacity(4),
            operands: Vec::with_capacity(16),
            options: Vec::with_capacity(NUM_OPTIONS),
            functions,
            tags,
            mutable_globals,
            funcref_tables,
            referenced_functions: referenced_functions.into_iter().collect(),
            table_init_possible,
            memory32,
            memory64,

            global_dropped_i32,
            global_dropped_i64,
            global_dropped_f32,
            global_dropped_f64,
            global_dropped_v128,
            globals_cnt: module.globals.len() as u32,
            new_globals: Vec::new(),
            disallow_exporting,
        }
    }

    pub(crate) fn builder<'a>(
        &'a mut self,
        func_ty: &'a FuncType,
        locals: &'a mut Vec<ValType>,
    ) -> CodeBuilder<'a> {
        self.controls.clear();
        self.controls.push(Control {
            kind: ControlKind::Block,
            params: vec![],
            results: func_ty.results.to_vec(),
            height: 0,
        });

        self.operands.clear();
        self.options.clear();

        CodeBuilder {
            func_ty,
            locals,
            allocs: self,
            extra_locals: Vec::new(),
            f32_scratch: None,
            f64_scratch: None,
            v128_scratch: None,
        }
    }

    pub fn finish(self, u: &mut Unstructured<'_>, module: &mut Module) -> arbitrary::Result<()> {
        // Any globals injected as part of dropping operands on the stack get
        // injected into the module here. Each global is then exported, most of
        // the time (if additional exports are allowed), to ensure it's part of
        // the "image" of this module available for differential execution for
        // example.
        for (ty, init) in self.new_globals {
            let global_idx = module.globals.len() as u32;
            module.globals.push(GlobalType {
                val_type: ty,
                mutable: true,
                shared: false,
            });
            module.defined_globals.push((global_idx, init));

            if self.disallow_exporting || u.ratio(1, 100).unwrap_or(false) {
                continue;
            }

            let name = unique_string(1_000, &mut module.export_names, u)?;
            module.add_arbitrary_export(name, ExportKind::Global, global_idx)?;
        }
        Ok(())
    }
}

impl CodeBuilder<'_> {
    fn pop_control(&mut self) -> Control {
        let control = self.allocs.controls.pop().unwrap();

        // Pop the actual types on the stack (which could be subtypes of the
        // declared types) and then push the declared types. This avoids us
        // accidentally generating code that relies on erased subtypes.
        for _ in &control.results {
            self.pop_operand();
        }
        for ty in &control.results {
            self.push_operand(Some(*ty));
        }

        control
    }

    fn push_control(
        &mut self,
        kind: ControlKind,
        params: impl Into<Vec<ValType>>,
        results: impl Into<Vec<ValType>>,
    ) {
        let params = params.into();
        let results = results.into();

        // Similar to in `pop_control`, we want to pop the actual argument types
        // off the stack (which could be subtypes of the declared parameter
        // types) and then push the parameter types. This effectively does type
        // erasure of any subtyping that exists so that we don't accidentally
        // generate code that relies on the specific subtypes.
        for _ in &params {
            self.pop_operand();
        }
        self.push_operands(&params);

        let height = self.allocs.operands.len() - params.len();
        self.allocs.controls.push(Control {
            kind,
            params,
            results,
            height,
        });
    }

    /// Get the operands that are in-scope within the current control frame.
    #[inline]
    fn operands(&self) -> &[Option<ValType>] {
        let height = self.allocs.controls.last().map_or(0, |c| c.height);
        &self.allocs.operands[height..]
    }

    /// Pop a single operand from the stack, regardless of expected type.
    #[inline]
    fn pop_operand(&mut self) -> Option<ValType> {
        self.allocs.operands.pop().unwrap()
    }

    #[inline]
    fn pop_operands(&mut self, module: &Module, to_pop: &[ValType]) {
        debug_assert!(self.types_on_stack(module, to_pop));
        self.allocs
            .operands
            .truncate(self.allocs.operands.len() - to_pop.len());
    }

    #[inline]
    fn push_operands(&mut self, to_push: &[ValType]) {
        self.allocs
            .operands
            .extend(to_push.iter().copied().map(Some));
    }

    #[inline]
    fn push_operand(&mut self, ty: Option<ValType>) {
        self.allocs.operands.push(ty);
    }

    fn pop_label_types(&mut self, module: &Module, target: u32) {
        let target = usize::try_from(target).unwrap();
        let control = &self.allocs.controls[self.allocs.controls.len() - 1 - target];
        debug_assert!(self.label_types_on_stack(module, control));
        self.allocs
            .operands
            .truncate(self.allocs.operands.len() - control.label_types().len());
    }

    fn push_label_types(&mut self, target: u32) {
        let target = usize::try_from(target).unwrap();
        let control = &self.allocs.controls[self.allocs.controls.len() - 1 - target];
        self.allocs
            .operands
            .extend(control.label_types().iter().copied().map(Some));
    }

    /// Pop the target label types, and then push them again.
    ///
    /// This is not a no-op due to subtyping: if we have a `T <: U` on the
    /// stack, and the target label's type is `[U]`, then this will erase the
    /// information about `T` and subsequent operations may only operate on `U`.
    fn pop_push_label_types(&mut self, module: &Module, target: u32) {
        self.pop_label_types(module, target);
        self.push_label_types(target)
    }

    fn label_types_on_stack(&self, module: &Module, to_check: &Control) -> bool {
        self.types_on_stack(module, to_check.label_types())
    }

    /// Is the given type on top of the stack?
    #[inline]
    fn type_on_stack(&self, module: &Module, ty: ValType) -> bool {
        self.type_on_stack_at(module, 0, ty)
    }

    /// Is the given type on the stack at the given index (indexing from the top
    /// of the stack towards the bottom).
    #[inline]
    fn type_on_stack_at(&self, module: &Module, at: usize, expected: ValType) -> bool {
        let operands = self.operands();
        if at >= operands.len() {
            return false;
        }
        match operands[operands.len() - 1 - at] {
            None => true,
            Some(actual) => module.val_type_is_sub_type(actual, expected),
        }
    }

    /// Are the given types on top of the stack?
    #[inline]
    fn types_on_stack(&self, module: &Module, types: &[ValType]) -> bool {
        self.operands().len() >= types.len()
            && types
                .iter()
                .rev()
                .enumerate()
                .all(|(idx, ty)| self.type_on_stack_at(module, idx, *ty))
    }

    /// Are the given field types on top of the stack?
    #[inline]
    fn field_types_on_stack(&self, module: &Module, types: &[FieldType]) -> bool {
        self.operands().len() >= types.len()
            && types
                .iter()
                .rev()
                .enumerate()
                .all(|(idx, ty)| self.type_on_stack_at(module, idx, ty.element_type.unpack()))
    }

    /// Is the given field type on top of the stack?
    #[inline]
    fn field_type_on_stack(&self, module: &Module, ty: FieldType) -> bool {
        self.type_on_stack(module, ty.element_type.unpack())
    }

    /// Is the given field type on the stack at the given position (indexed from
    /// the top of the stack)?
    #[inline]
    fn field_type_on_stack_at(&self, module: &Module, at: usize, ty: FieldType) -> bool {
        self.type_on_stack_at(module, at, ty.element_type.unpack())
    }

    /// Get the ref type on the top of the operand stack, if any.
    ///
    /// * `None` means no reftype on the stack.
    /// * `Some(None)` means that the stack is polymorphic.
    /// * `Some(Some(r))` means that `r` is the ref type on top of the stack.
    fn ref_type_on_stack(&self) -> Option<Option<RefType>> {
        match self.operands().last().copied()? {
            Some(ValType::Ref(r)) => Some(Some(r)),
            Some(_) => None,
            None => Some(None),
        }
    }

    /// Is there a `(ref null? <index>)` on the stack at the given position? If
    /// so return its nullability and type index.
    fn concrete_ref_type_on_stack_at(&self, at: usize) -> Option<(bool, u32)> {
        match self.operands().iter().copied().rev().nth(at)?? {
            ValType::Ref(RefType {
                nullable,
                heap_type: HeapType::Concrete(ty),
            }) => Some((nullable, ty)),
            _ => None,
        }
    }

    /// Is there a `(ref null? <index>)` at the given stack position that
    /// references a concrete array type?
    fn concrete_array_ref_type_on_stack_at(
        &self,
        module: &Module,
        at: usize,
    ) -> Option<(bool, u32, ArrayType)> {
        let (nullable, ty) = self.concrete_ref_type_on_stack_at(at)?;
        match &module.ty(ty).composite_type {
            CompositeType::Array(a) => Some((nullable, ty, *a)),
            _ => None,
        }
    }

    /// Is there a `(ref null? <index>)` at the given stack position that
    /// references a concrete struct type?
    fn concrete_struct_ref_type_on_stack_at<'a>(
        &self,
        module: &'a Module,
        at: usize,
    ) -> Option<(bool, u32, &'a StructType)> {
        let (nullable, ty) = self.concrete_ref_type_on_stack_at(at)?;
        match &module.ty(ty).composite_type {
            CompositeType::Struct(s) => Some((nullable, ty, s)),
            _ => None,
        }
    }

    /// Pop a reference type from the stack and return it.
    ///
    /// When in unreachable code and the stack is polymorphic, returns `None`.
    fn pop_ref_type(&mut self) -> Option<RefType> {
        let ref_ty = self.ref_type_on_stack().unwrap();
        self.pop_operand();
        ref_ty
    }

    /// Pops a `(ref null? <index>)` from the stack and return its nullability
    /// and type index.
    fn pop_concrete_ref_type(&mut self) -> (bool, u32) {
        let ref_ty = self.pop_ref_type().unwrap();
        match ref_ty.heap_type {
            HeapType::Concrete(i) => (ref_ty.nullable, i),
            _ => panic!("not a concrete ref type"),
        }
    }

    /// Get the `(ref null? <index>)` type on the top of the stack that
    /// references a function type, if any.
    fn concrete_funcref_on_stack(&self, module: &Module) -> Option<RefType> {
        match self.operands().last().copied()?? {
            ValType::Ref(r) => match r.heap_type {
                HeapType::Concrete(idx) => match &module.ty(idx).composite_type {
                    CompositeType::Func(_) => Some(r),
                    CompositeType::Struct(_) | CompositeType::Array(_) => None,
                },
                _ => None,
            },
            _ => None,
        }
    }

    /// Is there a `(ref null? <index>)` on the top of the stack that references
    /// a struct type with at least one field?
    fn non_empty_struct_ref_on_stack(&self, module: &Module, allow_null_refs: bool) -> bool {
        match self.operands().last() {
            Some(Some(ValType::Ref(RefType {
                nullable,
                heap_type: HeapType::Concrete(idx),
            }))) => match &module.ty(*idx).composite_type {
                CompositeType::Struct(s) => !s.fields.is_empty() && (!nullable || allow_null_refs),
                _ => false,
            },
            _ => false,
        }
    }

    #[inline(never)]
    fn arbitrary_block_type(&self, u: &mut Unstructured, module: &Module) -> Result<BlockType> {
        let mut options: Vec<Box<dyn Fn(&mut Unstructured) -> Result<BlockType>>> = vec![
            Box::new(|_| Ok(BlockType::Empty)),
            Box::new(|u| Ok(BlockType::Result(module.arbitrary_valtype(u)?))),
        ];
        if module.config.multi_value_enabled {
            for (i, ty) in module.func_types() {
                if self.types_on_stack(module, &ty.params) {
                    options.push(Box::new(move |_| Ok(BlockType::FunctionType(i as u32))));
                }
            }
        }
        let f = u.choose(&options)?;
        f(u)
    }

    pub(crate) fn arbitrary(
        mut self,
        u: &mut Unstructured,
        module: &Module,
    ) -> Result<Vec<Instruction>> {
        let max_instructions = module.config.max_instructions;
        let allowed_instructions = module.config.allowed_instructions;
        let mut instructions = vec![];

        while !self.allocs.controls.is_empty() {
            let keep_going = instructions.len() < max_instructions && u.arbitrary::<u8>()? != 0;
            if !keep_going {
                self.end_active_control_frames(
                    u,
                    module,
                    &mut instructions,
                    module.config.disallow_traps,
                )?;
                break;
            }

            match choose_instruction(u, module, allowed_instructions, &mut self) {
                Some(f) => {
                    f(u, module, &mut self, &mut instructions)?;
                }
                // Choosing an instruction can fail because there is not enough
                // underlying data, so we really cannot generate any more
                // instructions. In this case we swallow that error and instead
                // just terminate our wasm function's frames.
                None => {
                    self.end_active_control_frames(
                        u,
                        module,
                        &mut instructions,
                        module.config.disallow_traps,
                    )?;
                    break;
                }
            }

            // If the configuration for this module requests nan
            // canonicalization then perform that here based on whether or not
            // the previous instruction needs canonicalization. Note that this
            // is based off Cranelift's pass for nan canonicalization for which
            // instructions to canonicalize, but the general idea is most
            // floating-point operations.
            if module.config.canonicalize_nans {
                match instructions.last().unwrap() {
                    Instruction::F32Ceil
                    | Instruction::F32Floor
                    | Instruction::F32Nearest
                    | Instruction::F32Sqrt
                    | Instruction::F32Trunc
                    | Instruction::F32Div
                    | Instruction::F32Max
                    | Instruction::F32Min
                    | Instruction::F32Mul
                    | Instruction::F32Sub
                    | Instruction::F32Add => self.canonicalize_nan(Float::F32, &mut instructions),
                    Instruction::F64Ceil
                    | Instruction::F64Floor
                    | Instruction::F64Nearest
                    | Instruction::F64Sqrt
                    | Instruction::F64Trunc
                    | Instruction::F64Div
                    | Instruction::F64Max
                    | Instruction::F64Min
                    | Instruction::F64Mul
                    | Instruction::F64Sub
                    | Instruction::F64Add => self.canonicalize_nan(Float::F64, &mut instructions),
                    Instruction::F32x4Ceil
                    | Instruction::F32x4Floor
                    | Instruction::F32x4Nearest
                    | Instruction::F32x4Sqrt
                    | Instruction::F32x4Trunc
                    | Instruction::F32x4Div
                    | Instruction::F32x4Max
                    | Instruction::F32x4Min
                    | Instruction::F32x4Mul
                    | Instruction::F32x4Sub
                    | Instruction::F32x4Add => {
                        self.canonicalize_nan(Float::F32x4, &mut instructions)
                    }
                    Instruction::F64x2Ceil
                    | Instruction::F64x2Floor
                    | Instruction::F64x2Nearest
                    | Instruction::F64x2Sqrt
                    | Instruction::F64x2Trunc
                    | Instruction::F64x2Div
                    | Instruction::F64x2Max
                    | Instruction::F64x2Min
                    | Instruction::F64x2Mul
                    | Instruction::F64x2Sub
                    | Instruction::F64x2Add => {
                        self.canonicalize_nan(Float::F64x2, &mut instructions)
                    }
                    _ => {}
                }
            }
        }

        self.locals.extend(self.extra_locals.drain(..));

        Ok(instructions)
    }

    fn canonicalize_nan(&mut self, ty: Float, ins: &mut Vec<Instruction>) {
        // We'll need to temporarily save the top of the stack into a local, so
        // figure out that local here. Note that this tries to use the same
        // local if canonicalization happens more than once in a function.
        let (local, val_ty) = match ty {
            Float::F32 => (&mut self.f32_scratch, ValType::F32),
            Float::F64 => (&mut self.f64_scratch, ValType::F64),
            Float::F32x4 | Float::F64x2 => (&mut self.v128_scratch, ValType::V128),
        };
        let local = match *local {
            Some(i) => i as u32,
            None => self.alloc_local(val_ty),
        };

        // Save the previous instruction's result into a local. This also leaves
        // a value on the stack as `val1` for the `select` instruction.
        ins.push(Instruction::LocalTee(local));

        // The `val2` value input to the `select` below, our nan pattern.
        //
        // The nan patterns here are chosen to be a canonical representation
        // which is still NaN but the wasm will always produce the same bits of
        // a nan so if the wasm takes a look at the nan inside it'll always see
        // the same representation.
        const CANON_32BIT_NAN: u32 = 0b01111111110000000000000000000000;
        const CANON_64BIT_NAN: u64 =
            0b0111111111111000000000000000000000000000000000000000000000000000;
        ins.push(match ty {
            Float::F32 => Instruction::F32Const(f32::from_bits(CANON_32BIT_NAN)),
            Float::F64 => Instruction::F64Const(f64::from_bits(CANON_64BIT_NAN)),
            Float::F32x4 => {
                let nan = CANON_32BIT_NAN as i128;
                let nan = nan | (nan << 32) | (nan << 64) | (nan << 96);
                Instruction::V128Const(nan)
            }
            Float::F64x2 => {
                let nan = CANON_64BIT_NAN as i128;
                let nan = nan | (nan << 64);
                Instruction::V128Const(nan)
            }
        });

        // the condition of the `select`, which is the float's equality test
        // with itself.
        ins.push(Instruction::LocalGet(local));
        ins.push(Instruction::LocalGet(local));
        ins.push(match ty {
            Float::F32 => Instruction::F32Eq,
            Float::F64 => Instruction::F64Eq,
            Float::F32x4 => Instruction::F32x4Eq,
            Float::F64x2 => Instruction::F64x2Eq,
        });

        // Select the result. If the condition is nonzero (aka the float is
        // equal to itself) it picks `val1`, otherwise if zero (aka the float
        // is nan) it picks `val2`.
        ins.push(match ty {
            Float::F32 | Float::F64 => Instruction::Select,
            Float::F32x4 | Float::F64x2 => Instruction::V128Bitselect,
        });
    }

    fn alloc_local(&mut self, ty: ValType) -> u32 {
        let val = self.locals.len() + self.func_ty.params.len() + self.extra_locals.len();
        self.extra_locals.push(ty);
        u32::try_from(val).unwrap()
    }

    fn end_active_control_frames(
        &mut self,
        u: &mut Unstructured<'_>,
        module: &Module,
        instructions: &mut Vec<Instruction>,
        disallow_traps: bool,
    ) -> Result<()> {
        while !self.allocs.controls.is_empty() {
            // Ensure that this label is valid by placing the right types onto
            // the operand stack for the end of the label.
            self.guarantee_label_results(u, module, instructions, disallow_traps)?;

            // Remove the label and clear the operand stack since the label has
            // been removed.
            let label = self.allocs.controls.pop().unwrap();
            self.allocs.operands.truncate(label.height);

            // If this is an `if` that is not stack neutral, then it
            // must have an `else`. Generate synthetic results here in the same
            // manner we did above.
            if label.kind == ControlKind::If && label.params != label.results {
                instructions.push(Instruction::Else);
                self.allocs.controls.push(label.clone());
                self.allocs
                    .operands
                    .extend(label.params.into_iter().map(Some));
                self.guarantee_label_results(u, module, instructions, disallow_traps)?;
                self.allocs.controls.pop();
                self.allocs.operands.truncate(label.height);
            }

            // The last control frame for the function return does not
            // need an `end` instruction.
            if !self.allocs.controls.is_empty() {
                instructions.push(Instruction::End);
            }

            // Place the results of the label onto the operand stack for use
            // after the label.
            self.allocs
                .operands
                .extend(label.results.into_iter().map(Some));
        }
        Ok(())
    }

    /// Modifies the instruction stream to guarantee that the current control
    /// label's results are on the stack and ready for the control label to return.
    fn guarantee_label_results(
        &mut self,
        u: &mut Unstructured<'_>,
        module: &Module,
        instructions: &mut Vec<Instruction>,
        disallow_traps: bool,
    ) -> Result<()> {
        let operands = self.operands();
        let label = self.allocs.controls.last().unwrap();

        // Already done, yay!
        if label.results.len() == operands.len() && self.types_on_stack(module, &label.results) {
            return Ok(());
        }

        // Generating an unreachable instruction is always a valid way to
        // generate any types for a label, but it's not too interesting, so
        // don't favor it.
        if !disallow_traps && u.ratio(1, u16::MAX)? {
            instructions.push(Instruction::Unreachable);
            return Ok(());
        }

        // Arbitrarily massage the stack to get the expected results. First we
        // drop all extraneous results to we're only dealing with those we want
        // to deal with. Afterwards we start at the bottom of the stack and move
        // up, figuring out what matches and what doesn't. As soon as something
        // doesn't match we throw out that and everything else remaining,
        // filling in results with dummy values.
        let operands = operands.to_vec();
        let mut operands = operands.as_slice();
        let label_results = label.results.to_vec();
        while operands.len() > label_results.len() {
            self.drop_operand(u, *operands.last().unwrap(), instructions)?;
            operands = &operands[..operands.len() - 1];
        }
        for (i, expected) in label_results.iter().enumerate() {
            if let Some(actual) = operands.get(i) {
                if Some(*expected) == *actual {
                    continue;
                }
                for ty in operands[i..].iter().rev() {
                    self.drop_operand(u, *ty, instructions)?;
                }
                operands = &[];
            }
            instructions.push(module.arbitrary_const_instruction(*expected, u)?);
        }
        Ok(())
    }

    fn drop_operand(
        &mut self,
        u: &mut Unstructured<'_>,
        ty: Option<ValType>,
        instructions: &mut Vec<Instruction>,
    ) -> Result<()> {
        if !self.mix_operand_into_global(u, ty, instructions)? {
            instructions.push(Instruction::Drop);
        }
        Ok(())
    }

    /// Attempts to drop the top operand on the stack by "mixing" it into a
    /// global.
    ///
    /// This is done to avoid dropping values on the floor to ensure that
    /// everything is part of some computation somewhere. Otherwise, for
    /// example, most function results are dropped on the floor as the stack
    /// won't happen to match the function type that we're generating.
    ///
    /// This will return `true` if the operand has been dropped, and `false` if
    /// it didn't for one reason or another.
    fn mix_operand_into_global(
        &mut self,
        u: &mut Unstructured<'_>,
        ty: Option<ValType>,
        instructions: &mut Vec<Instruction>,
    ) -> Result<bool> {
        // If the type of this operand isn't known, for example if it's relevant
        // to unreachable code, then it can't be combined, so return `false`.
        let ty = match ty {
            Some(ty) => ty,
            None => return Ok(false),
        };

        // Use the input stream to allow a small chance of dropping the value
        // without combining it.
        if u.ratio(1, 100)? {
            return Ok(false);
        }

        // Depending on the type lookup or inject a global to place this value
        // into.
        let (global, combine) = match ty {
            ValType::I32 => {
                let global = *self.allocs.global_dropped_i32.get_or_insert_with(|| {
                    self.allocs.new_globals.push((ty, ConstExpr::i32_const(0)));
                    inc(&mut self.allocs.globals_cnt)
                });
                (global, Instruction::I32Xor)
            }
            ValType::I64 => {
                let global = *self.allocs.global_dropped_i64.get_or_insert_with(|| {
                    self.allocs.new_globals.push((ty, ConstExpr::i64_const(0)));
                    inc(&mut self.allocs.globals_cnt)
                });
                (global, Instruction::I64Xor)
            }
            ValType::F32 => {
                let global = *self.allocs.global_dropped_f32.get_or_insert_with(|| {
                    self.allocs
                        .new_globals
                        .push((ValType::I32, ConstExpr::i32_const(0)));
                    inc(&mut self.allocs.globals_cnt)
                });
                instructions.push(Instruction::I32ReinterpretF32);
                (global, Instruction::I32Xor)
            }
            ValType::F64 => {
                let global = *self.allocs.global_dropped_f64.get_or_insert_with(|| {
                    self.allocs
                        .new_globals
                        .push((ValType::I64, ConstExpr::i64_const(0)));
                    inc(&mut self.allocs.globals_cnt)
                });
                instructions.push(Instruction::I64ReinterpretF64);
                (global, Instruction::I64Xor)
            }
            ValType::V128 => {
                let global = *self.allocs.global_dropped_v128.get_or_insert_with(|| {
                    self.allocs.new_globals.push((ty, ConstExpr::v128_const(0)));
                    inc(&mut self.allocs.globals_cnt)
                });
                (global, Instruction::V128Xor)
            }

            // Don't know how to combine reference types at this time, so just
            // let it get dropped.
            ValType::Ref(_) => return Ok(false),
        };
        instructions.push(Instruction::GlobalGet(global));
        instructions.push(combine);
        instructions.push(Instruction::GlobalSet(global));

        return Ok(true);

        fn inc(val: &mut u32) -> u32 {
            let ret = *val;
            *val += 1;
            ret
        }
    }
}

#[inline]
fn unreachable_valid(module: &Module, _: &mut CodeBuilder) -> bool {
    !module.config.disallow_traps
}

fn unreachable(
    _: &mut Unstructured,
    _: &Module,
    _: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    instructions.push(Instruction::Unreachable);
    Ok(())
}

fn nop(
    _: &mut Unstructured,
    _: &Module,
    _: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    instructions.push(Instruction::Nop);
    Ok(())
}

fn block(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let block_ty = builder.arbitrary_block_type(u, module)?;
    let (params, results) = module.params_results(&block_ty);
    builder.push_control(ControlKind::Block, params, results);
    instructions.push(Instruction::Block(block_ty));
    Ok(())
}

#[inline]
fn try_table_valid(module: &Module, _: &mut CodeBuilder) -> bool {
    module.config.exceptions_enabled
}

fn try_table(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let block_ty = builder.arbitrary_block_type(u, module)?;

    let mut catch_options: Vec<
        Box<dyn Fn(&mut Unstructured<'_>, &mut CodeBuilder<'_>) -> Result<Catch>>,
    > = Vec::new();

    for (i, ctrl) in builder.allocs.controls.iter().rev().enumerate() {
        let i = i as u32;

        let label_types = ctrl.label_types();

        // Empty labels are candidates for a `catch_all` since nothing is
        // pushed in that case.
        if label_types.is_empty() {
            catch_options.push(Box::new(move |_, _| Ok(Catch::All { label: i })));
        }

        // Labels with just an `externref` are suitable for `catch_all_refs`,
        // which first pushes nothing since there's no tag and then pushes
        // the caught exception value.
        if label_types == [ValType::EXNREF] {
            catch_options.push(Box::new(move |_, _| Ok(Catch::AllRef { label: i })));
        }

        // If there is a tag which exactly matches the types of the label we're
        // looking at then that tag can be used as part of a `catch` branch.
        // That tag's parameters, which are the except values, are pushed
        // for the label.
        if builder.allocs.tags.contains_key(label_types) {
            let label_types = label_types.to_vec();
            catch_options.push(Box::new(move |u, builder| {
                Ok(Catch::One {
                    tag: *u.choose(&builder.allocs.tags[&label_types])?,
                    label: i,
                })
            }));
        }

        // And finally the last type of catch label, `catch_ref`. If the label
        // ends with `exnref`, then use everything except the last `exnref` to
        // see if there's a matching tag. If so then `catch_ref` can be used
        // with that tag when branching to this label.
        if let Some((&ValType::EXNREF, rest)) = label_types.split_last() {
            if builder.allocs.tags.contains_key(rest) {
                let rest = rest.to_vec();
                catch_options.push(Box::new(move |u, builder| {
                    Ok(Catch::OneRef {
                        tag: *u.choose(&builder.allocs.tags[&rest])?,
                        label: i,
                    })
                }));
            }
        }
    }

    let mut catches = Vec::new();
    if catch_options.len() > 0 {
        for _ in 0..u.int_in_range(0..=10)? {
            catches.push(u.choose(&mut catch_options)?(u, builder)?);
        }
    }

    let (params, results) = module.params_results(&block_ty);
    builder.push_control(ControlKind::TryTable, params, results);

    instructions.push(Instruction::TryTable(block_ty, catches.into()));
    Ok(())
}

fn r#loop(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let block_ty = builder.arbitrary_block_type(u, module)?;
    let (params, results) = module.params_results(&block_ty);
    builder.push_control(ControlKind::Loop, params, results);
    instructions.push(Instruction::Loop(block_ty));
    Ok(())
}

#[inline]
fn if_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    builder.type_on_stack(module, ValType::I32)
}

fn r#if(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    let block_ty = builder.arbitrary_block_type(u, module)?;
    let (params, results) = module.params_results(&block_ty);
    builder.push_control(ControlKind::If, params, results);
    instructions.push(Instruction::If(block_ty));
    Ok(())
}

#[inline]
fn else_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    let last_control = builder.allocs.controls.last().unwrap();
    last_control.kind == ControlKind::If
        && builder.operands().len() == last_control.results.len()
        && builder.types_on_stack(module, &last_control.results)
}

fn r#else(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let control = builder.pop_control();
    builder.pop_operands(module, &control.results);
    builder.push_operands(&control.params);
    builder.push_control(ControlKind::Block, control.params, control.results);
    instructions.push(Instruction::Else);
    Ok(())
}

#[inline]
fn end_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    // Note: first control frame is the function return's control frame, which
    // does not have an associated `end`.
    if builder.allocs.controls.len() <= 1 {
        return false;
    }
    let control = builder.allocs.controls.last().unwrap();
    builder.operands().len() == control.results.len()
        && builder.types_on_stack(module, &control.results)
        // `if`s that don't leave the stack as they found it must have an
        // `else`.
        && !(control.kind == ControlKind::If && control.params != control.results)
}

fn end(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_control();
    instructions.push(Instruction::End);
    Ok(())
}

#[inline]
fn br_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    builder
        .allocs
        .controls
        .iter()
        .any(|l| builder.label_types_on_stack(module, l))
}

fn br(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let n = builder
        .allocs
        .controls
        .iter()
        .filter(|l| builder.label_types_on_stack(module, l))
        .count();
    debug_assert!(n > 0);
    let i = u.int_in_range(0..=n - 1)?;
    let (target, _) = builder
        .allocs
        .controls
        .iter()
        .rev()
        .enumerate()
        .filter(|(_, l)| builder.label_types_on_stack(module, l))
        .nth(i)
        .unwrap();
    let target = u32::try_from(target).unwrap();
    builder.pop_label_types(module, target);
    instructions.push(Instruction::Br(target));
    Ok(())
}

#[inline]
fn br_if_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    if !builder.type_on_stack(module, ValType::I32) {
        return false;
    }
    let ty = builder.allocs.operands.pop().unwrap();
    let is_valid = builder
        .allocs
        .controls
        .iter()
        .any(|l| builder.label_types_on_stack(module, l));
    builder.allocs.operands.push(ty);
    is_valid
}

fn br_if(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);

    let n = builder
        .allocs
        .controls
        .iter()
        .filter(|l| builder.label_types_on_stack(module, l))
        .count();
    debug_assert!(n > 0);
    let i = u.int_in_range(0..=n - 1)?;
    let (target, _) = builder
        .allocs
        .controls
        .iter()
        .rev()
        .enumerate()
        .filter(|(_, l)| builder.label_types_on_stack(module, l))
        .nth(i)
        .unwrap();
    let target = u32::try_from(target).unwrap();
    builder.pop_push_label_types(module, target);
    instructions.push(Instruction::BrIf(target));
    Ok(())
}

#[inline]
fn br_table_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    if !builder.type_on_stack(module, ValType::I32) {
        return false;
    }
    let ty = builder.allocs.operands.pop().unwrap();
    let is_valid = br_valid(module, builder);
    builder.allocs.operands.push(ty);
    is_valid
}

fn br_table(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);

    let n = builder
        .allocs
        .controls
        .iter()
        .filter(|l| builder.label_types_on_stack(module, l))
        .count();
    debug_assert!(n > 0);

    let i = u.int_in_range(0..=n - 1)?;
    let (default_target, _) = builder
        .allocs
        .controls
        .iter()
        .rev()
        .enumerate()
        .filter(|(_, l)| builder.label_types_on_stack(module, l))
        .nth(i)
        .unwrap();
    let control = &builder.allocs.controls[builder.allocs.controls.len() - 1 - default_target];

    let targets = builder
        .allocs
        .controls
        .iter()
        .rev()
        .enumerate()
        .filter(|(_, l)| l.label_types() == control.label_types())
        .map(|(t, _)| t as u32)
        .collect();

    let tys = control.label_types().to_vec();
    builder.pop_operands(module, &tys);

    instructions.push(Instruction::BrTable(targets, default_target as u32));
    Ok(())
}

#[inline]
fn return_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    builder.label_types_on_stack(module, &builder.allocs.controls[0])
}

fn r#return(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let results = builder.allocs.controls[0].results.clone();
    builder.pop_operands(module, &results);
    instructions.push(Instruction::Return);
    Ok(())
}

#[inline]
fn call_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    builder
        .allocs
        .functions
        .keys()
        .any(|func_ty| builder.types_on_stack(module, &func_ty.params))
}

fn call(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let candidates = builder
        .allocs
        .functions
        .iter()
        .filter(|(func_ty, _)| builder.types_on_stack(module, &func_ty.params))
        .flat_map(|(_, v)| v.iter().copied())
        .collect::<Vec<_>>();
    assert!(candidates.len() > 0);
    let i = u.int_in_range(0..=candidates.len() - 1)?;
    let (func_idx, ty) = module.funcs().nth(candidates[i] as usize).unwrap();
    builder.pop_operands(module, &ty.params);
    builder.push_operands(&ty.results);
    instructions.push(Instruction::Call(func_idx as u32));
    Ok(())
}

#[inline]
fn call_ref_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    if !module.config.gc_enabled {
        return false;
    }
    let funcref = match builder.concrete_funcref_on_stack(module) {
        Some(f) => f,
        None => return false,
    };
    if module.config.disallow_traps && funcref.nullable {
        return false;
    }
    match funcref.heap_type {
        HeapType::Concrete(idx) => {
            let ty = builder.allocs.operands.pop().unwrap();
            let params = &module.ty(idx).unwrap_func().params;
            let valid = builder.types_on_stack(module, params);
            builder.allocs.operands.push(ty);
            valid
        }
        _ => unreachable!(),
    }
}

fn call_ref(
    _u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let heap_ty = match builder.pop_operand() {
        Some(ValType::Ref(r)) => r.heap_type,
        _ => unreachable!(),
    };
    let idx = match heap_ty {
        HeapType::Concrete(idx) => idx,
        _ => unreachable!(),
    };
    let func_ty = match &module.ty(idx).composite_type {
        CompositeType::Func(f) => f,
        _ => unreachable!(),
    };
    builder.pop_operands(module, &func_ty.params);
    builder.push_operands(&func_ty.results);
    instructions.push(Instruction::CallRef(idx));
    Ok(())
}

#[inline]
fn call_indirect_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    if builder.allocs.funcref_tables.is_empty() || !builder.type_on_stack(module, ValType::I32) {
        return false;
    }
    if module.config.disallow_traps {
        // We have no way to reflect, at run time, on a `funcref` in
        // the `i`th slot in a table and dynamically avoid trapping
        // `call_indirect`s. Therefore, we can't emit *any*
        // `call_indirect` instructions if we want to avoid traps.
        return false;
    }
    let ty = builder.allocs.operands.pop().unwrap();
    let is_valid = module
        .func_types()
        .any(|(_, ty)| builder.types_on_stack(module, &ty.params));
    builder.allocs.operands.push(ty);
    is_valid
}

fn call_indirect(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);

    let choices = module
        .func_types()
        .filter(|(_, ty)| builder.types_on_stack(module, &ty.params))
        .collect::<Vec<_>>();
    let (type_idx, ty) = u.choose(&choices)?;
    builder.pop_operands(module, &ty.params);
    builder.push_operands(&ty.results);
    let table = *u.choose(&builder.allocs.funcref_tables)?;
    instructions.push(Instruction::CallIndirect {
        ty: *type_idx as u32,
        table,
    });
    Ok(())
}

#[inline]
fn return_call_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    if !module.config.tail_call_enabled {
        return false;
    }

    builder.allocs.functions.keys().any(|func_ty| {
        builder.types_on_stack(module, &func_ty.params)
            && builder.allocs.controls[0].label_types() == &func_ty.results
    })
}

fn return_call(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let candidates = builder
        .allocs
        .functions
        .iter()
        .filter(|(func_ty, _)| {
            builder.types_on_stack(module, &func_ty.params)
                && builder.allocs.controls[0].label_types() == &func_ty.results
        })
        .flat_map(|(_, v)| v.iter().copied())
        .collect::<Vec<_>>();
    assert!(candidates.len() > 0);
    let i = u.int_in_range(0..=candidates.len() - 1)?;
    let (func_idx, ty) = module.funcs().nth(candidates[i] as usize).unwrap();
    builder.pop_operands(module, &ty.params);
    builder.push_operands(&ty.results);
    instructions.push(Instruction::ReturnCall(func_idx as u32));
    Ok(())
}

#[inline]
fn return_call_ref_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    if !module.config.gc_enabled {
        return false;
    }

    let ref_ty = match builder.concrete_funcref_on_stack(module) {
        None => return false,
        Some(r) if r.nullable && module.config.disallow_traps => return false,
        Some(r) => r,
    };

    let idx = match ref_ty.heap_type {
        HeapType::Concrete(idx) => idx,
        _ => unreachable!(),
    };
    let func_ty = match &module.ty(idx).composite_type {
        CompositeType::Func(f) => f,
        CompositeType::Array(_) | CompositeType::Struct(_) => return false,
    };

    let ty = builder.allocs.operands.pop().unwrap();
    let valid = builder.types_on_stack(module, &func_ty.params)
        && builder.func_ty.results == func_ty.results;
    builder.allocs.operands.push(ty);
    valid
}

fn return_call_ref(
    _u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let heap_ty = match builder.pop_operand() {
        Some(ValType::Ref(r)) => r.heap_type,
        _ => unreachable!(),
    };
    let idx = match heap_ty {
        HeapType::Concrete(idx) => idx,
        _ => unreachable!(),
    };
    let func_ty = match &module.ty(idx).composite_type {
        CompositeType::Func(f) => f,
        _ => unreachable!(),
    };
    builder.pop_operands(module, &func_ty.params);
    builder.push_operands(&func_ty.results);
    instructions.push(Instruction::ReturnCallRef(idx));
    Ok(())
}

#[inline]
fn return_call_indirect_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    if !module.config.tail_call_enabled
        || builder.allocs.funcref_tables.is_empty()
        || !builder.type_on_stack(module, ValType::I32)
    {
        return false;
    }

    if module.config.disallow_traps {
        // See comment in `call_indirect_valid`; same applies here.
        return false;
    }

    let ty = builder.allocs.operands.pop().unwrap();
    let is_valid = module.func_types().any(|(_, ty)| {
        builder.types_on_stack(module, &ty.params)
            && builder.allocs.controls[0].label_types() == &ty.results
    });
    builder.allocs.operands.push(ty);
    is_valid
}

fn return_call_indirect(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);

    let choices = module
        .func_types()
        .filter(|(_, ty)| {
            builder.types_on_stack(module, &ty.params)
                && builder.allocs.controls[0].label_types() == &ty.results
        })
        .collect::<Vec<_>>();
    let (type_idx, ty) = u.choose(&choices)?;
    builder.pop_operands(module, &ty.params);
    builder.push_operands(&ty.results);
    let table = *u.choose(&builder.allocs.funcref_tables)?;
    instructions.push(Instruction::ReturnCallIndirect {
        ty: *type_idx as u32,
        table,
    });
    Ok(())
}

#[inline]
fn throw_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.exceptions_enabled
        && builder
            .allocs
            .tags
            .keys()
            .any(|k| builder.types_on_stack(module, k))
}

fn throw(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let candidates = builder
        .allocs
        .tags
        .iter()
        .filter(|(k, _)| builder.types_on_stack(module, k))
        .flat_map(|(_, v)| v.iter().copied())
        .collect::<Vec<_>>();
    assert!(candidates.len() > 0);
    let i = u.int_in_range(0..=candidates.len() - 1)?;
    let (tag_idx, tag_type) = module.tags().nth(candidates[i] as usize).unwrap();
    // Tags have no results, throwing cannot return
    assert!(tag_type.func_type.results.len() == 0);
    builder.pop_operands(module, &tag_type.func_type.params);
    instructions.push(Instruction::Throw(tag_idx as u32));
    Ok(())
}

#[inline]
fn throw_ref_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.exceptions_enabled && builder.types_on_stack(module, &[ValType::EXNREF])
}

fn throw_ref(
    _u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::EXNREF]);
    instructions.push(Instruction::ThrowRef);
    Ok(())
}

#[inline]
fn br_on_null_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    if !module.config.gc_enabled {
        return false;
    }
    if builder.ref_type_on_stack().is_none() {
        return false;
    }
    let ty = builder.allocs.operands.pop().unwrap();
    let valid = br_valid(module, builder);
    builder.allocs.operands.push(ty);
    valid
}

fn br_on_null(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let heap_type = match builder.pop_ref_type() {
        Some(r) => r.heap_type,
        None => {
            if !module.types.is_empty() && u.arbitrary()? {
                HeapType::Concrete(u.int_in_range(0..=u32::try_from(module.types.len()).unwrap())?)
            } else {
                *u.choose(&[
                    HeapType::Func,
                    HeapType::Extern,
                    HeapType::Any,
                    HeapType::None,
                    HeapType::NoExtern,
                    HeapType::NoFunc,
                    HeapType::Eq,
                    HeapType::Struct,
                    HeapType::Array,
                    HeapType::I31,
                ])?
            }
        }
    };

    let n = builder
        .allocs
        .controls
        .iter()
        .filter(|l| builder.label_types_on_stack(module, l))
        .count();
    debug_assert!(n > 0);

    let i = u.int_in_range(0..=n - 1)?;
    let (target, _) = builder
        .allocs
        .controls
        .iter()
        .rev()
        .enumerate()
        .filter(|(_, l)| builder.label_types_on_stack(module, l))
        .nth(i)
        .unwrap();
    let target = u32::try_from(target).unwrap();

    builder.pop_push_label_types(module, target);
    builder.push_operands(&[ValType::Ref(RefType {
        nullable: false,
        heap_type,
    })]);

    instructions.push(Instruction::BrOnNull(target));
    Ok(())
}

fn is_valid_br_on_non_null_control(
    module: &Module,
    control: &Control,
    builder: &CodeBuilder,
) -> bool {
    let ref_ty = match control.label_types().last() {
        Some(ValType::Ref(r)) => *r,
        Some(_) | None => return false,
    };
    let nullable_ref_ty = RefType {
        nullable: true,
        ..ref_ty
    };
    builder.type_on_stack(module, ValType::Ref(nullable_ref_ty))
        && control
            .label_types()
            .iter()
            .rev()
            .enumerate()
            .skip(1)
            .all(|(idx, ty)| builder.type_on_stack_at(module, idx, *ty))
}

#[inline]
fn br_on_non_null_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.gc_enabled
        && builder
            .allocs
            .controls
            .iter()
            .any(|l| is_valid_br_on_non_null_control(module, l, builder))
}

fn br_on_non_null(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let n = builder
        .allocs
        .controls
        .iter()
        .filter(|l| is_valid_br_on_non_null_control(module, l, builder))
        .count();
    debug_assert!(n > 0);

    let i = u.int_in_range(0..=n - 1)?;
    let (target, _) = builder
        .allocs
        .controls
        .iter()
        .rev()
        .enumerate()
        .filter(|(_, l)| is_valid_br_on_non_null_control(module, l, builder))
        .nth(i)
        .unwrap();
    let target = u32::try_from(target).unwrap();

    builder.pop_push_label_types(module, target);
    builder.pop_ref_type();
    instructions.push(Instruction::BrOnNonNull(target));
    Ok(())
}

fn is_valid_br_on_cast_control(
    module: &Module,
    builder: &CodeBuilder,
    control: &Control,
    from_ref_ty: Option<RefType>,
) -> bool {
    // The last label type is a sub type of the type we are casting from...
    let to_ref_ty = match control.label_types().last() {
        Some(ValType::Ref(r)) => *r,
        _ => return false,
    };
    if let Some(from_ty) = from_ref_ty {
        if !module.ref_type_is_sub_type(to_ref_ty, from_ty) {
            return false;
        }
    }
    // ... and the rest of the label types are on the stack.
    control
        .label_types()
        .iter()
        .rev()
        .enumerate()
        .skip(1)
        .all(|(idx, ty)| builder.type_on_stack_at(module, idx, *ty))
}

#[inline]
fn br_on_cast_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    let from_ref_ty = match builder.ref_type_on_stack() {
        None => return false,
        Some(r) => r,
    };
    module.config.gc_enabled
        && builder
            .allocs
            .controls
            .iter()
            .any(|l| is_valid_br_on_cast_control(module, builder, l, from_ref_ty))
}

/// Compute the [type difference] between the two given ref types.
///
/// [type difference]: https://webassembly.github.io/gc/core/valid/conventions.html#aux-reftypediff
fn ref_type_difference(a: RefType, b: RefType) -> RefType {
    RefType {
        nullable: if b.nullable { false } else { a.nullable },
        heap_type: a.heap_type,
    }
}

fn br_on_cast(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let from_ref_type = builder.ref_type_on_stack().unwrap();

    let n = builder
        .allocs
        .controls
        .iter()
        .filter(|l| is_valid_br_on_cast_control(module, builder, l, from_ref_type))
        .count();
    debug_assert!(n > 0);

    let i = u.int_in_range(0..=n - 1)?;
    let (relative_depth, control) = builder
        .allocs
        .controls
        .iter()
        .rev()
        .enumerate()
        .filter(|(_, l)| is_valid_br_on_cast_control(module, builder, l, from_ref_type))
        .nth(i)
        .unwrap();
    let relative_depth = u32::try_from(relative_depth).unwrap();

    let num_label_types = control.label_types().len();
    let to_ref_type = match control.label_types().last() {
        Some(ValType::Ref(r)) => *r,
        _ => unreachable!(),
    };

    let to_ref_type = module.arbitrary_matching_ref_type(u, to_ref_type)?;
    let from_ref_type = from_ref_type.unwrap_or(to_ref_type);
    let from_ref_type = module.arbitrary_super_type_of_ref_type(u, from_ref_type)?;

    // Do `pop_push_label_types` but without its debug assert that the types are
    // on the stack, since we know that we have a `from_ref_type` but the label
    // requires a `to_ref_type`.
    for _ in 0..num_label_types {
        builder.pop_operand();
    }
    builder.push_label_types(relative_depth);

    // Replace the label's `to_ref_type` with the type difference.
    builder.pop_operand();
    builder.push_operands(&[ValType::Ref(ref_type_difference(
        from_ref_type,
        to_ref_type,
    ))]);

    instructions.push(Instruction::BrOnCast {
        from_ref_type,
        to_ref_type,
        relative_depth,
    });
    Ok(())
}

fn is_valid_br_on_cast_fail_control(
    module: &Module,
    builder: &CodeBuilder,
    control: &Control,
    from_ref_type: Option<RefType>,
) -> bool {
    control
        .label_types()
        .last()
        .map_or(false, |label_ty| match (label_ty, from_ref_type) {
            (ValType::Ref(label_ty), Some(from_ty)) => {
                module.ref_type_is_sub_type(from_ty, *label_ty)
            }
            (ValType::Ref(_), None) => true,
            _ => false,
        })
        && control
            .label_types()
            .iter()
            .rev()
            .enumerate()
            .skip(1)
            .all(|(idx, ty)| builder.type_on_stack_at(module, idx, *ty))
}

#[inline]
fn br_on_cast_fail_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    let from_ref_ty = match builder.ref_type_on_stack() {
        None => return false,
        Some(r) => r,
    };
    module.config.gc_enabled
        && builder
            .allocs
            .controls
            .iter()
            .any(|l| is_valid_br_on_cast_fail_control(module, builder, l, from_ref_ty))
}

fn br_on_cast_fail(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let from_ref_type = builder.ref_type_on_stack().unwrap();

    let n = builder
        .allocs
        .controls
        .iter()
        .filter(|l| is_valid_br_on_cast_fail_control(module, builder, l, from_ref_type))
        .count();
    debug_assert!(n > 0);

    let i = u.int_in_range(0..=n - 1)?;
    let (relative_depth, control) = builder
        .allocs
        .controls
        .iter()
        .rev()
        .enumerate()
        .filter(|(_, l)| is_valid_br_on_cast_fail_control(module, builder, l, from_ref_type))
        .nth(i)
        .unwrap();
    let relative_depth = u32::try_from(relative_depth).unwrap();

    let from_ref_type =
        from_ref_type.unwrap_or_else(|| match control.label_types().last().unwrap() {
            ValType::Ref(r) => *r,
            _ => unreachable!(),
        });
    let to_ref_type = module.arbitrary_matching_ref_type(u, from_ref_type)?;

    // Pop-push the label types and then replace its last reference type with
    // our `to_ref_type`.
    builder.pop_push_label_types(module, relative_depth);
    builder.pop_operand();
    builder.push_operand(Some(ValType::Ref(to_ref_type)));

    instructions.push(Instruction::BrOnCastFail {
        from_ref_type,
        to_ref_type,
        relative_depth,
    });
    Ok(())
}

#[inline]
fn drop_valid(_module: &Module, builder: &mut CodeBuilder) -> bool {
    !builder.operands().is_empty()
}

fn drop(
    u: &mut Unstructured,
    _module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let ty = builder.pop_operand();
    builder.drop_operand(u, ty, instructions)?;
    Ok(())
}

#[inline]
fn select_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    if !(builder.operands().len() >= 3 && builder.type_on_stack(module, ValType::I32)) {
        return false;
    }
    let t = builder.operands()[builder.operands().len() - 2];
    let u = builder.operands()[builder.operands().len() - 3];
    t.is_none() || u.is_none() || t == u
}

fn select(
    _: &mut Unstructured,
    _module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operand();
    let t = builder.pop_operand();
    let u = builder.pop_operand();
    let ty = t.or(u);
    builder.allocs.operands.push(ty);
    match ty {
        Some(ty @ ValType::Ref(_)) => instructions.push(Instruction::TypedSelect(ty)),
        Some(ValType::I32) | Some(ValType::I64) | Some(ValType::F32) | Some(ValType::F64)
        | Some(ValType::V128) | None => instructions.push(Instruction::Select),
    }
    Ok(())
}

#[inline]
fn local_get_valid(_module: &Module, builder: &mut CodeBuilder) -> bool {
    !builder.func_ty.params.is_empty() || !builder.locals.is_empty()
}

fn local_get(
    u: &mut Unstructured,
    _module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let num_params = builder.func_ty.params.len();
    let n = num_params + builder.locals.len();
    debug_assert!(n > 0);
    let i = u.int_in_range(0..=n - 1)?;
    builder.allocs.operands.push(Some(if i < num_params {
        builder.func_ty.params[i]
    } else {
        builder.locals[i - num_params]
    }));
    instructions.push(Instruction::LocalGet(i as u32));
    Ok(())
}

#[inline]
fn local_set_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    builder
        .func_ty
        .params
        .iter()
        .chain(builder.locals.iter())
        .any(|ty| builder.type_on_stack(module, *ty))
}

fn local_set(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let n = builder
        .func_ty
        .params
        .iter()
        .chain(builder.locals.iter())
        .filter(|ty| builder.type_on_stack(module, **ty))
        .count();
    debug_assert!(n > 0);
    let i = u.int_in_range(0..=n - 1)?;
    let (j, _) = builder
        .func_ty
        .params
        .iter()
        .chain(builder.locals.iter())
        .enumerate()
        .filter(|(_, ty)| builder.type_on_stack(module, **ty))
        .nth(i)
        .unwrap();
    builder.allocs.operands.pop();
    instructions.push(Instruction::LocalSet(j as u32));
    Ok(())
}

fn local_tee(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let n = builder
        .func_ty
        .params
        .iter()
        .chain(builder.locals.iter())
        .filter(|ty| builder.type_on_stack(module, **ty))
        .count();
    debug_assert!(n > 0);
    let i = u.int_in_range(0..=n - 1)?;
    let (j, _) = builder
        .func_ty
        .params
        .iter()
        .chain(builder.locals.iter())
        .enumerate()
        .filter(|(_, ty)| builder.type_on_stack(module, **ty))
        .nth(i)
        .unwrap();
    instructions.push(Instruction::LocalTee(j as u32));
    Ok(())
}

#[inline]
fn global_get_valid(module: &Module, _: &mut CodeBuilder) -> bool {
    module.globals.len() > 0
}

fn global_get(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    debug_assert!(module.globals.len() > 0);
    let global_idx = u.int_in_range(0..=module.globals.len() - 1)?;
    builder
        .allocs
        .operands
        .push(Some(module.globals[global_idx].val_type));
    instructions.push(Instruction::GlobalGet(global_idx as u32));
    Ok(())
}

#[inline]
fn global_set_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    builder
        .allocs
        .mutable_globals
        .iter()
        .any(|(ty, _)| builder.type_on_stack(module, *ty))
}

fn global_set(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let candidates = builder
        .allocs
        .mutable_globals
        .iter()
        .find(|(ty, _)| builder.type_on_stack(module, **ty))
        .unwrap()
        .1;
    let i = u.int_in_range(0..=candidates.len() - 1)?;
    builder.allocs.operands.pop();
    instructions.push(Instruction::GlobalSet(candidates[i]));
    Ok(())
}

#[inline]
fn have_memory(module: &Module, _: &mut CodeBuilder) -> bool {
    module.memories.len() > 0
}

#[inline]
fn have_memory_and_offset(module: &Module, builder: &mut CodeBuilder) -> bool {
    (builder.allocs.memory32.len() > 0 && builder.type_on_stack(module, ValType::I32))
        || (builder.allocs.memory64.len() > 0 && builder.type_on_stack(module, ValType::I64))
}

#[inline]
fn have_data(module: &Module, _: &mut CodeBuilder) -> bool {
    module.data.len() > 0
}

fn i32_load(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let memarg = mem_arg(u, module, builder, &[0, 1, 2])?;
    builder.allocs.operands.push(Some(ValType::I32));
    if module.config.disallow_traps {
        no_traps::load(Instruction::I32Load(memarg), module, builder, instructions);
    } else {
        instructions.push(Instruction::I32Load(memarg));
    }
    Ok(())
}

fn i64_load(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let memarg = mem_arg(u, module, builder, &[0, 1, 2, 3])?;
    builder.allocs.operands.push(Some(ValType::I64));
    if module.config.disallow_traps {
        no_traps::load(Instruction::I64Load(memarg), module, builder, instructions);
    } else {
        instructions.push(Instruction::I64Load(memarg));
    }
    Ok(())
}

fn f32_load(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let memarg = mem_arg(u, module, builder, &[0, 1, 2])?;
    builder.allocs.operands.push(Some(ValType::F32));
    if module.config.disallow_traps {
        no_traps::load(Instruction::F32Load(memarg), module, builder, instructions);
    } else {
        instructions.push(Instruction::F32Load(memarg));
    }
    Ok(())
}

fn f64_load(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let memarg = mem_arg(u, module, builder, &[0, 1, 2, 3])?;
    builder.allocs.operands.push(Some(ValType::F64));
    if module.config.disallow_traps {
        no_traps::load(Instruction::F64Load(memarg), module, builder, instructions);
    } else {
        instructions.push(Instruction::F64Load(memarg));
    }
    Ok(())
}

fn i32_load_8_s(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let memarg = mem_arg(u, module, builder, &[0])?;
    builder.allocs.operands.push(Some(ValType::I32));
    if module.config.disallow_traps {
        no_traps::load(
            Instruction::I32Load8S(memarg),
            module,
            builder,
            instructions,
        );
    } else {
        instructions.push(Instruction::I32Load8S(memarg));
    }
    Ok(())
}

fn i32_load_8_u(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let memarg = mem_arg(u, module, builder, &[0])?;
    builder.allocs.operands.push(Some(ValType::I32));
    if module.config.disallow_traps {
        no_traps::load(
            Instruction::I32Load8U(memarg),
            module,
            builder,
            instructions,
        );
    } else {
        instructions.push(Instruction::I32Load8U(memarg));
    }
    Ok(())
}

fn i32_load_16_s(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let memarg = mem_arg(u, module, builder, &[0, 1])?;
    builder.allocs.operands.push(Some(ValType::I32));
    if module.config.disallow_traps {
        no_traps::load(
            Instruction::I32Load16S(memarg),
            module,
            builder,
            instructions,
        );
    } else {
        instructions.push(Instruction::I32Load16S(memarg));
    }
    Ok(())
}

fn i32_load_16_u(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let memarg = mem_arg(u, module, builder, &[0, 1])?;
    builder.allocs.operands.push(Some(ValType::I32));
    if module.config.disallow_traps {
        no_traps::load(
            Instruction::I32Load16U(memarg),
            module,
            builder,
            instructions,
        );
    } else {
        instructions.push(Instruction::I32Load16U(memarg));
    }
    Ok(())
}

fn i64_load_8_s(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let memarg = mem_arg(u, module, builder, &[0])?;
    builder.allocs.operands.push(Some(ValType::I64));
    if module.config.disallow_traps {
        no_traps::load(
            Instruction::I64Load8S(memarg),
            module,
            builder,
            instructions,
        );
    } else {
        instructions.push(Instruction::I64Load8S(memarg));
    }
    Ok(())
}

fn i64_load_16_s(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let memarg = mem_arg(u, module, builder, &[0, 1])?;
    builder.allocs.operands.push(Some(ValType::I64));
    if module.config.disallow_traps {
        no_traps::load(
            Instruction::I64Load16S(memarg),
            module,
            builder,
            instructions,
        );
    } else {
        instructions.push(Instruction::I64Load16S(memarg));
    }
    Ok(())
}

fn i64_load_32_s(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let memarg = mem_arg(u, module, builder, &[0, 1, 2])?;
    builder.allocs.operands.push(Some(ValType::I64));
    if module.config.disallow_traps {
        no_traps::load(
            Instruction::I64Load32S(memarg),
            module,
            builder,
            instructions,
        );
    } else {
        instructions.push(Instruction::I64Load32S(memarg));
    }
    Ok(())
}

fn i64_load_8_u(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let memarg = mem_arg(u, module, builder, &[0])?;
    builder.allocs.operands.push(Some(ValType::I64));
    if module.config.disallow_traps {
        no_traps::load(
            Instruction::I64Load8U(memarg),
            module,
            builder,
            instructions,
        );
    } else {
        instructions.push(Instruction::I64Load8U(memarg));
    }
    Ok(())
}

fn i64_load_16_u(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let memarg = mem_arg(u, module, builder, &[0, 1])?;
    builder.allocs.operands.push(Some(ValType::I64));
    if module.config.disallow_traps {
        no_traps::load(
            Instruction::I64Load16U(memarg),
            module,
            builder,
            instructions,
        );
    } else {
        instructions.push(Instruction::I64Load16U(memarg));
    }
    Ok(())
}

fn i64_load_32_u(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let memarg = mem_arg(u, module, builder, &[0, 1, 2])?;
    builder.allocs.operands.push(Some(ValType::I64));
    if module.config.disallow_traps {
        no_traps::load(
            Instruction::I64Load32U(memarg),
            module,
            builder,
            instructions,
        );
    } else {
        instructions.push(Instruction::I64Load32U(memarg));
    }
    Ok(())
}

#[inline]
fn store_valid(module: &Module, builder: &mut CodeBuilder, f: impl Fn() -> ValType) -> bool {
    (builder.allocs.memory32.len() > 0 && builder.types_on_stack(module, &[ValType::I32, f()]))
        || (builder.allocs.memory64.len() > 0
            && builder.types_on_stack(module, &[ValType::I64, f()]))
}

#[inline]
fn i32_store_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    store_valid(module, builder, || ValType::I32)
}

fn i32_store(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    let memarg = mem_arg(u, module, builder, &[0, 1, 2])?;
    if module.config.disallow_traps {
        no_traps::store(Instruction::I32Store(memarg), module, builder, instructions);
    } else {
        instructions.push(Instruction::I32Store(memarg));
    }
    Ok(())
}

#[inline]
fn i64_store_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    store_valid(module, builder, || ValType::I64)
}

fn i64_store(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64]);
    let memarg = mem_arg(u, module, builder, &[0, 1, 2, 3])?;
    if module.config.disallow_traps {
        no_traps::store(Instruction::I64Store(memarg), module, builder, instructions);
    } else {
        instructions.push(Instruction::I64Store(memarg));
    }
    Ok(())
}

#[inline]
fn f32_store_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    store_valid(module, builder, || ValType::F32)
}

fn f32_store(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32]);
    let memarg = mem_arg(u, module, builder, &[0, 1, 2])?;
    if module.config.disallow_traps {
        no_traps::store(Instruction::F32Store(memarg), module, builder, instructions);
    } else {
        instructions.push(Instruction::F32Store(memarg));
    }
    Ok(())
}

#[inline]
fn f64_store_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    store_valid(module, builder, || ValType::F64)
}

fn f64_store(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64]);
    let memarg = mem_arg(u, module, builder, &[0, 1, 2, 3])?;
    if module.config.disallow_traps {
        no_traps::store(Instruction::F64Store(memarg), module, builder, instructions);
    } else {
        instructions.push(Instruction::F64Store(memarg));
    }
    Ok(())
}

fn i32_store_8(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    let memarg = mem_arg(u, module, builder, &[0])?;
    if module.config.disallow_traps {
        no_traps::store(
            Instruction::I32Store8(memarg),
            module,
            builder,
            instructions,
        );
    } else {
        instructions.push(Instruction::I32Store8(memarg));
    }
    Ok(())
}

fn i32_store_16(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    let memarg = mem_arg(u, module, builder, &[0, 1])?;
    if module.config.disallow_traps {
        no_traps::store(
            Instruction::I32Store16(memarg),
            module,
            builder,
            instructions,
        );
    } else {
        instructions.push(Instruction::I32Store16(memarg));
    }
    Ok(())
}

fn i64_store_8(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64]);
    let memarg = mem_arg(u, module, builder, &[0])?;
    if module.config.disallow_traps {
        no_traps::store(
            Instruction::I64Store8(memarg),
            module,
            builder,
            instructions,
        );
    } else {
        instructions.push(Instruction::I64Store8(memarg));
    }
    Ok(())
}

fn i64_store_16(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64]);
    let memarg = mem_arg(u, module, builder, &[0, 1])?;
    if module.config.disallow_traps {
        no_traps::store(
            Instruction::I64Store16(memarg),
            module,
            builder,
            instructions,
        );
    } else {
        instructions.push(Instruction::I64Store16(memarg));
    }
    Ok(())
}

fn i64_store_32(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64]);
    let memarg = mem_arg(u, module, builder, &[0, 1, 2])?;
    if module.config.disallow_traps {
        no_traps::store(
            Instruction::I64Store32(memarg),
            module,
            builder,
            instructions,
        );
    } else {
        instructions.push(Instruction::I64Store32(memarg));
    }
    Ok(())
}

fn memory_size(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let i = u.int_in_range(0..=module.memories.len() - 1)?;
    let ty = if module.memories[i].memory64 {
        ValType::I64
    } else {
        ValType::I32
    };
    builder.push_operands(&[ty]);
    instructions.push(Instruction::MemorySize(i as u32));
    Ok(())
}

#[inline]
fn memory_grow_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    (builder.allocs.memory32.len() > 0 && builder.type_on_stack(module, ValType::I32))
        || (builder.allocs.memory64.len() > 0 && builder.type_on_stack(module, ValType::I64))
}

fn memory_grow(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let ty = if builder.type_on_stack(module, ValType::I32) {
        ValType::I32
    } else {
        ValType::I64
    };
    let index = memory_index(u, builder, ty)?;
    builder.pop_operands(module, &[ty]);
    builder.push_operands(&[ty]);
    instructions.push(Instruction::MemoryGrow(index));
    Ok(())
}

#[inline]
fn memory_init_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.bulk_memory_enabled
        && have_data(module, builder)
        && !module.config.disallow_traps // Non-trapping memory init not yet implemented
        && (builder.allocs.memory32.len() > 0
            && builder.types_on_stack(module, &[ValType::I32, ValType::I32, ValType::I32])
            || (builder.allocs.memory64.len() > 0
                && builder.types_on_stack(module, &[ValType::I64, ValType::I32, ValType::I32])))
}

fn memory_init(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    let ty = if builder.type_on_stack(module, ValType::I32) {
        ValType::I32
    } else {
        ValType::I64
    };
    let mem = memory_index(u, builder, ty)?;
    let data_index = data_index(u, module)?;
    builder.pop_operands(module, &[ty]);
    instructions.push(Instruction::MemoryInit { mem, data_index });
    Ok(())
}

#[inline]
fn memory_fill_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.bulk_memory_enabled
        && !module.config.disallow_traps // Non-trapping memory fill generation not yet implemented
        && (builder.allocs.memory32.len() > 0
            && builder.types_on_stack(module, &[ValType::I32, ValType::I32, ValType::I32])
            || (builder.allocs.memory64.len() > 0
                && builder.types_on_stack(module, &[ValType::I64, ValType::I32, ValType::I64])))
}

fn memory_fill(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let ty = if builder.type_on_stack(module, ValType::I32) {
        ValType::I32
    } else {
        ValType::I64
    };
    let mem = memory_index(u, builder, ty)?;
    builder.pop_operands(module, &[ty, ValType::I32, ty]);
    instructions.push(Instruction::MemoryFill(mem));
    Ok(())
}

#[inline]
fn memory_copy_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    if !module.config.bulk_memory_enabled {
        return false;
    }

    // The non-trapping case for memory copy has not yet been implemented,
    // so we are excluding them for now
    if module.config.disallow_traps {
        return false;
    }

    if builder.types_on_stack(module, &[ValType::I64, ValType::I64, ValType::I64])
        && builder.allocs.memory64.len() > 0
    {
        return true;
    }
    if builder.types_on_stack(module, &[ValType::I32, ValType::I32, ValType::I32])
        && builder.allocs.memory32.len() > 0
    {
        return true;
    }
    if builder.types_on_stack(module, &[ValType::I64, ValType::I32, ValType::I32])
        && builder.allocs.memory32.len() > 0
        && builder.allocs.memory64.len() > 0
    {
        return true;
    }
    if builder.types_on_stack(module, &[ValType::I32, ValType::I64, ValType::I32])
        && builder.allocs.memory32.len() > 0
        && builder.allocs.memory64.len() > 0
    {
        return true;
    }
    false
}

fn memory_copy(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let (src_mem, dst_mem) =
        if builder.types_on_stack(module, &[ValType::I64, ValType::I64, ValType::I64]) {
            builder.pop_operands(module, &[ValType::I64, ValType::I64, ValType::I64]);
            (
                memory_index(u, builder, ValType::I64)?,
                memory_index(u, builder, ValType::I64)?,
            )
        } else if builder.types_on_stack(module, &[ValType::I32, ValType::I32, ValType::I32]) {
            builder.pop_operands(module, &[ValType::I32, ValType::I32, ValType::I32]);
            (
                memory_index(u, builder, ValType::I32)?,
                memory_index(u, builder, ValType::I32)?,
            )
        } else if builder.types_on_stack(module, &[ValType::I64, ValType::I32, ValType::I32]) {
            builder.pop_operands(module, &[ValType::I64, ValType::I32, ValType::I32]);
            (
                memory_index(u, builder, ValType::I32)?,
                memory_index(u, builder, ValType::I64)?,
            )
        } else if builder.types_on_stack(module, &[ValType::I32, ValType::I64, ValType::I32]) {
            builder.pop_operands(module, &[ValType::I32, ValType::I64, ValType::I32]);
            (
                memory_index(u, builder, ValType::I64)?,
                memory_index(u, builder, ValType::I32)?,
            )
        } else {
            unreachable!()
        };
    instructions.push(Instruction::MemoryCopy { dst_mem, src_mem });
    Ok(())
}

#[inline]
fn data_drop_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    have_data(module, builder) && module.config.bulk_memory_enabled
}

fn data_drop(
    u: &mut Unstructured,
    module: &Module,
    _builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    instructions.push(Instruction::DataDrop(data_index(u, module)?));
    Ok(())
}

fn i32_const(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.push_operands(&[ValType::I32]);
    instructions.push(module.arbitrary_const_instruction(ValType::I32, u)?);
    Ok(())
}

fn i64_const(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.push_operands(&[ValType::I64]);
    instructions.push(module.arbitrary_const_instruction(ValType::I64, u)?);
    Ok(())
}

fn f32_const(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.push_operands(&[ValType::F32]);
    instructions.push(module.arbitrary_const_instruction(ValType::F32, u)?);
    Ok(())
}

fn f64_const(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.push_operands(&[ValType::F64]);
    instructions.push(module.arbitrary_const_instruction(ValType::F64, u)?);
    Ok(())
}

#[inline]
fn i32_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    builder.type_on_stack(module, ValType::I32)
}

fn i32_eqz(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32Eqz);
    Ok(())
}

#[inline]
fn i32_i32_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    builder.types_on_stack(module, &[ValType::I32, ValType::I32])
}

fn i32_eq(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32Eq);
    Ok(())
}

fn i32_ne(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32Ne);
    Ok(())
}

fn i32_lt_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32LtS);
    Ok(())
}

fn i32_lt_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32LtU);
    Ok(())
}

fn i32_gt_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32GtS);
    Ok(())
}

fn i32_gt_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32GtU);
    Ok(())
}

fn i32_le_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32LeS);
    Ok(())
}

fn i32_le_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32LeU);
    Ok(())
}

fn i32_ge_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32GeS);
    Ok(())
}

fn i32_ge_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32GeU);
    Ok(())
}

#[inline]
fn i64_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    builder.types_on_stack(module, &[ValType::I64])
}

fn i64_eqz(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I64Eqz);
    Ok(())
}

#[inline]
fn i64_i64_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    builder.types_on_stack(module, &[ValType::I64, ValType::I64])
}

fn i64_eq(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I64Eq);
    Ok(())
}

fn i64_ne(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I64Ne);
    Ok(())
}

fn i64_lt_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I64LtS);
    Ok(())
}

fn i64_lt_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I64LtU);
    Ok(())
}

fn i64_gt_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I64GtS);
    Ok(())
}

fn i64_gt_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I64GtU);
    Ok(())
}

fn i64_le_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I64LeS);
    Ok(())
}

fn i64_le_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I64LeU);
    Ok(())
}

fn i64_ge_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I64GeS);
    Ok(())
}

fn i64_ge_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I64GeU);
    Ok(())
}

fn f32_f32_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    builder.types_on_stack(module, &[ValType::F32, ValType::F32])
}

fn f32_eq(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::F32Eq);
    Ok(())
}

fn f32_ne(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::F32Ne);
    Ok(())
}

fn f32_lt(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::F32Lt);
    Ok(())
}

fn f32_gt(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::F32Gt);
    Ok(())
}

fn f32_le(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::F32Le);
    Ok(())
}

fn f32_ge(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::F32Ge);
    Ok(())
}

fn f64_f64_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    builder.types_on_stack(module, &[ValType::F64, ValType::F64])
}

fn f64_eq(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::F64Eq);
    Ok(())
}

fn f64_ne(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::F64Ne);
    Ok(())
}

fn f64_lt(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::F64Lt);
    Ok(())
}

fn f64_gt(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::F64Gt);
    Ok(())
}

fn f64_le(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::F64Le);
    Ok(())
}

fn f64_ge(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::F64Ge);
    Ok(())
}

fn i32_clz(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32Clz);
    Ok(())
}

fn i32_ctz(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32Ctz);
    Ok(())
}

fn i32_popcnt(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32Popcnt);
    Ok(())
}

fn i32_add(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32Add);
    Ok(())
}

fn i32_sub(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32Sub);
    Ok(())
}

fn i32_mul(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32Mul);
    Ok(())
}

fn i32_div_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    if module.config.disallow_traps {
        no_traps::signed_div_rem(Instruction::I32DivS, builder, instructions);
    } else {
        instructions.push(Instruction::I32DivS);
    }
    Ok(())
}

fn i32_div_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    if module.config.disallow_traps {
        no_traps::unsigned_div_rem(Instruction::I32DivU, builder, instructions);
    } else {
        instructions.push(Instruction::I32DivU);
    }
    Ok(())
}

fn i32_rem_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    if module.config.disallow_traps {
        no_traps::signed_div_rem(Instruction::I32RemS, builder, instructions);
    } else {
        instructions.push(Instruction::I32RemS);
    }
    Ok(())
}

fn i32_rem_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    if module.config.disallow_traps {
        no_traps::unsigned_div_rem(Instruction::I32RemU, builder, instructions);
    } else {
        instructions.push(Instruction::I32RemU);
    }
    Ok(())
}

fn i32_and(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32And);
    Ok(())
}

fn i32_or(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32Or);
    Ok(())
}

fn i32_xor(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32Xor);
    Ok(())
}

fn i32_shl(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32Shl);
    Ok(())
}

fn i32_shr_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32ShrS);
    Ok(())
}

fn i32_shr_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32ShrU);
    Ok(())
}

fn i32_rotl(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32Rotl);
    Ok(())
}

fn i32_rotr(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32Rotr);
    Ok(())
}

fn i64_clz(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64Clz);
    Ok(())
}

fn i64_ctz(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64Ctz);
    Ok(())
}

fn i64_popcnt(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64Popcnt);
    Ok(())
}

fn i64_add(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64Add);
    Ok(())
}

fn i64_sub(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64Sub);
    Ok(())
}

fn i64_mul(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64Mul);
    Ok(())
}

fn i64_div_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    if module.config.disallow_traps {
        no_traps::signed_div_rem(Instruction::I64DivS, builder, instructions);
    } else {
        instructions.push(Instruction::I64DivS);
    }
    Ok(())
}

fn i64_div_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    if module.config.disallow_traps {
        no_traps::unsigned_div_rem(Instruction::I64DivU, builder, instructions);
    } else {
        instructions.push(Instruction::I64DivU);
    }
    Ok(())
}

fn i64_rem_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    if module.config.disallow_traps {
        no_traps::signed_div_rem(Instruction::I64RemS, builder, instructions);
    } else {
        instructions.push(Instruction::I64RemS);
    }
    Ok(())
}

fn i64_rem_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    if module.config.disallow_traps {
        no_traps::unsigned_div_rem(Instruction::I64RemU, builder, instructions);
    } else {
        instructions.push(Instruction::I64RemU);
    }
    Ok(())
}

fn i64_and(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64And);
    Ok(())
}

fn i64_or(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64Or);
    Ok(())
}

fn i64_xor(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64Xor);
    Ok(())
}

fn i64_shl(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64Shl);
    Ok(())
}

fn i64_shr_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64ShrS);
    Ok(())
}

fn i64_shr_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64ShrU);
    Ok(())
}

fn i64_rotl(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64Rotl);
    Ok(())
}

fn i64_rotr(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64Rotr);
    Ok(())
}

#[inline]
fn f32_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    builder.types_on_stack(module, &[ValType::F32])
}

fn f32_abs(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32Abs);
    Ok(())
}

fn f32_neg(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32Neg);
    Ok(())
}

fn f32_ceil(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32Ceil);
    Ok(())
}

fn f32_floor(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32Floor);
    Ok(())
}

fn f32_trunc(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32Trunc);
    Ok(())
}

fn f32_nearest(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32Nearest);
    Ok(())
}

fn f32_sqrt(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32Sqrt);
    Ok(())
}

fn f32_add(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32Add);
    Ok(())
}

fn f32_sub(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32Sub);
    Ok(())
}

fn f32_mul(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32Mul);
    Ok(())
}

fn f32_div(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32Div);
    Ok(())
}

fn f32_min(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32Min);
    Ok(())
}

fn f32_max(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32Max);
    Ok(())
}

fn f32_copysign(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32Copysign);
    Ok(())
}

#[inline]
fn f64_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    builder.types_on_stack(module, &[ValType::F64])
}

fn f64_abs(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64Abs);
    Ok(())
}

fn f64_neg(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64Neg);
    Ok(())
}

fn f64_ceil(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64Ceil);
    Ok(())
}

fn f64_floor(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64Floor);
    Ok(())
}

fn f64_trunc(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64Trunc);
    Ok(())
}

fn f64_nearest(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64Nearest);
    Ok(())
}

fn f64_sqrt(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64Sqrt);
    Ok(())
}

fn f64_add(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64Add);
    Ok(())
}

fn f64_sub(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64Sub);
    Ok(())
}

fn f64_mul(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64Mul);
    Ok(())
}

fn f64_div(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64Div);
    Ok(())
}

fn f64_min(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64Min);
    Ok(())
}

fn f64_max(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64Max);
    Ok(())
}

fn f64_copysign(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64Copysign);
    Ok(())
}

fn i32_wrap_i64(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32WrapI64);
    Ok(())
}

fn nontrapping_f32_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.saturating_float_to_int_enabled && f32_on_stack(module, builder)
}

fn i32_trunc_f32_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    if module.config.disallow_traps {
        no_traps::trunc(Instruction::I32TruncF32S, builder, instructions);
    } else {
        instructions.push(Instruction::I32TruncF32S);
    }
    Ok(())
}

fn i32_trunc_f32_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    if module.config.disallow_traps {
        no_traps::trunc(Instruction::I32TruncF32U, builder, instructions);
    } else {
        instructions.push(Instruction::I32TruncF32U);
    }
    Ok(())
}

fn nontrapping_f64_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.saturating_float_to_int_enabled && f64_on_stack(module, builder)
}

fn i32_trunc_f64_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    if module.config.disallow_traps {
        no_traps::trunc(Instruction::I32TruncF64S, builder, instructions);
    } else {
        instructions.push(Instruction::I32TruncF64S);
    }
    Ok(())
}

fn i32_trunc_f64_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    if module.config.disallow_traps {
        no_traps::trunc(Instruction::I32TruncF64U, builder, instructions);
    } else {
        instructions.push(Instruction::I32TruncF64U);
    }
    Ok(())
}

fn i64_extend_i32_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64ExtendI32S);
    Ok(())
}

fn i64_extend_i32_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64ExtendI32U);
    Ok(())
}

fn i64_trunc_f32_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32]);
    builder.push_operands(&[ValType::I64]);
    if module.config.disallow_traps {
        no_traps::trunc(Instruction::I64TruncF32S, builder, instructions);
    } else {
        instructions.push(Instruction::I64TruncF32S);
    }
    Ok(())
}

fn i64_trunc_f32_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32]);
    builder.push_operands(&[ValType::I64]);
    if module.config.disallow_traps {
        no_traps::trunc(Instruction::I64TruncF32U, builder, instructions);
    } else {
        instructions.push(Instruction::I64TruncF32U);
    }
    Ok(())
}

fn i64_trunc_f64_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64]);
    builder.push_operands(&[ValType::I64]);
    if module.config.disallow_traps {
        no_traps::trunc(Instruction::I64TruncF64S, builder, instructions);
    } else {
        instructions.push(Instruction::I64TruncF64S);
    }
    Ok(())
}

fn i64_trunc_f64_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64]);
    builder.push_operands(&[ValType::I64]);
    if module.config.disallow_traps {
        no_traps::trunc(Instruction::I64TruncF64U, builder, instructions);
    } else {
        instructions.push(Instruction::I64TruncF64U);
    }
    Ok(())
}

fn f32_convert_i32_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32ConvertI32S);
    Ok(())
}

fn f32_convert_i32_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32ConvertI32U);
    Ok(())
}

fn f32_convert_i64_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32ConvertI64S);
    Ok(())
}

fn f32_convert_i64_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32ConvertI64U);
    Ok(())
}

fn f32_demote_f64(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32DemoteF64);
    Ok(())
}

fn f64_convert_i32_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64ConvertI32S);
    Ok(())
}

fn f64_convert_i32_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64ConvertI32U);
    Ok(())
}

fn f64_convert_i64_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64ConvertI64S);
    Ok(())
}

fn f64_convert_i64_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64ConvertI64U);
    Ok(())
}

fn f64_promote_f32(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64PromoteF32);
    Ok(())
}

fn i32_reinterpret_f32(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32ReinterpretF32);
    Ok(())
}

fn i64_reinterpret_f64(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64ReinterpretF64);
    Ok(())
}

fn f32_reinterpret_i32(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    builder.push_operands(&[ValType::F32]);
    instructions.push(Instruction::F32ReinterpretI32);
    Ok(())
}

fn f64_reinterpret_i64(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64]);
    builder.push_operands(&[ValType::F64]);
    instructions.push(Instruction::F64ReinterpretI64);
    Ok(())
}

fn extendable_i32_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.sign_extension_ops_enabled && i32_on_stack(module, builder)
}

fn i32_extend_8_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32Extend8S);
    Ok(())
}

fn i32_extend_16_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32Extend16S);
    Ok(())
}

fn extendable_i64_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.sign_extension_ops_enabled && i64_on_stack(module, builder)
}

fn i64_extend_8_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64Extend8S);
    Ok(())
}

fn i64_extend_16_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64Extend16S);
    Ok(())
}

fn i64_extend_32_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64Extend32S);
    Ok(())
}

fn i32_trunc_sat_f32_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32TruncSatF32S);
    Ok(())
}

fn i32_trunc_sat_f32_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32TruncSatF32U);
    Ok(())
}

fn i32_trunc_sat_f64_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32TruncSatF64S);
    Ok(())
}

fn i32_trunc_sat_f64_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::I32TruncSatF64U);
    Ok(())
}

fn i64_trunc_sat_f32_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64TruncSatF32S);
    Ok(())
}

fn i64_trunc_sat_f32_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F32]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64TruncSatF32U);
    Ok(())
}

fn i64_trunc_sat_f64_s(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64TruncSatF64S);
    Ok(())
}

fn i64_trunc_sat_f64_u(
    _: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::F64]);
    builder.push_operands(&[ValType::I64]);
    instructions.push(Instruction::I64TruncSatF64U);
    Ok(())
}

fn memory_offset(u: &mut Unstructured, module: &Module, memory_index: u32) -> Result<u64> {
    let MemoryOffsetChoices(a, b, c) = module.config.memory_offset_choices;
    assert!(a + b + c != 0);

    let memory_type = &module.memories[memory_index as usize];
    let min = memory_type
        .minimum
        .saturating_mul(crate::page_size(memory_type).into());
    let max = memory_type
        .maximum
        .map(|max| max.saturating_mul(crate::page_size(memory_type).into()))
        .unwrap_or(u64::MAX);

    let (min, max, true_max) = match (memory_type.memory64, module.config.disallow_traps) {
        (true, false) => {
            // 64-bit memories can use the limits calculated above as-is
            (min, max, u64::MAX)
        }
        (false, false) => {
            // 32-bit memories can't represent a full 4gb offset, so if that's the
            // min/max sizes then we need to switch the m to `u32::MAX`.
            (
                u64::from(u32::try_from(min).unwrap_or(u32::MAX)),
                u64::from(u32::try_from(max).unwrap_or(u32::MAX)),
                u64::from(u32::MAX),
            )
        }
        // The logic for non-trapping versions of load/store involves pushing
        // the offset + load/store size onto the stack as either an i32 or i64
        // value. So even though offsets can normally be as high as u32 or u64,
        // we need to limit them to lower in order for our non-trapping logic to
        // work. 16 is the number of bytes of the largest load type (V128).
        (true, true) => {
            let no_trap_max = (i64::MAX - 16) as u64;
            (min.min(no_trap_max), no_trap_max, no_trap_max)
        }
        (false, true) => {
            let no_trap_max = (i32::MAX - 16) as u64;
            (min.min(no_trap_max), no_trap_max, no_trap_max)
        }
    };

    let choice = u.int_in_range(0..=a + b + c - 1)?;
    if choice < a {
        u.int_in_range(0..=min)
    } else if choice < a + b {
        u.int_in_range(min..=max)
    } else {
        u.int_in_range(max..=true_max)
    }
}

fn mem_arg(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    alignments: &[u32],
) -> Result<MemArg> {
    let memory_index = if builder.type_on_stack(module, ValType::I32) {
        builder.pop_operands(module, &[ValType::I32]);
        memory_index(u, builder, ValType::I32)?
    } else {
        builder.pop_operands(module, &[ValType::I64]);
        memory_index(u, builder, ValType::I64)?
    };
    let offset = memory_offset(u, module, memory_index)?;
    let align = *u.choose(alignments)?;
    Ok(MemArg {
        memory_index,
        offset,
        align,
    })
}

fn memory_index(u: &mut Unstructured, builder: &CodeBuilder, ty: ValType) -> Result<u32> {
    if ty == ValType::I32 {
        Ok(*u.choose(&builder.allocs.memory32)?)
    } else {
        Ok(*u.choose(&builder.allocs.memory64)?)
    }
}

fn data_index(u: &mut Unstructured, module: &Module) -> Result<u32> {
    let data = module.data.len() as u32;
    assert!(data > 0);
    if data == 1 {
        Ok(0)
    } else {
        u.int_in_range(0..=data - 1)
    }
}

#[inline]
fn ref_null_valid(module: &Module, _: &mut CodeBuilder) -> bool {
    module.config.reference_types_enabled
}

fn ref_null(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let mut choices = vec![RefType::EXTERNREF, RefType::FUNCREF];
    if module.config.exceptions_enabled {
        choices.push(RefType::EXNREF);
    }
    if module.config.gc_enabled {
        let r = |heap_type| RefType {
            nullable: true,
            heap_type,
        };
        choices.push(r(HeapType::Any));
        choices.push(r(HeapType::Eq));
        choices.push(r(HeapType::Array));
        choices.push(r(HeapType::Struct));
        choices.push(r(HeapType::I31));
        choices.push(r(HeapType::None));
        choices.push(r(HeapType::NoFunc));
        choices.push(r(HeapType::NoExtern));
        for i in 0..module.types.len() {
            let i = u32::try_from(i).unwrap();
            choices.push(r(HeapType::Concrete(i)));
        }
    }
    let ty = *u.choose(&choices)?;
    builder.push_operand(Some(ty.into()));
    instructions.push(Instruction::RefNull(ty.heap_type));
    Ok(())
}

#[inline]
fn ref_func_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.reference_types_enabled && builder.allocs.referenced_functions.len() > 0
}

fn ref_func(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let i = *u.choose(&builder.allocs.referenced_functions)?;
    let ty = module.funcs[usize::try_from(i).unwrap()].0;
    builder.push_operand(Some(ValType::Ref(if module.config.gc_enabled {
        RefType {
            nullable: false,
            heap_type: HeapType::Concrete(ty),
        }
    } else {
        RefType::FUNCREF
    })));
    instructions.push(Instruction::RefFunc(i));
    Ok(())
}

#[inline]
fn ref_as_non_null_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.gc_enabled && builder.ref_type_on_stack().is_some()
}

fn ref_as_non_null(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let ref_ty = match builder.pop_ref_type() {
        Some(r) => r,
        None => module.arbitrary_ref_type(u)?,
    };
    builder.push_operand(Some(ValType::Ref(RefType {
        nullable: false,
        heap_type: ref_ty.heap_type,
    })));
    instructions.push(Instruction::RefAsNonNull);
    Ok(())
}

#[inline]
fn ref_eq_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    let eq_ref = ValType::Ref(RefType {
        nullable: true,
        heap_type: HeapType::Eq,
    });
    module.config.gc_enabled && builder.types_on_stack(module, &[eq_ref, eq_ref])
}

fn ref_eq(
    _u: &mut Unstructured,
    _module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operand();
    builder.pop_operand();
    builder.push_operand(Some(ValType::I32));
    instructions.push(Instruction::RefEq);
    Ok(())
}

#[inline]
fn ref_test_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.gc_enabled && builder.ref_type_on_stack().is_some()
}

fn ref_test(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let ref_ty = match builder.pop_ref_type() {
        Some(r) => r,
        None => module.arbitrary_ref_type(u)?,
    };
    builder.push_operand(Some(ValType::I32));

    let sub_ty = module.arbitrary_matching_heap_type(u, ref_ty.heap_type)?;
    instructions.push(if !ref_ty.nullable || u.arbitrary()? {
        Instruction::RefTestNonNull(sub_ty)
    } else {
        Instruction::RefTestNullable(sub_ty)
    });
    Ok(())
}

#[inline]
fn ref_cast_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    !module.config.disallow_traps
        && module.config.gc_enabled
        && builder.ref_type_on_stack().is_some()
}

fn ref_cast(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let ref_ty = match builder.pop_ref_type() {
        Some(r) => r,
        None => module.arbitrary_ref_type(u)?,
    };
    let sub_ty = RefType {
        nullable: if !ref_ty.nullable {
            false
        } else {
            u.arbitrary()?
        },
        heap_type: module.arbitrary_matching_heap_type(u, ref_ty.heap_type)?,
    };
    builder.push_operand(Some(ValType::Ref(sub_ty)));

    instructions.push(if !sub_ty.nullable {
        Instruction::RefCastNonNull(sub_ty.heap_type)
    } else {
        Instruction::RefCastNullable(sub_ty.heap_type)
    });
    Ok(())
}

#[inline]
fn ref_is_null_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.reference_types_enabled
        && (builder.type_on_stack(module, ValType::EXTERNREF)
            || builder.type_on_stack(module, ValType::FUNCREF))
}

fn ref_is_null(
    _: &mut Unstructured,
    _module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_ref_type();
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::RefIsNull);
    Ok(())
}

#[inline]
fn table_fill_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.reference_types_enabled
        && module.config.bulk_memory_enabled
        && !module.config.disallow_traps // Non-trapping table fill generation not yet implemented
        && [ValType::EXTERNREF, ValType::FUNCREF].iter().any(|ty| {
            builder.types_on_stack(module, &[ValType::I32, *ty, ValType::I32])
                && module.tables.iter().any(|t| {
                    module.val_type_is_sub_type(*ty, t.element_type.into())
                })
        })
}

fn table_fill(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    let table = match builder.pop_ref_type() {
        Some(ty) => table_index(ty, u, module)?,
        // Stack polymorphic, can choose any reference type we have a table for,
        // so just choose the table directly.
        None => u.int_in_range(0..=u32::try_from(module.tables.len()).unwrap())?,
    };
    builder.pop_operands(module, &[ValType::I32]);
    instructions.push(Instruction::TableFill(table));
    Ok(())
}

#[inline]
fn table_set_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.reference_types_enabled
    && !module.config.disallow_traps // Non-trapping table.set generation not yet implemented
        && [ValType::EXTERNREF, ValType::FUNCREF].iter().any(|ty| {
            builder.types_on_stack(module, &[ValType::I32, *ty])
                && module.tables.iter().any(|t| {
                    module.val_type_is_sub_type(*ty, t.element_type.into())
                })
        })
}

fn table_set(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let table = match builder.pop_ref_type() {
        Some(ty) => table_index(ty, u, module)?,
        // Stack polymorphic, can choose any reference type we have a table for,
        // so just choose the table directly.
        None => u.int_in_range(0..=u32::try_from(module.tables.len()).unwrap())?,
    };
    builder.pop_operands(module, &[ValType::I32]);
    instructions.push(Instruction::TableSet(table));
    Ok(())
}

#[inline]
fn table_get_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.reference_types_enabled
    && !module.config.disallow_traps // Non-trapping table.get generation not yet implemented
        && builder.type_on_stack(module, ValType::I32)
        && module.tables.len() > 0
}

fn table_get(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    let idx = u.int_in_range(0..=module.tables.len() - 1)?;
    let ty = module.tables[idx].element_type;
    builder.push_operands(&[ty.into()]);
    instructions.push(Instruction::TableGet(idx as u32));
    Ok(())
}

#[inline]
fn table_size_valid(module: &Module, _: &mut CodeBuilder) -> bool {
    module.config.reference_types_enabled && module.tables.len() > 0
}

fn table_size(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let table = u.int_in_range(0..=module.tables.len() - 1)? as u32;
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::TableSize(table));
    Ok(())
}

#[inline]
fn table_grow_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.reference_types_enabled
        && [ValType::EXTERNREF, ValType::FUNCREF].iter().any(|ty| {
            builder.types_on_stack(module, &[*ty, ValType::I32])
                && module
                    .tables
                    .iter()
                    .any(|t| module.val_type_is_sub_type(*ty, t.element_type.into()))
        })
}

fn table_grow(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32]);
    let table = match builder.pop_ref_type() {
        Some(ty) => table_index(ty, u, module)?,
        // Stack polymorphic, can choose any reference type we have a table for,
        // so just choose the table directly.
        None => u.int_in_range(0..=u32::try_from(module.tables.len()).unwrap())?,
    };
    builder.push_operands(&[ValType::I32]);
    instructions.push(Instruction::TableGrow(table));
    Ok(())
}

#[inline]
fn table_copy_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.bulk_memory_enabled
    && !module.config.disallow_traps // Non-trapping table.copy generation not yet implemented
        && module.tables.len() > 0
        && builder.types_on_stack(module, &[ValType::I32, ValType::I32, ValType::I32])
}

fn table_copy(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32, ValType::I32]);
    let src_table = u.int_in_range(0..=module.tables.len() - 1)? as u32;
    let dst_table = table_index(module.tables[src_table as usize].element_type, u, module)?;
    instructions.push(Instruction::TableCopy {
        src_table,
        dst_table,
    });
    Ok(())
}

#[inline]
fn table_init_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.bulk_memory_enabled
    && !module.config.disallow_traps // Non-trapping table.init generation not yet implemented.
        && builder.allocs.table_init_possible
        && builder.types_on_stack(module, &[ValType::I32, ValType::I32, ValType::I32])
}

fn table_init(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::I32, ValType::I32, ValType::I32]);
    let segments = module
        .elems
        .iter()
        .enumerate()
        .filter(|(_, e)| module.tables.iter().any(|t| t.element_type == e.ty))
        .map(|(i, _)| i)
        .collect::<Vec<_>>();
    let segment = *u.choose(&segments)?;
    let table = table_index(module.elems[segment].ty, u, module)?;
    instructions.push(Instruction::TableInit {
        elem_index: segment as u32,
        table,
    });
    Ok(())
}

#[inline]
fn elem_drop_valid(module: &Module, _builder: &mut CodeBuilder) -> bool {
    module.config.bulk_memory_enabled && module.elems.len() > 0
}

fn elem_drop(
    u: &mut Unstructured,
    module: &Module,
    _builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let segment = u.int_in_range(0..=module.elems.len() - 1)? as u32;
    instructions.push(Instruction::ElemDrop(segment));
    Ok(())
}

#[inline]
fn struct_new_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.gc_enabled
        && module
            .struct_types
            .iter()
            .copied()
            .any(|i| builder.field_types_on_stack(module, &module.ty(i).unwrap_struct().fields))
}

fn struct_new(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let n = module
        .struct_types
        .iter()
        .filter(|i| builder.field_types_on_stack(module, &module.ty(**i).unwrap_struct().fields))
        .count();
    debug_assert!(n > 0);
    let i = u.int_in_range(0..=n - 1)?;
    let ty = module
        .struct_types
        .iter()
        .copied()
        .filter(|i| builder.field_types_on_stack(module, &module.ty(*i).unwrap_struct().fields))
        .nth(i)
        .unwrap();

    for _ in module.ty(ty).unwrap_struct().fields.iter() {
        builder.pop_operand();
    }
    builder.push_operand(Some(ValType::Ref(RefType {
        nullable: false,
        heap_type: HeapType::Concrete(ty),
    })));

    instructions.push(Instruction::StructNew(ty));
    Ok(())
}

#[inline]
fn struct_new_default_valid(module: &Module, _builder: &mut CodeBuilder) -> bool {
    module.config.gc_enabled
        && module.struct_types.iter().copied().any(|i| {
            module
                .ty(i)
                .unwrap_struct()
                .fields
                .iter()
                .all(|f| f.element_type.is_defaultable())
        })
}

fn struct_new_default(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let n = module
        .struct_types
        .iter()
        .filter(|i| {
            module
                .ty(**i)
                .unwrap_struct()
                .fields
                .iter()
                .all(|f| f.element_type.is_defaultable())
        })
        .count();
    debug_assert!(n > 0);
    let i = u.int_in_range(0..=n - 1)?;
    let ty = module
        .struct_types
        .iter()
        .copied()
        .filter(|i| {
            module
                .ty(*i)
                .unwrap_struct()
                .fields
                .iter()
                .all(|f| f.element_type.is_defaultable())
        })
        .nth(i)
        .unwrap();

    builder.push_operand(Some(ValType::Ref(RefType {
        nullable: false,
        heap_type: HeapType::Concrete(ty),
    })));

    instructions.push(Instruction::StructNewDefault(ty));
    Ok(())
}

#[inline]
fn struct_get_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.gc_enabled
        && !module.config.disallow_traps
        && builder.non_empty_struct_ref_on_stack(module, !module.config.disallow_traps)
}

fn struct_get(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let (_, struct_type_index) = builder.pop_concrete_ref_type();
    let struct_ty = module.ty(struct_type_index).unwrap_struct();
    let num_fields = u32::try_from(struct_ty.fields.len()).unwrap();
    debug_assert!(num_fields > 0);
    let field_index = u.int_in_range(0..=num_fields - 1)?;
    let (val_ty, ext) = match struct_ty.fields[usize::try_from(field_index).unwrap()].element_type {
        StorageType::I8 | StorageType::I16 => (ValType::I32, Some(u.arbitrary()?)),
        StorageType::Val(v) => (v, None),
    };
    builder.push_operand(Some(val_ty));
    instructions.push(match ext {
        None => Instruction::StructGet {
            struct_type_index,
            field_index,
        },
        Some(true) => Instruction::StructGetS {
            struct_type_index,
            field_index,
        },
        Some(false) => Instruction::StructGetU {
            struct_type_index,
            field_index,
        },
    });
    Ok(())
}

#[inline]
fn struct_set_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    if !module.config.gc_enabled {
        return false;
    }
    match builder.concrete_struct_ref_type_on_stack_at(module, 1) {
        None => return false,
        Some((true, _, _)) if module.config.disallow_traps => return false,
        Some((_, _, ty)) => ty
            .fields
            .iter()
            .any(|f| f.mutable && builder.type_on_stack(module, f.element_type.unpack())),
    }
}

fn struct_set(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let val_ty = builder.pop_operand();
    let (_, struct_type_index) = builder.pop_concrete_ref_type();
    let struct_ty = module.ty(struct_type_index).unwrap_struct();

    let valid_field = |f: &FieldType| -> bool {
        match val_ty {
            None => f.mutable,
            Some(val_ty) => {
                f.mutable && module.val_type_is_sub_type(val_ty, f.element_type.unpack())
            }
        }
    };

    let n = struct_ty.fields.iter().filter(|f| valid_field(f)).count();
    debug_assert!(n > 0);
    let i = u.int_in_range(0..=n - 1)?;
    let (field_index, _) = struct_ty
        .fields
        .iter()
        .enumerate()
        .filter(|(_, f)| valid_field(f))
        .nth(i)
        .unwrap();
    let field_index = u32::try_from(field_index).unwrap();

    instructions.push(Instruction::StructSet {
        struct_type_index,
        field_index,
    });
    Ok(())
}

#[inline]
fn array_new_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.gc_enabled
        && builder.type_on_stack(module, ValType::I32)
        && module
            .array_types
            .iter()
            .any(|i| builder.field_type_on_stack_at(module, 1, module.ty(*i).unwrap_array().0))
}

fn array_new(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let n = module
        .array_types
        .iter()
        .filter(|i| builder.field_type_on_stack_at(module, 1, module.ty(**i).unwrap_array().0))
        .count();
    debug_assert!(n > 0);
    let i = u.int_in_range(0..=n - 1)?;
    let ty = module
        .array_types
        .iter()
        .copied()
        .filter(|i| builder.field_type_on_stack_at(module, 1, module.ty(*i).unwrap_array().0))
        .nth(i)
        .unwrap();

    builder.pop_operand();
    builder.pop_operand();
    builder.push_operand(Some(ValType::Ref(RefType {
        nullable: false,
        heap_type: HeapType::Concrete(ty),
    })));

    instructions.push(Instruction::ArrayNew(ty));
    Ok(())
}

#[inline]
fn array_new_fixed_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.gc_enabled
        && module
            .array_types
            .iter()
            .any(|i| builder.field_type_on_stack(module, module.ty(*i).unwrap_array().0))
}

fn array_new_fixed(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let n = module
        .array_types
        .iter()
        .filter(|i| builder.field_type_on_stack(module, module.ty(**i).unwrap_array().0))
        .count();
    debug_assert!(n > 0);
    let i = u.int_in_range(0..=n - 1)?;
    let array_type_index = module
        .array_types
        .iter()
        .copied()
        .filter(|i| builder.field_type_on_stack(module, module.ty(*i).unwrap_array().0))
        .nth(i)
        .unwrap();
    let elem_ty = module
        .ty(array_type_index)
        .unwrap_array()
        .0
        .element_type
        .unpack();

    let m = (0..builder.operands().len())
        .take_while(|i| builder.type_on_stack_at(module, *i, elem_ty))
        .count();
    debug_assert!(m > 0);
    let array_size = u.int_in_range(0..=m - 1)?;
    let array_size = u32::try_from(array_size).unwrap();

    for _ in 0..array_size {
        builder.pop_operand();
    }
    builder.push_operand(Some(ValType::Ref(RefType {
        nullable: false,
        heap_type: HeapType::Concrete(array_type_index),
    })));

    instructions.push(Instruction::ArrayNewFixed {
        array_type_index,
        array_size,
    });
    Ok(())
}

#[inline]
fn array_new_default_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.gc_enabled
        && builder.type_on_stack(module, ValType::I32)
        && module
            .array_types
            .iter()
            .any(|i| module.ty(*i).unwrap_array().0.element_type.is_defaultable())
}

fn array_new_default(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let n = module
        .array_types
        .iter()
        .filter(|i| {
            module
                .ty(**i)
                .unwrap_array()
                .0
                .element_type
                .is_defaultable()
        })
        .count();
    debug_assert!(n > 0);
    let i = u.int_in_range(0..=n - 1)?;
    let array_type_index = module
        .array_types
        .iter()
        .copied()
        .filter(|i| module.ty(*i).unwrap_array().0.element_type.is_defaultable())
        .nth(i)
        .unwrap();

    builder.pop_operand();
    builder.push_operand(Some(ValType::Ref(RefType {
        nullable: false,
        heap_type: HeapType::Concrete(array_type_index),
    })));
    instructions.push(Instruction::ArrayNewDefault(array_type_index));
    Ok(())
}

#[inline]
fn array_new_data_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.gc_enabled
        && module.config.bulk_memory_enabled // Requires data count section
        && !module.config.disallow_traps
        && !module.data.is_empty()
        && builder.types_on_stack(module, &[ValType::I32, ValType::I32])
        && module.array_types.iter().any(|i| {
            let ty = module.ty(*i).unwrap_array().0.element_type.unpack();
            ty.is_numeric() | ty.is_vector()
        })
}

fn array_new_data(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let n = module
        .array_types
        .iter()
        .filter(|i| {
            let ty = module.ty(**i).unwrap_array().0.element_type.unpack();
            ty.is_numeric() | ty.is_vector()
        })
        .count();
    debug_assert!(n > 0);
    let i = u.int_in_range(0..=n - 1)?;
    let array_type_index = module
        .array_types
        .iter()
        .copied()
        .filter(|i| {
            let ty = module.ty(*i).unwrap_array().0.element_type.unpack();
            ty.is_numeric() | ty.is_vector()
        })
        .nth(i)
        .unwrap();

    let m = module.data.len();
    debug_assert!(m > 0);
    let array_data_index = u.int_in_range(0..=m - 1)?;
    let array_data_index = u32::try_from(array_data_index).unwrap();

    builder.pop_operand();
    builder.pop_operand();
    builder.push_operand(Some(ValType::Ref(RefType {
        nullable: false,
        heap_type: HeapType::Concrete(array_type_index),
    })));
    instructions.push(Instruction::ArrayNewData {
        array_type_index,
        array_data_index,
    });
    Ok(())
}

fn module_has_elem_segment_of_array_type(module: &Module, ty: &ArrayType) -> bool {
    module
        .elems
        .iter()
        .any(|elem| module.val_type_is_sub_type(ValType::Ref(elem.ty), ty.0.element_type.unpack()))
}

#[inline]
fn array_new_elem_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.gc_enabled
        && !module.config.disallow_traps
        && builder.types_on_stack(module, &[ValType::I32, ValType::I32])
        && module
            .array_types
            .iter()
            .any(|i| module_has_elem_segment_of_array_type(module, module.ty(*i).unwrap_array()))
}

fn array_new_elem(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let n = module
        .array_types
        .iter()
        .filter(|i| module_has_elem_segment_of_array_type(module, module.ty(**i).unwrap_array()))
        .count();
    debug_assert!(n > 0);
    let i = u.int_in_range(0..=n - 1)?;
    let array_type_index = module
        .array_types
        .iter()
        .copied()
        .filter(|i| module_has_elem_segment_of_array_type(module, module.ty(*i).unwrap_array()))
        .nth(i)
        .unwrap();
    let elem_ty = module
        .ty(array_type_index)
        .unwrap_array()
        .0
        .element_type
        .unpack();

    let m = module
        .elems
        .iter()
        .filter(|elem| module.val_type_is_sub_type(ValType::Ref(elem.ty), elem_ty))
        .count();
    debug_assert!(m > 0);
    let j = u.int_in_range(0..=m - 1)?;
    let (array_elem_index, _) = module
        .elems
        .iter()
        .enumerate()
        .filter(|(_, elem)| module.val_type_is_sub_type(ValType::Ref(elem.ty), elem_ty))
        .nth(j)
        .unwrap();
    let array_elem_index = u32::try_from(array_elem_index).unwrap();

    builder.pop_operand();
    builder.pop_operand();
    builder.push_operand(Some(ValType::Ref(RefType {
        nullable: false,
        heap_type: HeapType::Concrete(array_type_index),
    })));

    instructions.push(Instruction::ArrayNewElem {
        array_type_index,
        array_elem_index,
    });
    Ok(())
}

#[inline]
fn array_get_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.gc_enabled
        && !module.config.disallow_traps // TODO: add support for disallowing traps
        && builder.type_on_stack(module, ValType::I32)
        && builder.concrete_array_ref_type_on_stack_at(module, 1).is_some()
}

fn array_get(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operand();
    let (_, array_type_index) = builder.pop_concrete_ref_type();
    let elem_ty = module.ty(array_type_index).unwrap_array().0.element_type;
    builder.push_operand(Some(elem_ty.unpack()));
    instructions.push(match elem_ty {
        StorageType::I8 | StorageType::I16 => {
            if u.arbitrary()? {
                Instruction::ArrayGetS(array_type_index)
            } else {
                Instruction::ArrayGetU(array_type_index)
            }
        }
        StorageType::Val(_) => Instruction::ArrayGet(array_type_index),
    });
    Ok(())
}

#[inline]
fn array_set_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    if !module.config.gc_enabled
        // TODO: implement disallowing traps.
        || module.config.disallow_traps
        || !builder.type_on_stack_at(module, 1, ValType::I32)
    {
        return false;
    }
    match builder.concrete_array_ref_type_on_stack_at(module, 2) {
        None => false,
        Some((_nullable, _idx, array_ty)) => {
            array_ty.0.mutable && builder.field_type_on_stack(module, array_ty.0)
        }
    }
}

fn array_set(
    _u: &mut Unstructured,
    _module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operand();
    builder.pop_operand();
    let (_, ty) = builder.pop_concrete_ref_type();
    instructions.push(Instruction::ArraySet(ty));
    Ok(())
}

#[inline]
fn array_len_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.gc_enabled
        && builder.type_on_stack(
            module,
            ValType::Ref(RefType {
                nullable: true,
                heap_type: HeapType::Array,
            }),
        )
}

fn array_len(
    _u: &mut Unstructured,
    _module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operand();
    builder.push_operand(Some(ValType::I32));
    instructions.push(Instruction::ArrayLen);
    Ok(())
}

#[inline]
fn array_fill_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    if !module.config.gc_enabled
        // TODO: add support for disallowing traps
        || module.config.disallow_traps
        || !builder.type_on_stack_at(module, 0, ValType::I32)
        || !builder.type_on_stack_at(module, 2, ValType::I32)
    {
        return false;
    }
    match builder.concrete_array_ref_type_on_stack_at(module, 3) {
        None => return false,
        Some((_, _, array_ty)) => {
            array_ty.0.mutable && builder.field_type_on_stack_at(module, 1, array_ty.0)
        }
    }
}

fn array_fill(
    _u: &mut Unstructured,
    _module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operand();
    builder.pop_operand();
    builder.pop_operand();
    let (_, ty) = builder.pop_concrete_ref_type();
    instructions.push(Instruction::ArrayFill(ty));
    Ok(())
}

#[inline]
fn array_copy_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    if !module.config.gc_enabled
        // TODO: add support for disallowing traps
        || module.config.disallow_traps
        || !builder.type_on_stack_at(module, 0, ValType::I32)
        || !builder.type_on_stack_at(module, 1, ValType::I32)
        || !builder.type_on_stack_at(module, 3, ValType::I32)
    {
        return false;
    }
    let x = match builder.concrete_array_ref_type_on_stack_at(module, 4) {
        None => return false,
        Some((_, _, x)) => x,
    };
    if !x.0.mutable {
        return false;
    }
    let y = match builder.concrete_array_ref_type_on_stack_at(module, 2) {
        None => return false,
        Some((_, _, y)) => y,
    };
    match (x.0.element_type, y.0.element_type) {
        (StorageType::I8, StorageType::I8) => true,
        (StorageType::I8, _) => false,
        (StorageType::I16, StorageType::I16) => true,
        (StorageType::I16, _) => false,
        (StorageType::Val(x), StorageType::Val(y)) => module.val_type_is_sub_type(y, x),
        (StorageType::Val(_), _) => false,
    }
}

fn array_copy(
    _u: &mut Unstructured,
    _module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operand();
    builder.pop_operand();
    let (_, array_type_index_src) = builder.pop_concrete_ref_type();
    builder.pop_operand();
    let (_, array_type_index_dst) = builder.pop_concrete_ref_type();
    instructions.push(Instruction::ArrayCopy {
        array_type_index_dst,
        array_type_index_src,
    });
    Ok(())
}

#[inline]
fn array_init_data_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    if !module.config.gc_enabled
        || !module.config.bulk_memory_enabled // Requires data count section
        || module.config.disallow_traps
        || module.data.is_empty()
        || !builder.types_on_stack(module, &[ValType::I32, ValType::I32, ValType::I32])
    {
        return false;
    }
    match builder.concrete_array_ref_type_on_stack_at(module, 3) {
        None => return false,
        Some((_, _, ty)) => {
            let elem_ty = ty.0.element_type.unpack();
            ty.0.mutable && (elem_ty.is_numeric() || elem_ty.is_vector())
        }
    }
}

fn array_init_data(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operand();
    builder.pop_operand();
    builder.pop_operand();
    let (_, array_type_index) = builder.pop_concrete_ref_type();

    let n = module.data.len();
    debug_assert!(n > 0);
    let array_data_index = u.int_in_range(0..=n - 1)?;
    let array_data_index = u32::try_from(array_data_index).unwrap();

    instructions.push(Instruction::ArrayInitData {
        array_type_index,
        array_data_index,
    });
    Ok(())
}

#[inline]
fn array_init_elem_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    if !module.config.gc_enabled
        || module.config.disallow_traps
        || !builder.types_on_stack(module, &[ValType::I32, ValType::I32, ValType::I32])
    {
        return false;
    }
    match builder.concrete_array_ref_type_on_stack_at(module, 3) {
        None => return false,
        Some((_, _, array_ty)) => {
            array_ty.0.mutable && module_has_elem_segment_of_array_type(module, &array_ty)
        }
    }
}

fn array_init_elem(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operand();
    builder.pop_operand();
    builder.pop_operand();
    let (_, array_type_index) = builder.pop_concrete_ref_type();

    let elem_ty = module
        .ty(array_type_index)
        .unwrap_array()
        .0
        .element_type
        .unpack();

    let n = module
        .elems
        .iter()
        .filter(|elem| module.val_type_is_sub_type(ValType::Ref(elem.ty), elem_ty))
        .count();
    debug_assert!(n > 0);
    let j = u.int_in_range(0..=n - 1)?;
    let (array_elem_index, _) = module
        .elems
        .iter()
        .enumerate()
        .filter(|(_, elem)| module.val_type_is_sub_type(ValType::Ref(elem.ty), elem_ty))
        .nth(j)
        .unwrap();
    let array_elem_index = u32::try_from(array_elem_index).unwrap();

    instructions.push(Instruction::ArrayInitElem {
        array_type_index,
        array_elem_index,
    });
    Ok(())
}

#[inline]
fn ref_i31_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.gc_enabled && builder.type_on_stack(module, ValType::I32)
}

fn ref_i31(
    _u: &mut Unstructured,
    _module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operand();
    builder.push_operand(Some(ValType::Ref(RefType {
        nullable: false,
        heap_type: HeapType::I31,
    })));
    instructions.push(Instruction::RefI31);
    Ok(())
}

#[inline]
fn i31_get_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.gc_enabled
        && builder.type_on_stack(
            module,
            ValType::Ref(RefType {
                nullable: true,
                heap_type: HeapType::I31,
            }),
        )
}

fn i31_get(
    u: &mut Unstructured,
    _module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operand();
    builder.push_operand(Some(ValType::I32));
    instructions.push(if u.arbitrary()? {
        Instruction::I31GetS
    } else {
        Instruction::I31GetU
    });
    Ok(())
}

#[inline]
fn any_convert_extern_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.gc_enabled
        && builder.type_on_stack(
            module,
            ValType::Ref(RefType {
                nullable: true,
                heap_type: HeapType::Extern,
            }),
        )
}

fn any_convert_extern(
    u: &mut Unstructured,
    _module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let nullable = match builder.pop_ref_type() {
        None => u.arbitrary()?,
        Some(r) => r.nullable,
    };
    builder.push_operand(Some(ValType::Ref(RefType {
        nullable,
        heap_type: HeapType::Any,
    })));
    instructions.push(Instruction::AnyConvertExtern);
    Ok(())
}

#[inline]
fn extern_convert_any_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.gc_enabled
        && builder.type_on_stack(
            module,
            ValType::Ref(RefType {
                nullable: true,
                heap_type: HeapType::Any,
            }),
        )
}

fn extern_convert_any(
    u: &mut Unstructured,
    _module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    let nullable = match builder.pop_ref_type() {
        None => u.arbitrary()?,
        Some(r) => r.nullable,
    };
    builder.push_operand(Some(ValType::Ref(RefType {
        nullable,
        heap_type: HeapType::Extern,
    })));
    instructions.push(Instruction::ExternConvertAny);
    Ok(())
}

fn table_index(ty: RefType, u: &mut Unstructured, module: &Module) -> Result<u32> {
    let tables = module
        .tables
        .iter()
        .enumerate()
        .filter(|(_, t)| module.ref_type_is_sub_type(ty, t.element_type))
        .map(|t| t.0 as u32)
        .collect::<Vec<_>>();
    Ok(*u.choose(&tables)?)
}

fn lane_index(u: &mut Unstructured, number_of_lanes: u8) -> Result<u8> {
    u.int_in_range(0..=(number_of_lanes - 1))
}

#[inline]
fn simd_v128_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    !module.config.disallow_traps
        && module.config.simd_enabled
        && builder.types_on_stack(module, &[ValType::V128])
}

#[inline]
fn simd_v128_on_stack_relaxed(module: &Module, builder: &mut CodeBuilder) -> bool {
    !module.config.disallow_traps
        && module.config.relaxed_simd_enabled
        && builder.types_on_stack(module, &[ValType::V128])
}

#[inline]
fn simd_v128_v128_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    !module.config.disallow_traps
        && module.config.simd_enabled
        && builder.types_on_stack(module, &[ValType::V128, ValType::V128])
}

#[inline]
fn simd_v128_v128_on_stack_relaxed(module: &Module, builder: &mut CodeBuilder) -> bool {
    !module.config.disallow_traps
        && module.config.relaxed_simd_enabled
        && builder.types_on_stack(module, &[ValType::V128, ValType::V128])
}

#[inline]
fn simd_v128_v128_v128_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    !module.config.disallow_traps
        && module.config.simd_enabled
        && builder.types_on_stack(module, &[ValType::V128, ValType::V128, ValType::V128])
}

#[inline]
fn simd_v128_v128_v128_on_stack_relaxed(module: &Module, builder: &mut CodeBuilder) -> bool {
    !module.config.disallow_traps
        && module.config.relaxed_simd_enabled
        && builder.types_on_stack(module, &[ValType::V128, ValType::V128, ValType::V128])
}

#[inline]
fn simd_v128_i32_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    !module.config.disallow_traps
        && module.config.simd_enabled
        && builder.types_on_stack(module, &[ValType::V128, ValType::I32])
}

#[inline]
fn simd_v128_i64_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    !module.config.disallow_traps
        && module.config.simd_enabled
        && builder.types_on_stack(module, &[ValType::V128, ValType::I64])
}

#[inline]
fn simd_v128_f32_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    !module.config.disallow_traps
        && module.config.simd_enabled
        && builder.types_on_stack(module, &[ValType::V128, ValType::F32])
}

#[inline]
fn simd_v128_f64_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    !module.config.disallow_traps
        && module.config.simd_enabled
        && builder.types_on_stack(module, &[ValType::V128, ValType::F64])
}

#[inline]
fn simd_i32_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    !module.config.disallow_traps
        && module.config.simd_enabled
        && builder.type_on_stack(module, ValType::I32)
}

#[inline]
fn simd_i64_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    !module.config.disallow_traps
        && module.config.simd_enabled
        && builder.type_on_stack(module, ValType::I64)
}

#[inline]
fn simd_f32_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    !module.config.disallow_traps
        && module.config.simd_enabled
        && builder.type_on_stack(module, ValType::F32)
}

#[inline]
fn simd_f64_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    !module.config.disallow_traps
        && module.config.simd_enabled
        && builder.type_on_stack(module, ValType::F64)
}

#[inline]
fn simd_have_memory_and_offset(module: &Module, builder: &mut CodeBuilder) -> bool {
    !module.config.disallow_traps
        && module.config.simd_enabled
        && have_memory_and_offset(module, builder)
}

#[inline]
fn simd_have_memory_and_offset_and_v128(module: &Module, builder: &mut CodeBuilder) -> bool {
    !module.config.disallow_traps
        && module.config.simd_enabled
        && store_valid(module, builder, || ValType::V128)
}

#[inline]
fn simd_load_lane_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    // The SIMD non-trapping case is not yet implemented.
    !module.config.disallow_traps && simd_have_memory_and_offset_and_v128(module, builder)
}

#[inline]
fn simd_v128_store_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    !module.config.disallow_traps
        && module.config.simd_enabled
        && store_valid(module, builder, || ValType::V128)
}

#[inline]
fn simd_store_lane_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    // The SIMD non-trapping case is not yet implemented.
    !module.config.disallow_traps && simd_v128_store_valid(module, builder)
}

#[inline]
fn simd_enabled(module: &Module, _: &mut CodeBuilder) -> bool {
    module.config.simd_enabled
}

macro_rules! simd_load {
    ($instruction:ident, $generator_fn_name:ident, $alignments:expr) => {
        fn $generator_fn_name(
            u: &mut Unstructured,
            module: &Module,
            builder: &mut CodeBuilder,

            instructions: &mut Vec<Instruction>,
        ) -> Result<()> {
            let memarg = mem_arg(u, module, builder, $alignments)?;
            builder.push_operands(&[ValType::V128]);
            if module.config.disallow_traps {
                no_traps::load(
                    Instruction::$instruction(memarg),
                    module,
                    builder,
                    instructions,
                );
            } else {
                instructions.push(Instruction::$instruction(memarg));
            }
            Ok(())
        }
    };
}

simd_load!(V128Load, v128_load, &[0, 1, 2, 3, 4]);
simd_load!(V128Load8x8S, v128_load8x8s, &[0, 1, 2, 3]);
simd_load!(V128Load8x8U, v128_load8x8u, &[0, 1, 2, 3]);
simd_load!(V128Load16x4S, v128_load16x4s, &[0, 1, 2, 3]);
simd_load!(V128Load16x4U, v128_load16x4u, &[0, 1, 2, 3]);
simd_load!(V128Load32x2S, v128_load32x2s, &[0, 1, 2, 3]);
simd_load!(V128Load32x2U, v128_load32x2u, &[0, 1, 2, 3]);
simd_load!(V128Load8Splat, v128_load8_splat, &[0]);
simd_load!(V128Load16Splat, v128_load16_splat, &[0, 1]);
simd_load!(V128Load32Splat, v128_load32_splat, &[0, 1, 2]);
simd_load!(V128Load64Splat, v128_load64_splat, &[0, 1, 2, 3]);
simd_load!(V128Load32Zero, v128_load32_zero, &[0, 1, 2]);
simd_load!(V128Load64Zero, v128_load64_zero, &[0, 1, 2, 3]);

fn v128_store(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::V128]);
    let memarg = mem_arg(u, module, builder, &[0, 1, 2, 3, 4])?;
    if module.config.disallow_traps {
        no_traps::store(
            Instruction::V128Store(memarg),
            module,
            builder,
            instructions,
        );
    } else {
        instructions.push(Instruction::V128Store(memarg));
    }
    Ok(())
}

macro_rules! simd_load_lane {
    ($instruction:ident, $generator_fn_name:ident, $alignments:expr, $number_of_lanes:expr) => {
        fn $generator_fn_name(
            u: &mut Unstructured,
            module: &Module,
            builder: &mut CodeBuilder,
            instructions: &mut Vec<Instruction>,
        ) -> Result<()> {
            builder.pop_operands(module, &[ValType::V128]);
            let memarg = mem_arg(u, module, builder, $alignments)?;
            builder.push_operands(&[ValType::V128]);
            instructions.push(Instruction::$instruction {
                memarg,
                lane: lane_index(u, $number_of_lanes)?,
            });
            Ok(())
        }
    };
}

simd_load_lane!(V128Load8Lane, v128_load8_lane, &[0], 16);
simd_load_lane!(V128Load16Lane, v128_load16_lane, &[0, 1], 8);
simd_load_lane!(V128Load32Lane, v128_load32_lane, &[0, 1, 2], 4);
simd_load_lane!(V128Load64Lane, v128_load64_lane, &[0, 1, 2, 3], 2);

macro_rules! simd_store_lane {
    ($instruction:ident, $generator_fn_name:ident, $alignments:expr, $number_of_lanes:expr) => {
        fn $generator_fn_name(
            u: &mut Unstructured,
            module: &Module,
            builder: &mut CodeBuilder,
            instructions: &mut Vec<Instruction>,
        ) -> Result<()> {
            builder.pop_operands(module, &[ValType::V128]);
            let memarg = mem_arg(u, module, builder, $alignments)?;
            instructions.push(Instruction::$instruction {
                memarg,
                lane: lane_index(u, $number_of_lanes)?,
            });
            Ok(())
        }
    };
}

simd_store_lane!(V128Store8Lane, v128_store8_lane, &[0], 16);
simd_store_lane!(V128Store16Lane, v128_store16_lane, &[0, 1], 8);
simd_store_lane!(V128Store32Lane, v128_store32_lane, &[0, 1, 2], 4);
simd_store_lane!(V128Store64Lane, v128_store64_lane, &[0, 1, 2, 3], 2);

fn v128_const(
    u: &mut Unstructured,
    _module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.push_operands(&[ValType::V128]);
    let c = i128::from_le_bytes(u.arbitrary()?);
    instructions.push(Instruction::V128Const(c));
    Ok(())
}

fn i8x16_shuffle(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
    instructions: &mut Vec<Instruction>,
) -> Result<()> {
    builder.pop_operands(module, &[ValType::V128, ValType::V128]);
    builder.push_operands(&[ValType::V128]);
    let mut lanes = [0; 16];
    for i in 0..16 {
        lanes[i] = u.int_in_range(0..=31)?;
    }
    instructions.push(Instruction::I8x16Shuffle(lanes));
    Ok(())
}

macro_rules! simd_lane_access {
    ($instruction:ident, $generator_fn_name:ident, $in_types:expr => $out_types:expr, $number_of_lanes:expr) => {
        fn $generator_fn_name(
            u: &mut Unstructured,
            module: &Module,
            builder: &mut CodeBuilder,
            instructions: &mut Vec<Instruction>,
        ) -> Result<()> {
            builder.pop_operands(module, $in_types);
            builder.push_operands($out_types);
            instructions.push(Instruction::$instruction(lane_index(u, $number_of_lanes)?));
            Ok(())
        }
    };
}

simd_lane_access!(I8x16ExtractLaneS, i8x16_extract_lane_s, &[ValType::V128] => &[ValType::I32], 16);
simd_lane_access!(I8x16ExtractLaneU, i8x16_extract_lane_u, &[ValType::V128] => &[ValType::I32], 16);
simd_lane_access!(I8x16ReplaceLane, i8x16_replace_lane, &[ValType::V128, ValType::I32] => &[ValType::V128], 16);
simd_lane_access!(I16x8ExtractLaneS, i16x8_extract_lane_s, &[ValType::V128] => &[ValType::I32], 8);
simd_lane_access!(I16x8ExtractLaneU, i16x8_extract_lane_u, &[ValType::V128] => &[ValType::I32], 8);
simd_lane_access!(I16x8ReplaceLane, i16x8_replace_lane, &[ValType::V128, ValType::I32] => &[ValType::V128], 8);
simd_lane_access!(I32x4ExtractLane, i32x4_extract_lane, &[ValType::V128] => &[ValType::I32], 4);
simd_lane_access!(I32x4ReplaceLane, i32x4_replace_lane, &[ValType::V128, ValType::I32] => &[ValType::V128], 4);
simd_lane_access!(I64x2ExtractLane, i64x2_extract_lane, &[ValType::V128] => &[ValType::I64], 2);
simd_lane_access!(I64x2ReplaceLane, i64x2_replace_lane, &[ValType::V128, ValType::I64] => &[ValType::V128], 2);
simd_lane_access!(F32x4ExtractLane, f32x4_extract_lane, &[ValType::V128] => &[ValType::F32], 4);
simd_lane_access!(F32x4ReplaceLane, f32x4_replace_lane, &[ValType::V128, ValType::F32] => &[ValType::V128], 4);
simd_lane_access!(F64x2ExtractLane, f64x2_extract_lane, &[ValType::V128] => &[ValType::F64], 2);
simd_lane_access!(F64x2ReplaceLane, f64x2_replace_lane, &[ValType::V128, ValType::F64] => &[ValType::V128], 2);

macro_rules! simd_binop {
    ($instruction:ident, $generator_fn_name:ident) => {
        fn $generator_fn_name(
            _: &mut Unstructured,
            module: &Module,
            builder: &mut CodeBuilder,

            instructions: &mut Vec<Instruction>,
        ) -> Result<()> {
            builder.pop_operands(module, &[ValType::V128, ValType::V128]);
            builder.push_operands(&[ValType::V128]);
            instructions.push(Instruction::$instruction);
            Ok(())
        }
    };
}

macro_rules! simd_unop {
    ($instruction:ident, $generator_fn_name:ident) => {
        simd_unop!($instruction, $generator_fn_name, V128 -> V128);
    };

    ($instruction:ident, $generator_fn_name:ident, $in_type:ident -> $out_type:ident) => {
        fn $generator_fn_name(
            _: &mut Unstructured,
            module: &Module,
            builder: &mut CodeBuilder,

       instructions: &mut Vec<Instruction>, ) -> Result<()> {
            builder.pop_operands(module, &[ValType::$in_type]);
            builder.push_operands(&[ValType::$out_type]);
            instructions.push(Instruction::$instruction);
            Ok(())
        }
    };
}

macro_rules! simd_ternop {
    ($instruction:ident, $generator_fn_name:ident) => {
        fn $generator_fn_name(
            _: &mut Unstructured,
            module: &Module,
            builder: &mut CodeBuilder,

            instructions: &mut Vec<Instruction>,
        ) -> Result<()> {
            builder.pop_operands(module, &[ValType::V128, ValType::V128, ValType::V128]);
            builder.push_operands(&[ValType::V128]);
            instructions.push(Instruction::$instruction);
            Ok(())
        }
    };
}

macro_rules! simd_shift {
    ($instruction:ident, $generator_fn_name:ident) => {
        fn $generator_fn_name(
            _: &mut Unstructured,
            module: &Module,
            builder: &mut CodeBuilder,

            instructions: &mut Vec<Instruction>,
        ) -> Result<()> {
            builder.pop_operands(module, &[ValType::V128, ValType::I32]);
            builder.push_operands(&[ValType::V128]);
            instructions.push(Instruction::$instruction);
            Ok(())
        }
    };
}

simd_unop!(I8x16Splat, i8x16_splat, I32 -> V128);
simd_unop!(I16x8Splat, i16x8_splat, I32 -> V128);
simd_unop!(I32x4Splat, i32x4_splat, I32 -> V128);
simd_unop!(I64x2Splat, i64x2_splat, I64 -> V128);
simd_unop!(F32x4Splat, f32x4_splat, F32 -> V128);
simd_unop!(F64x2Splat, f64x2_splat, F64 -> V128);
simd_binop!(I8x16Swizzle, i8x16_swizzle);
simd_binop!(I8x16Eq, i8x16_eq);
simd_binop!(I8x16Ne, i8x16_ne);
simd_binop!(I8x16LtS, i8x16_lt_s);
simd_binop!(I8x16LtU, i8x16_lt_u);
simd_binop!(I8x16GtS, i8x16_gt_s);
simd_binop!(I8x16GtU, i8x16_gt_u);
simd_binop!(I8x16LeS, i8x16_le_s);
simd_binop!(I8x16LeU, i8x16_le_u);
simd_binop!(I8x16GeS, i8x16_ge_s);
simd_binop!(I8x16GeU, i8x16_ge_u);
simd_binop!(I16x8Eq, i16x8_eq);
simd_binop!(I16x8Ne, i16x8_ne);
simd_binop!(I16x8LtS, i16x8_lt_s);
simd_binop!(I16x8LtU, i16x8_lt_u);
simd_binop!(I16x8GtS, i16x8_gt_s);
simd_binop!(I16x8GtU, i16x8_gt_u);
simd_binop!(I16x8LeS, i16x8_le_s);
simd_binop!(I16x8LeU, i16x8_le_u);
simd_binop!(I16x8GeS, i16x8_ge_s);
simd_binop!(I16x8GeU, i16x8_ge_u);
simd_binop!(I32x4Eq, i32x4_eq);
simd_binop!(I32x4Ne, i32x4_ne);
simd_binop!(I32x4LtS, i32x4_lt_s);
simd_binop!(I32x4LtU, i32x4_lt_u);
simd_binop!(I32x4GtS, i32x4_gt_s);
simd_binop!(I32x4GtU, i32x4_gt_u);
simd_binop!(I32x4LeS, i32x4_le_s);
simd_binop!(I32x4LeU, i32x4_le_u);
simd_binop!(I32x4GeS, i32x4_ge_s);
simd_binop!(I32x4GeU, i32x4_ge_u);
simd_binop!(I64x2Eq, i64x2_eq);
simd_binop!(I64x2Ne, i64x2_ne);
simd_binop!(I64x2LtS, i64x2_lt_s);
simd_binop!(I64x2GtS, i64x2_gt_s);
simd_binop!(I64x2LeS, i64x2_le_s);
simd_binop!(I64x2GeS, i64x2_ge_s);
simd_binop!(F32x4Eq, f32x4_eq);
simd_binop!(F32x4Ne, f32x4_ne);
simd_binop!(F32x4Lt, f32x4_lt);
simd_binop!(F32x4Gt, f32x4_gt);
simd_binop!(F32x4Le, f32x4_le);
simd_binop!(F32x4Ge, f32x4_ge);
simd_binop!(F64x2Eq, f64x2_eq);
simd_binop!(F64x2Ne, f64x2_ne);
simd_binop!(F64x2Lt, f64x2_lt);
simd_binop!(F64x2Gt, f64x2_gt);
simd_binop!(F64x2Le, f64x2_le);
simd_binop!(F64x2Ge, f64x2_ge);
simd_unop!(V128Not, v128_not);
simd_binop!(V128And, v128_and);
simd_binop!(V128AndNot, v128_and_not);
simd_binop!(V128Or, v128_or);
simd_binop!(V128Xor, v128_xor);
simd_unop!(V128AnyTrue, v128_any_true, V128 -> I32);
simd_unop!(I8x16Abs, i8x16_abs);
simd_unop!(I8x16Neg, i8x16_neg);
simd_unop!(I8x16Popcnt, i8x16_popcnt);
simd_unop!(I8x16AllTrue, i8x16_all_true, V128 -> I32);
simd_unop!(I8x16Bitmask, i8x16_bitmask, V128 -> I32);
simd_binop!(I8x16NarrowI16x8S, i8x16_narrow_i16x8s);
simd_binop!(I8x16NarrowI16x8U, i8x16_narrow_i16x8u);
simd_shift!(I8x16Shl, i8x16_shl);
simd_shift!(I8x16ShrS, i8x16_shr_s);
simd_shift!(I8x16ShrU, i8x16_shr_u);
simd_binop!(I8x16Add, i8x16_add);
simd_binop!(I8x16AddSatS, i8x16_add_sat_s);
simd_binop!(I8x16AddSatU, i8x16_add_sat_u);
simd_binop!(I8x16Sub, i8x16_sub);
simd_binop!(I8x16SubSatS, i8x16_sub_sat_s);
simd_binop!(I8x16SubSatU, i8x16_sub_sat_u);
simd_binop!(I8x16MinS, i8x16_min_s);
simd_binop!(I8x16MinU, i8x16_min_u);
simd_binop!(I8x16MaxS, i8x16_max_s);
simd_binop!(I8x16MaxU, i8x16_max_u);
simd_binop!(I8x16AvgrU, i8x16_avgr_u);
simd_unop!(I16x8ExtAddPairwiseI8x16S, i16x8_extadd_pairwise_i8x16s);
simd_unop!(I16x8ExtAddPairwiseI8x16U, i16x8_extadd_pairwise_i8x16u);
simd_unop!(I16x8Abs, i16x8_abs);
simd_unop!(I16x8Neg, i16x8_neg);
simd_binop!(I16x8Q15MulrSatS, i16x8q15_mulr_sat_s);
simd_unop!(I16x8AllTrue, i16x8_all_true, V128 -> I32);
simd_unop!(I16x8Bitmask, i16x8_bitmask, V128 -> I32);
simd_binop!(I16x8NarrowI32x4S, i16x8_narrow_i32x4s);
simd_binop!(I16x8NarrowI32x4U, i16x8_narrow_i32x4u);
simd_unop!(I16x8ExtendLowI8x16S, i16x8_extend_low_i8x16s);
simd_unop!(I16x8ExtendHighI8x16S, i16x8_extend_high_i8x16s);
simd_unop!(I16x8ExtendLowI8x16U, i16x8_extend_low_i8x16u);
simd_unop!(I16x8ExtendHighI8x16U, i16x8_extend_high_i8x16u);
simd_shift!(I16x8Shl, i16x8_shl);
simd_shift!(I16x8ShrS, i16x8_shr_s);
simd_shift!(I16x8ShrU, i16x8_shr_u);
simd_binop!(I16x8Add, i16x8_add);
simd_binop!(I16x8AddSatS, i16x8_add_sat_s);
simd_binop!(I16x8AddSatU, i16x8_add_sat_u);
simd_binop!(I16x8Sub, i16x8_sub);
simd_binop!(I16x8SubSatS, i16x8_sub_sat_s);
simd_binop!(I16x8SubSatU, i16x8_sub_sat_u);
simd_binop!(I16x8Mul, i16x8_mul);
simd_binop!(I16x8MinS, i16x8_min_s);
simd_binop!(I16x8MinU, i16x8_min_u);
simd_binop!(I16x8MaxS, i16x8_max_s);
simd_binop!(I16x8MaxU, i16x8_max_u);
simd_binop!(I16x8AvgrU, i16x8_avgr_u);
simd_binop!(I16x8ExtMulLowI8x16S, i16x8_extmul_low_i8x16s);
simd_binop!(I16x8ExtMulHighI8x16S, i16x8_extmul_high_i8x16s);
simd_binop!(I16x8ExtMulLowI8x16U, i16x8_extmul_low_i8x16u);
simd_binop!(I16x8ExtMulHighI8x16U, i16x8_extmul_high_i8x16u);
simd_unop!(I32x4ExtAddPairwiseI16x8S, i32x4_extadd_pairwise_i16x8s);
simd_unop!(I32x4ExtAddPairwiseI16x8U, i32x4_extadd_pairwise_i16x8u);
simd_unop!(I32x4Abs, i32x4_abs);
simd_unop!(I32x4Neg, i32x4_neg);
simd_unop!(I32x4AllTrue, i32x4_all_true, V128 -> I32);
simd_unop!(I32x4Bitmask, i32x4_bitmask, V128 -> I32);
simd_unop!(I32x4ExtendLowI16x8S, i32x4_extend_low_i16x8s);
simd_unop!(I32x4ExtendHighI16x8S, i32x4_extend_high_i16x8s);
simd_unop!(I32x4ExtendLowI16x8U, i32x4_extend_low_i16x8u);
simd_unop!(I32x4ExtendHighI16x8U, i32x4_extend_high_i16x8u);
simd_shift!(I32x4Shl, i32x4_shl);
simd_shift!(I32x4ShrS, i32x4_shr_s);
simd_shift!(I32x4ShrU, i32x4_shr_u);
simd_binop!(I32x4Add, i32x4_add);
simd_binop!(I32x4Sub, i32x4_sub);
simd_binop!(I32x4Mul, i32x4_mul);
simd_binop!(I32x4MinS, i32x4_min_s);
simd_binop!(I32x4MinU, i32x4_min_u);
simd_binop!(I32x4MaxS, i32x4_max_s);
simd_binop!(I32x4MaxU, i32x4_max_u);
simd_binop!(I32x4DotI16x8S, i32x4_dot_i16x8s);
simd_binop!(I32x4ExtMulLowI16x8S, i32x4_extmul_low_i16x8s);
simd_binop!(I32x4ExtMulHighI16x8S, i32x4_extmul_high_i16x8s);
simd_binop!(I32x4ExtMulLowI16x8U, i32x4_extmul_low_i16x8u);
simd_binop!(I32x4ExtMulHighI16x8U, i32x4_extmul_high_i16x8u);
simd_unop!(I64x2Abs, i64x2_abs);
simd_unop!(I64x2Neg, i64x2_neg);
simd_unop!(I64x2AllTrue, i64x2_all_true, V128 -> I32);
simd_unop!(I64x2Bitmask, i64x2_bitmask, V128 -> I32);
simd_unop!(I64x2ExtendLowI32x4S, i64x2_extend_low_i32x4s);
simd_unop!(I64x2ExtendHighI32x4S, i64x2_extend_high_i32x4s);
simd_unop!(I64x2ExtendLowI32x4U, i64x2_extend_low_i32x4u);
simd_unop!(I64x2ExtendHighI32x4U, i64x2_extend_high_i32x4u);
simd_shift!(I64x2Shl, i64x2_shl);
simd_shift!(I64x2ShrS, i64x2_shr_s);
simd_shift!(I64x2ShrU, i64x2_shr_u);
simd_binop!(I64x2Add, i64x2_add);
simd_binop!(I64x2Sub, i64x2_sub);
simd_binop!(I64x2Mul, i64x2_mul);
simd_binop!(I64x2ExtMulLowI32x4S, i64x2_extmul_low_i32x4s);
simd_binop!(I64x2ExtMulHighI32x4S, i64x2_extmul_high_i32x4s);
simd_binop!(I64x2ExtMulLowI32x4U, i64x2_extmul_low_i32x4u);
simd_binop!(I64x2ExtMulHighI32x4U, i64x2_extmul_high_i32x4u);
simd_unop!(F32x4Ceil, f32x4_ceil);
simd_unop!(F32x4Floor, f32x4_floor);
simd_unop!(F32x4Trunc, f32x4_trunc);
simd_unop!(F32x4Nearest, f32x4_nearest);
simd_unop!(F32x4Abs, f32x4_abs);
simd_unop!(F32x4Neg, f32x4_neg);
simd_unop!(F32x4Sqrt, f32x4_sqrt);
simd_binop!(F32x4Add, f32x4_add);
simd_binop!(F32x4Sub, f32x4_sub);
simd_binop!(F32x4Mul, f32x4_mul);
simd_binop!(F32x4Div, f32x4_div);
simd_binop!(F32x4Min, f32x4_min);
simd_binop!(F32x4Max, f32x4_max);
simd_binop!(F32x4PMin, f32x4p_min);
simd_binop!(F32x4PMax, f32x4p_max);
simd_unop!(F64x2Ceil, f64x2_ceil);
simd_unop!(F64x2Floor, f64x2_floor);
simd_unop!(F64x2Trunc, f64x2_trunc);
simd_unop!(F64x2Nearest, f64x2_nearest);
simd_unop!(F64x2Abs, f64x2_abs);
simd_unop!(F64x2Neg, f64x2_neg);
simd_unop!(F64x2Sqrt, f64x2_sqrt);
simd_binop!(F64x2Add, f64x2_add);
simd_binop!(F64x2Sub, f64x2_sub);
simd_binop!(F64x2Mul, f64x2_mul);
simd_binop!(F64x2Div, f64x2_div);
simd_binop!(F64x2Min, f64x2_min);
simd_binop!(F64x2Max, f64x2_max);
simd_binop!(F64x2PMin, f64x2p_min);
simd_binop!(F64x2PMax, f64x2p_max);
simd_unop!(I32x4TruncSatF32x4S, i32x4_trunc_sat_f32x4s);
simd_unop!(I32x4TruncSatF32x4U, i32x4_trunc_sat_f32x4u);
simd_unop!(F32x4ConvertI32x4S, f32x4_convert_i32x4s);
simd_unop!(F32x4ConvertI32x4U, f32x4_convert_i32x4u);
simd_unop!(I32x4TruncSatF64x2SZero, i32x4_trunc_sat_f64x2s_zero);
simd_unop!(I32x4TruncSatF64x2UZero, i32x4_trunc_sat_f64x2u_zero);
simd_unop!(F64x2ConvertLowI32x4S, f64x2_convert_low_i32x4s);
simd_unop!(F64x2ConvertLowI32x4U, f64x2_convert_low_i32x4u);
simd_unop!(F32x4DemoteF64x2Zero, f32x4_demote_f64x2_zero);
simd_unop!(F64x2PromoteLowF32x4, f64x2_promote_low_f32x4);
simd_ternop!(V128Bitselect, v128_bitselect);
simd_binop!(I8x16RelaxedSwizzle, i8x16_relaxed_swizzle);
simd_unop!(I32x4RelaxedTruncF32x4S, i32x4_relaxed_trunc_f32x4s);
simd_unop!(I32x4RelaxedTruncF32x4U, i32x4_relaxed_trunc_f32x4u);
simd_unop!(I32x4RelaxedTruncF64x2SZero, i32x4_relaxed_trunc_f64x2s_zero);
simd_unop!(I32x4RelaxedTruncF64x2UZero, i32x4_relaxed_trunc_f64x2u_zero);
simd_ternop!(F32x4RelaxedMadd, f32x4_relaxed_madd);
simd_ternop!(F32x4RelaxedNmadd, f32x4_relaxed_nmadd);
simd_ternop!(F64x2RelaxedMadd, f64x2_relaxed_madd);
simd_ternop!(F64x2RelaxedNmadd, f64x2_relaxed_nmadd);
simd_ternop!(I8x16RelaxedLaneselect, i8x16_relaxed_laneselect);
simd_ternop!(I16x8RelaxedLaneselect, i16x8_relaxed_laneselect);
simd_ternop!(I32x4RelaxedLaneselect, i32x4_relaxed_laneselect);
simd_ternop!(I64x2RelaxedLaneselect, i64x2_relaxed_laneselect);
simd_binop!(F32x4RelaxedMin, f32x4_relaxed_min);
simd_binop!(F32x4RelaxedMax, f32x4_relaxed_max);
simd_binop!(F64x2RelaxedMin, f64x2_relaxed_min);
simd_binop!(F64x2RelaxedMax, f64x2_relaxed_max);
simd_binop!(I16x8RelaxedQ15mulrS, i16x8_relaxed_q15mulr_s);
simd_binop!(I16x8RelaxedDotI8x16I7x16S, i16x8_relaxed_dot_i8x16_i7x16_s);
simd_ternop!(
    I32x4RelaxedDotI8x16I7x16AddS,
    i32x4_relaxed_dot_i8x16_i7x16_add_s
);
