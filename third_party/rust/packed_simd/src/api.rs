//! Implements the Simd<[T; N]> APIs

#[macro_use]
mod bitmask;
pub(crate) mod cast;
#[macro_use]
mod cmp;
#[macro_use]
mod default;
#[macro_use]
mod fmt;
#[macro_use]
mod from;
#[macro_use]
mod hash;
#[macro_use]
mod math;
#[macro_use]
mod minimal;
#[macro_use]
mod ops;
#[macro_use]
mod ptr;
#[macro_use]
mod reductions;
#[macro_use]
mod select;
#[macro_use]
mod shuffle;
#[macro_use]
mod shuffle1_dyn;
#[macro_use]
mod slice;
#[macro_use]
mod swap_bytes;
#[macro_use]
mod bit_manip;

#[cfg(feature = "into_bits")]
pub(crate) mod into_bits;

macro_rules! impl_i {
    ([$elem_ty:ident; $elem_n:expr]: $tuple_id:ident, $mask_ty:ident
     | $ielem_ty:ident, $ibitmask_ty:ident | $test_tt:tt | $($elem_ids:ident),*
     | From: $($from_vec_ty:ident),* | $(#[$doc:meta])*) => {
        impl_minimal_iuf!([$elem_ty; $elem_n]: $tuple_id | $ielem_ty | $test_tt
                          | $($elem_ids),* | $(#[$doc])*);
        impl_ops_vector_arithmetic!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_ops_scalar_arithmetic!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_ops_vector_bitwise!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt | (!(0 as $elem_ty), 0)
        );
        impl_ops_scalar_bitwise!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt | (!(0 as $elem_ty), 0)
        );
        impl_ops_vector_shifts!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_ops_scalar_shifts!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_ops_vector_rotates!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_ops_vector_neg!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_ops_vector_int_min_max!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt
        );
        impl_reduction_integer_arithmetic!(
            [$elem_ty; $elem_n]: $tuple_id | $ielem_ty | $test_tt
        );
        impl_reduction_min_max!(
            [$elem_ty; $elem_n]: $tuple_id | $ielem_ty | $test_tt
        );
        impl_reduction_bitwise!(
            [$elem_ty; $elem_n]: $tuple_id | $ielem_ty | $test_tt
            | (|x|{ x as $elem_ty }) | (!(0 as $elem_ty), 0)
        );
        impl_fmt_debug!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_fmt_lower_hex!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_fmt_upper_hex!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_fmt_octal!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_fmt_binary!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_from_array!([$elem_ty; $elem_n]: $tuple_id | $test_tt | (1, 1));
        impl_from_vectors!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt | $($from_vec_ty),*
        );
        impl_default!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_hash!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_slice_from_slice!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_slice_write_to_slice!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_swap_bytes!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_bit_manip!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_shuffle1_dyn!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_cmp_partial_eq!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt | (0, 1)
        );
        impl_cmp_eq!([$elem_ty; $elem_n]: $tuple_id | $test_tt | (0, 1));
        impl_cmp_vertical!(
            [$elem_ty; $elem_n]: $tuple_id, $mask_ty, false, (1, 0) | $test_tt
        );
        impl_cmp_partial_ord!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_cmp_ord!([$elem_ty; $elem_n]: $tuple_id | $test_tt | (0, 1));
        impl_bitmask!($tuple_id | $ibitmask_ty | (-1, 0) | $test_tt);

        test_select!($elem_ty, $mask_ty, $tuple_id, (1, 2) | $test_tt);
        test_cmp_partial_ord_int!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        test_shuffle1_dyn!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
    }
}

macro_rules! impl_u {
    ([$elem_ty:ident; $elem_n:expr]: $tuple_id:ident, $mask_ty:ident
     | $ielem_ty:ident, $ibitmask_ty:ident | $test_tt:tt | $($elem_ids:ident),*
     | From: $($from_vec_ty:ident),* | $(#[$doc:meta])*) => {
        impl_minimal_iuf!([$elem_ty; $elem_n]: $tuple_id | $ielem_ty | $test_tt
                          | $($elem_ids),* | $(#[$doc])*);
        impl_ops_vector_arithmetic!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_ops_scalar_arithmetic!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_ops_vector_bitwise!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt | (!(0 as $elem_ty), 0)
        );
        impl_ops_scalar_bitwise!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt | (!(0 as $elem_ty), 0)
        );
        impl_ops_vector_shifts!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_ops_scalar_shifts!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_ops_vector_rotates!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_ops_vector_int_min_max!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt
        );
        impl_reduction_integer_arithmetic!(
            [$elem_ty; $elem_n]: $tuple_id | $ielem_ty | $test_tt
        );
        impl_reduction_min_max!(
            [$elem_ty; $elem_n]: $tuple_id | $ielem_ty | $test_tt
        );
        impl_reduction_bitwise!(
            [$elem_ty; $elem_n]: $tuple_id | $ielem_ty | $test_tt
            | (|x|{ x as $elem_ty }) | (!(0 as $elem_ty), 0)
        );
        impl_fmt_debug!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_fmt_lower_hex!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_fmt_upper_hex!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_fmt_octal!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_fmt_binary!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_from_array!([$elem_ty; $elem_n]: $tuple_id | $test_tt | (1, 1));
        impl_from_vectors!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt | $($from_vec_ty),*
        );
        impl_default!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_hash!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_slice_from_slice!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_slice_write_to_slice!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_swap_bytes!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_bit_manip!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_shuffle1_dyn!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_cmp_partial_eq!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt | (1, 0)
        );
        impl_cmp_eq!([$elem_ty; $elem_n]: $tuple_id | $test_tt | (0, 1));
        impl_cmp_vertical!(
            [$elem_ty; $elem_n]: $tuple_id, $mask_ty, false, (1, 0) | $test_tt
        );
        impl_cmp_partial_ord!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_cmp_ord!([$elem_ty; $elem_n]: $tuple_id | $test_tt | (0, 1));
        impl_bitmask!($tuple_id | $ibitmask_ty | ($ielem_ty::max_value(), 0) |
                      $test_tt);

        test_select!($elem_ty, $mask_ty, $tuple_id, (1, 2) | $test_tt);
        test_cmp_partial_ord_int!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        test_shuffle1_dyn!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
    }
}

macro_rules! impl_f {
    ([$elem_ty:ident; $elem_n:expr]: $tuple_id:ident, $mask_ty:ident
     | $ielem_ty:ident | $test_tt:tt | $($elem_ids:ident),*
     | From: $($from_vec_ty:ident),* | $(#[$doc:meta])*) => {
        impl_minimal_iuf!([$elem_ty; $elem_n]: $tuple_id | $ielem_ty | $test_tt
                          | $($elem_ids),* | $(#[$doc])*);
        impl_ops_vector_arithmetic!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_ops_scalar_arithmetic!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_ops_vector_neg!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_ops_vector_float_min_max!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt
        );
        impl_reduction_float_arithmetic!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_reduction_min_max!(
            [$elem_ty; $elem_n]: $tuple_id | $ielem_ty | $test_tt
        );
        impl_fmt_debug!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_from_array!([$elem_ty; $elem_n]: $tuple_id | $test_tt | (1., 1.));
        impl_from_vectors!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt | $($from_vec_ty),*
        );
        impl_default!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_cmp_partial_eq!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt | (1., 0.)
        );
        impl_slice_from_slice!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_slice_write_to_slice!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_shuffle1_dyn!([$elem_ty; $elem_n]: $tuple_id | $test_tt);

        impl_float_consts!([$elem_ty; $elem_n]: $tuple_id);
        impl_float_category!([$elem_ty; $elem_n]: $tuple_id, $mask_ty);

        // floating-point math
        impl_math_float_abs!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_math_float_cos!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_math_float_exp!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_math_float_ln!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_math_float_mul_add!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_math_float_mul_adde!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_math_float_powf!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_math_float_recpre!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_math_float_rsqrte!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_math_float_sin!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_math_float_sqrt!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_math_float_sqrte!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_math_float_tanh!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_cmp_vertical!(
            [$elem_ty; $elem_n]: $tuple_id, $mask_ty, false, (1., 0.)
                | $test_tt
        );

        test_select!($elem_ty, $mask_ty, $tuple_id, (1., 2.) | $test_tt);
        test_reduction_float_min_max!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt
        );
        test_shuffle1_dyn!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
    }
}

macro_rules! impl_m {
    ([$elem_ty:ident; $elem_n:expr]: $tuple_id:ident
     | $ielem_ty:ident, $ibitmask_ty:ident
     | $test_tt:tt | $($elem_ids:ident),* | From: $($from_vec_ty:ident),*
     | $(#[$doc:meta])*) => {
        impl_minimal_mask!(
            [$elem_ty; $elem_n]: $tuple_id | $ielem_ty | $test_tt
            | $($elem_ids),* | $(#[$doc])*
        );
        impl_ops_vector_mask_bitwise!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt | (true, false)
        );
        impl_ops_scalar_mask_bitwise!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt | (true, false)
        );
        impl_reduction_bitwise!(
            [bool; $elem_n]: $tuple_id | $ielem_ty | $test_tt
                | (|x|{ x != 0 }) | (true, false)
        );
        impl_reduction_mask!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_fmt_debug!([bool; $elem_n]: $tuple_id | $test_tt);
        impl_from_array!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt
            | (crate::$elem_ty::new(true), true)
        );
        impl_from_vectors!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt | $($from_vec_ty),*
        );
        impl_default!([bool; $elem_n]: $tuple_id | $test_tt);
        impl_cmp_partial_eq!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt | (true, false)
        );
        impl_cmp_eq!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt | (true, false)
        );
        impl_cmp_vertical!(
            [$elem_ty; $elem_n]: $tuple_id, $tuple_id, true, (true, false)
            | $test_tt
        );
        impl_select!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_cmp_partial_ord!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_cmp_ord!(
            [$elem_ty; $elem_n]: $tuple_id | $test_tt | (false, true)
        );
        impl_shuffle1_dyn!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        impl_bitmask!($tuple_id | $ibitmask_ty | (true, false) | $test_tt);

        test_cmp_partial_ord_mask!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
        test_shuffle1_dyn_mask!([$elem_ty; $elem_n]: $tuple_id | $test_tt);
    }
}

macro_rules! impl_const_p {
    ([$elem_ty:ty; $elem_n:expr]: $tuple_id:ident, $mask_ty:ident,
     $usize_ty:ident, $isize_ty:ident
     | $test_tt:tt | $($elem_ids:ident),*
     | From: $($from_vec_ty:ident),* | $(#[$doc:meta])*) => {
        impl_minimal_p!(
            [$elem_ty; $elem_n]: $tuple_id, $mask_ty, $usize_ty, $isize_ty
                | ref_ | $test_tt | $($elem_ids),*
                | (1 as $elem_ty, 0 as $elem_ty) | $(#[$doc])*
        );
        impl_ptr_read!([$elem_ty; $elem_n]: $tuple_id, $mask_ty | $test_tt);
    }
}

macro_rules! impl_mut_p {
    ([$elem_ty:ty; $elem_n:expr]: $tuple_id:ident, $mask_ty:ident,
     $usize_ty:ident, $isize_ty:ident
     | $test_tt:tt | $($elem_ids:ident),*
     | From: $($from_vec_ty:ident),* | $(#[$doc:meta])*) => {
        impl_minimal_p!(
            [$elem_ty; $elem_n]: $tuple_id, $mask_ty, $usize_ty, $isize_ty
                | ref_mut_ | $test_tt | $($elem_ids),*
                | (1 as $elem_ty, 0 as $elem_ty) | $(#[$doc])*
        );
        impl_ptr_read!([$elem_ty; $elem_n]: $tuple_id, $mask_ty | $test_tt);
        impl_ptr_write!([$elem_ty; $elem_n]: $tuple_id, $mask_ty | $test_tt);
    }
}
