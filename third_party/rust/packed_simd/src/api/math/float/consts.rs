macro_rules! impl_float_consts {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident) => {
        impl $id {
            /// Machine epsilon value.
            pub const EPSILON: $id = $id::splat(core::$elem_ty::EPSILON);

            /// Smallest finite value.
            pub const MIN: $id = $id::splat(core::$elem_ty::MIN);

            /// Smallest positive normal value.
            pub const MIN_POSITIVE: $id = $id::splat(core::$elem_ty::MIN_POSITIVE);

            /// Largest finite value.
            pub const MAX: $id = $id::splat(core::$elem_ty::MAX);

            /// Not a Number (NaN).
            pub const NAN: $id = $id::splat(core::$elem_ty::NAN);

            /// Infinity (∞).
            pub const INFINITY: $id = $id::splat(core::$elem_ty::INFINITY);

            /// Negative infinity (-∞).
            pub const NEG_INFINITY: $id = $id::splat(core::$elem_ty::NEG_INFINITY);

            /// Archimedes' constant (π)
            pub const PI: $id = $id::splat(core::$elem_ty::consts::PI);

            /// π/2
            pub const FRAC_PI_2: $id = $id::splat(core::$elem_ty::consts::FRAC_PI_2);

            /// π/3
            pub const FRAC_PI_3: $id = $id::splat(core::$elem_ty::consts::FRAC_PI_3);

            /// π/4
            pub const FRAC_PI_4: $id = $id::splat(core::$elem_ty::consts::FRAC_PI_4);

            /// π/6
            pub const FRAC_PI_6: $id = $id::splat(core::$elem_ty::consts::FRAC_PI_6);

            /// π/8
            pub const FRAC_PI_8: $id = $id::splat(core::$elem_ty::consts::FRAC_PI_8);

            /// 1/π
            pub const FRAC_1_PI: $id = $id::splat(core::$elem_ty::consts::FRAC_1_PI);

            /// 2/π
            pub const FRAC_2_PI: $id = $id::splat(core::$elem_ty::consts::FRAC_2_PI);

            /// 2/sqrt(π)
            pub const FRAC_2_SQRT_PI: $id = $id::splat(core::$elem_ty::consts::FRAC_2_SQRT_PI);

            /// sqrt(2)
            pub const SQRT_2: $id = $id::splat(core::$elem_ty::consts::SQRT_2);

            /// 1/sqrt(2)
            pub const FRAC_1_SQRT_2: $id = $id::splat(core::$elem_ty::consts::FRAC_1_SQRT_2);

            /// Euler's number (e)
            pub const E: $id = $id::splat(core::$elem_ty::consts::E);

            /// log<sub>2</sub>(e)
            pub const LOG2_E: $id = $id::splat(core::$elem_ty::consts::LOG2_E);

            /// log<sub>10</sub>(e)
            pub const LOG10_E: $id = $id::splat(core::$elem_ty::consts::LOG10_E);

            /// ln(2)
            pub const LN_2: $id = $id::splat(core::$elem_ty::consts::LN_2);

            /// ln(10)
            pub const LN_10: $id = $id::splat(core::$elem_ty::consts::LN_10);
        }
    };
}
