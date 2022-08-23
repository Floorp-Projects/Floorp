use super::{
    Elements, FuncType, GlobalInitExpr, Instruction, InstructionKind::*, InstructionKinds, Module,
    ValType,
};
use arbitrary::{Result, Unstructured};
use std::collections::{BTreeMap, BTreeSet};
use std::convert::TryFrom;
use wasm_encoder::{BlockType, MemArg};

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
            fn(&mut Unstructured<'_>, &Module, &mut CodeBuilder) -> Result<Instruction>
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
    (None, unreachable, Control, 990),
    (None, nop, Control, 800),
    (None, block, Control),
    (None, r#loop, Control),
    (Some(try_valid), r#try, Control),
    (Some(delegate_valid), delegate, Control),
    (Some(catch_valid), catch, Control),
    (Some(catch_all_valid), catch_all, Control),
    (Some(if_valid), r#if, Control),
    (Some(else_valid), r#else, Control),
    (Some(end_valid), end, Control),
    (Some(br_valid), br, Control),
    (Some(br_if_valid), br_if, Control),
    (Some(br_table_valid), br_table, Control),
    (Some(return_valid), r#return, Control, 900),
    (Some(call_valid), call, Control),
    (Some(call_indirect_valid), call_indirect, Control),
    (Some(throw_valid), throw, Control, 850),
    (Some(rethrow_valid), rethrow, Control),
    // Parametric instructions.
    (Some(drop_valid), drop, Parametric),
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
    // reference types proposal
    (Some(ref_null_valid), ref_null, Reference),
    (Some(ref_func_valid), ref_func, Reference),
    (Some(ref_is_null_valid), ref_is_null, Reference),
    (Some(table_fill_valid), table_fill, Reference),
    (Some(table_set_valid), table_set, Reference),
    (Some(table_get_valid), table_get, Reference),
    (Some(table_size_valid), table_size, Reference),
    (Some(table_grow_valid), table_grow, Reference),
    (Some(table_copy_valid), table_copy, Reference),
    (Some(table_init_valid), table_init, Reference),
    (Some(elem_drop_valid), elem_drop, Reference),
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
    (Some(simd_have_memory_and_offset_and_v128), v128_load8_lane, Vector),
    (Some(simd_have_memory_and_offset_and_v128), v128_load16_lane, Vector),
    (Some(simd_have_memory_and_offset_and_v128), v128_load32_lane, Vector),
    (Some(simd_have_memory_and_offset_and_v128), v128_load64_lane, Vector),
    (Some(simd_v128_store_valid), v128_store8_lane, Vector),
    (Some(simd_v128_store_valid), v128_store16_lane, Vector),
    (Some(simd_v128_store_valid), v128_store32_lane, Vector),
    (Some(simd_v128_store_valid), v128_store64_lane, Vector),
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
    (Some(simd_v128_v128_v128_on_stack_relaxed), i8x16_laneselect, Vector),
    (Some(simd_v128_v128_v128_on_stack_relaxed), i16x8_laneselect, Vector),
    (Some(simd_v128_v128_v128_on_stack_relaxed), i32x4_laneselect, Vector),
    (Some(simd_v128_v128_v128_on_stack_relaxed), i64x2_laneselect, Vector),
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
    (Some(simd_v128_v128_on_stack), i8x16_rounding_average_u, Vector),
    (Some(simd_v128_on_stack), i16x8_ext_add_pairwise_i8x16s, Vector),
    (Some(simd_v128_on_stack), i16x8_ext_add_pairwise_i8x16u, Vector),
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
    (Some(simd_v128_v128_on_stack), i16x8_rounding_average_u, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_ext_mul_low_i8x16s, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_ext_mul_high_i8x16s, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_ext_mul_low_i8x16u, Vector),
    (Some(simd_v128_v128_on_stack), i16x8_ext_mul_high_i8x16u, Vector),
    (Some(simd_v128_on_stack), i32x4_ext_add_pairwise_i16x8s, Vector),
    (Some(simd_v128_on_stack), i32x4_ext_add_pairwise_i16x8u, Vector),
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
    (Some(simd_v128_v128_on_stack), i32x4_ext_mul_low_i16x8s, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_ext_mul_high_i16x8s, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_ext_mul_low_i16x8u, Vector),
    (Some(simd_v128_v128_on_stack), i32x4_ext_mul_high_i16x8u, Vector),
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
    (Some(simd_v128_v128_on_stack), i64x2_ext_mul_low_i32x4s, Vector),
    (Some(simd_v128_v128_on_stack), i64x2_ext_mul_high_i32x4s, Vector),
    (Some(simd_v128_v128_on_stack), i64x2_ext_mul_low_i32x4u, Vector),
    (Some(simd_v128_v128_on_stack), i64x2_ext_mul_high_i32x4u, Vector),
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
    (Some(simd_v128_on_stack_relaxed), i32x4_relaxed_trunc_sat_f32x4s, Vector),
    (Some(simd_v128_on_stack_relaxed), i32x4_relaxed_trunc_sat_f32x4u, Vector),
    (Some(simd_v128_on_stack_relaxed), i32x4_relaxed_trunc_sat_f64x2s_zero, Vector),
    (Some(simd_v128_on_stack_relaxed), i32x4_relaxed_trunc_sat_f64x2u_zero, Vector),
    (Some(simd_v128_v128_v128_on_stack_relaxed), f32x4_fma, Vector),
    (Some(simd_v128_v128_v128_on_stack_relaxed), f32x4_fms, Vector),
    (Some(simd_v128_v128_v128_on_stack_relaxed), f64x2_fma, Vector),
    (Some(simd_v128_v128_v128_on_stack_relaxed), f64x2_fms, Vector),
    (Some(simd_v128_v128_on_stack_relaxed), f32x4_relaxed_min, Vector),
    (Some(simd_v128_v128_on_stack_relaxed), f32x4_relaxed_max, Vector),
    (Some(simd_v128_v128_on_stack_relaxed), f64x2_relaxed_min, Vector),
    (Some(simd_v128_v128_on_stack_relaxed), f64x2_relaxed_max, Vector),
}

pub(crate) struct CodeBuilderAllocations {
    // The control labels in scope right now.
    controls: Vec<Control>,

    // The types on the operand stack right now.
    operands: Vec<Option<ValType>>,

    // Dynamic set of options of instruction we can generate that are known to
    // be valid right now.
    options: Vec<(
        fn(&mut Unstructured, &Module, &mut CodeBuilder) -> Result<Instruction>,
        u32,
    )>,

    // Cached information about the module that we're generating functions for,
    // used to speed up validity checks. The mutable globals map is a map of the
    // type of global to the global indices which have that type (and they're
    // all mutable).
    mutable_globals: BTreeMap<ValType, Vec<u32>>,

    // Like mutable globals above this is a map from function types to the list
    // of functions that have that function type.
    functions: BTreeMap<Vec<ValType>, Vec<u32>>,

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
    Try,
    Catch,
    CatchAll,
}

enum Float {
    F32,
    F64,
    F32x4,
    F64x2,
}

impl CodeBuilderAllocations {
    pub(crate) fn new(module: &Module) -> Self {
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
                .entry(func.params.to_vec())
                .or_insert(Vec::new())
                .push(idx);
        }

        let mut funcref_tables = Vec::new();
        let mut table_tys = Vec::new();
        for (i, table) in module.tables.iter().enumerate() {
            table_tys.push(table.element_type);
            if table.element_type == ValType::FuncRef {
                funcref_tables.push(i as u32);
            }
        }

        let mut referenced_functions = BTreeSet::new();
        for (_, expr) in module.defined_globals.iter() {
            if let GlobalInitExpr::FuncRef(i) = *expr {
                referenced_functions.insert(i);
            }
        }
        for g in module.elems.iter() {
            match &g.items {
                Elements::Expressions(e) => {
                    let iter = e.iter().filter_map(|i| *i);
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
}

impl CodeBuilder<'_> {
    /// Get the operands that are in-scope within the current control frame.
    fn operands(&self) -> &[Option<ValType>] {
        let height = self.allocs.controls.last().map_or(0, |c| c.height);
        &self.allocs.operands[height..]
    }

    fn pop_operands(&mut self, to_pop: &[ValType]) {
        debug_assert!(self.types_on_stack(to_pop));
        self.allocs
            .operands
            .truncate(self.allocs.operands.len() - to_pop.len());
    }

    fn push_operands(&mut self, to_push: &[ValType]) {
        self.allocs
            .operands
            .extend(to_push.iter().copied().map(Some));
    }

    fn label_types_on_stack(&self, to_check: &Control) -> bool {
        self.types_on_stack(to_check.label_types())
    }

    fn type_on_stack(&self, ty: ValType) -> bool {
        match self.operands().last() {
            None => false,
            Some(None) => true,
            Some(Some(x)) => *x == ty,
        }
    }

    fn types_on_stack(&self, types: &[ValType]) -> bool {
        self.operands().len() >= types.len()
            && self
                .operands()
                .iter()
                .rev()
                .zip(types.iter().rev())
                .all(|(a, b)| match (a, b) {
                    (None, _) => true,
                    (Some(x), y) => x == y,
                })
    }

    #[inline(never)]
    fn arbitrary_block_type(&self, u: &mut Unstructured, module: &Module) -> Result<BlockType> {
        let mut options: Vec<Box<dyn Fn(&mut Unstructured) -> Result<BlockType>>> = vec![
            Box::new(|_| Ok(BlockType::Empty)),
            Box::new(|u| Ok(BlockType::Result(module.arbitrary_valtype(u)?))),
        ];
        if module.config.multi_value_enabled() {
            for (i, ty) in module.func_types() {
                if self.types_on_stack(&ty.params) {
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
        let max_instructions = module.config.max_instructions();
        let allowed_instructions = module.config.allowed_instructions();
        let mut instructions = vec![];

        while !self.allocs.controls.is_empty() {
            let keep_going = instructions.len() < max_instructions
                && u.arbitrary().map_or(false, |b: u8| b != 0);
            if !keep_going {
                self.end_active_control_frames(u, &mut instructions);
                break;
            }

            match choose_instruction(u, module, allowed_instructions, &mut self) {
                Some(f) => {
                    let inst = f(u, module, &mut self)?;
                    instructions.push(inst);
                }
                // Choosing an instruction can fail because there is not enough
                // underlying data, so we really cannot generate any more
                // instructions. In this case we swallow that error and instead
                // just terminate our wasm function's frames.
                None => {
                    self.end_active_control_frames(u, &mut instructions);
                    break;
                }
            }

            // If the configuration for this module requests nan
            // canonicalization then perform that here based on whether or not
            // the previous instruction needs canonicalization. Note that this
            // is based off Cranelift's pass for nan canonicalization for which
            // instructions to canonicalize, but the general idea is most
            // floating-point operations.
            if module.config.canonicalize_nans() {
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
        let local = match ty {
            Float::F32 => &mut self.f32_scratch,
            Float::F64 => &mut self.f64_scratch,
            Float::F32x4 | Float::F64x2 => &mut self.v128_scratch,
        };
        let local = match *local {
            Some(i) => i,
            None => {
                let val = self.locals.len() + self.func_ty.params.len() + self.extra_locals.len();
                *local = Some(val);
                self.extra_locals.push(match ty {
                    Float::F32 => ValType::F32,
                    Float::F64 => ValType::F64,
                    Float::F32x4 | Float::F64x2 => ValType::V128,
                });
                val
            }
        };
        let local = local as u32;

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

    fn end_active_control_frames(
        &mut self,
        u: &mut Unstructured<'_>,
        instructions: &mut Vec<Instruction>,
    ) {
        while !self.allocs.controls.is_empty() {
            // Ensure that this label is valid by placing the right types onto
            // the operand stack for the end of the label.
            self.guarantee_label_results(u, instructions);

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
                self.guarantee_label_results(u, instructions);
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
    }

    /// Modifies the instruction stream to guarantee that the current control
    /// label's results are on the stack and ready for the control label to return.
    fn guarantee_label_results(
        &mut self,
        u: &mut Unstructured<'_>,
        instructions: &mut Vec<Instruction>,
    ) {
        let mut operands = self.operands();
        let label = self.allocs.controls.last().unwrap();

        // Already done, yay!
        if label.results.len() == operands.len() && self.types_on_stack(&label.results) {
            return;
        }

        // Generating an unreachable instruction is always a valid way to
        // generate any types for a label, but it's not too interesting, so
        // don't favor it.
        if u.arbitrary::<u16>().unwrap_or(0) == 1 {
            instructions.push(Instruction::Unreachable);
            return;
        }

        // Arbitrarily massage the stack to get the expected results. First we
        // drop all extraneous results to we're only dealing with those we want
        // to deal with. Afterwards we start at the bottom of the stack and move
        // up, figuring out what matches and what doesn't. As soon as something
        // doesn't match we throw out that and everything else remaining,
        // filling in results with dummy values.
        while operands.len() > label.results.len() {
            instructions.push(Instruction::Drop);
            operands = &operands[..operands.len() - 1];
        }
        for (i, expected) in label.results.iter().enumerate() {
            if let Some(actual) = operands.get(i) {
                if Some(*expected) == *actual {
                    continue;
                }
                for _ in operands[i..].iter() {
                    instructions.push(Instruction::Drop);
                }
                operands = &[];
            }
            instructions.push(arbitrary_val(*expected, u));
        }
    }
}

fn arbitrary_val(ty: ValType, u: &mut Unstructured<'_>) -> Instruction {
    match ty {
        ValType::I32 => Instruction::I32Const(u.arbitrary().unwrap_or(0)),
        ValType::I64 => Instruction::I64Const(u.arbitrary().unwrap_or(0)),
        ValType::F32 => Instruction::F32Const(u.arbitrary().unwrap_or(0.0)),
        ValType::F64 => Instruction::F64Const(u.arbitrary().unwrap_or(0.0)),
        ValType::V128 => Instruction::V128Const(u.arbitrary().unwrap_or(0)),
        ValType::ExternRef => Instruction::RefNull(ValType::ExternRef),
        ValType::FuncRef => Instruction::RefNull(ValType::FuncRef),
    }
}

fn unreachable(_: &mut Unstructured, _: &Module, _: &mut CodeBuilder) -> Result<Instruction> {
    Ok(Instruction::Unreachable)
}

fn nop(_: &mut Unstructured, _: &Module, _: &mut CodeBuilder) -> Result<Instruction> {
    Ok(Instruction::Nop)
}

fn block(u: &mut Unstructured, module: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let block_ty = builder.arbitrary_block_type(u, module)?;
    let (params, results) = module.params_results(&block_ty);
    let height = builder.allocs.operands.len() - params.len();
    builder.allocs.controls.push(Control {
        kind: ControlKind::Block,
        params,
        results,
        height,
    });
    Ok(Instruction::Block(block_ty))
}

#[inline]
fn try_valid(module: &Module, _: &mut CodeBuilder) -> bool {
    module.config.exceptions_enabled()
}

fn r#try(u: &mut Unstructured, module: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let block_ty = builder.arbitrary_block_type(u, module)?;
    let (params, results) = module.params_results(&block_ty);
    let height = builder.allocs.operands.len() - params.len();
    builder.allocs.controls.push(Control {
        kind: ControlKind::Try,
        params,
        results,
        height,
    });
    Ok(Instruction::Try(block_ty))
}

#[inline]
fn delegate_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    let control_kind = builder.allocs.controls.last().unwrap().kind;
    // delegate is only valid if end could be used in a try control frame
    module.config.exceptions_enabled()
        && control_kind == ControlKind::Try
        && end_valid(module, builder)
}

fn delegate(u: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    // There will always be at least the function's return frame and try
    // control frame if we are emitting delegate
    let n = builder.allocs.controls.iter().count();
    debug_assert!(n >= 2);
    // Delegate must target an outer control from the try block, and is
    // encoded with relative depth from the outer control
    let target_relative_from_last = u.int_in_range(1..=n - 1)?;
    let target_relative_from_outer = target_relative_from_last - 1;
    // Delegate ends the try block
    builder.allocs.controls.pop();
    Ok(Instruction::Delegate(target_relative_from_outer as u32))
}

#[inline]
fn catch_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    let control_kind = builder.allocs.controls.last().unwrap().kind;
    // catch is only valid if end could be used in a try or catch (not
    // catch_all) control frame. There must also be a tag that we can catch.
    module.config.exceptions_enabled()
        && (control_kind == ControlKind::Try || control_kind == ControlKind::Catch)
        && end_valid(module, builder)
        && module.tags.len() > 0
}

fn catch(u: &mut Unstructured, module: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let tag_idx = u.int_in_range(0..=(module.tags.len() - 1))?;
    let tag_type = &module.tags[tag_idx];
    let control = builder.allocs.controls.pop().unwrap();
    // Pop the results for the previous try or catch
    builder.pop_operands(&control.results);
    // Push the params of the tag we're catching
    builder.push_operands(&tag_type.func_type.params);
    builder.allocs.controls.push(Control {
        kind: ControlKind::Catch,
        ..control
    });
    Ok(Instruction::Catch(tag_idx as u32))
}

#[inline]
fn catch_all_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    let control_kind = builder.allocs.controls.last().unwrap().kind;
    // catch_all is only valid if end could be used in a try or catch (not
    // catch_all) control frame.
    module.config.exceptions_enabled()
        && (control_kind == ControlKind::Try || control_kind == ControlKind::Catch)
        && end_valid(module, builder)
}

fn catch_all(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let control = builder.allocs.controls.pop().unwrap();
    // Pop the results for the previous try or catch
    builder.pop_operands(&control.results);
    builder.allocs.controls.push(Control {
        kind: ControlKind::CatchAll,
        ..control
    });
    Ok(Instruction::CatchAll)
}

fn r#loop(u: &mut Unstructured, module: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let block_ty = builder.arbitrary_block_type(u, module)?;
    let (params, results) = module.params_results(&block_ty);
    let height = builder.allocs.operands.len() - params.len();
    builder.allocs.controls.push(Control {
        kind: ControlKind::Loop,
        params,
        results,
        height,
    });
    Ok(Instruction::Loop(block_ty))
}

#[inline]
fn if_valid(_: &Module, builder: &mut CodeBuilder) -> bool {
    builder.type_on_stack(ValType::I32)
}

fn r#if(u: &mut Unstructured, module: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);

    let block_ty = builder.arbitrary_block_type(u, module)?;
    let (params, results) = module.params_results(&block_ty);
    let height = builder.allocs.operands.len() - params.len();
    builder.allocs.controls.push(Control {
        kind: ControlKind::If,
        params,
        results,
        height,
    });
    Ok(Instruction::If(block_ty))
}

#[inline]
fn else_valid(_: &Module, builder: &mut CodeBuilder) -> bool {
    let last_control = builder.allocs.controls.last().unwrap();
    last_control.kind == ControlKind::If
        && builder.operands().len() == last_control.results.len()
        && builder.types_on_stack(&last_control.results)
}

fn r#else(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let control = builder.allocs.controls.pop().unwrap();
    builder.pop_operands(&control.results);
    builder.push_operands(&control.params);
    builder.allocs.controls.push(Control {
        kind: ControlKind::Block,
        ..control
    });
    Ok(Instruction::Else)
}

#[inline]
fn end_valid(_: &Module, builder: &mut CodeBuilder) -> bool {
    // Note: first control frame is the function return's control frame, which
    // does not have an associated `end`.
    if builder.allocs.controls.len() <= 1 {
        return false;
    }
    let control = builder.allocs.controls.last().unwrap();
    builder.operands().len() == control.results.len()
        && builder.types_on_stack(&control.results)
        // `if`s that don't leave the stack as they found it must have an
        // `else`.
        && !(control.kind == ControlKind::If && control.params != control.results)
}

fn end(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.allocs.controls.pop();
    Ok(Instruction::End)
}

#[inline]
fn br_valid(_: &Module, builder: &mut CodeBuilder) -> bool {
    builder
        .allocs
        .controls
        .iter()
        .any(|l| builder.label_types_on_stack(l))
}

fn br(u: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let n = builder
        .allocs
        .controls
        .iter()
        .filter(|l| builder.label_types_on_stack(l))
        .count();
    debug_assert!(n > 0);
    let i = u.int_in_range(0..=n - 1)?;
    let (target, _) = builder
        .allocs
        .controls
        .iter()
        .rev()
        .enumerate()
        .filter(|(_, l)| builder.label_types_on_stack(l))
        .nth(i)
        .unwrap();
    let control = &builder.allocs.controls[builder.allocs.controls.len() - 1 - target];
    let tys = control.label_types().to_vec();
    builder.pop_operands(&tys);
    Ok(Instruction::Br(target as u32))
}

#[inline]
fn br_if_valid(_: &Module, builder: &mut CodeBuilder) -> bool {
    if !builder.type_on_stack(ValType::I32) {
        return false;
    }
    let ty = builder.allocs.operands.pop().unwrap();
    let is_valid = builder
        .allocs
        .controls
        .iter()
        .any(|l| builder.label_types_on_stack(l));
    builder.allocs.operands.push(ty);
    is_valid
}

fn br_if(u: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);

    let n = builder
        .allocs
        .controls
        .iter()
        .filter(|l| builder.label_types_on_stack(l))
        .count();
    debug_assert!(n > 0);
    let i = u.int_in_range(0..=n - 1)?;
    let (target, _) = builder
        .allocs
        .controls
        .iter()
        .rev()
        .enumerate()
        .filter(|(_, l)| builder.label_types_on_stack(l))
        .nth(i)
        .unwrap();
    Ok(Instruction::BrIf(target as u32))
}

#[inline]
fn br_table_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    if !builder.type_on_stack(ValType::I32) {
        return false;
    }
    let ty = builder.allocs.operands.pop().unwrap();
    let is_valid = br_valid(module, builder);
    builder.allocs.operands.push(ty);
    is_valid
}

fn br_table(u: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);

    let n = builder
        .allocs
        .controls
        .iter()
        .filter(|l| builder.label_types_on_stack(l))
        .count();
    debug_assert!(n > 0);

    let i = u.int_in_range(0..=n - 1)?;
    let (default_target, _) = builder
        .allocs
        .controls
        .iter()
        .rev()
        .enumerate()
        .filter(|(_, l)| builder.label_types_on_stack(l))
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
    builder.pop_operands(&tys);

    Ok(Instruction::BrTable(targets, default_target as u32))
}

#[inline]
fn return_valid(_: &Module, builder: &mut CodeBuilder) -> bool {
    builder.label_types_on_stack(&builder.allocs.controls[0])
}

fn r#return(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let results = builder.allocs.controls[0].results.clone();
    builder.pop_operands(&results);
    Ok(Instruction::Return)
}

#[inline]
fn call_valid(_: &Module, builder: &mut CodeBuilder) -> bool {
    builder
        .allocs
        .functions
        .keys()
        .any(|k| builder.types_on_stack(k))
}

fn call(u: &mut Unstructured, module: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let candidates = builder
        .allocs
        .functions
        .iter()
        .filter(|(k, _)| builder.types_on_stack(k))
        .flat_map(|(_, v)| v.iter().copied())
        .collect::<Vec<_>>();
    assert!(candidates.len() > 0);
    let i = u.int_in_range(0..=candidates.len() - 1)?;
    let (func_idx, ty) = module.funcs().nth(candidates[i] as usize).unwrap();
    builder.pop_operands(&ty.params);
    builder.push_operands(&ty.results);
    Ok(Instruction::Call(func_idx as u32))
}

#[inline]
fn call_indirect_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    if builder.allocs.funcref_tables.is_empty() || !builder.type_on_stack(ValType::I32) {
        return false;
    }
    let ty = builder.allocs.operands.pop().unwrap();
    let is_valid = module
        .func_types()
        .any(|(_, ty)| builder.types_on_stack(&ty.params));
    builder.allocs.operands.push(ty);
    is_valid
}

fn call_indirect(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);

    let choices = module
        .func_types()
        .filter(|(_, ty)| builder.types_on_stack(&ty.params))
        .collect::<Vec<_>>();
    let (type_idx, ty) = u.choose(&choices)?;
    builder.pop_operands(&ty.params);
    builder.push_operands(&ty.results);
    let table = *u.choose(&builder.allocs.funcref_tables)?;
    Ok(Instruction::CallIndirect {
        ty: *type_idx as u32,
        table,
    })
}

#[inline]
fn throw_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.exceptions_enabled()
        && builder
            .allocs
            .tags
            .keys()
            .any(|k| builder.types_on_stack(k))
}

fn throw(u: &mut Unstructured, module: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let candidates = builder
        .allocs
        .tags
        .iter()
        .filter(|(k, _)| builder.types_on_stack(k))
        .flat_map(|(_, v)| v.iter().copied())
        .collect::<Vec<_>>();
    assert!(candidates.len() > 0);
    let i = u.int_in_range(0..=candidates.len() - 1)?;
    let (tag_idx, tag_type) = module.tags().nth(candidates[i] as usize).unwrap();
    // Tags have no results, throwing cannot return
    assert!(tag_type.func_type.results.len() == 0);
    builder.pop_operands(&tag_type.func_type.params);
    Ok(Instruction::Throw(tag_idx as u32))
}

#[inline]
fn rethrow_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    // There must be a catch or catch_all control on the stack
    module.config.exceptions_enabled()
        && builder
            .allocs
            .controls
            .iter()
            .any(|l| l.kind == ControlKind::Catch || l.kind == ControlKind::CatchAll)
}

fn rethrow(u: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let n = builder
        .allocs
        .controls
        .iter()
        .filter(|l| l.kind == ControlKind::Catch || l.kind == ControlKind::CatchAll)
        .count();
    debug_assert!(n > 0);
    let i = u.int_in_range(0..=n - 1)?;
    let (target, _) = builder
        .allocs
        .controls
        .iter()
        .rev()
        .enumerate()
        .filter(|(_, l)| l.kind == ControlKind::Catch || l.kind == ControlKind::CatchAll)
        .nth(i)
        .unwrap();
    Ok(Instruction::Rethrow(target as u32))
}

#[inline]
fn drop_valid(_: &Module, builder: &mut CodeBuilder) -> bool {
    !builder.operands().is_empty()
}

fn drop(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.allocs.operands.pop();
    Ok(Instruction::Drop)
}

#[inline]
fn select_valid(_: &Module, builder: &mut CodeBuilder) -> bool {
    if !(builder.operands().len() >= 3 && builder.type_on_stack(ValType::I32)) {
        return false;
    }
    let t = builder.operands()[builder.operands().len() - 2];
    let u = builder.operands()[builder.operands().len() - 3];
    t.is_none() || u.is_none() || t == u
}

fn select(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.allocs.operands.pop();
    let t = builder.allocs.operands.pop().unwrap();
    let u = builder.allocs.operands.pop().unwrap();
    let ty = t.or(u);
    builder.allocs.operands.push(ty);
    match ty {
        Some(ty @ ValType::ExternRef) | Some(ty @ ValType::FuncRef) => {
            Ok(Instruction::TypedSelect(ty))
        }
        Some(ValType::I32) | Some(ValType::I64) | Some(ValType::F32) | Some(ValType::F64)
        | Some(ValType::V128) | None => Ok(Instruction::Select),
    }
}

#[inline]
fn local_get_valid(_: &Module, builder: &mut CodeBuilder) -> bool {
    !builder.func_ty.params.is_empty() || !builder.locals.is_empty()
}

fn local_get(u: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let num_params = builder.func_ty.params.len();
    let n = num_params + builder.locals.len();
    debug_assert!(n > 0);
    let i = u.int_in_range(0..=n - 1)?;
    builder.allocs.operands.push(Some(if i < num_params {
        builder.func_ty.params[i]
    } else {
        builder.locals[i - num_params]
    }));
    Ok(Instruction::LocalGet(i as u32))
}

#[inline]
fn local_set_valid(_: &Module, builder: &mut CodeBuilder) -> bool {
    builder
        .func_ty
        .params
        .iter()
        .chain(builder.locals.iter())
        .any(|ty| builder.type_on_stack(*ty))
}

fn local_set(u: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let n = builder
        .func_ty
        .params
        .iter()
        .chain(builder.locals.iter())
        .filter(|ty| builder.type_on_stack(**ty))
        .count();
    debug_assert!(n > 0);
    let i = u.int_in_range(0..=n - 1)?;
    let (j, _) = builder
        .func_ty
        .params
        .iter()
        .chain(builder.locals.iter())
        .enumerate()
        .filter(|(_, ty)| builder.type_on_stack(**ty))
        .nth(i)
        .unwrap();
    builder.allocs.operands.pop();
    Ok(Instruction::LocalSet(j as u32))
}

fn local_tee(u: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let n = builder
        .func_ty
        .params
        .iter()
        .chain(builder.locals.iter())
        .filter(|ty| builder.type_on_stack(**ty))
        .count();
    debug_assert!(n > 0);
    let i = u.int_in_range(0..=n - 1)?;
    let (j, _) = builder
        .func_ty
        .params
        .iter()
        .chain(builder.locals.iter())
        .enumerate()
        .filter(|(_, ty)| builder.type_on_stack(**ty))
        .nth(i)
        .unwrap();
    Ok(Instruction::LocalTee(j as u32))
}

#[inline]
fn global_get_valid(module: &Module, _: &mut CodeBuilder) -> bool {
    module.globals.len() > 0
}

fn global_get(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    debug_assert!(module.globals.len() > 0);
    let global_idx = u.int_in_range(0..=module.globals.len() - 1)?;
    builder
        .allocs
        .operands
        .push(Some(module.globals[global_idx].val_type));
    Ok(Instruction::GlobalGet(global_idx as u32))
}

#[inline]
fn global_set_valid(_: &Module, builder: &mut CodeBuilder) -> bool {
    builder
        .allocs
        .mutable_globals
        .iter()
        .any(|(ty, _)| builder.type_on_stack(*ty))
}

fn global_set(u: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let candidates = builder
        .allocs
        .mutable_globals
        .iter()
        .find(|(ty, _)| builder.type_on_stack(**ty))
        .unwrap()
        .1;
    let i = u.int_in_range(0..=candidates.len() - 1)?;
    builder.allocs.operands.pop();
    Ok(Instruction::GlobalSet(candidates[i]))
}

#[inline]
fn have_memory(module: &Module, _: &mut CodeBuilder) -> bool {
    module.memories.len() > 0
}

#[inline]
fn have_memory_and_offset(_module: &Module, builder: &mut CodeBuilder) -> bool {
    (builder.allocs.memory32.len() > 0 && builder.type_on_stack(ValType::I32))
        || (builder.allocs.memory64.len() > 0 && builder.type_on_stack(ValType::I64))
}

#[inline]
fn have_data(module: &Module, _: &mut CodeBuilder) -> bool {
    module.data.len() > 0
}

fn i32_load(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let memarg = mem_arg(u, module, builder, &[0, 1, 2])?;
    builder.allocs.operands.push(Some(ValType::I32));
    Ok(Instruction::I32Load(memarg))
}

fn i64_load(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let memarg = mem_arg(u, module, builder, &[0, 1, 2, 3])?;
    builder.allocs.operands.push(Some(ValType::I64));
    Ok(Instruction::I64Load(memarg))
}

fn f32_load(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let memarg = mem_arg(u, module, builder, &[0, 1, 2])?;
    builder.allocs.operands.push(Some(ValType::F32));
    Ok(Instruction::F32Load(memarg))
}

fn f64_load(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let memarg = mem_arg(u, module, builder, &[0, 1, 2, 3])?;
    builder.allocs.operands.push(Some(ValType::F64));
    Ok(Instruction::F64Load(memarg))
}

fn i32_load_8_s(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let memarg = mem_arg(u, module, builder, &[0])?;
    builder.allocs.operands.push(Some(ValType::I32));
    Ok(Instruction::I32Load8_S(memarg))
}

fn i32_load_8_u(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let memarg = mem_arg(u, module, builder, &[0])?;
    builder.allocs.operands.push(Some(ValType::I32));
    Ok(Instruction::I32Load8_U(memarg))
}

fn i32_load_16_s(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let memarg = mem_arg(u, module, builder, &[0, 1])?;
    builder.allocs.operands.push(Some(ValType::I32));
    Ok(Instruction::I32Load16_S(memarg))
}

fn i32_load_16_u(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let memarg = mem_arg(u, module, builder, &[0, 1])?;
    builder.allocs.operands.push(Some(ValType::I32));
    Ok(Instruction::I32Load16_U(memarg))
}

fn i64_load_8_s(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let memarg = mem_arg(u, module, builder, &[0])?;
    builder.allocs.operands.push(Some(ValType::I64));
    Ok(Instruction::I64Load8_S(memarg))
}

fn i64_load_16_s(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let memarg = mem_arg(u, module, builder, &[0, 1])?;
    builder.allocs.operands.push(Some(ValType::I64));
    Ok(Instruction::I64Load16_S(memarg))
}

fn i64_load_32_s(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let memarg = mem_arg(u, module, builder, &[0, 1, 2])?;
    builder.allocs.operands.push(Some(ValType::I64));
    Ok(Instruction::I64Load32_S(memarg))
}

fn i64_load_8_u(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let memarg = mem_arg(u, module, builder, &[0])?;
    builder.allocs.operands.push(Some(ValType::I64));
    Ok(Instruction::I64Load8_U(memarg))
}

fn i64_load_16_u(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let memarg = mem_arg(u, module, builder, &[0, 1])?;
    builder.allocs.operands.push(Some(ValType::I64));
    Ok(Instruction::I64Load16_U(memarg))
}

fn i64_load_32_u(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let memarg = mem_arg(u, module, builder, &[0, 1, 2])?;
    builder.allocs.operands.push(Some(ValType::I64));
    Ok(Instruction::I64Load32_U(memarg))
}

#[inline]
fn store_valid(_module: &Module, builder: &mut CodeBuilder, f: impl Fn() -> ValType) -> bool {
    (builder.allocs.memory32.len() > 0 && builder.types_on_stack(&[ValType::I32, f()]))
        || (builder.allocs.memory64.len() > 0 && builder.types_on_stack(&[ValType::I64, f()]))
}

#[inline]
fn i32_store_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    store_valid(module, builder, || ValType::I32)
}

fn i32_store(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);
    let memarg = mem_arg(u, module, builder, &[0, 1, 2])?;
    Ok(Instruction::I32Store(memarg))
}

#[inline]
fn i64_store_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    store_valid(module, builder, || ValType::I64)
}

fn i64_store(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64]);
    let memarg = mem_arg(u, module, builder, &[0, 1, 2, 3])?;
    Ok(Instruction::I64Store(memarg))
}

#[inline]
fn f32_store_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    store_valid(module, builder, || ValType::F32)
}

fn f32_store(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32]);
    let memarg = mem_arg(u, module, builder, &[0, 1, 2])?;
    Ok(Instruction::F32Store(memarg))
}

#[inline]
fn f64_store_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    store_valid(module, builder, || ValType::F64)
}

fn f64_store(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64]);
    let memarg = mem_arg(u, module, builder, &[0, 1, 2, 3])?;
    Ok(Instruction::F64Store(memarg))
}

fn i32_store_8(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);
    let memarg = mem_arg(u, module, builder, &[0])?;
    Ok(Instruction::I32Store8(memarg))
}

fn i32_store_16(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);
    let memarg = mem_arg(u, module, builder, &[0, 1])?;
    Ok(Instruction::I32Store16(memarg))
}

fn i64_store_8(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64]);
    let memarg = mem_arg(u, module, builder, &[0])?;
    Ok(Instruction::I64Store8(memarg))
}

fn i64_store_16(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64]);
    let memarg = mem_arg(u, module, builder, &[0, 1])?;
    Ok(Instruction::I64Store16(memarg))
}

fn i64_store_32(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64]);
    let memarg = mem_arg(u, module, builder, &[0, 1, 2])?;
    Ok(Instruction::I64Store32(memarg))
}

fn memory_size(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let i = u.int_in_range(0..=module.memories.len() - 1)?;
    let ty = if module.memories[i].memory64 {
        ValType::I64
    } else {
        ValType::I32
    };
    builder.push_operands(&[ty]);
    Ok(Instruction::MemorySize(i as u32))
}

#[inline]
fn memory_grow_valid(_module: &Module, builder: &mut CodeBuilder) -> bool {
    (builder.allocs.memory32.len() > 0 && builder.type_on_stack(ValType::I32))
        || (builder.allocs.memory64.len() > 0 && builder.type_on_stack(ValType::I64))
}

fn memory_grow(
    u: &mut Unstructured,
    _module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let ty = if builder.type_on_stack(ValType::I32) {
        ValType::I32
    } else {
        ValType::I64
    };
    let index = memory_index(u, builder, ty)?;
    builder.pop_operands(&[ty]);
    builder.push_operands(&[ty]);
    Ok(Instruction::MemoryGrow(index))
}

#[inline]
fn memory_init_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.bulk_memory_enabled()
        && have_data(module, builder)
        && (builder.allocs.memory32.len() > 0
            && builder.types_on_stack(&[ValType::I32, ValType::I32, ValType::I32])
            || (builder.allocs.memory64.len() > 0
                && builder.types_on_stack(&[ValType::I64, ValType::I32, ValType::I32])))
}

fn memory_init(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    let ty = if builder.type_on_stack(ValType::I32) {
        ValType::I32
    } else {
        ValType::I64
    };
    let mem = memory_index(u, builder, ty)?;
    let data = data_index(u, module)?;
    builder.pop_operands(&[ty]);
    Ok(Instruction::MemoryInit { mem, data })
}

#[inline]
fn memory_fill_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.bulk_memory_enabled()
        && (builder.allocs.memory32.len() > 0
            && builder.types_on_stack(&[ValType::I32, ValType::I32, ValType::I32])
            || (builder.allocs.memory64.len() > 0
                && builder.types_on_stack(&[ValType::I64, ValType::I32, ValType::I64])))
}

fn memory_fill(
    u: &mut Unstructured,
    _module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let ty = if builder.type_on_stack(ValType::I32) {
        ValType::I32
    } else {
        ValType::I64
    };
    let mem = memory_index(u, builder, ty)?;
    builder.pop_operands(&[ty, ValType::I32, ty]);
    Ok(Instruction::MemoryFill(mem))
}

#[inline]
fn memory_copy_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    if !module.config.bulk_memory_enabled() {
        return false;
    }

    if builder.types_on_stack(&[ValType::I64, ValType::I64, ValType::I64])
        && builder.allocs.memory64.len() > 0
    {
        return true;
    }
    if builder.types_on_stack(&[ValType::I32, ValType::I32, ValType::I32])
        && builder.allocs.memory32.len() > 0
    {
        return true;
    }
    if builder.types_on_stack(&[ValType::I64, ValType::I32, ValType::I32])
        && builder.allocs.memory32.len() > 0
        && builder.allocs.memory64.len() > 0
    {
        return true;
    }
    if builder.types_on_stack(&[ValType::I32, ValType::I64, ValType::I32])
        && builder.allocs.memory32.len() > 0
        && builder.allocs.memory64.len() > 0
    {
        return true;
    }
    false
}

fn memory_copy(
    u: &mut Unstructured,
    _module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let (src, dst) = if builder.types_on_stack(&[ValType::I64, ValType::I64, ValType::I64]) {
        builder.pop_operands(&[ValType::I64, ValType::I64, ValType::I64]);
        (
            memory_index(u, builder, ValType::I64)?,
            memory_index(u, builder, ValType::I64)?,
        )
    } else if builder.types_on_stack(&[ValType::I32, ValType::I32, ValType::I32]) {
        builder.pop_operands(&[ValType::I32, ValType::I32, ValType::I32]);
        (
            memory_index(u, builder, ValType::I32)?,
            memory_index(u, builder, ValType::I32)?,
        )
    } else if builder.types_on_stack(&[ValType::I64, ValType::I32, ValType::I32]) {
        builder.pop_operands(&[ValType::I64, ValType::I32, ValType::I32]);
        (
            memory_index(u, builder, ValType::I32)?,
            memory_index(u, builder, ValType::I64)?,
        )
    } else if builder.types_on_stack(&[ValType::I32, ValType::I64, ValType::I32]) {
        builder.pop_operands(&[ValType::I32, ValType::I64, ValType::I32]);
        (
            memory_index(u, builder, ValType::I64)?,
            memory_index(u, builder, ValType::I32)?,
        )
    } else {
        unreachable!()
    };
    Ok(Instruction::MemoryCopy { dst, src })
}

#[inline]
fn data_drop_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    have_data(module, builder) && module.config.bulk_memory_enabled()
}

fn data_drop(
    u: &mut Unstructured,
    module: &Module,
    _builder: &mut CodeBuilder,
) -> Result<Instruction> {
    Ok(Instruction::DataDrop(data_index(u, module)?))
}

fn i32_const(u: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let x = u.arbitrary()?;
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32Const(x))
}

fn i64_const(u: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let x = u.arbitrary()?;
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64Const(x))
}

fn f32_const(u: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let x = u.arbitrary()?;
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32Const(x))
}

fn f64_const(u: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let x = u.arbitrary()?;
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64Const(x))
}

#[inline]
fn i32_on_stack(_: &Module, builder: &mut CodeBuilder) -> bool {
    builder.type_on_stack(ValType::I32)
}

fn i32_eqz(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32Eqz)
}

#[inline]
fn i32_i32_on_stack(_: &Module, builder: &mut CodeBuilder) -> bool {
    builder.types_on_stack(&[ValType::I32, ValType::I32])
}

fn i32_eq(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32Eq)
}

fn i32_ne(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32Ne)
}

fn i32_lt_s(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32LtS)
}

fn i32_lt_u(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32LtU)
}

fn i32_gt_s(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32GtS)
}

fn i32_gt_u(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32GtU)
}

fn i32_le_s(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32LeS)
}

fn i32_le_u(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32LeU)
}

fn i32_ge_s(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32GeS)
}

fn i32_ge_u(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32GeU)
}

#[inline]
fn i64_on_stack(_: &Module, builder: &mut CodeBuilder) -> bool {
    builder.types_on_stack(&[ValType::I64])
}

fn i64_eqz(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I64Eqz)
}

#[inline]
fn i64_i64_on_stack(_: &Module, builder: &mut CodeBuilder) -> bool {
    builder.types_on_stack(&[ValType::I64, ValType::I64])
}

fn i64_eq(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I64Eq)
}

fn i64_ne(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I64Ne)
}

fn i64_lt_s(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I64LtS)
}

fn i64_lt_u(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I64LtU)
}

fn i64_gt_s(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I64GtS)
}

fn i64_gt_u(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I64GtU)
}

fn i64_le_s(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I64LeS)
}

fn i64_le_u(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I64LeU)
}

fn i64_ge_s(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I64GeS)
}

fn i64_ge_u(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I64GeU)
}

fn f32_f32_on_stack(_: &Module, builder: &mut CodeBuilder) -> bool {
    builder.types_on_stack(&[ValType::F32, ValType::F32])
}

fn f32_eq(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::F32Eq)
}

fn f32_ne(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::F32Ne)
}

fn f32_lt(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::F32Lt)
}

fn f32_gt(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::F32Gt)
}

fn f32_le(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::F32Le)
}

fn f32_ge(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::F32Ge)
}

fn f64_f64_on_stack(_: &Module, builder: &mut CodeBuilder) -> bool {
    builder.types_on_stack(&[ValType::F64, ValType::F64])
}

fn f64_eq(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::F64Eq)
}

fn f64_ne(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::F64Ne)
}

fn f64_lt(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::F64Lt)
}

fn f64_gt(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::F64Gt)
}

fn f64_le(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::F64Le)
}

fn f64_ge(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::F64Ge)
}

fn i32_clz(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32Clz)
}

fn i32_ctz(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32Ctz)
}

fn i32_popcnt(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32Popcnt)
}

fn i32_add(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32Add)
}

fn i32_sub(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32Sub)
}

fn i32_mul(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32Mul)
}

fn i32_div_s(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32DivS)
}

fn i32_div_u(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32DivU)
}

fn i32_rem_s(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32RemS)
}

fn i32_rem_u(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32RemU)
}

fn i32_and(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32And)
}

fn i32_or(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32Or)
}

fn i32_xor(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32Xor)
}

fn i32_shl(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32Shl)
}

fn i32_shr_s(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32ShrS)
}

fn i32_shr_u(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32ShrU)
}

fn i32_rotl(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32Rotl)
}

fn i32_rotr(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32Rotr)
}

fn i64_clz(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64Clz)
}

fn i64_ctz(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64Ctz)
}

fn i64_popcnt(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64Popcnt)
}

fn i64_add(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64Add)
}

fn i64_sub(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64Sub)
}

fn i64_mul(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64Mul)
}

fn i64_div_s(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64DivS)
}

fn i64_div_u(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64DivU)
}

fn i64_rem_s(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64RemS)
}

fn i64_rem_u(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64RemU)
}

fn i64_and(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64And)
}

fn i64_or(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64Or)
}

fn i64_xor(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64Xor)
}

fn i64_shl(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64Shl)
}

fn i64_shr_s(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64ShrS)
}

fn i64_shr_u(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64ShrU)
}

fn i64_rotl(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64Rotl)
}

fn i64_rotr(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64, ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64Rotr)
}

#[inline]
fn f32_on_stack(_: &Module, builder: &mut CodeBuilder) -> bool {
    builder.types_on_stack(&[ValType::F32])
}

fn f32_abs(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32Abs)
}

fn f32_neg(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32Neg)
}

fn f32_ceil(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32Ceil)
}

fn f32_floor(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32Floor)
}

fn f32_trunc(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32Trunc)
}

fn f32_nearest(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32Nearest)
}

fn f32_sqrt(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32Sqrt)
}

fn f32_add(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32Add)
}

fn f32_sub(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32Sub)
}

fn f32_mul(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32Mul)
}

fn f32_div(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32Div)
}

fn f32_min(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32Min)
}

fn f32_max(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32Max)
}

fn f32_copysign(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32, ValType::F32]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32Copysign)
}

#[inline]
fn f64_on_stack(_: &Module, builder: &mut CodeBuilder) -> bool {
    builder.types_on_stack(&[ValType::F64])
}

fn f64_abs(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64Abs)
}

fn f64_neg(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64Neg)
}

fn f64_ceil(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64Ceil)
}

fn f64_floor(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64Floor)
}

fn f64_trunc(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64Trunc)
}

fn f64_nearest(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64Nearest)
}

fn f64_sqrt(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64Sqrt)
}

fn f64_add(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64Add)
}

fn f64_sub(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64Sub)
}

fn f64_mul(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64Mul)
}

fn f64_div(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64Div)
}

fn f64_min(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64Min)
}

fn f64_max(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64Max)
}

fn f64_copysign(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64, ValType::F64]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64Copysign)
}

fn i32_wrap_i64(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32WrapI64)
}

fn nontrapping_f32_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.saturating_float_to_int_enabled() && f32_on_stack(module, builder)
}

fn i32_trunc_f32_s(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32TruncF32S)
}

fn i32_trunc_f32_u(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32TruncF32U)
}

fn nontrapping_f64_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.saturating_float_to_int_enabled() && f64_on_stack(module, builder)
}

fn i32_trunc_f64_s(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32TruncF64S)
}

fn i32_trunc_f64_u(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32TruncF64U)
}

fn i64_extend_i32_s(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64ExtendI32S)
}

fn i64_extend_i32_u(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64ExtendI32U)
}

fn i64_trunc_f32_s(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64TruncF32S)
}

fn i64_trunc_f32_u(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64TruncF32U)
}

fn i64_trunc_f64_s(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64TruncF64S)
}

fn i64_trunc_f64_u(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64TruncF64U)
}

fn f32_convert_i32_s(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32ConvertI32S)
}

fn f32_convert_i32_u(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32ConvertI32U)
}

fn f32_convert_i64_s(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32ConvertI64S)
}

fn f32_convert_i64_u(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32ConvertI64U)
}

fn f32_demote_f64(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32DemoteF64)
}

fn f64_convert_i32_s(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64ConvertI32S)
}

fn f64_convert_i32_u(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64ConvertI32U)
}

fn f64_convert_i64_s(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64ConvertI64S)
}

fn f64_convert_i64_u(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64ConvertI64U)
}

fn f64_promote_f32(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64PromoteF32)
}

fn i32_reinterpret_f32(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32ReinterpretF32)
}

fn i64_reinterpret_f64(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64ReinterpretF64)
}

fn f32_reinterpret_i32(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);
    builder.push_operands(&[ValType::F32]);
    Ok(Instruction::F32ReinterpretI32)
}

fn f64_reinterpret_i64(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64]);
    builder.push_operands(&[ValType::F64]);
    Ok(Instruction::F64ReinterpretI64)
}

fn extendable_i32_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.sign_extension_ops_enabled() && i32_on_stack(module, builder)
}

fn i32_extend_8_s(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32Extend8S)
}

fn i32_extend_16_s(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32Extend16S)
}

fn extendable_i64_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.sign_extension_ops_enabled() && i64_on_stack(module, builder)
}

fn i64_extend_8_s(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64Extend8S)
}

fn i64_extend_16_s(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64Extend16S)
}

fn i64_extend_32_s(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64Extend32S)
}

fn i32_trunc_sat_f32_s(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32TruncSatF32S)
}

fn i32_trunc_sat_f32_u(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32TruncSatF32U)
}

fn i32_trunc_sat_f64_s(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32TruncSatF64S)
}

fn i32_trunc_sat_f64_u(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64]);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::I32TruncSatF64U)
}

fn i64_trunc_sat_f32_s(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64TruncSatF32S)
}

fn i64_trunc_sat_f32_u(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F32]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64TruncSatF32U)
}

fn i64_trunc_sat_f64_s(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64TruncSatF64S)
}

fn i64_trunc_sat_f64_u(
    _: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::F64]);
    builder.push_operands(&[ValType::I64]);
    Ok(Instruction::I64TruncSatF64U)
}

fn memory_offset(u: &mut Unstructured, module: &Module, memory_index: u32) -> Result<u64> {
    let (a, b, c) = module.config.memory_offset_choices();
    assert!(a + b + c != 0);

    let memory_type = &module.memories[memory_index as usize];
    let min = memory_type.minimum.saturating_mul(65536);
    let max = memory_type
        .maximum
        .map(|max| max.saturating_mul(65536))
        .unwrap_or(u64::MAX);
    let (min, max, true_max) = if memory_type.memory64 {
        // 64-bit memories can use the limits calculated above as-is
        (min, max, u64::MAX)
    } else {
        // 32-bit memories can't represent a full 4gb offset, so if that's the
        // min/max sizes then we need to switch the m to `u32::MAX`.
        (
            u64::from(u32::try_from(min).unwrap_or(u32::MAX)),
            u64::from(u32::try_from(max).unwrap_or(u32::MAX)),
            u64::from(u32::MAX),
        )
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
    let memory_index = if builder.type_on_stack(ValType::I32) {
        builder.pop_operands(&[ValType::I32]);
        memory_index(u, builder, ValType::I32)?
    } else {
        builder.pop_operands(&[ValType::I64]);
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
    module.config.reference_types_enabled()
}

fn ref_null(u: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let ty = *u.choose(&[ValType::ExternRef, ValType::FuncRef])?;
    builder.push_operands(&[ty]);
    Ok(Instruction::RefNull(ty))
}

#[inline]
fn ref_func_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.reference_types_enabled() && builder.allocs.referenced_functions.len() > 0
}

fn ref_func(u: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    let i = *u.choose(&builder.allocs.referenced_functions)?;
    builder.push_operands(&[ValType::FuncRef]);
    Ok(Instruction::RefFunc(i))
}

#[inline]
fn ref_is_null_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.reference_types_enabled()
        && (builder.type_on_stack(ValType::ExternRef) || builder.type_on_stack(ValType::FuncRef))
}

fn ref_is_null(_: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    pop_reference_type(builder);
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::RefIsNull)
}

#[inline]
fn table_fill_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.reference_types_enabled()
        && module.config.bulk_memory_enabled()
        && [ValType::ExternRef, ValType::FuncRef].iter().any(|ty| {
            builder.types_on_stack(&[ValType::I32, *ty, ValType::I32])
                && module.tables.iter().any(|t| t.element_type == *ty)
        })
}

fn table_fill(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);
    let ty = pop_reference_type(builder);
    builder.pop_operands(&[ValType::I32]);
    let table = table_index(ty, u, module)?;
    Ok(Instruction::TableFill { table })
}

#[inline]
fn table_set_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.reference_types_enabled()
        && [ValType::ExternRef, ValType::FuncRef].iter().any(|ty| {
            builder.types_on_stack(&[ValType::I32, *ty])
                && module.tables.iter().any(|t| t.element_type == *ty)
        })
}

fn table_set(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let ty = pop_reference_type(builder);
    builder.pop_operands(&[ValType::I32]);
    let table = table_index(ty, u, module)?;
    Ok(Instruction::TableSet { table })
}

#[inline]
fn table_get_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.reference_types_enabled()
        && builder.type_on_stack(ValType::I32)
        && module.tables.len() > 0
}

fn table_get(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);
    let idx = u.int_in_range(0..=module.tables.len() - 1)?;
    let ty = module.tables[idx].element_type;
    builder.push_operands(&[ty]);
    Ok(Instruction::TableGet { table: idx as u32 })
}

#[inline]
fn table_size_valid(module: &Module, _: &mut CodeBuilder) -> bool {
    module.config.reference_types_enabled() && module.tables.len() > 0
}

fn table_size(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let table = u.int_in_range(0..=module.tables.len() - 1)? as u32;
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::TableSize { table })
}

#[inline]
fn table_grow_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.reference_types_enabled()
        && [ValType::ExternRef, ValType::FuncRef].iter().any(|ty| {
            builder.types_on_stack(&[*ty, ValType::I32])
                && module.tables.iter().any(|t| t.element_type == *ty)
        })
}

fn table_grow(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32]);
    let ty = pop_reference_type(builder);
    let table = table_index(ty, u, module)?;
    builder.push_operands(&[ValType::I32]);
    Ok(Instruction::TableGrow { table })
}

#[inline]
fn table_copy_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.reference_types_enabled()
        && module.tables.len() > 0
        && builder.types_on_stack(&[ValType::I32, ValType::I32, ValType::I32])
}

fn table_copy(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32, ValType::I32]);
    let src = u.int_in_range(0..=module.tables.len() - 1)? as u32;
    let dst = table_index(module.tables[src as usize].element_type, u, module)?;
    Ok(Instruction::TableCopy { src, dst })
}

#[inline]
fn table_init_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.reference_types_enabled()
        && builder.allocs.table_init_possible
        && builder.types_on_stack(&[ValType::I32, ValType::I32, ValType::I32])
}

fn table_init(
    u: &mut Unstructured,
    module: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::I32, ValType::I32, ValType::I32]);
    let segments = module
        .elems
        .iter()
        .enumerate()
        .filter(|(_, e)| module.tables.iter().any(|t| t.element_type == e.ty))
        .map(|(i, _)| i)
        .collect::<Vec<_>>();
    let segment = *u.choose(&segments)?;
    let table = table_index(module.elems[segment].ty, u, module)?;
    Ok(Instruction::TableInit {
        segment: segment as u32,
        table,
    })
}

#[inline]
fn elem_drop_valid(module: &Module, _builder: &mut CodeBuilder) -> bool {
    module.config.reference_types_enabled() && module.elems.len() > 0
}

fn elem_drop(
    u: &mut Unstructured,
    module: &Module,
    _builder: &mut CodeBuilder,
) -> Result<Instruction> {
    let segment = u.int_in_range(0..=module.elems.len() - 1)? as u32;
    Ok(Instruction::ElemDrop { segment })
}

fn pop_reference_type(builder: &mut CodeBuilder) -> ValType {
    if builder.type_on_stack(ValType::ExternRef) {
        builder.pop_operands(&[ValType::ExternRef]);
        ValType::ExternRef
    } else {
        builder.pop_operands(&[ValType::FuncRef]);
        ValType::FuncRef
    }
}

fn table_index(ty: ValType, u: &mut Unstructured, module: &Module) -> Result<u32> {
    let tables = module
        .tables
        .iter()
        .enumerate()
        .filter(|(_, t)| t.element_type == ty)
        .map(|t| t.0 as u32)
        .collect::<Vec<_>>();
    Ok(*u.choose(&tables)?)
}

fn lane_index(u: &mut Unstructured, number_of_lanes: u8) -> Result<u8> {
    u.int_in_range(0..=(number_of_lanes - 1))
}

#[inline]
fn simd_v128_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.simd_enabled() && builder.types_on_stack(&[ValType::V128])
}

#[inline]
fn simd_v128_on_stack_relaxed(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.relaxed_simd_enabled() && builder.types_on_stack(&[ValType::V128])
}

#[inline]
fn simd_v128_v128_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.simd_enabled() && builder.types_on_stack(&[ValType::V128, ValType::V128])
}

#[inline]
fn simd_v128_v128_on_stack_relaxed(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.relaxed_simd_enabled() && builder.types_on_stack(&[ValType::V128, ValType::V128])
}

#[inline]
fn simd_v128_v128_v128_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.simd_enabled()
        && builder.types_on_stack(&[ValType::V128, ValType::V128, ValType::V128])
}

#[inline]
fn simd_v128_v128_v128_on_stack_relaxed(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.relaxed_simd_enabled()
        && builder.types_on_stack(&[ValType::V128, ValType::V128, ValType::V128])
}

#[inline]
fn simd_v128_i32_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.simd_enabled() && builder.types_on_stack(&[ValType::V128, ValType::I32])
}

#[inline]
fn simd_v128_i64_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.simd_enabled() && builder.types_on_stack(&[ValType::V128, ValType::I64])
}

#[inline]
fn simd_v128_f32_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.simd_enabled() && builder.types_on_stack(&[ValType::V128, ValType::F32])
}

#[inline]
fn simd_v128_f64_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.simd_enabled() && builder.types_on_stack(&[ValType::V128, ValType::F64])
}

#[inline]
fn simd_i32_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.simd_enabled() && builder.type_on_stack(ValType::I32)
}

#[inline]
fn simd_i64_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.simd_enabled() && builder.type_on_stack(ValType::I64)
}

#[inline]
fn simd_f32_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.simd_enabled() && builder.type_on_stack(ValType::F32)
}

#[inline]
fn simd_f64_on_stack(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.simd_enabled() && builder.type_on_stack(ValType::F64)
}

#[inline]
fn simd_have_memory_and_offset(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.simd_enabled() && have_memory_and_offset(module, builder)
}

#[inline]
fn simd_have_memory_and_offset_and_v128(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.simd_enabled() && store_valid(module, builder, || ValType::V128)
}

#[inline]
fn simd_v128_store_valid(module: &Module, builder: &mut CodeBuilder) -> bool {
    module.config.simd_enabled() && store_valid(module, builder, || ValType::V128)
}

#[inline]
fn simd_enabled(module: &Module, _: &mut CodeBuilder) -> bool {
    module.config.simd_enabled()
}

macro_rules! simd_load {
    ($instruction:ident, $generator_fn_name:ident, $alignments:expr) => {
        fn $generator_fn_name(
            u: &mut Unstructured,
            module: &Module,
            builder: &mut CodeBuilder,
        ) -> Result<Instruction> {
            let memarg = mem_arg(u, module, builder, $alignments)?;
            builder.push_operands(&[ValType::V128]);
            Ok(Instruction::$instruction { memarg })
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
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::V128]);
    let memarg = mem_arg(u, module, builder, &[0, 1, 2, 3, 4])?;
    Ok(Instruction::V128Store { memarg })
}

macro_rules! simd_load_lane {
    ($instruction:ident, $generator_fn_name:ident, $alignments:expr, $number_of_lanes:expr) => {
        fn $generator_fn_name(
            u: &mut Unstructured,
            module: &Module,
            builder: &mut CodeBuilder,
        ) -> Result<Instruction> {
            builder.pop_operands(&[ValType::V128]);
            let memarg = mem_arg(u, module, builder, $alignments)?;
            builder.push_operands(&[ValType::V128]);
            Ok(Instruction::$instruction {
                memarg,
                lane: lane_index(u, $number_of_lanes)?,
            })
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
        ) -> Result<Instruction> {
            builder.pop_operands(&[ValType::V128]);
            let memarg = mem_arg(u, module, builder, $alignments)?;
            Ok(Instruction::$instruction {
                memarg,
                lane: lane_index(u, $number_of_lanes)?,
            })
        }
    };
}

simd_store_lane!(V128Store8Lane, v128_store8_lane, &[0], 16);
simd_store_lane!(V128Store16Lane, v128_store16_lane, &[0, 1], 8);
simd_store_lane!(V128Store32Lane, v128_store32_lane, &[0, 1, 2], 4);
simd_store_lane!(V128Store64Lane, v128_store64_lane, &[0, 1, 2, 3], 2);

fn v128_const(u: &mut Unstructured, _: &Module, builder: &mut CodeBuilder) -> Result<Instruction> {
    builder.push_operands(&[ValType::V128]);
    let c = i128::from_le_bytes(u.arbitrary()?);
    Ok(Instruction::V128Const(c))
}

fn i8x16_shuffle(
    u: &mut Unstructured,
    _: &Module,
    builder: &mut CodeBuilder,
) -> Result<Instruction> {
    builder.pop_operands(&[ValType::V128, ValType::V128]);
    builder.push_operands(&[ValType::V128]);
    let mut lanes = [0; 16];
    for i in 0..16 {
        lanes[i] = u.int_in_range(0..=31)?;
    }
    Ok(Instruction::I8x16Shuffle { lanes })
}

macro_rules! simd_lane_access {
    ($instruction:ident, $generator_fn_name:ident, $in_types:expr => $out_types:expr, $number_of_lanes:expr) => {
        fn $generator_fn_name(
            u: &mut Unstructured,
            _: &Module,
            builder: &mut CodeBuilder,
        ) -> Result<Instruction> {
            builder.pop_operands($in_types);
            builder.push_operands($out_types);
            Ok(Instruction::$instruction {
                lane: lane_index(u, $number_of_lanes)?,
            })
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
            _: &Module,
            builder: &mut CodeBuilder,
        ) -> Result<Instruction> {
            builder.pop_operands(&[ValType::V128, ValType::V128]);
            builder.push_operands(&[ValType::V128]);
            Ok(Instruction::$instruction)
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
            _: &Module,
            builder: &mut CodeBuilder,
        ) -> Result<Instruction> {
            builder.pop_operands(&[ValType::$in_type]);
            builder.push_operands(&[ValType::$out_type]);
            Ok(Instruction::$instruction)
        }
    };
}

macro_rules! simd_ternop {
    ($instruction:ident, $generator_fn_name:ident) => {
        fn $generator_fn_name(
            _: &mut Unstructured,
            _: &Module,
            builder: &mut CodeBuilder,
        ) -> Result<Instruction> {
            builder.pop_operands(&[ValType::V128, ValType::V128, ValType::V128]);
            builder.push_operands(&[ValType::V128]);
            Ok(Instruction::$instruction)
        }
    };
}

macro_rules! simd_shift {
    ($instruction:ident, $generator_fn_name:ident) => {
        fn $generator_fn_name(
            _: &mut Unstructured,
            _: &Module,
            builder: &mut CodeBuilder,
        ) -> Result<Instruction> {
            builder.pop_operands(&[ValType::V128, ValType::I32]);
            builder.push_operands(&[ValType::V128]);
            Ok(Instruction::$instruction)
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
simd_binop!(I8x16RoundingAverageU, i8x16_rounding_average_u);
simd_unop!(I16x8ExtAddPairwiseI8x16S, i16x8_ext_add_pairwise_i8x16s);
simd_unop!(I16x8ExtAddPairwiseI8x16U, i16x8_ext_add_pairwise_i8x16u);
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
simd_binop!(I16x8RoundingAverageU, i16x8_rounding_average_u);
simd_binop!(I16x8ExtMulLowI8x16S, i16x8_ext_mul_low_i8x16s);
simd_binop!(I16x8ExtMulHighI8x16S, i16x8_ext_mul_high_i8x16s);
simd_binop!(I16x8ExtMulLowI8x16U, i16x8_ext_mul_low_i8x16u);
simd_binop!(I16x8ExtMulHighI8x16U, i16x8_ext_mul_high_i8x16u);
simd_unop!(I32x4ExtAddPairwiseI16x8S, i32x4_ext_add_pairwise_i16x8s);
simd_unop!(I32x4ExtAddPairwiseI16x8U, i32x4_ext_add_pairwise_i16x8u);
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
simd_binop!(I32x4ExtMulLowI16x8S, i32x4_ext_mul_low_i16x8s);
simd_binop!(I32x4ExtMulHighI16x8S, i32x4_ext_mul_high_i16x8s);
simd_binop!(I32x4ExtMulLowI16x8U, i32x4_ext_mul_low_i16x8u);
simd_binop!(I32x4ExtMulHighI16x8U, i32x4_ext_mul_high_i16x8u);
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
simd_binop!(I64x2ExtMulLowI32x4S, i64x2_ext_mul_low_i32x4s);
simd_binop!(I64x2ExtMulHighI32x4S, i64x2_ext_mul_high_i32x4s);
simd_binop!(I64x2ExtMulLowI32x4U, i64x2_ext_mul_low_i32x4u);
simd_binop!(I64x2ExtMulHighI32x4U, i64x2_ext_mul_high_i32x4u);
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
simd_unop!(I32x4RelaxedTruncSatF32x4S, i32x4_relaxed_trunc_sat_f32x4s);
simd_unop!(I32x4RelaxedTruncSatF32x4U, i32x4_relaxed_trunc_sat_f32x4u);
simd_unop!(
    I32x4RelaxedTruncSatF64x2SZero,
    i32x4_relaxed_trunc_sat_f64x2s_zero
);
simd_unop!(
    I32x4RelaxedTruncSatF64x2UZero,
    i32x4_relaxed_trunc_sat_f64x2u_zero
);
simd_ternop!(F32x4Fma, f32x4_fma);
simd_ternop!(F32x4Fms, f32x4_fms);
simd_ternop!(F64x2Fma, f64x2_fma);
simd_ternop!(F64x2Fms, f64x2_fms);
simd_ternop!(I8x16LaneSelect, i8x16_laneselect);
simd_ternop!(I16x8LaneSelect, i16x8_laneselect);
simd_ternop!(I32x4LaneSelect, i32x4_laneselect);
simd_ternop!(I64x2LaneSelect, i64x2_laneselect);
simd_binop!(F32x4RelaxedMin, f32x4_relaxed_min);
simd_binop!(F32x4RelaxedMax, f32x4_relaxed_max);
simd_binop!(F64x2RelaxedMin, f64x2_relaxed_min);
simd_binop!(F64x2RelaxedMax, f64x2_relaxed_max);
