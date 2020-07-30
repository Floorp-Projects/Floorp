#ifndef LUCET_VAL_H
#define LUCET_VAL_H 1

/**
 * Typed values
 *
 * `struct lucet_val` represents a typed value, used in arguments lists.
 * Such arguments can be built with the `LUCET_VAL_*` convenience macros.
 *
 * A guest function call with these arguments eventually returns a
 * `struct lucet_untyped_retval` value, that can be converted to a
 * native type with the `LUCET_UNTYPED_RETVAL_TO_*` macros.
 *
 * Usage:
 *
 * lucet_instance_run(inst, "add_2", 2, (struct lucet_val[]){ LUCET_VAL_U64(123), LUCET_VAL_U64(456)
 * }); lucet_instance_state(inst, &state); uint64_t res =
 * LUCET_UNTYPED_RETVAL_TO_U64(state.val.returned);
 */

#include <sys/types.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Creates a lucet_val value from the given type

#define LUCET_VAL_T(T, C, X) \
    ((struct lucet_val[1]){ { .ty = lucet_val_type_##T, .inner_val.C = (X) } }[0])

#define LUCET_VAL_C_PTR(X) LUCET_VAL_T(c_ptr, as_c_ptr, X)
#define LUCET_VAL_GUEST_PTR(X) LUCET_VAL_T(guest_ptr, as_u64, X)

#define LUCET_VAL_U8(X) LUCET_VAL_T(u8, as_u64, X)
#define LUCET_VAL_U16(X) LUCET_VAL_T(u16, as_u64, X)
#define LUCET_VAL_U32(X) LUCET_VAL_T(u32, as_u64, X)
#define LUCET_VAL_U64(X) LUCET_VAL_T(u64, as_u64, X)

#define LUCET_VAL_I8(X) LUCET_VAL_T(i8, as_i64, X)
#define LUCET_VAL_I16(X) LUCET_VAL_T(i16, as_i64, X)
#define LUCET_VAL_I32(X) LUCET_VAL_T(i32, as_i64, X)
#define LUCET_VAL_I64(X) LUCET_VAL_T(i64, as_i64, X)

#define LUCET_VAL_USIZE(X) LUCET_VAL_T(usize, as_u64, X)
#define LUCET_VAL_ISIZE(X) LUCET_VAL_T(isize, as_i64, X)

#define LUCET_VAL_BOOL(X) LUCET_VAL_T(bool, as_u64, X)

#define LUCET_VAL_F32(X) LUCET_VAL_T(f32, as_f32, X)
#define LUCET_VAL_F64(X) LUCET_VAL_T(f64, as_f64, X)

// Converts a lucet_val value to the given type

#define LUCET_VAL_TO_T(T, C, V) ((T)((V).inner_val.C))

#define LUCET_VAL_TO_C_PTR(X) LUCET_VAL_TO_T(void *, as_c_ptr, X)
#define LUCET_VAL_TO_GUEST_PTR(X) LUCET_VAL_TO_T(guest_ptr_t, as_u64, X)

#define LUCET_VAL_TO_U8(X) LUCET_VAL_TO_T(uint8_t, as_u64, X)
#define LUCET_VAL_TO_U16(X) LUCET_VAL_TO_T(uint16_t, as_u64, X)
#define LUCET_VAL_TO_U32(X) LUCET_VAL_TO_T(uint32_t, as_u64, X)
#define LUCET_VAL_TO_U64(X) LUCET_VAL_TO_T(uint64_t, as_u64, X)

#define LUCET_VAL_TO_I8(X) LUCET_VAL_TO_T(int8_t, as_i64, X)
#define LUCET_VAL_TO_I16(X) LUCET_VAL_TO_T(int16_t, as_i64, X)
#define LUCET_VAL_TO_I32(X) LUCET_VAL_TO_T(int32_t, as_i64, X)
#define LUCET_VAL_TO_I64(X) LUCET_VAL_TO_T(int64_t, as_i64, X)

#define LUCET_VAL_TO_USIZE(X) LUCET_VAL_TO_T(size_t, as_u64, X)
#define LUCET_VAL_TO_ISIZE(X) LUCET_VAL_TO_T(ssize_t, as_i64, X)

#define LUCET_VAL_TO_BOOL(X) LUCET_VAL_TO_T(bool, as_u64, X)

#define LUCET_VAL_TO_F32(X) LUCET_VAL_TO_T(float, as_f32, X)
#define LUCET_VAL_TO_F64(X) LUCET_VAL_TO_T(double, as_f64, X)

// Converts an untyped return value to the given type

#define LUCET_UNTYPED_RETVAL_TO_GP_T(T, C, X) ((T) lucet_retval_gp(&(X)).C)
#define LUCET_UNTYPED_RETVAL_TO_C_PTR(X) LUCET_UNTYPED_RETVAL_TO_GP_T(void *, as_c_ptr, &(X))

#define LUCET_UNTYPED_RETVAL_TO_GUEST_PTR(X) LUCET_UNTYPED_RETVAL_TO_GP_T(guest_ptr_t, as_u64, X)

#define LUCET_UNTYPED_RETVAL_TO_U8(X) LUCET_UNTYPED_RETVAL_TO_GP_T(uint8_t, as_u64, X)
#define LUCET_UNTYPED_RETVAL_TO_U16(X) LUCET_UNTYPED_RETVAL_TO_GP_T(uint16_t, as_u64, X)
#define LUCET_UNTYPED_RETVAL_TO_U32(X) LUCET_UNTYPED_RETVAL_TO_GP_T(uint32_t, as_u64, X)
#define LUCET_UNTYPED_RETVAL_TO_U64(X) LUCET_UNTYPED_RETVAL_TO_GP_T(uint64_t, as_u64, X)

#define LUCET_UNTYPED_RETVAL_TO_I8(X) LUCET_UNTYPED_RETVAL_TO_GP_T(int8_t, as_i64, X)
#define LUCET_UNTYPED_RETVAL_TO_I16(X) LUCET_UNTYPED_RETVAL_TO_GP_T(int16_t, as_i64, X)
#define LUCET_UNTYPED_RETVAL_TO_I32(X) LUCET_UNTYPED_RETVAL_TO_GP_T(int32_t, as_i64, X)
#define LUCET_UNTYPED_RETVAL_TO_I64(X) LUCET_UNTYPED_RETVAL_TO_GP_T(int64_t, as_i64, X)

#define LUCET_UNTYPED_RETVAL_TO_USIZE(X) LUCET_UNTYPED_RETVAL_TO_GP_T(size_t, as_u64, X)
#define LUCET_UNTYPED_RETVAL_TO_ISIZE(X) LUCET_UNTYPED_RETVAL_TO_GP_T(ssize_t, as_i64, X)

#define LUCET_UNTYPED_RETVAL_TO_BOOL(X) LUCET_UNTYPED_RETVAL_TO_GP_T(bool, as_u64, X)

#define LUCET_UNTYPED_RETVAL_TO_F32(X) lucet_retval_f32(&(X))
#define LUCET_UNTYPED_RETVAL_TO_F64(X) lucet_retval_f64(&(X))

#endif
