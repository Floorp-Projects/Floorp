# any_all_workaround

This is a workaround for bad codegen ([Rust bug](https://github.com/rust-lang/portable-simd/issues/146), [LLVM bug](https://github.com/llvm/llvm-project/issues/50466)) for the `any()` and `all()` reductions for NEON-backed SIMD vectors on 32-bit ARM. On other platforms these delegate to `any()` and `all()` in `core::simd`.

The plan is to abandon this crate once the LLVM bug is fixed or `core::simd` works around the LLVM bug.

The code is forked from the [`packed_simd` crate](https://raw.githubusercontent.com/hsivonen/packed_simd/d938e39bee9bc5c222f5f2f2a0df9e53b5ce36ae/src/codegen/reductions/mask/arm.rs).

This crate requires Nightly Rust as it depends on the `portable_simd` feature.

# License

`MIT OR Apache-2.0`, since that's how `packed_simd` is licensed. (The ARM intrinsics Rust version workaround is from qcms, see LICENSE-MIT-QCMS.)
